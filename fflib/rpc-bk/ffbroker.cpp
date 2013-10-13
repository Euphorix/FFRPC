#include "rpc/ffbroker.h"
#include "base/log.h"
#include "base/arg_helper.h"

using namespace ff;

ffbroker_t::ffbroker_t():
    m_alloc_slave_broker_index(0),
    m_master_broker_sock(NULL),
    m_node_id_index(0)
{
}
ffbroker_t::~ffbroker_t()
{

}

//! 分配一个nodeid
uint32_t ffbroker_t::alloc_id()
{
    uint32_t ret = ++m_node_id_index;
    if (0 == ret)
    {
        return ++m_node_id_index;
    }
    return ret;
}

int ffbroker_t::open(arg_helper_t& arg)
{
    net_factory_t::start(1);
    m_listen_host = arg.get_option_value("-broker");
    acceptor_i* acceptor = net_factory_t::listen(m_listen_host, this);
    if (NULL == acceptor)
    {
        LOGERROR((BROKER, "ffbroker_t::open failed<%s>", m_listen_host));
        return -1;
    }
    //! 绑定cmd 对应的回调函数
    m_ffslot.bind(BROKER_SLAVE_REGISTER, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_slave_register, this))
            .bind(BROKER_CLIENT_REGISTER, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_client_register, this))
            .bind(BROKER_ROUTE_MSG, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_route_msg, this))
            .bind(CLIENT_REGISTER_TO_SLAVE_BROKER, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_client_register_slave_broker, this))
            .bind(BROKER_SYNC_DATA_MSG, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_broker_sync_data, this))
            .bind(BROKER_TO_BRIDGE_ROUTE_MSG, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_broker_to_bridge_route_msg, this))
            .bind(BRIDGE_TO_BROKER_ROUTE_MSG, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_bridge_to_broker_route_msg, this))
            .bind(BRIDGE_BROKER_TO_BROKER_MSG, ffrpc_ops_t::gen_callback(&ffbroker_t::bridge_handle_broker_to_broker_msg, this))
            .bind(BROKER_BRIDGE_REGISTER, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_broker_register_bridge, this));

            

    //! 任务队列绑定线程
    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);

    LOGTRACE((BROKER, "ffbroker_t::open accept ok<%s>", m_listen_host));
    if (arg.is_enable_option("-master_broker"))
    {
        m_broker_host = arg.get_option_value("-master_broker");
        if (connect_to_master_broker())
        {
            return -1;
        }
        //! 注册到master broker
    }
    else//! 内存中注册此broker
    {
        singleton_t<ffrpc_memory_route_t>::instance().add_node(BROKER_MASTER_NODE_ID, this);
    }
    
    //! 如果需要连接broker bridge，解析参数，连接
    if (arg.is_enable_option("-bridge_broker"))
    {
        string broker_bridge_ops = arg.get_option_value("-bridge_broker");
        vector<string> bridge_config;
        strtool::split(broker_bridge_ops, bridge_config, "@");//! @前边的是本身的组名称
        assert(bridge_config.size() == 2);
        m_master_group_name = bridge_config[0];
        //! 支持多个broker bridge，以逗号相隔
        vector<string> bridge_hosts;
        strtool::split(bridge_config[1], bridge_hosts, ",");
        for (size_t i = 0; i < bridge_hosts.size(); ++i)
        {
            broker_bridge_info_t broker_bridge_info;
            broker_bridge_info.host = bridge_hosts[i];
            broker_bridge_info.broker_group_id[m_master_group_name] = alloc_id();//! TODO remove
            m_broker_bridge_info[alloc_id()] = broker_bridge_info;
        }
        connect_to_bridge_broker();
    }
    return 0;
}

//! 连接到broker master
int ffbroker_t::connect_to_master_broker()
{
    if (m_master_broker_sock)
    {
        return 0;
    }
    LOGINFO((BROKER, "ffbroker_t::connect_to_master_broker begin<%s>", m_broker_host.c_str()));
    m_master_broker_sock = net_factory_t::connect(m_broker_host, this);
    if (NULL == m_master_broker_sock)
    {
        LOGERROR((BROKER, "ffbroker_t::register_to_broker_master failed, can't connect to master broker<%s>", m_broker_host.c_str()));
        return -1;
    }
    session_data_t* psession = new session_data_t(BROKER_MASTER_NODE_ID);
    m_master_broker_sock->set_data(psession);

    //! 发送注册消息给master broker
    register_slave_broker_t::in_t msg;
    msg.host = m_listen_host;
    msg_sender_t::send(m_master_broker_sock, BROKER_BRIDGE_REGISTER, msg);
    LOGINFO((BROKER, "ffbroker_t::connect_to_master_broker ok<%s>", m_broker_host.c_str()));
    return 0;
}

//! 连接到broker bridge
int ffbroker_t::connect_to_bridge_broker()
{
    LOGINFO((BROKER, "ffbroker_t::connect_to_bridge_broker begin<%s>", m_master_group_name.c_str()));
    int ret = 0;
    map<uint32_t/*broker bridge id*/, broker_bridge_info_t>::iterator it = m_broker_bridge_info.begin();
    for (; it != m_broker_bridge_info.end(); ++it)
    {
        broker_bridge_info_t& broker_bridge_info = it->second;
        if (broker_bridge_info.sock)
        {
            continue;
        }
        broker_bridge_info.sock = net_factory_t::connect(broker_bridge_info.host, this);
        if (NULL == broker_bridge_info.sock)
        {
            LOGERROR((BROKER, "ffbroker_t::connect_to_bridge_broker failed, can't connect to bridge broker<%s>", broker_bridge_info.host.c_str()));
            ret = -1;
            continue;
        }
        session_data_t* psession = new session_data_t(it->first);
        broker_bridge_info.sock->set_data(psession);

        //! 发送注册消息给master broker
        register_bridge_broker_t::in_t msg;
        msg.broker_group = m_master_group_name;
        msg_sender_t::send(broker_bridge_info.sock, BROKER_BRIDGE_REGISTER, msg);
        LOGINFO((BROKER, "ffbroker_t::connect_to_bridge_broker dest<%s>, node_id[%u]", broker_bridge_info.host, psession->get_node_id()));
    }

    LOGINFO((BROKER, "ffbroker_t::connect_to_bridge_broker ok<%s>", m_master_group_name.c_str()));
    return ret;
}
//! 判断是否是master broker
bool ffbroker_t::is_master()
{
    return m_broker_host.empty();
}
//! 获取任务队列对象
task_queue_t& ffbroker_t::get_tq()
{
    return m_tq;
}
//! 定时器
timer_service_t& ffbroker_t::get_timer()
{
    return m_timer;
}

static void route_call_reconnect(ffbroker_t* ffbroker_);
static void reconnect_loop(ffbroker_t* ffbroker_)
{
    if (ffbroker_->connect_to_master_broker())
    {
        ffbroker_->get_timer().once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, ffbroker_));
    }
    if (ffbroker_->connect_to_bridge_broker())
    {
        ffbroker_->get_timer().once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, ffbroker_));
    }
}
//! 投递到ffrpc 特定的线程
static void route_call_reconnect(ffbroker_t* ffbroker_)
{
    ffbroker_->get_tq().produce(task_binder_t::gen(&reconnect_loop, ffbroker_));
}

int ffbroker_t::close()
{
    m_tq.close();
    m_thread.join();
    return 0;
}

int ffbroker_t::handle_broken(socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffbroker_t::handle_broken_impl, this, sock_));
    return 0;
}
int ffbroker_t::handle_msg(const message_t& msg_, socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffbroker_t::handle_msg_impl, this, msg_, sock_));
    return 0;
}
//! 当有连接断开，则被回调
int ffbroker_t::handle_broken_impl(socket_ptr_t sock_)
{
    if (NULL == sock_->get_data<session_data_t>())
    {
        sock_->safe_delete();
        return 0;
    }
    uint32_t node_id = sock_->get_data<session_data_t>()->get_node_id();
    if (node_id == BROKER_MASTER_NODE_ID && false == is_master())
    {
        //! slave 连接到master 的连接断开，重连
        //! 设置定时器重连
        m_broker_client_info.clear();
        m_timer.once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, this));
    }
    else
    {
        m_slave_broker_sockets.erase(node_id);
        m_broker_client_info.erase(node_id);
        
        if (m_broker_bridge_info.find(node_id) != m_broker_bridge_info.end())
        {
            //!连接到broker bridge的连接断开了, 定时重连
            m_timer.once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, this));
        }
        else
        {
            //! broker bridge 上所有的 broker master group 信息
            map<string/*group name*/, broker_group_info_t>::iterator it = m_broker_group_info.begin();
            for (; it != m_broker_group_info.end(); ++it)
            {
                if (it->second.sock == sock_)
                {
                    m_broker_group_info.erase(it);
                    break;
                }
            }
        }
    }

    if (is_master())
    {
        //! 如果是master， //!如果是slave broker断开连接，需要为原来的service重新分配slave broker
        sync_all_register_info(NULL);
    }
    delete sock_->get_data<session_data_t>();
    sock_->set_data(NULL);
    sock_->safe_delete();
    return 0;
}
//! 当有消息到来，被回调
int ffbroker_t::handle_msg_impl(const message_t& msg_, socket_ptr_t sock_)
{
    uint16_t cmd = msg_.get_cmd();
    LOGTRACE((BROKER, "ffbroker_t::handle_msg_impl cmd<%u> begin", cmd));

    ffslot_t::callback_t* cb = m_ffslot.get_callback(cmd);
    if (cb)
    {
        try
        {
            ffslot_msg_arg arg(msg_.get_body(), sock_);
            cb->exe(&arg);
            LOGTRACE((BROKER, "ffbroker_t::handle_msg_impl cmd<%u> end ok", cmd));
            return 0;
        }
        catch(exception& e_)
        {
            LOGERROR((BROKER, "ffbroker_t::handle_msg_impl exception<%s>", e_.what()));
            return -1;
        }
    }
    return -1;
}

//! 处理broker master 注册到broker bridge
int ffbroker_t::handle_broker_register_bridge(register_bridge_broker_t::in_t& msg_, socket_ptr_t sock_)
{
    session_data_t* psession = new session_data_t(alloc_id());
    sock_->set_data(psession);
    m_broker_group_info[msg_.broker_group].sock = sock_;
    LOGINFO((BROKER, "ffbroker_t::handle_broker_register_bridge end ok broker group[%s]", msg_.broker_group));
    return 0;
}

//! 处理borker slave 注册消息
int ffbroker_t::handle_slave_register(register_slave_broker_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_slave_register begin"));
    session_data_t* psession = new session_data_t(alloc_id());
    sock_->set_data(psession);
    slave_broker_info_t& slave_broker_info = m_slave_broker_sockets[psession->get_node_id()];
    slave_broker_info.host = msg_.host;
    slave_broker_info.sock = sock_;
    sync_all_register_info(sock_);
    LOGTRACE((BROKER, "ffbroker_t::handle_slave_register end ok new node id[%u]", psession->get_node_id()));
    return 0;
}

//! 处理borker client 注册消息
int ffbroker_t::handle_client_register(register_broker_client_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_client_register begin"));

    if (msg_.service_name.empty() == false)
    {
        for (map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.begin();
             it != m_broker_client_info.end(); ++it)
        {
            broker_client_info_t& broker_client_info = it->second;
            if (broker_client_info.service_name == msg_.service_name)
            {
                sock_->close();
                LOGTRACE((BROKER, "ffbroker_t::handle_client_register service<%s> has been registed",
                                    msg_.service_name.c_str()));
                return -1;
            }
        }
    }
    session_data_t* psession = new session_data_t(alloc_id());
    sock_->set_data(psession);

    LOGTRACE((BROKER, "ffbroker_t::handle_client_register alloc node id<%u>,binder_broker[%u]", psession->get_node_id(), msg_.binder_broker_node_id));

    broker_client_info_t& broker_client_info = m_broker_client_info[psession->get_node_id()];
    broker_client_info.bind_broker_id        = msg_.binder_broker_node_id;
    if (broker_client_info.bind_broker_id < 0)
    {
        broker_client_info.bind_broker_id        = alloc_broker_id();
    }
    broker_client_info.service_name          = msg_.service_name;
    broker_client_info.sock                  = sock_;

    for (std::set<string>::iterator it = msg_.msg_names.begin(); it != msg_.msg_names.end(); ++it)
    {
        if (m_msg2id.find(*it) != m_msg2id.end())
        {
            continue;
        }
        m_msg2id[*it] = alloc_id();
    }

    sync_all_register_info(sock_);
    LOGTRACE((BROKER, "ffbroker_t::handle_client_register end ok"));
    return 0;
}
//! 分配一个broker 给client,以后client的消息都通过此broker转发
uint32_t ffbroker_t::alloc_broker_id()
{
    //! 若本进程内有broker 那么直接分配本进程的
    if (false == m_slave_broker_sockets.empty())
    {
        uint32_t index = m_alloc_slave_broker_index++;
        index = index % m_slave_broker_sockets.size();
        //! 分配一个broker slave，如果有的话
        map<uint32_t, slave_broker_info_t>::iterator it = m_slave_broker_sockets.begin();
        for (uint32_t i = 0; it != m_slave_broker_sockets.end(); ++it)
        {
            if (i >= index)
            {
                return it->first; 
            }
            ++i;
        }
    }
    return BROKER_MASTER_NODE_ID;
}

//! 同步所有的当前的注册接口信息
int ffbroker_t::sync_all_register_info(socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::sync_all_register_info begin"));
    broker_sync_all_registered_data_t::out_t msg;
    msg.msg2id = m_msg2id;
    for (map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.begin();
         it != m_broker_client_info.end(); ++it)
    {
        //!在取值之前，要检测一下对应的broker node id，是否存在，若已经失效，则重新分配一个
        uint32_t& bind_broker_node_id = it->second.bind_broker_id;
        if (is_master() && m_slave_broker_sockets.find(bind_broker_node_id) == m_slave_broker_sockets.end() &&
            bind_broker_node_id != BROKER_MASTER_NODE_ID)
        {
            bind_broker_node_id = alloc_broker_id();
        }
        msg.broker_client_info[it->first].bind_broker_id      = bind_broker_node_id;
        msg.broker_client_info[it->first].service_name        = it->second.service_name;
    }
    //! 把所有已注册的broker slave节点赋值到消息
    for (map<uint32_t, slave_broker_info_t>::iterator it = m_slave_broker_sockets.begin();
         it != m_slave_broker_sockets.end(); ++it)
    {
        msg.slave_broker_info[it->first].host = it->second.host;
    }
    
    //! 给所有已注册的broker slave节点推送所有的消息
    for (map<uint32_t, slave_broker_info_t>::iterator it = m_slave_broker_sockets.begin();
         it != m_slave_broker_sockets.end(); ++it)
    {
        if (sock_ == it->second.sock)
        {
            msg.node_id = sock_->get_data<session_data_t>()->get_node_id();
        }
        else
        {
            msg.node_id = 0;
        }
        msg_sender_t::send(it->second.sock, BROKER_SYNC_DATA_MSG, msg);
    }
    //! 给所有已注册的broker client节点推送所有的消息
    for (map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.begin();
         it != m_broker_client_info.end(); ++it)
    {
        if (sock_ == it->second.sock)
        {
            msg.node_id = sock_->get_data<session_data_t>()->get_node_id();
        }
        else
        {
            msg.node_id = 0;
        }
        msg_sender_t::send(it->second.sock, BROKER_SYNC_DATA_MSG, msg);
        LOGTRACE((BROKER, "ffbroker_t::sync_all_register_info to node_id[%u]", msg.node_id));
    }
    LOGTRACE((BROKER, "ffbroker_t::sync_all_register_info end ok"));
    return 0;
}

//! 转发消息
int ffbroker_t::handle_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_)
{
    return route_msg_to_broker_client(msg_);
}
pair<string, socket_ptr_t> ffbroker_t::get_broker_group_name_by_id(uint32_t bridge_route_id_)
{
    map<uint32_t/*broker bridge id*/, broker_bridge_info_t>::iterator it = m_broker_bridge_info.begin();
    
    for (; it != m_broker_bridge_info.end(); ++it)
    {
        map<string/*group name*/, uint32_t>::iterator it2 = it->second.broker_group_id.begin();
        for (; it2 != it->second.broker_group_id.end(); ++it2)
        {
            if (it2->second == bridge_route_id_)
            {
                return make_pair(it2->first, it->second.sock);
            }
        }
    }
    return pair<string, socket_ptr_t>();
}
//! client [1] -> master [2] -> bridge[3] -> master [4] -> client [5]
//! broker client 转发消息给broker master, 再转给bridge broker
//! [1]
int ffbroker_t::route_msg_broker_to_bridge(const string& from_broker_group_name_, const string& dest_broker_group_name_, const string& service_name_,
                                           const string& msg_name_, const string& body_,
                                           uint32_t from_node_id_, uint32_t dest_node_id_,
                                           uint32_t callback_id_, socket_ptr_t sock_)
{
    broker_route_to_bridge_t::in_t dest_msg;
    dest_msg.from_broker_group_name = from_broker_group_name_;
    dest_msg.dest_broker_group_name = dest_broker_group_name_;
    dest_msg.service_name = service_name_;//!  服务名
    dest_msg.msg_name = msg_name_;//!消息名
    dest_msg.body = body_;//! msg data
    dest_msg.from_node_id = from_node_id_;
    dest_msg.dest_node_id = dest_node_id_;
    dest_msg.callback_id  = callback_id_;//! 回调函数的id
    msg_sender_t::send(sock_, BROKER_TO_BRIDGE_ROUTE_MSG, dest_msg);
    LOGINFO((BROKER, "ffbroker_t::route_msg_broker_to_bridge dest broker group[%s]", dest_broker_group_name_));
    return 0;
}
//! [2]
//! 这里是broker master 发给 broker bridge 的消息
int ffbroker_t::handle_broker_to_bridge_route_msg(broker_route_to_bridge_t::in_t& msg_, socket_ptr_t sock_)
{
    socket_ptr_t dest_sock = NULL;
    LOGINFO((BROKER, "ffbroker_t::handle_broker_to_bridge_route_msg begin broker_group[%s]", msg_.dest_broker_group_name));
    //! 找到目标broker group在哪个bridge上
    map<uint32_t/*broker bridge id*/, broker_bridge_info_t>::iterator it = m_broker_bridge_info.begin();
    for (; it != m_broker_bridge_info.end(); ++it)
    {
        if (it->second.broker_group_id.find(msg_.dest_broker_group_name) != it->second.broker_group_id.end())
        {
            dest_sock = it->second.sock;
            LOGINFO((BROKER, "ffbroker_t::handle_broker_to_bridge_route_msg find group[%s]", msg_.dest_broker_group_name));
            break;
        }
    }

    bridge_route_to_broker_t::in_t dest_msg;
    dest_msg.from_broker_group_name = m_master_group_name;
    dest_msg.dest_broker_group_name = msg_.dest_broker_group_name;
    dest_msg.service_name = msg_.service_name;//!  服务名
    dest_msg.msg_name = msg_.msg_name;//!消息名
    dest_msg.body = msg_.body;//! msg data
    dest_msg.from_node_id = msg_.from_node_id;
    dest_msg.dest_node_id = msg_.dest_node_id;
    dest_msg.callback_id  = msg_.callback_id;//! 回调函数的id
    msg_sender_t::send(dest_sock, BRIDGE_BROKER_TO_BROKER_MSG, dest_msg);
    LOGINFO((BROKER, "ffbroker_t::handle_broker_to_bridge_route_msg end ok dest node id[%u]", dest_msg.dest_node_id));
    return 0;
}

//! [3] bridge的处理函数，从broker master转发到另外的broker master
int ffbroker_t::bridge_handle_broker_to_broker_msg(bridge_route_to_broker_t::in_t& msg_, socket_ptr_t sock_)
{
    map<string/*group name*/, broker_group_info_t>::iterator it = m_broker_group_info.find(msg_.dest_broker_group_name);
    if (it == m_broker_group_info.end())
    {
        return -1;
    }
    msg_sender_t::send(it->second.sock, BRIDGE_TO_BROKER_ROUTE_MSG, msg_);
    LOGINFO((BROKER, "ffbroker_t::bridge_handle_broker_to_broker_msg end ok dest node id[%u]", msg_.dest_node_id));
    return 0;
}

//! [4]
//! 处理broker bridge 转发给broker master的消息
int ffbroker_t::handle_bridge_to_broker_route_msg(bridge_route_to_broker_t::in_t& msg_, socket_ptr_t sock_)
{
    //! 需要转发给broker client
    broker_route_t::in_t dest_msg;
    dest_msg.from_node_id = msg_.from_node_id;//! 来自哪个节点
    
    dest_msg.dest_node_id = msg_.dest_node_id;//! 需要转发到哪个节点上
    
    //! 记录所有服务/接口信息
    if (dest_msg.dest_node_id == 0 && msg_.service_name.empty() == false)//! 说明是调用消息
    {
        map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.begin();//! node id -> service
        for (; it != m_broker_client_info.end(); ++it)
        {
            if (it->second.service_name == msg_.service_name)
            {
                dest_msg.dest_node_id = it->first;
                break;
            }
        }
    }

    dest_msg.msg_id = m_msg2id[msg_.msg_name];//! 调用的是哪个接口
    dest_msg.callback_id = msg_.callback_id;
    dest_msg.body = msg_.body;
    
    uint32_t bridge_node_id = sock_->get_data<session_data_t>()->get_node_id();
    dest_msg.bridge_route_id = m_broker_bridge_info[bridge_node_id].broker_group_id[msg_.from_broker_group_name];
    LOGINFO((BROKER, "ffbroker_t::handle_bridge_to_broker_route_msg begin dest node id[%u], bridge_node_id[%u], dest_msg.bridge_route_id[%u]",
             msg_.dest_node_id, bridge_node_id, dest_msg.bridge_route_id));
    return route_msg_to_broker_client(dest_msg);
}

//! 内存间传递消息
int ffbroker_t::memory_route_msg(broker_route_t::in_t& msg_)
{
    if (msg_.bridge_route_id != 0)
    {
        LOGINFO((BROKER, "ffbroker_t::memory_route_msg bridge_route_id[%u]", msg_.bridge_route_id));
        //! 需要转发给bridge broker标记，若此值不为0，说明目标node在其他broker组
        //! 需要broker master转发到bridge broker上, 这个是callback消息
        pair<string, socket_ptr_t> data = get_broker_group_name_by_id(msg_.bridge_route_id);
        return route_msg_broker_to_bridge(m_master_group_name, data.first, "",
                                          "",msg_.body,
                                          msg_.from_node_id, msg_.dest_node_id,
                                          msg_.callback_id, data.second);
    }
    return route_msg_to_broker_client(msg_);
}
//! 转发消息给master client
int ffbroker_t::route_msg_to_broker_client(broker_route_t::in_t& msg_)
{
    LOGTRACE((BROKER, "ffbroker_t::route_msg_to_broker_client begin dest node id[%u]", msg_.dest_node_id));
    
    if (0 == singleton_t<ffrpc_memory_route_t>::instance().broker_route_to_client(msg_))
    {
        LOGTRACE((BROKER, "ffbroker_t::handle_route_msg same process dest_node_id[%u]", msg_.dest_node_id));
        return 0;
    }

    map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.find(msg_.dest_node_id);
    if (it == m_broker_client_info.end())
    {
        LOGERROR((BROKER, "ffbroker_t::route_msg_to_broker_client no dest_node_id[%u]", msg_.dest_node_id));
        return -1;
    }
    msg_sender_t::send(it->second.sock, BROKER_TO_CLIENT_MSG, msg_);
    LOGTRACE((BROKER, "ffbroker_t::route_msg_to_broker_client end ok sock[%ld]", long(it->second.sock)));
    return 0;
}
//! 处理broker client连接到broker slave的消息
int ffbroker_t::handle_client_register_slave_broker(register_client_to_slave_broker_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_client_register_slave_broker begin"));
    map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.find(msg_.node_id);
    if (it == m_broker_client_info.end())
    {
        LOGERROR((BROKER, "ffbroker_t::handle_client_register_slave_broker no this node id[%u]", msg_.node_id));
        return -1;
    }

    broker_client_info_t& broker_client_info = it->second;
    broker_client_info.sock = sock_;
    LOGTRACE((BROKER, "ffbroker_t::handle_client_register_slave_broker end ok node id[%u], sock[%d]", msg_.node_id, long(sock_)));
    return 0;
}
//! 处理broker同步消息，broker master 会把master上注册的所有信息同步给所有的client
int ffbroker_t::handle_broker_sync_data(broker_sync_all_registered_data_t::out_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_broker_sync_data begin"));
    if (msg_.node_id != 0)
    {
        m_node_id = msg_.node_id;
        //! 如果本身是slave，内存中注册本身
        singleton_t<ffrpc_memory_route_t>::instance().add_node(m_node_id, this);
    }

    m_msg2id = msg_.msg2id;
    map<uint32_t, broker_client_info_t> tmp_broker_info_data = m_broker_client_info;
    m_broker_client_info.clear();

    map<uint32_t, broker_sync_all_registered_data_t::broker_client_info_t>::iterator it4 = msg_.broker_client_info.begin();
    for (; it4 != msg_.broker_client_info.end(); ++it4)
    {
        ffbroker_t::broker_client_info_t& broker_client_info = m_broker_client_info[it4->first];
        broker_client_info.bind_broker_id = it4->second.bind_broker_id;
        broker_client_info.service_name   = it4->second.service_name;
        if (tmp_broker_info_data[it4->first].service_name == it4->second.service_name)
        {
            broker_client_info.sock = tmp_broker_info_data[it4->first].sock;
        }

        LOGTRACE((BROKER, "ffbroker_t::handle_broker_sync_data name[%s] -> node id[%u]", broker_client_info.service_name, it4->first));
    }
    LOGTRACE((BROKER, "ffbroker_t::handle_broker_sync_data end ok"));
    return 0;
}
