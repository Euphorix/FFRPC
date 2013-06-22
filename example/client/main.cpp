#include <stdio.h>
#include "base/daemon_tool.h"
#include "base/arg_helper.h"
#include "base/strtool.h"
#include "base/smart_ptr.h"

#include "rpc/ffrpc.h"
#include "rpc/ffbroker.h"
#include "base/log.h"
#include "base/signal_helper.h"

using namespace ff;
#define FFRPC "FFRPC"

class handler_t:public msg_handler_i
{
public:
    //! 处理连接断开
    int handle_broken(socket_ptr_t sock_)
    {
        printf("handle_broken xxxxxxxx\n");
        return 0;
    }
    //! 处理消息
    int handle_msg(const message_t& msg_, socket_ptr_t sock_)
    {
        printf("handle_msg xxxxxxxxTTTT=%s\n", msg_.get_body().c_str());
        return 0;
    }
};
int main(int argc, char* argv[])
{
    //! 美丽的日志组件，shell输出是彩色滴！！
    LOG.start("-log_path ./log -log_filename log -log_class XX,BROKER,FFRPC -log_print_screen true -log_print_file false -log_level 6");

    if (argc == 1)
    {
        printf("usage: %s -gate tcp://127.0.0.1:10241\n", argv[0]);
        return 1;
    }
    string name ="helloworld";
    arg_helper_t arg_helper(argc, argv);
    if (false == arg_helper.is_enable_option("-gate"))
    {
        printf("usage: %s -gate tcp://127.0.0.1:10241\n", argv[0]);
        return 1;
    }
    if (arg_helper.is_enable_option("-name"))
    {
        name = arg_helper.get_option_value("-name");
    }
    printf("name=%s\n", name.c_str());
    handler_t handler;
    string host_ = arg_helper.get_option_value("-gate");
    socket_ptr_t sock = net_factory_t::connect(host_, &handler);
    if (NULL == sock)
    {
        LOGERROR((FFRPC, "can't connect to remote broker<%s>", host_.c_str()));
        return -1;
    }
    msg_sender_t::send(sock, 0, name);

    int cmd = 1;
    while(cmd)
    {
        cin >> cmd;
        msg_sender_t::send(sock, cmd, "[1,2,3]");
    }
    sleep(100);
    return 0;
}
