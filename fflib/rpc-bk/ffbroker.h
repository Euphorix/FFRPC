
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
class ffbroker_t: public msg_handler_i
{
    //! 每个连接都要分配一个session，用于记录该socket，对应的信息s
    struct session_data_t;
    //! 记录每个broker slave 的接口信息
    struct slave_broker_info_t;
    //! 记录每个broker client 的接口信息
    struct broker_client_info_t;
    //! broker master 上 所有的broker bridge 信息
    struct broker_bridge_info_t;
    //! broker bridge 上所有的 broker master group 信息
    struct broker_group_info_t;
public:
    ffbroker_t();
    virtual ~ffbroker_t();

    //! 当有连接断开，则被回调
    int handle_broken(socket_ptr_t sock_);
    //! 当有消息到来，被回调
    int handle_msg(const message_t& msg_, socket_ptr_t sock_);

    int open(arg_helper_t& opt_);
    int close();
    //! 分配一个nodeid
    uint32_t alloc_id();
    //! 获取任务队列对象
    task_queue_t& get_tq();
    //! 定时器
    timer_service_t& get_timer();
    
    //! 连接到broker master
    int connect_to_master_broker();
    //! 连接到broker bridge
    int connect_to_bridge_broker();
    //! 转发消息给master client
    int route_msg_to_broker_client(broker_route_t::in_t& msg_);
    //! 内存间传递消息
    int memory_route_msg(broker_route_t::in_t& msg_);
private:
    //! 当有连接断开，则被回调
    int handle_broken_impl(socket_ptr_t sock_);
    //! 当有消息到来，被回调
    int handle_msg_impl(const message_t& msg_, socket_ptr_t sock_);
    //! 同步所有的当前的注册接口信息
    int sync_all_register_info(socket_ptr_t sock_);
    //! 处理borker slave 注册消息
    int handle_slave_register(register_slave_broker_t::in_t& msg_, socket_ptr_t sock_);
    //! 处理borker client 注册消息
    int handle_client_register(register_broker_client_t::in_t& msg_, socket_ptr_t sock_);

    //! 转发消息
    int handle_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_);
    //! 处理broker client连接到broker slave的消息
    int handle_client_register_slave_broker(register_client_to_slave_broker_t::in_t& msg_, socket_ptr_t sock_);
    //! 处理broker同步消息，broker master 会把master上注册的所有信息同步给所有的client
    int handle_broker_sync_data(broker_sync_all_registered_data_t::out_t& msg_, socket_ptr_t sock_);
    //! 判断是否是master broker
    bool is_master();
    //! 分配一个broker 给client,以后client的消息都通过此broker转发
    uint32_t alloc_broker_id();
    //! 转发消息给broker bridge
    int route_msg_broker_to_bridge(const string& from_broker_group_name_, const string& dest_broker_group_name_, const string& service_name_,
                                    const string& msg_name_, const string& body_,
                                    uint32_t from_node_id_, uint32_t dest_node_id_,
                                    uint32_t callback_id_, socket_ptr_t sock_);
    //! 这里是broker master 发给 broker bridge 的消息
    int handle_broker_to_bridge_route_msg(broker_route_to_bridge_t::in_t& msg_, socket_ptr_t sock_);
    pair<string, socket_ptr_t> get_broker_group_name_by_id(uint32_t id_);
    //! 处理broker bridge 转发给broker master的消息
    int handle_bridge_to_broker_route_msg(bridge_route_to_broker_t::in_t& msg_, socket_ptr_t sock_);
    //! [3] bridge的处理函数，从broker master转发到另外的broker master
    int bridge_handle_broker_to_broker_msg(bridge_route_to_broker_t::in_t& msg_, socket_ptr_t sock_);

    
    //! 处理broker master 注册到broker bridge
    int handle_broker_register_bridge(register_bridge_broker_t::in_t& msg_, socket_ptr_t sock_);
private:
    //! 分配broker slave的索引id
    uint32_t                                m_alloc_slave_broker_index;
    //! 本 broker的监听信息
    string                                  m_listen_host;
    //!broker master 的host信息
    string                                  m_broker_host;
    //! 连接到master 的连接socket
    socket_ptr_t                            m_master_broker_sock;
    //! broker 分配的slave node id
    uint32_t                                m_node_id;
    timer_service_t                         m_timer;
    //! 用于分配nodeid
    uint32_t                                m_node_id_index;
    //! 记录所有注册到此节点上的连接
    task_queue_t                            m_tq;
    thread_t                                m_thread;
    //! 用于绑定回调函数
    ffslot_t                                m_ffslot;
    //! 记录所有的broker socket对应node id
    map<uint32_t, slave_broker_info_t>      m_slave_broker_sockets;
    //! 记录所有的消息名称对应的消息id值
    map<string, uint32_t>                   m_msg2id;
    //! 记录所有服务/接口信息
    map<uint32_t, broker_client_info_t>     m_broker_client_info;//! node id -> service
    string                                  m_master_group_name;//! broker master 被分配的名称
    //! 记录所有的broker bridge信息
    map<uint32_t/*broker bridge id*/, broker_bridge_info_t> m_broker_bridge_info;
    //! broker bridge 上所有的 broker master group 信息
    map<string/*group name*/, broker_group_info_t>          m_broker_group_info;
};

//! 每个连接都要分配一个session，用于记录该socket，对应的信息
struct ffbroker_t::session_data_t
{
    session_data_t(uint32_t n = 0):
        node_id(n)
    {}
    uint32_t get_node_id() { return node_id; }
    //! 被分配的唯一的节点id
    uint32_t node_id;
};
//! 记录每个broker client 的接口信息
struct ffbroker_t::broker_client_info_t
{
    broker_client_info_t():
        bind_broker_id(0),
        sock(NULL)
    {}
    //! 被绑定的节点broker node id
    uint32_t bind_broker_id;
    string   service_name;
    socket_ptr_t sock;
};
//! 记录每个broker slave 的接口信息
struct ffbroker_t::slave_broker_info_t
{
    slave_broker_info_t():
        sock(NULL)
    {}
    string          host;
    socket_ptr_t    sock;
};

struct ffbroker_t::broker_bridge_info_t
{
    broker_bridge_info_t():
        sock(NULL)
    {}
    string          host;
    socket_ptr_t    sock;
    map<string/*group name*/, uint32_t> broker_group_id;
};

//! broker bridge 上所有的 broker master group 信息
struct ffbroker_t::broker_group_info_t
{
    broker_group_info_t():
        sock(NULL)
    {}
    socket_ptr_t    sock;
};
}

#endif
