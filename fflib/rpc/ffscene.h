#ifndef _FF_FFSCENE_H_
#define _FF_FFSCENE_H_

#include <assert.h>
#include <string>
using namespace std;

#include "base/ffslot.h"
#include "net/socket_i.h"
#include "base/fftype.h"
#include "net/codec.h"
#include "rpc/ffrpc.h"
#include "base/arg_helper.h"

namespace ff
{

class ffscene_t
{
public:
    struct callback_info_t
    {
        callback_info_t():
            verify_callback(NULL),
            enter_callback(NULL),
            offline_callback(NULL),
            logic_callback(NULL)
        {}
        ffslot_t::callback_t*   verify_callback;
        ffslot_t::callback_t*   enter_callback;
        ffslot_t::callback_t*   offline_callback;
        ffslot_t::callback_t*   logic_callback;
    };
    
    class session_verify_arg;
    class session_enter_arg;
    class session_offline_arg;
    class logic_msg_arg;
    
    //! 记录session的信息
    struct session_info_t
    {
        string gate_name;//! 所在的gate
    };
public:
    ffscene_t();
    virtual ~ffscene_t();
    virtual int open(arg_helper_t& arg);
    virtual int close();

    callback_info_t& callback_info();
    
    //! 发送消息给特定的client
    int send_msg_session(const string& session_id_, uint16_t cmd_, const string& data_);
    //! 多播
    int multicast_msg_session(const vector<string>& session_id_, uint16_t cmd_, const string& data_);
    //! 广播
    int broadcast_msg_session(uint16_t cmd_, const string& data_);
    //! 广播 整个gate
    int broadcast_msg_gate(const string& gate_name_, uint16_t cmd_, const string& data_);
    //! 关闭某个session
    int close_session(const string& session_id_);
    //! 切换scene
    int change_session_scene(const string& session_id_, const string& to_scene_, const string& extra_data);

private:
    //! 处理client 上线
    int process_session_verify(ffreq_t<session_verify_t::in_t, session_verify_t::out_t>& req_);
    //! 处理client 进入场景
    int process_session_enter(ffreq_t<session_enter_scene_t::in_t, session_enter_scene_t::out_t>& req_);
    //! 处理client 下线
    int process_session_offline(ffreq_t<session_offline_t::in_t, session_offline_t::out_t>& req_);
    //! 转发client消息
    int process_session_req(ffreq_t<route_logic_msg_t::in_t, route_logic_msg_t::out_t>& req_);
    
protected:
    string                                      m_logic_name;
    shared_ptr_t<ffrpc_t>                       m_ffrpc;
    callback_info_t                             m_callback_info;
    map<string/*sessionid*/, session_info_t>    m_session_info;
};



class ffscene_t::session_verify_arg: public ffslot_t::callback_arg_t
{
public:
    session_verify_arg(const string& s_, int64_t t_, const string& ip_, const string& gate_):
        session_key(s_),
        online_time(t_),
        ip(ip_),
        gate_name(gate_)
    {}
    virtual int type()
    {
        return TYPEID(session_verify_arg);
    }
    string          session_key;
    int64_t         online_time;
    string          ip;
    string          gate_name;

    //! 验证后的sessionid
    string          alloc_session_id;
    //! 需要额外的返回给client的消息内容
    string          extra_data;
};

class ffscene_t::session_enter_arg: public ffslot_t::callback_arg_t
{
public:
    session_enter_arg(const string& s_, const string& from_, const string& to_, const string& data_):
        session_id(s_),
        from_scene(from_),
        to_scene(to_),
        extra_data(data_)
    {}
    virtual int type()
    {
        return TYPEID(session_enter_arg);
    }
    string    session_id;//! 包含用户id
    string    from_scene;//! 从哪个scene跳转过来,若是第一次上线，from_scene为空
    string    to_scene;//! 跳到哪个scene上面去,若是下线，to_scene为空
    string    extra_data;//! 附带数据
};
class ffscene_t::session_offline_arg: public ffslot_t::callback_arg_t
{
public:
    session_offline_arg(const string& s_, int64_t t_):
        session_id(s_),
        online_time(t_)
    {}
    virtual int type()
    {
        return TYPEID(session_offline_arg);
    }
    string          session_id;
    int64_t         online_time;
};
class ffscene_t::logic_msg_arg: public ffslot_t::callback_arg_t
{
public:
    logic_msg_arg(const string& s_, uint16_t cmd_, const string& t_):
        session_id(s_),
        cmd(cmd_),
        body(t_)
    {}
    virtual int type()
    {
        return TYPEID(logic_msg_arg);
    }
    string          session_id;
    uint16_t        cmd;
    string          body;
};

}


#endif
