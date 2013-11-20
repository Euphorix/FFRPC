
#ifndef _FF_BROKER_H_
#define _FF_BROKER_H_

#include <assert.h>
#include <string>
#include <map>
#include <set>
using namespace std;

#include "net/msg_handler_i.h"
#include "base/task_queue_impl.h"
#include "base/ffslot.h"
#include "base/thread.h"
#include "base/smart_ptr.h"
#include "net/net_factory.h"
#include "rpc/ffrpc_ops.h"
#include "base/arg_helper.h"

namespace ff
{
//! 各个节点的类型
enum node_type_e
{
    BRIDGE_BROKER, //! 连接各个区服的代理服务器
    MASTER_BROKER, //! 每个区服的主服务器
    SLAVE_BROKER,  //! 从服务器
    RPC_NODE,      //! rpc节点
};

class ffbroker_t: public msg_handler_i
{
    //! 每个连接都要分配一个session，用于记录该socket，对应的信息s
    struct session_data_t;

    //!新版本*****************************
    //!所有的注册到此broker的节点信息
    struct registered_node_info_t
    {
        //!各个节点对应的连接信息
        map<uint64_t/* node id*/, socket_ptr_t>     node_sockets;
        //!service对应的nodeid
        register_to_broker_t::out_t                 broker_data;
    };

public:
    ffbroker_t();
    virtual ~ffbroker_t();

    //! 当有连接断开，则被回调
    int handle_broken(socket_ptr_t sock_);
    //! 当有消息到来，被回调
    int handle_msg(const message_t& msg_, socket_ptr_t sock_);

    //! 处理其他broker或者client注册到此server
    int handle_regiter_to_broker(register_to_broker_t::in_t& msg_, socket_ptr_t sock_);
    //! 处理转发消息的操作
    int handle_broker_route_msg(broker_route_msg_t::in_t& msg_, socket_ptr_t sock_);
    //!处理注册到master broker的消息
    int handle_register_master_ret(register_to_broker_t::out_t& msg_, socket_ptr_t sock_);
    
    //!ff 获取节点信息
    uint64_t alloc_node_id(socket_ptr_t sock_);
    //!本身是否是master broker
    bool is_master_broker() { return m_broker_host.empty() == true; }
protected:
    //!此broker归属于哪一个组
    string                              m_namespace;
    //!本broker的host信息
    //!此broker的上级broker的host配置
    string                              m_higher_broker_host;
    //!本身的node id[ip_port]
    uint64_t                            m_node_id;
    //!所有的注册到此broker的节点信息
    registered_node_info_t              m_all_registered_info;

    //!工具类
    task_queue_t                            m_tq;
    thread_t                                m_thread;
    //! 用于绑定回调函数
    ffslot_t                                m_ffslot;
    //! 如果本身是一个slave broker，需要连接到master broker
    socket_ptr_t                            m_master_broker_sock;
public:
    int open(arg_helper_t& opt_);
    int close();

    //! 获取任务队列对象
    task_queue_t& get_tq();
    //! 定时器
    timer_service_t& get_timer();
    
    //! 内存间传递消息
    int memory_route_msg(broker_route_t::in_t& msg_);
       
    //!ff
    //! 连接到broker master
    int connect_to_master_broker();
private:
    //! 当有连接断开，则被回调
    int handle_broken_impl(socket_ptr_t sock_);
    //! 当有消息到来，被回调
    int handle_msg_impl(const message_t& msg_, socket_ptr_t sock_);
    
    //! 转发消息
    int handle_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_);

private:
    //! 本 broker的监听信息
    string                                  m_listen_host;
    //!broker master 的host信息
    string                                  m_broker_host;
    //! broker 分配的slave node id
    //uint32_t                                m_node_id;
    timer_service_t                         m_timer;
};

//! 每个连接都要分配一个session，用于记录该socket，对应的信息
struct ffbroker_t::session_data_t
{
    session_data_t(int type_ = 0, uint64_t node_ = 0, string s_ = ""):
        node_type(type_),
        node_id(node_),
        service_name(s_)
    {}
    uint64_t get_node_id() const { return node_id; }
    int      get_type()    const { return node_type;}
    //!节点的类型
    int      node_type;
    //! 被分配的唯一的节点id
    uint64_t node_id;
    //! 服务名
    string   service_name;
};

}

#endif
