#include "rpc/ffbroker.h"
#include "base/log.h"
#include "base/arg_helper.h"

using namespace ff;

ffbroker_t::ffbroker_t()
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
            .bind(BROKER_ROUTE_MSG, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_broker_route_msg, this));

    //! 任务队列绑定线程
    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);

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
    map<uint64_t/* node id*/, socket_ptr_t>::iterator it = m_all_registered_info.node_sockets.begin();
    for (; it != m_all_registered_info.node_sockets.end(); ++it){
        if (it->second == sock_)
            break;
    }
    if (it == m_all_registered_info.node_sockets.end())
    {
        sock_->safe_delete();
        return 0;
    }
    uint64_t node_id = it->first;
    m_all_registered_info.node_sockets.erase(it);
    LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl noid_id<%u> close",node_id ));
    
    map<string, uint64_t>::iterator it2 = m_all_registered_info.broker_data.service2node_id.begin();
    for (; it2 != m_all_registered_info.broker_data.service2node_id.end(); ++it2)
    {
        if (node_id == it2->second)
        {
            m_all_registered_info.broker_data.service2node_id.erase(it2);
            break;
        }
    }
    
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


//! 处理其他broker或者client注册到此server
int ffbroker_t::handle_regiter_to_broker(register_to_broker_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_regiter_to_broker begin"));

    register_to_broker_t::out_t ret_msg;
    ret_msg.register_flag = false;
    if (m_all_registered_info.broker_data.service2node_id.find(msg_.service_name) == m_all_registered_info.broker_data.service2node_id.end())
    {
        uint64_t node_id = alloc_node_id(sock_);
        
        m_all_registered_info.node_sockets[node_id] = sock_;
        m_all_registered_info.broker_data.service2node_id[msg_.service_name] = node_id;
        ret_msg = m_all_registered_info.broker_data;
        ret_msg.node_id = node_id;
        ret_msg.register_flag = true;
    }
    else
    {
        LOGERROR((BROKER, "ffbroker_t::handle_regiter_to_broker service<%s> exist", msg_.service_name));
    }
    msg_sender_t::send(sock_, REGISTER_TO_BROKER_RET, ret_msg);
    LOGTRACE((BROKER, "ffbroker_t::handle_regiter_to_broker end ok id=%d", ret_msg.node_id));
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
