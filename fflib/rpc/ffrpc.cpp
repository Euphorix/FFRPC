#include "rpc/ffrpc.h"
#include "rpc/ffrpc_ops.h"
#include "base/log.h"
#include "net/net_factory.h"

using namespace ff;

#define FFRPC                   "FFRPC"

ffrpc_t::ffrpc_t(string service_name_):
    m_service_name(service_name_),
    m_node_id(0),
    m_callback_id(0),
    m_master_broker_sock(NULL),
    m_bind_broker_id(0)
{
    if (m_service_name.empty())
    {
        char tmp[512];
        sprintf(tmp, "FFRPCClient-%ld-%p", ::time(NULL), this);
        m_service_name = tmp;
    }
}

ffrpc_t::~ffrpc_t()
{
    
}

int ffrpc_t::open(const string& opt_)
{
    net_factory_t::start(1);
    m_host = opt_;

    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);
            
    //!新版本
    m_ffslot.bind(REGISTER_TO_BROKER_RET, ffrpc_ops_t::gen_callback(&ffrpc_t::handle_broker_reg_response, this))
            .bind(BROKER_TO_CLIENT_MSG, ffrpc_ops_t::gen_callback(&ffrpc_t::handle_call_service_msg, this));

    m_master_broker_sock = connect_to_broker(m_host, BROKER_MASTER_NODE_ID);

    if (NULL == m_master_broker_sock)
    {
        LOGERROR((FFRPC, "ffrpc_t::open failed, can't connect to remote broker<%s>", m_host.c_str()));
        return -1;
    }

    while(m_node_id == 0)
    {
        usleep(1);
        if (m_master_broker_sock == NULL)
        {
            LOGERROR((FFRPC, "ffrpc_t::open failed"));
            return -1;
        }
    }
    singleton_t<ffrpc_memory_route_t>::instance().add_node(m_node_id, this);
    LOGTRACE((FFRPC, "ffrpc_t::open end ok m_node_id[%u]", m_node_id));
    return 0;
}

//! 连接到broker master
socket_ptr_t ffrpc_t::connect_to_broker(const string& host_, uint32_t node_id_)
{
    LOGINFO((FFRPC, "ffrpc_t::connect_to_broker begin...host_<%s>,node_id_[%u]", host_.c_str(), node_id_));
    socket_ptr_t sock = net_factory_t::connect(host_, this);
    if (NULL == sock)
    {
        LOGERROR((FFRPC, "ffrpc_t::register_to_broker_master failed, can't connect to remote broker<%s>", host_.c_str()));
        return sock;
    }
    session_data_t* psession = new session_data_t(node_id_);
    sock->set_data(psession);

    //!新版发送注册消息
    register_to_broker_t::in_t reg_msg;
    reg_msg.service_name = m_service_name;
    reg_msg.node_id = m_node_id;
    msg_sender_t::send(sock, REGISTER_TO_BROKER_REQ, reg_msg);
    return sock;
}
//! 投递到ffrpc 特定的线程
static void route_call_reconnect(ffrpc_t* ffrpc_)
{
    ffrpc_->get_tq().produce(task_binder_t::gen(&ffrpc_t::timer_reconnect_broker, ffrpc_));
}
//! 定时重连 broker master
void ffrpc_t::timer_reconnect_broker()
{
    LOGINFO((FFRPC, "ffrpc_t::timer_reconnect_broker begin..."));
    if (NULL == m_master_broker_sock)
    {
        m_master_broker_sock = connect_to_broker(m_host, BROKER_MASTER_NODE_ID);
        if (NULL == m_master_broker_sock)
        {
            LOGERROR((FFRPC, "ffrpc_t::timer_reconnect_broker failed, can't connect to remote broker<%s>", m_host.c_str()));
            //! 设置定时器重连
            m_timer.once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, this));
        }
        else
        {
            LOGWARN((FFRPC, "ffrpc_t::timer_reconnect_broker failed, connect to master remote broker<%s> ok", m_host.c_str()));
        }
    }
    //!检测是否需要连接slave broker
    map<string/*host*/, uint64_t>::iterator it = m_broker_data.slave_broker_data.begin();
    for (; it != m_broker_data.slave_broker_data.end(); ++it)
    {
        uint64_t node_id = it->second;
        string host = it->first;
        if (m_broker_sockets.find(node_id) == m_broker_sockets.end())//!重连
        {
            socket_ptr_t sock = connect_to_broker(host, node_id);
            if (sock == NULL)
            {
                LOGERROR((FFRPC, "ffrpc_t::timer_reconnect_broker failed, can't connect to remote broker<%s>", host.c_str()));
            }
            else
            {
                m_broker_sockets[node_id] = sock;
                LOGWARN((FFRPC, "ffrpc_t::timer_reconnect_broker failed, connect to slave remote broker<%s> ok", host.c_str()));
            }
        }
    }
    LOGINFO((FFRPC, "ffrpc_t::timer_reconnect_broker  end ok"));
}

//! 获取任务队列对象
task_queue_t& ffrpc_t::get_tq()
{
    return m_tq;
}
int ffrpc_t::handle_broken(socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffrpc_t::handle_broken_impl, this, sock_));
    return 0;
}
int ffrpc_t::handle_msg(const message_t& msg_, socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffrpc_t::handle_msg_impl, this, msg_, sock_));
    return 0;
}

int ffrpc_t::handle_broken_impl(socket_ptr_t sock_)
{
    //! 设置定时器重练
    if (m_master_broker_sock == sock_)
    {
        m_master_broker_sock = NULL;
    }
    else
    {
        map<uint64_t, socket_ptr_t>::iterator it = m_broker_sockets.begin();
        for (; it != m_broker_sockets.end(); ++it)
        {
            m_broker_sockets.erase(it);
            break;
        }
    }
    sock_->safe_delete();
    m_timer.once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, this));
    return 0;
}

int ffrpc_t::handle_msg_impl(const message_t& msg_, socket_ptr_t sock_)
{
    uint16_t cmd = msg_.get_cmd();
    LOGDEBUG((FFRPC, "ffrpc_t::handle_msg_impl cmd[%u] begin", cmd));

    ffslot_t::callback_t* cb = m_ffslot.get_callback(cmd);
    if (cb)
    {
        try
        {
            ffslot_msg_arg arg(msg_.get_body(), sock_);
            cb->exe(&arg);
            LOGDEBUG((FFRPC, "ffrpc_t::handle_msg_impl cmd[%u] call end ok", cmd));
            return 0;
        }
        catch(exception& e_)
        {
            LOGDEBUG((BROKER, "ffrpc_t::handle_msg_impl exception<%s> body_size=%u", e_.what(), msg_.get_body().size()));
            return -1;
        }
    }
    LOGWARN((FFRPC, "ffrpc_t::handle_msg_impl cmd[%u] end ok", cmd));
    return -1;
}


//! 新版 调用消息对应的回调函数
int ffrpc_t::handle_call_service_msg(broker_route_msg_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((FFRPC, "ffrpc_t::handle_call_service_msg msg begin dest_msg_name=%s callback_id=%u", msg_.dest_msg_name, msg_.callback_id));
    
    if (msg_.dest_service_name.empty() == false)
    {
        ffslot_t::callback_t* cb = m_ffslot_interface.get_callback(msg_.dest_msg_name);
        if (cb)
        {
            //ffslot_req_arg arg(msg_.body, msg_.from_node_id, msg_.callback_id, msg_.bridge_route_id, this);
            ffslot_req_arg arg(msg_.body, msg_.from_node_id, msg_.callback_id, 0, err_info_, this);
            cb->exe(&arg);
            return 0;
        }
        else
        {
            LOGERROR((FFRPC, "ffrpc_t::handle_call_service_msg service=%s and msg_name=%s not found", msg_.dest_service_name, msg_.dest_msg_name));
        }
    }
    else
    {
        ffslot_t::callback_t* cb = m_ffslot_callback.get_callback(msg_.callback_id);
        if (cb)
        {
            //ffslot_req_arg arg(msg_.body, msg_.from_node_id, msg_.callback_id, msg_.bridge_route_id, this);
            ffslot_req_arg arg(msg_.body, 0, 0, 0, err_info_, this);
            cb->exe(&arg);
            m_ffslot_callback.del(msg_.callback_id);
            return 0;
        }
        else
        {
            LOGERROR((FFRPC, "ffrpc_t::handle_call_service_msg callback_id[%u] or dest_msg=%s not found", msg_.callback_id, msg_.dest_msg_name));
        }
    }
    LOGTRACE((FFRPC, "ffrpc_t::handle_call_service_msg msg end ok"));
    return 0;
}

int ffrpc_t::call_impl(const string& service_name_, const string& msg_name_, const string& body_, ffslot_t::callback_t* callback_)
{
    //!调用远程消息
    LOGTRACE((FFRPC, "ffrpc_t::call_impl begin service_name_<%s>, msg_name_<%s> body_size=%u", service_name_.c_str(), msg_name_.c_str(), body_.size()));
    int64_t callback_id  = int64_t(callback_);
    m_ffslot_callback.bind(callback_id, callback_);
    
    broker_route_msg_t::in_t dest_msg;
    dest_msg.dest_service_name = service_name_;
    dest_msg.dest_msg_name = msg_name_;
    dest_msg.dest_node_id  = m_broker_data.service2node_id[service_name_];
    dest_msg.from_node_id  = m_node_id;
    dest_msg.callback_id = callback_id;
    dest_msg.body = body_;
    msg_sender_t::send(get_broker_socket(), BROKER_ROUTE_MSG, dest_msg);
    LOGTRACE((FFRPC, "ffrpc_t::call_impl end dest_id=%u", dest_msg.dest_node_id));

    return 0;
}
//! 判断某个service是否存在
bool ffrpc_t::is_exist(const string& service_name_)
{
    map<string, uint32_t>::iterator it = m_broker_client_name2nodeid.find(service_name_);
    if (it == m_broker_client_name2nodeid.end())
    {
        return false;
    }
    return true;
}
//! 通过bridge broker调用远程的service TODO
int ffrpc_t::bridge_call_impl(const string& broker_group_, const string& service_name_, const string& msg_name_,
                              const string& body_, ffslot_t::callback_t* callback_)
{
    broker_route_to_bridge_t::in_t dest_msg;
    dest_msg.dest_broker_group_name = broker_group_;
    dest_msg.service_name           = service_name_;//!  服务名
    dest_msg.msg_name               = msg_name_;//!消息名
    dest_msg.body                   = body_;//! msg data
    dest_msg.from_node_id           = m_node_id;
    dest_msg.dest_node_id           = 0;
    dest_msg.callback_id            = 0;
    if (callback_)
    {
        dest_msg.callback_id = get_callback_id();
        m_ffslot_callback.bind(dest_msg.callback_id, callback_);
    }

    msg_sender_t::send(get_broker_socket(), BROKER_TO_BRIDGE_ROUTE_MSG, dest_msg);
    LOGINFO((FFRPC, "ffrpc_t::bridge_call_impl group<%s> service[%s] end ok", broker_group_, service_name_));
    return 0;
}
//! 通过node id 发送消息给broker
void ffrpc_t::send_to_dest_node(const string& dest_namespace_, const string& msg_name_,  uint64_t dest_node_id_, uint32_t callback_id_, const string& body_)
{
    LOGINFO((FFRPC, "ffrpc_t::send_to_broker_by_nodeid begin dest_node_id[%u]", dest_node_id));
    broker_route_msg_t::in_t dest_msg;
    dest_msg.dest_service_name = dest_namespace_;
    dest_msg.dest_msg_name = msg_name_;
    dest_msg.dest_node_id = dest_node_id_;
    dest_msg.callback_id = callback_id_;
    dest_msg.body = body_;

    dest_msg.from_node_id = m_node_id;
    msg_sender_t::send(get_broker_socket(), BROKER_ROUTE_MSG, dest_msg);
    return;
}

//! 调用接口后，需要回调消息给请求者
void ffrpc_t::response(const string& dest_namespace_, const string& msg_name_,  uint64_t dest_node_id_, uint32_t callback_id_, const string& body_)
{
    m_tq.produce(task_binder_t::gen(&ffrpc_t::send_to_dest_node, this, dest_namespace_, msg_name_, dest_node_id_, callback_id_, body_));
}

//! 处理注册, 
int ffrpc_t::handle_broker_reg_response(register_to_broker_t::out_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((FFRPC, "ffbroker_t::handle_broker_reg_response begin node_id=%d", msg_.node_id));
    if (msg_.register_flag < 0)
    {
        LOGERROR((FFRPC, "ffbroker_t::handle_broker_reg_response register failed, service exist"));
        return -1;
    }
    if (msg_.register_flag == 1)
    {
        m_node_id = msg_.node_id;//! -1表示注册失败，0表示同步消息，1表示注册成功
        m_bind_broker_id = msg_.bind_broker_id;
    }
    m_broker_data = msg_;
    
    timer_reconnect_broker();
    LOGTRACE((FFRPC, "ffbroker_t::handle_broker_reg_response end service num=%d", m_broker_data.service2node_id.size()));
    return 0;
}

//!获取broker socket
socket_ptr_t ffrpc_t::get_broker_socket()
{
    if (m_bind_broker_id == 0)
    {
        return m_master_broker_sock;
    }
    return m_broker_sockets[m_bind_broker_id];     
}
