#ifndef _FF_JSON_TOOL_H_
#define _FF_JSON_TOOL_H_

//线程间传递数据!

#include "rapidjson/document.h"     // rapidjson's DOM-style API                                                                                             
#include "rapidjson/prettywriter.h" // for stringify JSON
#include "rapidjson/filestream.h"   // wrapper of C stream for prettywriter as output
#include "rapidjson/stringbuffer.h"   // wrapper of C stream for prettywriter as output

#include "base/smart_ptr.h"

typedef runtime_error       msg_exception_t;
typedef rapidjson::Document json_dom_t;
typedef rapidjson::Value    json_value_t;

namespace ff{

struct ffjson_tool_t{
    ffjson_tool_t():
        allocator(new rapidjson::Document::AllocatorType()),
        jval(new json_dom_t())
    {}
    ffjson_tool_t(const ffjson_tool_t& src):
        allocator(src.allocator),
        jval(src.jval)
    {}
    ffjson_tool_t& operator=(const ffjson_tool_t& src)
    {
        allocator = src.allocator;
        jval = src.jval;
        return *this;
    }
    
    shared_ptr_t<rapidjson::Document::AllocatorType>  allocator;
    shared_ptr_t<json_dom_t>                          jval;
};

class task_dispather_i{
public:
    virtual ~task_dispather_i(){}
    //! 线程间传递消息
    virtual    void post_task(const string& func_name, const ffjson_tool_t& task_args, long callback_id) = 0;
};


class task_dispather_mgr_t{
public:
    void add(const string& name, task_dispather_i* d){ m_task_register[name] = d; }
    void del(const string& name) { m_task_register.erase(name); }
    task_dispather_i* get(const string& name)
    {
        map<string, task_dispather_i*>::iterator it = m_task_register.find(name);
        if (it != m_task_register.end())
        {
            return it->second;
        }
        return NULL;
    }
    map<string, task_dispather_i*>  m_task_register;
};

}
#endif

