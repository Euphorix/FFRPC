#include "python/ffpython.h"

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

socket_ptr_t sock = NULL;
ffpython_t* g_ffpython = NULL;
struct session_data_t
{
    session_data_t(int i = 0):index(i){}
    int index;
};
class handler_t:public msg_handler_i
{
public:
    //! 处理连接断开
    int handle_broken(socket_ptr_t sock_)
    {
        if (NULL == sock_->get_data<session_data_t>())
        {
            sock_->safe_delete();
            return 0;
        }
        //socket_info[sock_->get_data<session_data_t>()->index] = NULL;
        printf("handle_broken xxxxxxxx=%d\n", sock_->get_data<session_data_t>()->index);
        delete sock_->get_data<session_data_t>();
        sock_->safe_delete();
        
        if (sock == sock_)
        {
            sock = NULL;
        }
        return 0;
    }
    //! 处理消息
    int handle_msg(const message_t& msg_, socket_ptr_t sock_)
    {
        printf("handle_msg xxxxxxxxTTTT cmd=%d\n", msg_.get_cmd());
        
        (*g_ffpython).call<void>("main", "process_recv", msg_.get_cmd(), msg_.get_body());
        return 0;
    }
    
    map<int, socket_ptr_t> socket_info;
};

struct ffext_t
{
    static void send_msg(int cmd_, const string& msg_)
    {
        cout << "ffext_t::send_msg:" << msg_.size() <<endl;
        msg_sender_t::send(sock, cmd_, msg_);
    }
};

int main(int argc, char* argv[])
{
    //! 美丽的日志组件，shell输出是彩色滴！！
    LOG.start("-log_path ./log -log_filename log -log_class XX,BROKER,FFRPC -log_print_screen true -log_print_file false -log_level 6");

    if (argc == 1)
    {
        printf("usage: %s -gate tcp://127.0.0.1:10242\n", argv[0]);
        return 1;
    }
    string name ="helloworld";
    arg_helper_t arg_helper(argc, argv);
    if (false == arg_helper.is_enable_option("-gate"))
    {
        printf("usage: %s -gate tcp://127.0.0.1:10242\n", argv[0]);
        return 1;
    }
    if (arg_helper.is_enable_option("-name"))
    {
        name = arg_helper.get_option_value("-name");
    }
    printf("name=%s\n", name.c_str());
    handler_t handler;
    string host_ = arg_helper.get_option_value("-gate");
    
    
    sock = net_factory_t::connect(host_, &handler);
    if (NULL == sock)
    {
        LOGERROR((FFRPC, "can't connect to remote broker<%s>", host_.c_str()));
        return -1;
    }
    sock->set_data(new session_data_t(0));
    
    ffpython_t ffpython;
    ffpython_t::add_path("./");
    
    ffpython.reg(&ffext_t::send_msg, "send_msg");
    ffpython.init("ff");
    ffpython.load("main");

    ffpython.call<void>("main", "process_connect");
    //msg_sender_t::send(sock, 0, name);
    g_ffpython = &ffpython;
    string cmd;
    while(cmd != "quit")
    {
        cin >> cmd;
        if (cmd.empty())
        {
            break;
        }
        try
        {
            if (cmd == "reload")
            {
                ffpython.reload("main");
                continue;
            }
            int ret = ffpython.call<int>("main", "process_gm", cmd);
            if (ret != 0)
                break;
        }
        catch(exception& e_)
        {
            cout << e_.what() << endl;
        }
        cmd.clear();
        sleep(1);
    }
    sleep(1);
    return 0;
}

