
#include "lua/fflua.h"
#include "server/ffscene_lua.h"
#include "base/performance_daemon.h"

namespace ff{

template<> struct lua_op_t<ffjson_tool_t>
{
    static void push_stack(lua_State* ls_, const ffjson_tool_t& jtool_)
	{
	    lua_op_t<ffjson_tool_t>::push_stack(ls_, *(jtool_.jval.get()));
	}
	static void push_stack(lua_State* ls_, const json_value_t& jval)
	{
	    //const char* kTypeNames[] = { "Null", "False", "True", "Object", "Array", "String", "Number" };
	    //printf("jval type=%s\n", kTypeNames[jval.GetType()]);
	    if (jval.IsString())
	    {
	        lua_pushlstring(ls_, jval.GetString(), jval.GetStringLength());
	        //lua_pushstring(ls_, jval.GetString());
	    }
	    else if (jval.IsBool())
	    {
	        lua_pushboolean(ls_, jval.GetBool());
	    }
	    else if (jval.IsNumber())
	    {
	        lua_pushnumber(ls_, (lua_Number)(jval.GetDouble()));
	    }
		else if (jval.IsArray())
		{
		    lua_newtable(ls_);
		    for (int i = 0; i < (int)jval.Size(); i++)
		    {
		        lua_op_t<int>::push_stack(ls_, i);
		        lua_op_t<ffjson_tool_t>::push_stack(ls_, jval[i]);
		        lua_settable(ls_, -3);
		    }
		}
		else if (jval.IsObject())
		{
		    lua_newtable(ls_);
		    for (json_value_t::ConstMemberIterator itr = jval.MemberBegin(); itr != jval.MemberEnd(); ++itr)
		    {
		        string tmp = itr->name.GetString();
		        lua_op_t<string>::push_stack(ls_, tmp);
		        lua_op_t<ffjson_tool_t>::push_stack(ls_, itr->value);
		        lua_settable(ls_, -3);
            }
		}
		else
	    {
	        lua_pushnil(ls_);
	    }
	}
	/*
	static int get_ret_value(lua_State* ls_, int pos_, ffjson_tool_t& param_)
	{
		return 0;
	}
	*/
	static int lua_to_value(lua_State* ls_, int pos_, ffjson_tool_t& ffjson_tool)
	{
	    lua_op_t<ffjson_tool_t>::lua_to_value(ls_, pos_, ffjson_tool, *(ffjson_tool.jval));
	    return 0;
	}
	static int lua_to_value(lua_State* ls_, int pos_, ffjson_tool_t& ffjson_tool, json_value_t& jval)
	{
        int t = lua_type(ls_, pos_);
        //printf("lua_to_value	%s -\n", lua_typename(ls_, lua_type(ls_, pos_)));
        switch (t)
        {
            case LUA_TSTRING:
            {
                //printf("LUA_TSTRING `%s'\n", lua_tostring(ls_, pos_));
                jval.SetString(lua_tostring(ls_, pos_), *ffjson_tool.allocator);
            }
            break;
            case LUA_TBOOLEAN:
            {
                //printf(lua_toboolean(ls_, pos_) ? "true" : "false");
                jval.SetBool(lua_toboolean(ls_, pos_));
            }
            break;
            case LUA_TNUMBER:
            {
                printf("`%g`", lua_tonumber(ls_, pos_));
                jval.SetDouble(lua_tonumber(ls_, pos_));
            }
            break;
            case LUA_TTABLE:
            {
            	//printf("table begin pos_=%d\n", pos_);
            	int table_pos = lua_gettop(ls_);
            	if (pos_ < 0)
            	{
            	    pos_= table_pos + (pos_ - (-1));
            	}
            	
            	bool is_array = true;
            	lua_pushnil(ls_);
            	int index = 0;
                while (lua_next(ls_, pos_) != 0) {
                    if (lua_type(ls_, -2) == LUA_TNUMBER)
                    {
                        if (double(++index) != lua_tonumber(ls_, -2))
                        {
                            is_array = false;
                        }
                    }
                    else
                    {
                        is_array = false;
                    }
                    lua_pop(ls_, 1);
                }
                if (is_array)
                {
                    jval.SetArray();
                    lua_pushnil(ls_);
                    while (lua_next(ls_, pos_) != 0) {
                        //printf("	%s - %s\n",
                        //        lua_typename(ls_, lua_type(ls_, -2)),
                        //        lua_typename(ls_, lua_type(ls_, -1)));

                        json_value_t tmp_val;
                        lua_op_t<ffjson_tool_t>::lua_to_value(ls_, -1, ffjson_tool, tmp_val);
                        
                        //printf("key=`%s'\n", lua_tostring(ls_, -2));
                        jval.PushBack(tmp_val, *ffjson_tool.allocator);
                        lua_pop(ls_, 1);
                    }
                }
                else
                {
                    jval.SetObject();
                    lua_pushnil(ls_);
                    while (lua_next(ls_, pos_) != 0) {
                        //printf("	%s - %s\n",
                        //        lua_typename(ls_, lua_type(ls_, -2)),
                        //        lua_typename(ls_, lua_type(ls_, -1)));
                                
                        json_value_t tmp_key;
                        json_value_t tmp_val;
                        lua_op_t<ffjson_tool_t>::lua_to_value(ls_, -2, ffjson_tool, tmp_key);
                        lua_op_t<ffjson_tool_t>::lua_to_value(ls_, -1, ffjson_tool, tmp_val);
                        jval.AddMember(tmp_key, tmp_val, *(ffjson_tool.allocator));
                        //printf("key=`%s'\n", lua_tostring(ls_, -2));
                        
                        lua_pop(ls_, 1);
                    }
                }
                //printf("table end\n");
            }
            break;
            default:
            {
                jval.SetNull();
                //printf("`%s`", lua_typename(ls_, t));
            }
            break;
        }
		return 0;
	}
};
}

using namespace ff;

ffscene_lua_t::ffscene_lua_t():
    m_arg_helper("")
{
    m_fflua = new fflua_t();
}
ffscene_lua_t::~ffscene_lua_t()
{
    delete m_fflua;
    m_fflua = NULL;
}

static string json_encode(ffjson_tool_t& ffjson_tool)
{
    if (ffjson_tool.jval->IsNull())
    {
        return "null";
    }
    typedef rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::Document::AllocatorType> FFStringBuffer;
    FFStringBuffer            str_buff(ffjson_tool.allocator.get());
    rapidjson::Writer<FFStringBuffer> writer(str_buff, ffjson_tool.allocator.get());
    ffjson_tool.jval->Accept(writer);
    string output(str_buff.GetString(), str_buff.Size());
    //printf("output=%s\n", output.c_str());
    return output;
}

static ffjson_tool_t json_decode(const string& src)
{
    ffjson_tool_t ffjson_tool;
    if (false == ffjson_tool.jval->Parse<0>(src.c_str()).HasParseError())
    {
        
    }
    return ffjson_tool;
}
static void lua_reg(lua_State* ls)  
{
    //! 注册基类函数, ctor() 为构造函数的类型  
    fflua_register_t<ffscene_lua_t, ctor()>(ls, "ffscene_t")  //! 注册构造函数
                    .def(&ffscene_lua_t::send_msg_session, "send_msg_session")
                    .def(&ffscene_lua_t::multicast_msg_session, "multicast_msg_session")
                    .def(&ffscene_lua_t::broadcast_msg_session, "broadcast_msg_session")
                    .def(&ffscene_lua_t::broadcast_msg_gate, "broadcast_msg_gate")
                    .def(&ffscene_lua_t::close_session, "close_session")
                    .def(&ffscene_lua_t::change_session_scene, "change_session_scene")
                    .def(&ffscene_lua_t::once_timer, "once_timer")
                    .def(&ffscene_lua_t::reload, "reload")
                    .def(&ffscene_lua_t::pylog, "pylog")
                    .def(&ffscene_lua_t::is_exist, "is_exist")
                    .def(&ffscene_lua_t::connect_db, "connect_db")
                    .def(&ffscene_lua_t::db_query, "db_query")
                    .def(&ffscene_lua_t::sync_db_query, "sync_db_query")
                    .def(&ffscene_lua_t::call_service, "call_service")    
                    .def(&ffscene_lua_t::bridge_call_service, "bridge_call_service");

    fflua_register_t<>(ls)  
                    .def(&ffdb_t::escape, "escape")
                    .def(&json_encode, "json_encode")
                    .def(&json_decode, "json_decode");

}

int ffscene_lua_t::open(arg_helper_t& arg_helper)
{
    LOGTRACE((FFSCENE_PYTHON, "ffscene_lua_t::open begin"));
    m_arg_helper = arg_helper;

    (*m_fflua).reg(lua_reg);
    (*m_fflua).set_global_variable("ffscene_obj", (ffscene_lua_t*)this);

    this->callback_info().verify_callback = gen_verify_callback();
    this->callback_info().enter_callback = gen_enter_callback();
    this->callback_info().offline_callback = gen_offline_callback();
    this->callback_info().logic_callback = gen_logic_callback();
    this->callback_info().scene_call_callback = gen_scene_call_callback();

    (*m_fflua).add_package_path("./lualib");
    if (arg_helper.is_enable_option("-lua_path"))
    {
        (*m_fflua).add_package_path(arg_helper.get_option_value("-lua_path"));
    }

    m_db_mgr.start();

    int ret = ffscene_t::open(arg_helper);
    try{
        (*m_fflua).load_file("main.lua");
        ret = (*m_fflua).call<int>("init");
        
        //rapidjson::Document::AllocatorType allocator;
        ffjson_tool_t ffjson_tool;
        ffjson_tool.jval->SetObject();
        //json_value_t jval(rapidjson::kObjectType);
        json_value_t ibj_json(rapidjson::kObjectType);
        string dest_ = "TTTT";
        json_value_t tmp_val;
        tmp_val.SetString(dest_.c_str(), dest_.length(), *ffjson_tool.allocator);
        ibj_json.AddMember("dumy", tmp_val, *ffjson_tool.allocator);
        ffjson_tool.jval->AddMember("foo", ibj_json, *(ffjson_tool.allocator));
     

        (*m_fflua).call<void>("test", ffjson_tool);
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::open failed er=<%s>", e_.what()));

        return -1;
    }
    LOGTRACE((FFSCENE_PYTHON, "ffscene_lua_t::open end ok"));
    return ret;
}

int ffscene_lua_t::close()
{
    int ret = 0;
    try
    {
        ret = (*m_fflua).call<int>("main", string("cleanup"));
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::close failed er=<%s>", e_.what()));
    }
    ffscene_t::close();
    m_db_mgr.stop();
    return ret;
}

string ffscene_lua_t::reload(const string& name_)
{
    AUTO_PERF();
    LOGTRACE((FFSCENE_PYTHON, "ffscene_lua_t::reload begin name_[%s]", name_));
    try
    {
        //! ffpython_t::reload(name_);
        //! TODO
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::reload exeception=%s", e_.what()));
        return e_.what();
    }
    LOGTRACE((FFSCENE_PYTHON, "ffscene_lua_t::reload end ok name_[%s]", name_));
    return "";
}

void ffscene_lua_t::pylog(int level_, const string& mod_, const string& content_)
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
bool ffscene_lua_t::is_exist(const string& service_name_)
{
    return m_ffrpc->is_exist(service_name_);
}

ffslot_t::callback_t* ffscene_lua_t::gen_verify_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
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
                vector<string> ret = ffscene->get_fflua().call<vector<string> >(func_name.c_str(), data->key_id,
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
                LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::gen_verify_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}

ffslot_t::callback_t* ffscene_lua_t::gen_enter_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
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
                ffscene->get_fflua().call<void>(func_name.c_str(),
                                               data->session_id, data->from_scene,
                                               data->extra_data);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::gen_enter_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}

ffslot_t::callback_t* ffscene_lua_t::gen_offline_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
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
                ffscene->get_fflua().call<void>(func_name.c_str(),
                                               data->session_id, data->online_time);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::gen_offline_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}
ffslot_t::callback_t* ffscene_lua_t::gen_logic_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(logic_msg_arg))
            {
                return;
            }
            logic_msg_arg* data = (logic_msg_arg*)args_;
            static string func_name  = LOGIC_CB_NAME;
            LOGDEBUG((FFSCENE_PYTHON, "ffscene_lua_t::gen_logic_callback len[%lu]", data->body.size()));
            
            AUTO_CMD_PERF("logic_callback", data->cmd);
            try
            {
                ffscene->get_fflua().call<void>(func_name.c_str(),
                                               data->session_id, data->cmd, data->body);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::gen_logic_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}

ffslot_t::callback_t* ffscene_lua_t::gen_scene_call_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(scene_call_msg_arg))
            {
                return;
            }
            scene_call_msg_arg* data = (scene_call_msg_arg*)args_;
            static string func_name  = SCENE_CALL_CB_NAME;
            LOGINFO((FFSCENE_PYTHON, "ffscene_lua_t::gen_scene_call_callback len[%lu]", data->body.size()));
            
            AUTO_CMD_PERF("scene_callback", data->cmd);
            try
            {
                vector<string> ret = ffscene->get_fflua().call<vector<string> >(func_name.c_str(),
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
                LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::gen_scene_call_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}

//! 定时器接口
int ffscene_lua_t::once_timer(int timeout_, uint64_t id_)
{
    struct lambda_cb
    {
        static void call_py(ffscene_lua_t* ffscene, uint64_t id)
        {
            LOGDEBUG((FFSCENE_PYTHON, "ffscene_lua_t::once_timer call_py id<%u>", id));
            static string func_name  = TIMER_CB_NAME;
            PERF("once_timer");
            try
            {
                ffscene->get_fflua().call<void>(func_name.c_str(), id);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::gen_logic_callback exception<%s>", e_.what()));
            }
        }
        static void callback(ffscene_lua_t* ffscene, task_queue_t* tq_, uint64_t id)
        {
            tq_->produce(task_binder_t::gen(&lambda_cb::call_py, ffscene, id));
        }
    };
    LOGDEBUG((FFSCENE_PYTHON, "ffscene_lua_t::once_timer begin id<%u>", id_));
    m_ffrpc->get_timer().once_timer(timeout_, task_binder_t::gen(&lambda_cb::callback, this, &(m_ffrpc->get_tq()), id_));
    return 0;
}

ffslot_t::callback_t* ffscene_lua_t::gen_db_query_callback(long callback_id_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p, long callback_id_):ffscene(p), callback_id(callback_id_){}
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
        static void call_python(ffscene_lua_t* ffscene, long callback_id_,
                                bool ok, const vector<vector<string> >& ret_, const vector<string>& col_)
        {
            PERF("db_query_callback");
            static string func_name   = DB_QUERY_CB_NAME;
            try
            {
                ffscene->get_fflua().call<void>(func_name.c_str(),
                                               callback_id_, ok, ret_, col_);

            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::gen_db_query_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene, callback_id); }
        ffscene_lua_t* ffscene;
        long              callback_id;
    };
    return new lambda_cb(this, callback_id_);
}


//! 创建数据库连接
long ffscene_lua_t::connect_db(const string& host_)
{
    return m_db_mgr.connect_db(host_);
}
void ffscene_lua_t::db_query(long db_id_,const string& sql_, long callback_id_)
{
    m_db_mgr.db_query(db_id_, sql_, gen_db_query_callback(callback_id_));
}
vector<vector<string> > ffscene_lua_t::sync_db_query(long db_id_,const string& sql_)
{
    vector<vector<string> > ret;
    m_db_mgr.sync_db_query(db_id_, sql_, ret);
    return ret;
}
void ffscene_lua_t::call_service(const string& name_, long cmd_, const string& msg_, long id_)
{
    scene_call_msg_t::in_t inmsg;
    inmsg.cmd = cmd_;
    inmsg.body = msg_;
    m_ffrpc->call(name_, inmsg, ffrpc_ops_t::gen_callback(&ffscene_lua_t::call_service_return_msg, this, id_));
}
void ffscene_lua_t::bridge_call_service(const string& group_name_, const string& name_, long cmd_, const string& msg_, long id_)
{
    scene_call_msg_t::in_t inmsg;
    inmsg.cmd = cmd_;
    inmsg.body = msg_;
    m_ffrpc->call(group_name_, name_, inmsg, ffrpc_ops_t::gen_callback(&ffscene_lua_t::call_service_return_msg, this, id_));
}
void ffscene_lua_t::call_service_return_msg(ffreq_t<scene_call_msg_t::out_t>& req_, long id_)
{
    AUTO_PERF();
    static string func_name   = CALL_SERVICE_RETURN_MSG_CB_NAME;
    try
    {
        (*m_fflua).call<void>(func_name.c_str(),
                              id_, req_.msg.err, req_.msg.msg_type, req_.msg.body);
         
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::gen_db_query_callback exception<%s>", e_.what()));
    }
}
//! 线程间传递消息
void ffscene_lua_t::post_task(const string& func_name, const ffjson_tool_t& task_args)
{
    m_ffrpc->get_tq().produce(task_binder_t::gen(&ffscene_lua_t::post_task_impl, this, func_name, task_args));
}

void ffscene_lua_t::post_task_impl(const string& func_name, const ffjson_tool_t& task_args)
{
    try
    {
        (*m_fflua).call<void>(func_name.c_str(), task_args);
         
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_PYTHON, "ffscene_lua_t::post_task_impl exception<%s>", e_.what()));
    }
}
