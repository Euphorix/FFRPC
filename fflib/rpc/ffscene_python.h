#ifndef _FF_FFSCENE_PYTHON_H_
#define _FF_FFSCENE_PYTHON_H_

#include <assert.h>
#include <string>
using namespace std;

#include "rpc/ffscene.h"
#include "python/ffpython.h"
#include "base/log.h"

namespace ff
{
#define FFSCENE_PYTHON "FFSCENE_PYTHON"

#define MOD_NAME            "ffext"
#define VERIFY_CB_NAME      "ff_session_verify"
#define ENTER_CB_NAME       "ff_session_enter"
#define OFFLINE_CB_NAME     "ff_session_offline"
#define LOGIC_CB_NAME       "ff_session_logic"
#define TIMER_CB_NAME       "ff_timer_callback"

class ffscene_python_t: public ffscene_t
{
public:  
    int open(arg_helper_t& arg_helper)
    {
        LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::open begin"));
        m_ext_name = MOD_NAME;
        m_ffpython.reg_class<ffscene_python_t, PYCTOR()>("ffscene_t")
    			  .reg(&ffscene_python_t::send_msg_session, "send_msg_session")
    			  .reg(&ffscene_python_t::multicast_msg_session, "multicast_msg_session")
    			  .reg(&ffscene_python_t::broadcast_msg_session, "broadcast_msg_session")
    			  .reg(&ffscene_python_t::broadcast_msg_gate, "broadcast_msg_gate")
    			  .reg(&ffscene_python_t::close_session, "close_session")
                  .reg(&ffscene_python_t::change_session_scene, "change_session_scene")
                  .reg(&ffscene_python_t::once_timer, "once_timer")
                  .reg(&ffscene_python_t::reload, "reload");
    
        m_ffpython.init("ff");
        m_ffpython.set_global_var("ff", "ffscene_obj", (ffscene_python_t*)this);

        this->callback_info().verify_callback = gen_verify_callback();
        this->callback_info().enter_callback = gen_enter_callback();
        this->callback_info().offline_callback = gen_offline_callback();
        this->callback_info().logic_callback = gen_logic_callback();

        if (arg_helper.is_enable_option("-python_path"))
        {
            ffpython_t::add_path(arg_helper.get_option_value("-python_path"));
        }
        m_ffpython.load("main");
        int ret = ffscene_t::open(arg_helper);
        LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::open end ok"));
        return ret;
    }
    int close()
    {
        ffscene_t::close();
        Py_Finalize();
        return 0;
    }
    int reload(const string& name_)
    {
        LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::reload begin name_[%s]", name_));
        try
        {
            ffpython_t::reload(name_);
        }
        catch(exception& e_)
        {
            LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::reload exeception=%s", e_.what()));
            return -1;
        }
        LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::reload end ok name_[%s]", name_));
        return 0;
    }
    ffslot_t::callback_t* gen_verify_callback()
    {
        struct lambda_cb: public ffslot_t::callback_t
        {
            lambda_cb(ffscene_python_t* p):ffscene(p){}
            virtual void exe(ffslot_t::callback_arg_t* args_)
            {
                if (args_->type() != TYPEID(session_verify_arg))
                {
                    return;
                }
                session_verify_arg* data = (session_verify_arg*)args_;
                static string func_name  = VERIFY_CB_NAME;
                try
                {
                    vector<string> ret = ffscene->m_ffpython.call<vector<string> >(ffscene->m_ext_name, func_name,
                                                                                   data->session_key, data->online_time,
                                                                                   data->ip, data->gate_name);
                    if (ret.size() >= 1)
                    {
                        data->alloc_session_id = ret[0];
                    }
                    if (ret.size() >= 2)
                    {
                        data->extra_data = ret[1];
                    }
                }
                catch(exception& e_)
                {
                    LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::gen_verify_callback exception<%s>", e_.what()));
                }
            }
            virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
            ffscene_python_t* ffscene;
        };
        return new lambda_cb(this);
    }
        
    ffslot_t::callback_t* gen_enter_callback()
    {
        struct lambda_cb: public ffslot_t::callback_t
        {
            lambda_cb(ffscene_python_t* p):ffscene(p){}
            virtual void exe(ffslot_t::callback_arg_t* args_)
            {
                if (args_->type() != TYPEID(session_enter_arg))
                {
                    return;
                }
                session_enter_arg* data = (session_enter_arg*)args_;
                static string func_name  = ENTER_CB_NAME;
                try
                {
                    ffscene->m_ffpython.call<void>(ffscene->m_ext_name, func_name,
                                                   data->session_id, data->from_scene,
                                                   data->extra_data);
                }
                catch(exception& e_)
                {
                    LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::gen_enter_callback exception<%s>", e_.what()));
                }
            }
            virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
            ffscene_python_t* ffscene;
        };
        return new lambda_cb(this);
    }
        
    ffslot_t::callback_t* gen_offline_callback()
    {
        struct lambda_cb: public ffslot_t::callback_t
        {
            lambda_cb(ffscene_python_t* p):ffscene(p){}
            virtual void exe(ffslot_t::callback_arg_t* args_)
            {
                if (args_->type() != TYPEID(session_offline_arg))
                {
                    return;
                }
                session_offline_arg* data = (session_offline_arg*)args_;
                static string func_name   = OFFLINE_CB_NAME;
                try
                {
                    ffscene->m_ffpython.call<void>(ffscene->m_ext_name, func_name,
                                                   data->session_id, data->online_time);
                }
                catch(exception& e_)
                {
                    LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::gen_offline_callback exception<%s>", e_.what()));
                }
            }
            virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
            ffscene_python_t* ffscene;
        };
        return new lambda_cb(this);
    }
        
    ffslot_t::callback_t* gen_logic_callback()
    {
        struct lambda_cb: public ffslot_t::callback_t
        {
            lambda_cb(ffscene_python_t* p):ffscene(p){}
            virtual void exe(ffslot_t::callback_arg_t* args_)
            {
                if (args_->type() != TYPEID(logic_msg_arg))
                {
                    return;
                }
                logic_msg_arg* data = (logic_msg_arg*)args_;
                static string func_name  = LOGIC_CB_NAME;
                LOGINFO((FFSCENE_PYTHON, "ffscene_python_t::gen_logic_callback body<%s>,len[%lu]", data->body, data->body.size()));
                try
                {
                    ffscene->m_ffpython.call<void>(ffscene->m_ext_name, func_name,
                                                   data->session_id, data->cmd, data->body);
                }
                catch(exception& e_)
                {
                    LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::gen_logic_callback exception<%s>", e_.what()));
                }
            }
            virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
            ffscene_python_t* ffscene;
        };
        return new lambda_cb(this);
    }

    //! 定时器接口
    int once_timer(int timeout_, uint64_t id_)
    {
        struct lambda_cb
        {
            static void call_py(ffscene_python_t* ffscene, uint64_t id)
            {
                LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::once_timer call_py id<%u>", id));
                static string func_name  = TIMER_CB_NAME;
                try
                {
                    ffscene->m_ffpython.call<void>(ffscene->m_ext_name, func_name, id);
                }
                catch(exception& e_)
                {
                    LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::gen_logic_callback exception<%s>", e_.what()));
                }
            }
            static void callback(ffscene_python_t* ffscene, task_queue_t* tq_, uint64_t id)
            {
                tq_->produce(task_binder_t::gen(&lambda_cb::call_py, ffscene, id));
            }
        };
        LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::once_timer begin id<%u>", id_));
        m_ffrpc->get_timer().once_timer(timeout_, task_binder_t::gen(&lambda_cb::callback, this, &(m_ffrpc->get_tq()), id_));
        return 0;
    }
    

public:
    ffpython_t      m_ffpython;
    string          m_ext_name;
};

}

#endif
