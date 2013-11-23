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

//! 定义echo 接口的消息， in_t代表输入消息，out_t代表的结果消息
//! 提醒大家的是，这里没有为echo_t定义神马cmd，也没有制定其名称，ffmsg_t会自动能够获取echo_t的名称
struct echo_t
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << data;
        }
        void decode()
        {
            decoder() >> data;
        }
        string data;
    };
    struct out_t: public ffmsg_t<out_t>
    {
        void encode()
        {
            encoder() << data;
        }
        void decode()
        {
            decoder() >> data;
        }
        string data;
    };
};

int g_times = 100;
struct foo_t
{
    //! echo接口，返回请求的发送的消息ffreq_t可以提供两个模板参数，第一个表示输入的消息（请求者发送的）
    //! 第二个模板参数表示该接口要返回的结果消息类型
    void echo(ffreq_t<echo_t::in_t, echo_t::out_t>& req_)
    {
        echo_t::out_t out;
        out.data = req_.msg.data;
        LOGDEBUG(("XX", "foo_t::echo: %s", req_.msg.data.c_str()));
        req_.response(out);
    }
    //! 远程调用接口，可以指定回调函数（也可以留空），同样使用ffreq_t指定输入消息类型，并且可以使用lambda绑定参数
    void echo_callback(ffreq_t<echo_t::out_t>& req_, int index, ffrpc_t* ffrpc_client)
    {
        LOGDEBUG(("XX", "%s %s %d", __FUNCTION__, req_.msg.data.c_str(), index));
        if (req_.error())
        {
            LOGDEBUG(("XX", "error_msg <%s>", req_.error_msg()));
        }
        return;
        if (index != g_times)
        {
            echo_t::in_t in;
            in.data = req_.msg.data;
            ffrpc_client->call("echo", in, ffrpc_ops_t::gen_callback(&foo_t::echo_callback, this, ++index, ffrpc_client));
        }
        else
        {
            struct timeval tm_end;
            gettimeofday(&tm_end, NULL);
            long cost = (tm_end.tv_sec - tm_begin.tv_sec)*1000*1000 + (tm_end.tv_usec - tm_begin.tv_usec);
            long sec = cost/(1000*1000);
            long msec = cost/(1000) - sec*1000;
            long usec = cost%1000;
            printf("%ld秒,%ld毫秒,%ld微妙\n", sec, msec, usec);
            printf("收到第%d个包，ctrl-C结束\n", index);
        }
        if (g_times == 100)
            sleep(1);
    }
    struct timeval tm_begin;
};

int main(int argc, char* argv[])
{
    //! 美丽的日志组件，shell输出是彩色滴！！
    LOG.start("-log_path ./log -log_filename log -log_class XX,BROKER,FFRPC -log_print_screen true -log_print_file false -log_level 6");

    if (argc == 1)
    {
        printf("usage: %s -l tcp://127.0.0.1:10241\n", argv[0]);
        return 1;
    }
    arg_helper_t arg_helper(argc, argv);
    if (arg_helper.is_enable_option("-times"))
    {
        g_times = atoi(arg_helper.get_option_value("-times").c_str());
    }
    //! 启动broker，负责网络相关的操作，如消息转发，节点注册，重连等

    ffbroker_t ffbroker;
    if (ffbroker.open(arg_helper))
    {
        return -1;
    }
    
    foo_t foo;
    //! broker客户端，可以注册到broker，并注册服务以及接口，也可以远程调用其他节点的接口
    ffrpc_t ffrpc_service("echo");
    ffrpc_service.reg(&foo_t::echo, &foo);

    if (ffrpc_service.open(arg_helper))
    {
        return -1;
    }
    ffrpc_t ffrpc_client;
    if (ffrpc_client.open(arg_helper))
    {
        return -1;
    }
    echo_t::in_t in;
    in.data = "helloworld";

    gettimeofday(&foo.tm_begin, NULL);

    sleep(1);
    ffrpc_client.call("test", "echo", in, ffrpc_ops_t::gen_callback(&foo_t::echo_callback, &foo, 1, &ffrpc_client));

    signal_helper_t::wait();

    return 0;
}
