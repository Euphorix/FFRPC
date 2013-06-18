#include "rpc/ffscene.h"
#include "base/log.h"
using namespace ff;

#define FFSCENE                   "FFSCENE"

ffscene_t::ffscene_t()
{
    
}
ffscene_t::~ffscene_t()
{
    
}
int ffscene_t::open(arg_helper_t& arg_helper)
{
    LOGTRACE((FFSCENE, "ffscene_t::open begin"));
    if (false == arg_helper.is_enable_option("-logic"))
    {
        LOGERROR((FFSCENE, "ffscene_t::open failed without -gate argmuent"));
        return -1;
    }
    m_logic_name = arg_helper.get_option_value("-logic");
    m_ffrpc = new ffrpc_t(m_logic_name);
    
    m_ffrpc->reg(&ffscene_t::process_session_verify, this);
    m_ffrpc->reg(&ffscene_t::process_session_offline, this);
    m_ffrpc->reg(&ffscene_t::process_session_req, this);
    
    if (m_ffrpc->open(arg_helper.get_option_value("-broker")))
    {
        LOGERROR((FFSCENE, "ffscene_t::open failed check -broker argmuent"));
        return -1;
    }
    
    return 0;
}
int ffscene_t::close()
{
    return 0;
}

//! 处理client 上线
int ffscene_t::process_session_verify(ffreq_t<session_verify_t::in_t, session_verify_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_verify begin"));
    if (m_callback_info.verify_callback)
    {
        session_verify_arg arg(req_.arg.session_key, req_.arg.online_time, req_.arg.gate_name);
        m_callback_info.verify_callback->exe(&arg);
    }
    LOGTRACE((FFSCENE, "ffscene_t::process_session_verify end ok"));
    return 0;
}
//! 处理client 进入场景
int ffscene_t::process_session_enter(ffreq_t<session_enter_scene_t::in_t, session_enter_scene_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_enter begin"));
    if (m_callback_info.enter_callback)
    {
        session_enter_arg arg(req_.arg.session_id, req_.arg.from_scene, req_.arg.to_scene, req_.arg.extra_data);
        m_callback_info.enter_callback->exe(&arg);
    }
    LOGTRACE((FFSCENE, "ffscene_t::process_session_enter end ok"));
    return 0;
}

//! 处理client 下线
int ffscene_t::process_session_offline(ffreq_t<session_offline_t::in_t, session_offline_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_offline begin"));
    if (m_callback_info.offline_callback)
    {
        session_offline_arg arg(req_.arg.session_id, req_.arg.online_time);
        m_callback_info.offline_callback->exe(&arg);
    }
    LOGTRACE((FFSCENE, "ffscene_t::process_session_offline end ok"));
    return 0;
}
//! 转发client消息
int ffscene_t::process_session_req(ffreq_t<route_logic_msg_t::in_t, route_logic_msg_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_req begin"));
    if (m_callback_info.logic_callback)
    {
        logic_msg_arg arg(req_.arg.session_id, req_.arg.body);
        m_callback_info.logic_callback->exe(&arg);
    }
    LOGTRACE((FFSCENE, "ffscene_t::process_session_req end ok"));
    return 0;
}

ffscene_t::callback_info_t& ffscene_t::callback_info()
{
    return m_callback_info;
}

//! 发送消息给特定的client
int ffscene_t::send_msg_session(const string& session_id_, const string& data_)
{
    map<string/*sessionid*/, session_info_t>::iterator it = m_session_info.find(session_id_);
    if (it == m_session_info.end())
    {
        LOGWARN((FFSCENE, "ffscene_t::send_msg_session no session id[%s]", session_id_));
        return -1;
    }
    gate_route_msg_to_session_t::in_t msg;
    msg.session_id.push_back(session_id_);
    msg.body = data_;
    m_ffrpc->call(it->second.gate_name, msg);
    return 0;
}
//! 多播
int ffscene_t::multicast_msg_session(const vector<string>& session_id_, const string& data_)
{
    vector<string>::const_iterator it = session_id_.begin();
    for (; it != session_id_.end(); ++it)
    {
        send_msg_session(*it, data_);
    }
    return 0;
}
//! 广播
int ffscene_t::broadcast_msg_session(const string& data_)
{
    map<string/*sessionid*/, session_info_t>::iterator it = m_session_info.begin();
    for (; it != m_session_info.end(); ++it)
    {
        gate_route_msg_to_session_t::in_t msg;
        msg.session_id.push_back(it->first);
        msg.body = data_;
        m_ffrpc->call(it->second.gate_name, msg);
    }
    return 0;
}
//! 广播 整个gate
int ffscene_t::broadcast_msg_gate(const string& gate_name_, const string& data_)
{
    gate_broadcast_msg_to_session_t::in_t msg;
    msg.body = data_;
    m_ffrpc->call(gate_name_, msg);
    return 0;
}
//! 关闭某个session
int ffscene_t::close_session(const string& session_id_)
{
    map<string/*sessionid*/, session_info_t>::iterator it = m_session_info.find(session_id_);
    if (it == m_session_info.end())
    {
        LOGWARN((FFSCENE, "ffscene_t::send_msg_session no session id[%s]", session_id_));
        return -1;
    }
    
    gate_close_session_t::in_t msg;
    msg.session_id = session_id_;
    m_ffrpc->call(it->second.gate_name, msg);
    return 0;
}