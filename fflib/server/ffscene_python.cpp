
#include "server/ffscene_python.h"
#include "base/performance_daemon.h"

#include "python/ffpython.h"

using namespace ff;
//namespace ff {
template<>
struct pytype_traits_t<ffjson_tool_t>
{
    static PyObject* pyobj_from_cppobj(const ffjson_tool_t& val_)
    {
        return pytype_traits_t<ffjson_tool_t>::pyobj_from_json_obj(*(val_.jval.get()));
    }
    static PyObject* pyobj_from_json_obj(const json_value_t& jval)
	{
	    //const char* kTypeNames[] = { "Null", "False", "True", "Object", "Array", "String", "Number" };
	    //printf("jval type=%s\n", kTypeNames[jval.GetType()]);
	    if (jval.IsString())
	    {
	        return PyString_FromStringAndSize(jval.GetString(), jval.GetStringLength());
	    }
	    else if (jval.IsBool())
	    {
	        if (jval.GetBool())
	        {
                Py_RETURN_TRUE;
            }
            Py_RETURN_FALSE;
    	}
	    else if (jval.IsNumber())
	    {
	        if (jval.IsInt())
	        {
	            return PyLong_FromLong(long(jval.GetInt()));
	        }
	        else if (jval.IsInt64())
	        {
	            return PyLong_FromLong(long(jval.GetInt64()));
	        }
	        return PyFloat_FromDouble(jval.GetDouble());
	    }
		else if (jval.IsArray())
		{
		    PyObject* ret = PyList_New(jval.Size());

            for (size_t i = 0; i < jval.Size(); ++i)
            {
                PyList_SetItem(ret, i, pytype_traits_t<ffjson_tool_t>::pyobj_from_json_obj(jval[i]));
            }
            return ret;
		}
		else if (jval.IsObject())
		{
		    PyObject* ret = PyDict_New();
		    for (json_value_t::ConstMemberIterator itr = jval.MemberBegin(); itr != jval.MemberEnd(); ++itr)
		    {   
		        PyObject *k = pytype_traits_t<ffjson_tool_t>::pyobj_from_json_obj(itr->name);
                PyObject *v = pytype_traits_t<ffjson_tool_t>::pyobj_from_json_obj(itr->value);
                PyDict_SetItem(ret, k, v);
                Py_DECREF(k);
                Py_DECREF(v);
            }
            return ret;
		}
		else
	    {
	        Py_RETURN_NONE;
	    }
	}
    
    static int pyobj_to_cppobj(PyObject *pvalue_, ffjson_tool_t& ffjson_tool)
    {
        pytype_traits_t<ffjson_tool_t>::pyobj_to_json_obj(pvalue_, ffjson_tool, *(ffjson_tool.jval.get()));
        return 0;
    }
    
    static int pyobj_to_json_obj(PyObject *pvalue_, ffjson_tool_t& ffjson_tool, json_value_t& jval)
    {
        if (Py_False ==  pvalue_|| Py_None == pvalue_)
        {
            jval.SetBool(false);
        }
        else if (Py_True == pvalue_)
        {
            jval.SetBool(true);
        }
        else if (true == PyString_Check(pvalue_))
        {
            jval.SetString(PyString_AsString(pvalue_), PyString_Size(pvalue_), *ffjson_tool.allocator);
        }
        else if (true == PyUnicode_Check(pvalue_))
        {
#ifdef _WIN32
            PyObject* retStr = PyUnicode_AsEncodedString(pvalue_, "gbk", "");
#else
            PyObject* retStr = PyUnicode_AsUTF8String(pvalue_);
#endif
            if (retStr)
            {
                jval.SetString(PyString_AsString(retStr), PyString_Size(retStr), *ffjson_tool.allocator);
                Py_XDECREF(retStr);
                return 0;
            }
        }
        else if (true == PyLong_Check(pvalue_))
        {
            jval.SetInt64((long)PyLong_AsLong(pvalue_));
        }
        else if (true == PyInt_Check(pvalue_))
        {
            jval.SetInt((int)PyLong_AsLong(pvalue_));
        }
        else if (true == PyTuple_Check(pvalue_))
        {
            jval.SetArray();
            int n = PyTuple_Size(pvalue_);
            for (int i = 0; i < n; ++i)
            {
                json_value_t tmp_val;
                pytype_traits_t<ffjson_tool_t>::pyobj_to_json_obj(PyTuple_GetItem(pvalue_, i), ffjson_tool, tmp_val);
                jval.PushBack(tmp_val, *ffjson_tool.allocator);
            }
        }
        else if (true == PyList_Check(pvalue_))
        {
            jval.SetArray();
            int n = PyList_Size(pvalue_);
            for (int i = 0; i < n; ++i)
            {
                json_value_t tmp_val;
                pytype_traits_t<ffjson_tool_t>::pyobj_to_json_obj(PyList_GetItem(pvalue_, i), ffjson_tool, tmp_val);
                jval.PushBack(tmp_val, *ffjson_tool.allocator);
            }
        }
        else if (true == PyDict_Check(pvalue_))
        {
            jval.SetObject();
            PyObject *key = NULL, *value = NULL;
            Py_ssize_t pos = 0;

            while (PyDict_Next(pvalue_, &pos, &key, &value))
            {
                json_value_t tmp_key;
                json_value_t tmp_val;
                if (pytype_traits_t<ffjson_tool_t>::pyobj_to_json_obj(key, ffjson_tool, tmp_key) ||
                    pytype_traits_t<ffjson_tool_t>::pyobj_to_json_obj(value, ffjson_tool, tmp_val))
                {
                    return -1;
                }
                jval.AddMember(tmp_key, tmp_val, *(ffjson_tool.allocator));
            }
        }
        else
        {
            jval.SetNull();
        }
        return 0;
    }
    
    static const char* get_typename() { return "ffjson_tool_t";}
};
//}

ffscene_python_t::ffscene_python_t()
{
    m_ffpython = new ffpython_t();
}
ffscene_python_t::~ffscene_python_t()
{
    delete m_ffpython;
    m_ffpython = NULL;
}

arg_helper_t ffscene_python_t::g_arg_helper("");

void ffscene_python_t::py_send_msg_session(const userid_t& session_id_, uint16_t cmd_, const string& data_)
{
    singleton_t<ffscene_python_t>::instance().send_msg_session(session_id_, cmd_, data_);
}
void ffscene_python_t::py_broadcast_msg_session(uint16_t cmd_, const string& data_)
{
    singleton_t<ffscene_python_t>::instance().broadcast_msg_session(cmd_, data_);
}
string ffscene_python_t::py_get_config(const string& key_)
{
    return g_arg_helper.get_option_value(key_);
}
void ffscene_python_t::py_verify_session_id(long key, const userid_t& session_id_, const string& data_)
{
    singleton_t<ffscene_python_t>::instance().verify_session_id(key, session_id_, data_);
}

static bool py_post_task(const string& name, const string& func_name, const ffjson_tool_t& task_args, long callback_id)
{
    task_dispather_i* d = singleton_t<task_dispather_mgr_t>::instance().get(name);
    if (d)
    {
        d->post_task(func_name, task_args, callback_id);
    }
    return d != NULL;
}

int ffscene_python_t::open(arg_helper_t& arg_helper)
{
    LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::open begin"));
    
    ffscene_python_t::g_arg_helper = arg_helper;
    m_ext_name = MOD_NAME;
    (*m_ffpython).reg_class<ffscene_python_t, PYCTOR()>("ffscene_t")
              .reg(&ffscene_python_t::send_msg_session, "send_msg_session")
              .reg(&ffscene_python_t::multicast_msg_session, "multicast_msg_session")
              .reg(&ffscene_python_t::broadcast_msg_session, "broadcast_msg_session")
              .reg(&ffscene_python_t::broadcast_msg_gate, "broadcast_msg_gate")
              .reg(&ffscene_python_t::close_session, "close_session")
              .reg(&ffscene_python_t::change_session_scene, "change_session_scene")
              .reg(&ffscene_python_t::once_timer, "once_timer")
              .reg(&ffscene_python_t::reload, "reload")
              .reg(&ffscene_python_t::pylog, "pylog")
              .reg(&ffscene_python_t::is_exist, "is_exist")
              .reg(&ffscene_python_t::connect_db, "connect_db")
              .reg(&ffscene_python_t::db_query, "db_query")
              .reg(&ffscene_python_t::sync_db_query, "sync_db_query")
              .reg(&ffscene_python_t::call_service, "call_service")
              .reg(&ffscene_python_t::bridge_call_service, "bridge_call_service");


    (*m_ffpython).reg(&ffdb_t::escape, "escape")
                 .reg(&ffscene_python_t::py_send_msg_session, "py_send_msg_session")
                 .reg(&ffscene_python_t::py_broadcast_msg_session, "py_broadcast_msg_session")
                 .reg(&ffscene_python_t::py_verify_session_id, "py_verify_session_id")
                 .reg(&ffscene_python_t::py_get_config, "py_get_config")
                 .reg(&::py_post_task, "py_post_task");

    (*m_ffpython).init("ff");
    (*m_ffpython).set_global_var("ff", "ffscene_obj", (ffscene_python_t*)this);

    this->callback_info().verify_callback = gen_verify_callback();
    this->callback_info().enter_callback = gen_enter_callback();
    this->callback_info().offline_callback = gen_offline_callback();
    this->callback_info().logic_callback = gen_logic_callback();
    this->callback_info().scene_call_callback = gen_scene_call_callback();

    ffpython_t::add_path("./pylib");
    if (arg_helper.is_enable_option("-python_path"))
    {
        ffpython_t::add_path(arg_helper.get_option_value("-python_path"));
    }
    
    if (arg_helper.is_enable_option("-python_mod"))
    {
        singleton_t<task_dispather_mgr_t>::instance().add(arg_helper.get_option_value("python_mod"), this);
    }
    

    m_db_mgr.start();

    int ret = ffscene_t::open(arg_helper);
    try{
        (*m_ffpython).load("main");
        ret = (*m_ffpython).call<int>("main", string("init"));
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::open failed er=<%s>", e_.what()));
        try{
            (*m_ffpython).call<void>("main", string("cleanup"));
        }
        catch(exception& ee_)
        {
            LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::open failed er=<%s>", ee_.what()));
        }
        return -1;
    }
    LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::open end ok"));
    return ret;
}

int ffscene_python_t::close()
{
    int ret = 0;
    try
    {
        ret = (*m_ffpython).call<int>("main", string("cleanup"));
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::close failed er=<%s>", e_.what()));
    }
    ffscene_t::close();
    Py_Finalize();
    m_db_mgr.stop();
    return ret;
}

string ffscene_python_t::reload(const string& name_)
{
    AUTO_PERF();
    LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::reload begin name_[%s]", name_));
    try
    {
        ffpython_t::reload(name_);
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::reload exeception=%s", e_.what()));
        return e_.what();
    }
    LOGTRACE((FFSCENE_PYTHON, "ffscene_python_t::reload end ok name_[%s]", name_));
    return "";
}

void ffscene_python_t::pylog(int level_, const string& mod_, const string& content_)
{
    switch (level_)
    {
        case 1:
        {
            LOGFATAL((mod_.c_str(), "%s", content_));
        }
        break;
        case 2:
        {
            LOGERROR((mod_.c_str(), "%s", content_));
        }
        break;
        case 3:
        {
            LOGWARN((mod_.c_str(), "%s", content_));
        }
        break;
        case 4:
        {
            LOGINFO((mod_.c_str(), "%s", content_));
        }
        break;
        case 5:
        {
            LOGDEBUG((mod_.c_str(), "%s", content_));
        }
        break;
        case 6:
        {
            LOGTRACE((mod_.c_str(), "%s", content_));
        }
        break;
        default:
        {
            LOGTRACE((mod_.c_str(), "%s", content_));
        }
        break;
    }
}
//! 判断某个service是否存在
bool ffscene_python_t::is_exist(const string& service_name_)
{
    return m_ffrpc->is_exist(service_name_);
}

ffslot_t::callback_t* ffscene_python_t::gen_verify_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_python_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            PERF("verify_callback");
            if (args_->type() != TYPEID(session_verify_arg))
            {
                return;
            }
            session_verify_arg* data = (session_verify_arg*)args_;
            static string func_name  = VERIFY_CB_NAME;
            try
            {
                vector<string> ret = ffscene->get_ffpython().call<vector<string> >(ffscene->m_ext_name, func_name, data->key_id,
                                                                               data->session_key, data->online_time,
                                                                               data->ip, data->gate_name);
                if (ret.size() >= 1)
                {
                    data->flag_verify = true;
                    data->alloc_session_id = ::atol(ret[0].c_str());
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

ffslot_t::callback_t* ffscene_python_t::gen_enter_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_python_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            PERF("enter_callback");
            if (args_->type() != TYPEID(session_enter_arg))
            {
                return;
            }
            session_enter_arg* data = (session_enter_arg*)args_;
            static string func_name  = ENTER_CB_NAME;
            try
            {
                ffscene->get_ffpython().call<void>(ffscene->m_ext_name, func_name,
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

ffslot_t::callback_t* ffscene_python_t::gen_offline_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_python_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            PERF("offline_callback");
            if (args_->type() != TYPEID(session_offline_arg))
            {
                return;
            }
            session_offline_arg* data = (session_offline_arg*)args_;
            static string func_name   = OFFLINE_CB_NAME;
            try
            {
                ffscene->get_ffpython().call<void>(ffscene->m_ext_name, func_name,
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
ffslot_t::callback_t* ffscene_python_t::gen_logic_callback()
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
            LOGDEBUG((FFSCENE_PYTHON, "ffscene_python_t::gen_logic_callback len[%lu]", data->body.size()));
            
            AUTO_CMD_PERF("logic_callback", data->cmd);
            try
            {
                ffscene->get_ffpython().call<void>(ffscene->m_ext_name, func_name,
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

ffslot_t::callback_t* ffscene_python_t::gen_scene_call_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_python_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(scene_call_msg_arg))
            {
                return;
            }
            scene_call_msg_arg* data = (scene_call_msg_arg*)args_;
            static string func_name  = SCENE_CALL_CB_NAME;
            LOGINFO((FFSCENE_PYTHON, "ffscene_python_t::gen_scene_call_callback len[%lu]", data->body.size()));
            
            AUTO_CMD_PERF("scene_callback", data->cmd);
            try
            {
                vector<string> ret = ffscene->get_ffpython().call<vector<string> >(ffscene->m_ext_name, func_name,
                                                                                data->cmd, data->body);
                if (ret.size() == 2)
                {
                    data->msg_type = ret[0];
                    data->ret      = ret[1];
                }
            }
            catch(exception& e_)
            {
                data->err = e_.what();
                LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::gen_scene_call_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_python_t* ffscene;
    };
    return new lambda_cb(this);
}

//! 定时器接口
int ffscene_python_t::once_timer(int timeout_, uint64_t id_)
{
    struct lambda_cb
    {
        static void call_py(ffscene_python_t* ffscene, uint64_t id)
        {
            LOGDEBUG((FFSCENE_PYTHON, "ffscene_python_t::once_timer call_py id<%u>", id));
            static string func_name  = TIMER_CB_NAME;
            PERF("once_timer");
            try
            {
                ffscene->get_ffpython().call<void>(ffscene->m_ext_name, func_name, id);
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
    LOGDEBUG((FFSCENE_PYTHON, "ffscene_python_t::once_timer begin id<%u>", id_));
    m_ffrpc->get_timer().once_timer(timeout_, task_binder_t::gen(&lambda_cb::callback, this, &(m_ffrpc->get_tq()), id_));
    return 0;
}

ffslot_t::callback_t* ffscene_python_t::gen_db_query_callback(long callback_id_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_python_t* p, long callback_id_):ffscene(p), callback_id(callback_id_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(db_mgr_t::db_query_result_t))
            {
                return;
            }
            db_mgr_t::db_query_result_t* data = (db_mgr_t::db_query_result_t*)args_;

            ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&lambda_cb::call_python, ffscene, callback_id,
                                                                   data->ok, data->result_data, data->col_names));
        }
        static void call_python(ffscene_python_t* ffscene, long callback_id_,
                                bool ok, const vector<vector<string> >& ret_, const vector<string>& col_)
        {
            PERF("db_query_callback");
            static string func_name   = DB_QUERY_CB_NAME;
            try
            {
                ffscene->get_ffpython().call<void>(ffscene->m_ext_name, func_name,
                                               callback_id_, ok, ret_, col_);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::gen_db_query_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene, callback_id); }
        ffscene_python_t* ffscene;
        long              callback_id;
    };
    return new lambda_cb(this, callback_id_);
}


//! 创建数据库连接
long ffscene_python_t::connect_db(const string& host_)
{
    return m_db_mgr.connect_db(host_);
}
void ffscene_python_t::db_query(long db_id_,const string& sql_, long callback_id_)
{
    m_db_mgr.db_query(db_id_, sql_, gen_db_query_callback(callback_id_));
}
vector<vector<string> > ffscene_python_t::sync_db_query(long db_id_,const string& sql_)
{
    vector<vector<string> > ret;
    m_db_mgr.sync_db_query(db_id_, sql_, ret);
    return ret;
}
void ffscene_python_t::call_service(const string& name_, long cmd_, const string& msg_, long id_)
{
    scene_call_msg_t::in_t inmsg;
    inmsg.cmd = cmd_;
    inmsg.body = msg_;
    m_ffrpc->call(name_, inmsg, ffrpc_ops_t::gen_callback(&ffscene_python_t::call_service_return_msg, this, id_));
}
void ffscene_python_t::bridge_call_service(const string& group_name_, const string& name_, long cmd_, const string& msg_, long id_)
{
    scene_call_msg_t::in_t inmsg;
    inmsg.cmd = cmd_;
    inmsg.body = msg_;
    m_ffrpc->call(group_name_, name_, inmsg, ffrpc_ops_t::gen_callback(&ffscene_python_t::call_service_return_msg, this, id_));
}
void ffscene_python_t::call_service_return_msg(ffreq_t<scene_call_msg_t::out_t>& req_, long id_)
{
    AUTO_PERF();
    static string func_name   = CALL_SERVICE_RETURN_MSG_CB_NAME;
    try
    {
        (*m_ffpython).call<void>(m_ext_name, func_name,
                              id_, req_.msg.err, req_.msg.msg_type, req_.msg.body);
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::gen_db_query_callback exception<%s>", e_.what()));
    }
}

//! 线程间传递消息
void ffscene_python_t::post_task(const string& func_name, const ffjson_tool_t& task_args, long callback_id)
{
    m_ffrpc->get_tq().produce(task_binder_t::gen(&ffscene_python_t::post_task_impl, this, func_name, task_args, callback_id));
}

void ffscene_python_t::post_task_impl(const string& func_name, const ffjson_tool_t& task_args, long callback_id)
{
    try
    {
        (*m_ffpython).call<void>(m_ext_name, func_name.c_str(), task_args, callback_id);
         
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_python_t::post_task_impl exception<%s>", e_.what()));
    }
}

