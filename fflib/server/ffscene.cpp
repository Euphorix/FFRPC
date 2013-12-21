#include "server/ffscene.h"
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
    m_ffrpc->reg(&ffscene_t::process_scene_call, this);
    
    if (m_ffrpc->open(arg_helper))
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
//! 为session 分配session Id
int ffscene_t::verify_session_id(long key, const userid_t& session_id_, string extra_data)
{
    map<long, ffreq_verify_t>::iterator it = m_cache_verify_req.find(key);
    if (it != m_cache_verify_req.end())
    {
        session_verify_t::out_t out;
        out.session_id = session_id_;
        out.extra_data = extra_data;
        it->second.response(out);
        m_cache_verify_req.erase(it);
        return -1;
    }
    return 0;
}

//! 处理client 上线
int ffscene_t::process_session_verify(ffreq_t<session_verify_t::in_t, session_verify_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_verify begin session_key size=%u", req_.msg.session_key.size()));
    session_verify_t::out_t out;
    if (m_callback_info.verify_callback)
    {
        long key_id = (long)&(req_.msg);
        m_cache_verify_req[key_id] = req_;
        session_verify_arg arg(req_.msg.session_key, req_.msg.online_time, req_.msg.ip, req_.msg.gate_name, key_id);
        m_callback_info.verify_callback->exe(&arg);
        if (arg.flag_verify)
        {
            out.session_id = arg.alloc_session_id;
            out.extra_data = arg.extra_data;
            req_.response(out);
            m_cache_verify_req.erase(key_id);
        }
    }
    else
    {
        req_.response(out);
    }
    LOGTRACE((FFSCENE, "ffscene_t::process_session_verify end ok"));
    return 0;
}
//! 处理client 进入场景
int ffscene_t::process_session_enter(ffreq_t<session_enter_scene_t::in_t, session_enter_scene_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_enter begin gate[%s]", req_.msg.from_gate));
    m_session_info[req_.msg.session_id].gate_name = req_.msg.from_gate;
    session_enter_scene_t::out_t out;
    if (m_callback_info.enter_callback)
    {
        session_enter_arg arg(req_.msg.session_id, req_.msg.from_scene, req_.msg.to_scene, req_.msg.extra_data);
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
    m_session_info.erase(req_.msg.session_id);
    session_offline_t::out_t out;
    if (m_callback_info.offline_callback)
    {
        session_offline_arg arg(req_.msg.session_id, req_.msg.online_time);
        m_callback_info.offline_callback->exe(&arg);
    }
    req_.response(out);
    LOGTRACE((FFSCENE, "ffscene_t::process_session_offline end ok"));
    return 0;
}
//! 转发client消息
int ffscene_t::process_session_req(ffreq_t<route_logic_msg_t::in_t, route_logic_msg_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_req begin cmd[%u]", req_.msg.cmd));
    route_logic_msg_t::out_t out;
    if (m_callback_info.logic_callback)
    {
        logic_msg_arg arg(req_.msg.session_id, req_.msg.cmd, req_.msg.body);
        m_callback_info.logic_callback->exe(&arg);
    }
    req_.response(out);
    LOGTRACE((FFSCENE, "ffscene_t::process_session_req end ok"));
    return 0;
}

//! scene 之间的互调用
int ffscene_t::process_scene_call(ffreq_t<scene_call_msg_t::in_t, scene_call_msg_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_scene_call begin cmd[%u]", req_.msg.cmd));
    scene_call_msg_t::out_t out;
    if (m_callback_info.scene_call_callback)
    {
        scene_call_msg_arg arg(req_.msg.cmd, req_.msg.body, out.err, out.msg_type, out.body);
        m_callback_info.scene_call_callback->exe(&arg);
    }
    else
    {
        out.err = "no scene_call_callback bind";
    }
    req_.response(out);

    LOGTRACE((FFSCENE, "ffscene_t::process_scene_call end ok"));
    return 0;
}

ffscene_t::callback_info_t& ffscene_t::callback_info()
{
    return m_callback_info;
}

//! 发送消息给特定的client
int ffscene_t::send_msg_session(const userid_t& session_id_, uint16_t cmd_, const string& data_)
{
    LOGTRACE((FFSCENE, "ffscene_t::send_msg_session begin session_id_<%ld>", session_id_));
    map<userid_t/*sessionid*/, session_info_t>::iterator it = m_session_info.find(session_id_);
    if (it == m_session_info.end())
    {
        LOGWARN((FFSCENE, "ffscene_t::send_msg_session no session id[%ld]", session_id_));
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
int ffscene_t::multicast_msg_session(const vector<userid_t>& session_id_, uint16_t cmd_, const string& data_)
{
    vector<userid_t>::const_iterator it = session_id_.begin();
    for (; it != session_id_.end(); ++it)
    {
        send_msg_session(*it, cmd_, data_);
    }
    return 0;
}
//! 广播
int ffscene_t::broadcast_msg_session(uint16_t cmd_, const string& data_)
{
    map<userid_t/*sessionid*/, session_info_t>::iterator it = m_session_info.begin();
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
int ffscene_t::close_session(const userid_t& session_id_)
{
    map<userid_t/*sessionid*/, session_info_t>::iterator it = m_session_info.find(session_id_);
    if (it == m_session_info.end())
    {
        LOGWARN((FFSCENE, "ffscene_t::send_msg_session no session id[%ld]", session_id_));
        return -1;
    }
    
    gate_close_session_t::in_t msg;
    msg.session_id = session_id_;
    m_ffrpc->call(it->second.gate_name, msg);
    m_session_info.erase(it);
    return 0;
}
//! 切换scene
int ffscene_t::change_session_scene(const userid_t& session_id_, const string& to_scene_, const string& extra_data_)
{
	LOGTRACE((FFSCENE, "ffscene_t::change_session_scene session id[%ld]", session_id_));
    map<userid_t/*sessionid*/, session_info_t>::iterator it = m_session_info.find(session_id_);
    if (it == m_session_info.end())
    {
        LOGWARN((FFSCENE, "ffscene_t::change_session_scene no session id[%ld]", session_id_));
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
