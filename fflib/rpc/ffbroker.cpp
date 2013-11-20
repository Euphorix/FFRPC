#include "rpc/ffbroker.h"
#include "base/log.h"
#include "base/arg_helper.h"

using namespace ff;

static void route_call_reconnect(ffbroker_t* ffbroker_);

ffbroker_t::ffbroker_t():
    m_node_id(0),
    m_master_broker_sock(NULL)
{
}
ffbroker_t::~ffbroker_t()
{

}
//!ff 获取节点信息
uint64_t ffbroker_t::alloc_node_id(socket_ptr_t sock_)
{
    static uint64_t g_id = 0;
    return ++g_id;
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
    //!新版本
    //! 处理其他broker或者client注册到此server
    m_ffslot.bind(REGISTER_TO_BROKER_REQ, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_regiter_to_broker, this))
            .bind(BROKER_ROUTE_MSG, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_broker_route_msg, this))
            .bind(REGISTER_TO_BROKER_RET, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_register_master_ret, this));

    //! 任务队列绑定线程
    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);

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
    return 0;
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
    LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl begin"));
    session_data_t* psession = sock_->get_data<session_data_t>();
    if (NULL == psession)
    {
        sock_->safe_delete();
        LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl no session"));
        return 0;
    }

    if (sock_ == m_master_broker_sock)
    {
        m_master_broker_sock = NULL;
        //!检测是否需要重连master broker
        m_timer.once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, this));
        sock_->safe_delete();
        LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl check master reconnect"));
        return 0;
    }
    else
    {
        m_all_registered_info.node_sockets.erase(psession->node_id);
        LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl node_id<%u> close", node_id ));
        m_all_registered_info.broker_data.service2node_id.erase(psession->service_name);
    }
    delete psession;
    sock_->set_data(NULL);
    sock_->safe_delete();
    LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl end ok"));
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


//! 处理其他broker或者client注册到此server
int ffbroker_t::handle_regiter_to_broker(register_to_broker_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_regiter_to_broker begin service_name=%s", msg_.service_name));

    session_data_t* psession = sock_->get_data<session_data_t>();
    if (NULL != psession)
    {
        sock_->close();
        return 0;
    }

    register_to_broker_t::out_t ret_msg;
    ret_msg.register_flag = -1;//! -1表示注册失败，0表示同步消息，1表示注册成功,

    if (is_master_broker() == false)//!如果自己是slave broker，不做任何的处理
    {
        m_all_registered_info.node_sockets[msg_.node_id] = sock_;
        LOGINFO((BROKER, "ffbroker_t::handle_regiter_to_broker register slave broker service_name=%s, node_id=%d", msg_.service_name, msg_.node_id));
        return 0;
    }
    
    //! 那么自己是master broker，处理来自slave 和 rpc node的注册消息
    if (SLAVE_BROKER == msg_.node_type)//! 分配slave broker 节点id，同步slave的监听信息到其他rpc node
    {
        uint64_t node_id = alloc_node_id(sock_);
        session_data_t* psession = new session_data_t(msg_.node_type, node_id);
        psession->node_id = node_id;
        sock_->set_data(psession);

        m_all_registered_info.node_sockets[node_id] = sock_;
        //!如果是slave broker，那么host参数会被赋值
        m_all_registered_info.broker_data.slave_broker_data[msg_.host] = node_id;
        LOGTRACE((BROKER, "ffbroker_t::handle_regiter_to_broker slave register=%s", msg_.host));

        ret_msg = m_all_registered_info.broker_data;
        ret_msg.node_id = node_id;
    }
    else if (RPC_NODE == msg_.node_type)
    {
        if (m_all_registered_info.broker_data.service2node_id.find(msg_.service_name) != m_all_registered_info.broker_data.service2node_id.end())
        {
            msg_sender_t::send(sock_, REGISTER_TO_BROKER_RET, ret_msg);
            LOGERROR((BROKER, "ffbroker_t::handle_regiter_to_broker service<%s> exist", msg_.service_name));
            return 0;
        }

        uint64_t node_id = alloc_node_id(sock_);
        session_data_t* psession = new session_data_t(msg_.node_type, node_id);
        psession->node_id = node_id;
        sock_->set_data(psession);

        m_all_registered_info.node_sockets[node_id] = sock_;

        if (msg_.service_name.empty() == false)
        {
            psession->service_name = service_name;
            m_all_registered_info.broker_data.service2node_id[msg_.service_name] = node_id;
        }
        ret_msg = m_all_registered_info.broker_data;
        ret_msg.node_id = node_id;
    }
    else
    {
        msg_sender_t::send(sock_, REGISTER_TO_BROKER_RET, ret_msg);
        LOGERROR((BROKER, "ffbroker_t::handle_regiter_to_broker type invalid<%d>", msg_.node_type));
        return 0;
    }
    
    //!广播给所有的子节点
    map<uint64_t/* node id*/, socket_ptr_t>::iterator it = m_all_registered_info.node_sockets.begin();
    for (; it != m_all_registered_info.node_sockets.end(); ++it)
    {
        if (sock_ == it->second)
        {
            ret_msg.register_flag  = 1;
            ret_msg.bind_broker_id = m_node_id;//!如果没有slave，那么绑定自己身上
            if (m_all_registered_info.broker_data.slave_broker_data.empty() == false)
            {
                int index = node_id % m_all_registered_info.broker_data.slave_broker_data.size();
                map<string/*host*/, uint64_t>::iterator it_broker = m_all_registered_info.broker_data.slave_broker_data.begin();
                for (int i =0;i < index; ++i)
                {
                    ++it_broker;
                }
                ret_msg.bind_broker_id = it_broker->second;
            }
        }
        else
        {
            ret_msg.register_flag = 0;
        }
        msg_sender_t::send(it->second, REGISTER_TO_BROKER_RET, ret_msg);
    }
    LOGTRACE((BROKER, "ffbroker_t::handle_regiter_to_broker end ok id=%d, type=%d", ret_msg.node_id, msg_.node_type));
    return 0;
}

//! 处理转发消息的操作
int ffbroker_t::handle_broker_route_msg(broker_route_msg_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_broker_route_msg begin"));
    
    //!如果找到对应的节点，那么发给对应的节点
    map<uint64_t/* node id*/, socket_ptr_t>::iterator it = m_all_registered_info.node_sockets.find(msg_.dest_node_id);
    if (it != m_all_registered_info.node_sockets.end())
    {
        msg_sender_t::send(it->second, BROKER_TO_CLIENT_MSG, msg_);
    }
    else
    {
        LOGERROR((BROKER, "ffbroker_t::handle_broker_route_msg end failed node=%d none exist", msg_.dest_node_id));
        return 0;
    }
    LOGTRACE((BROKER, "ffbroker_t::handle_broker_route_msg end ok msg body_size=%d", msg_.body.size()));
    return 0;
}

//!ff TODO remove
int ffbroker_t::memory_route_msg(broker_route_t::in_t& msg_)
{
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
    session_data_t* psession = new session_data_t(MASTER_BROKER, BROKER_MASTER_NODE_ID);
    m_master_broker_sock->set_data(psession);

    //! 发送注册消息给master broker
    //!新版发送注册消息
    register_to_broker_t::in_t reg_msg;
    reg_msg.node_type = SLAVE_BROKER;
    reg_msg.host      = m_listen_host;
    reg_msg.node_id   = 0;
    msg_sender_t::send(m_master_broker_sock, REGISTER_TO_BROKER_REQ, reg_msg);

    LOGINFO((BROKER, "ffbroker_t::connect_to_master_broker ok<%s>", m_broker_host.c_str()));
    return 0;
}

//!处理注册到master broker的消息
int ffbroker_t::handle_register_master_ret(register_to_broker_t::out_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_register_master_ret begin"));
    session_data_t* psession = sock_->get_data<session_data_t>();
    if (NULL == psession)
    {
        sock_->close();
        return 0;
    }

    if (psession->get_type() == MASTER_BROKER)//!注册到master broker的返回消息
    {
        if (msg_.register_flag < 0)
        {
            LOGERROR((BROKER, "ffbroker_t::handle_register_master_ret register failed, service exist"));
            return -1;
        }
        if (msg_.register_flag == 1)
        {
            m_node_id = msg_.node_id;//! -1表示注册失败，0表示同步消息，1表示注册成功
        }
        m_all_registered_info.broker_data = msg_;
    }
    LOGINFO((BROKER, "ffbroker_t::handle_register_master_ret end ok m_node_id=%d", m_node_id));
    return 0;
}

static void reconnect_loop(ffbroker_t* ffbroker_)
{
    if (ffbroker_->connect_to_master_broker())
    {
        ffbroker_->get_timer().once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, ffbroker_));
    }
}
//! 投递到ffrpc 特定的线程
static void route_call_reconnect(ffbroker_t* ffbroker_)
{
    ffbroker_->get_tq().produce(task_binder_t::gen(&reconnect_loop, ffbroker_));
}
