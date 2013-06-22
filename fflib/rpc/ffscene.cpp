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
    if (false == arg_helper.is_enable_option("-scene"))
    {
        LOGERROR((FFSCENE, "ffscene_t::open failed without -scene argmuent"));
        return -1;
    }
    m_logic_name = arg_helper.get_option_value("-scene");
    m_ffrpc = new ffrpc_t(m_logic_name);
    
    m_ffrpc->reg(&ffscene_t::process_session_verify, this);
    m_ffrpc->reg(&ffscene_t::process_session_enter, this);
    m_ffrpc->reg(&ffscene_t::process_session_offline, this);
    m_ffrpc->reg(&ffscene_t::process_session_req, this);
    
    if (m_ffrpc->open(arg_helper.get_option_value("-broker")))
    {
        LOGERROR((FFSCENE, "ffscene_t::open failed check -broker argmuent"));
        return -1;
    }
    
    LOGTRACE((FFSCENE, "ffscene_t::open end ok"));
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
    session_verify_t::out_t out;
    if (m_callback_info.verify_callback)
    {
        session_verify_arg arg(req_.arg.session_key, req_.arg.online_time, req_.arg.ip, req_.arg.gate_name);
        m_callback_info.verify_callback->exe(&arg);
        out.session_id = arg.alloc_session_id;
        out.extra_data = arg.extra_data;
    }
    
    req_.response(out);
    LOGTRACE((FFSCENE, "ffscene_t::process_session_verify end ok"));
    return 0;
}
//! 处理client 进入场景
int ffscene_t::process_session_enter(ffreq_t<session_enter_scene_t::in_t, session_enter_scene_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_enter begin gate[%s]", req_.arg.from_gate));
    m_session_info[req_.arg.session_id].gate_name = req_.arg.from_gate;
    session_enter_scene_t::out_t out;
    if (m_callback_info.enter_callback)
    {
        session_enter_arg arg(req_.arg.session_id, req_.arg.from_scene, req_.arg.to_scene, req_.arg.extra_data);
        m_callback_info.enter_callback->exe(&arg);
    }
    req_.response(out);
    LOGTRACE((FFSCENE, "ffscene_t::process_session_enter end ok"));
    return 0;
}

//! 处理client 下线
int ffscene_t::process_session_offline(ffreq_t<session_offline_t::in_t, session_offline_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_offline begin"));
    m_session_info.erase(req_.arg.session_id);
    session_offline_t::out_t out;
    if (m_callback_info.offline_callback)
    {
        session_offline_arg arg(req_.arg.session_id, req_.arg.online_time);
        m_callback_info.offline_callback->exe(&arg);
    }
    req_.response(out);
    LOGTRACE((FFSCENE, "ffscene_t::process_session_offline end ok"));
    return 0;
}
//! 转发client消息
int ffscene_t::process_session_req(ffreq_t<route_logic_msg_t::in_t, route_logic_msg_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_req begin cmd[%u]", req_.arg.cmd));
    route_logic_msg_t::out_t out;
    if (m_callback_info.logic_callback)
    {
        logic_msg_arg arg(req_.arg.session_id, req_.arg.cmd, req_.arg.body);
        m_callback_info.logic_callback->exe(&arg);
    }
    req_.response(out);
    LOGTRACE((FFSCENE, "ffscene_t::process_session_req end ok"));
    return 0;
}

ffscene_t::callback_info_t& ffscene_t::callback_info()
{
    return m_callback_info;
}

//! 发送消息给特定的client
int ffscene_t::send_msg_session(const string& session_id_, uint16_t cmd_, const string& data_)
{
    LOGTRACE((FFSCENE, "ffscene_t::send_msg_session begin session_id_<%s>", session_id_));
    map<string/*sessionid*/, session_info_t>::iterator it = m_session_info.find(session_id_);
    if (it == m_session_info.end())
    {
        LOGWARN((FFSCENE, "ffscene_t::send_msg_session no session id[%s]", session_id_));
        return -1;
    }
    gate_route_msg_to_session_t::in_t msg;
    msg.session_id.push_back(session_id_);
    msg.cmd  = cmd_;
    msg.body = data_;
    m_ffrpc->call(it->second.gate_name, msg);
    LOGTRACE((FFSCENE, "ffscene_t::send_msg_session end ok gate[%s]", it->second.gate_name));
    return 0;
}
//! 多播
int ffscene_t::multicast_msg_session(const vector<string>& session_id_, uint16_t cmd_, const string& data_)
{
    vector<string>::const_iterator it = session_id_.begin();
    for (; it != session_id_.end(); ++it)
    {
        send_msg_session(*it, cmd_, data_);
    }
    return 0;
}
//! 广播
int ffscene_t::broadcast_msg_session(uint16_t cmd_, const string& data_)
{
    map<string/*sessionid*/, session_info_t>::iterator it = m_session_info.begin();
    for (; it != m_session_info.end(); ++it)
    {
        gate_route_msg_to_session_t::in_t msg;
        msg.session_id.push_back(it->first);
        msg.cmd = cmd_;
        msg.body = data_;
        m_ffrpc->call(it->second.gate_name, msg);
    }
    return 0;
}
//! 广播 整个gate
int ffscene_t::broadcast_msg_gate(const string& gate_name_, uint16_t cmd_, const string& data_)
{
    gate_broadcast_msg_to_session_t::in_t msg;
    msg.cmd = cmd_;
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
    m_session_info.erase(it);
    return 0;
}
//! 切换scene
int ffscene_t::change_session_scene(const string& session_id_, const string& to_scene_, const string& extra_data_)
{
    map<string/*sessionid*/, session_info_t>::iterator it = m_session_info.find(session_id_);
    if (it == m_session_info.end())
    {
        LOGWARN((FFSCENE, "ffscene_t::change_session_scene no session id[%s]", session_id_));
        return -1;
    }
    
    gate_change_logic_node_t::in_t msg;
    msg.session_id = session_id_;
    msg.alloc_logic_service = to_scene_;
    msg.extra_data = extra_data_;
    m_ffrpc->call(it->second.gate_name, msg);
    m_session_info.erase(it);
    return 0;
}
