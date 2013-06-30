#include <stdio.h>
#include "base/daemon_tool.h"
#include "base/arg_helper.h"
#include "base/strtool.h"
#include "base/smart_ptr.h"

#include "rpc/ffrpc.h"
#include "rpc/ffbroker.h"
#include "base/log.h"
#include "rpc/ffscene_python.h"
#include "rpc/ffgate.h"
#include "base/signal_helper.h"
#include "base/daemon_tool.h"

using namespace ff;
//./example/redrabbit/app_redrabbit -gate gate@0 -broker tcp://127.0.0.1:10241 -gate_listen tcp://121.199.21.238:10242 -python_path /home/evan/work/ffrpc -scene scene@0

int main(int argc, char* argv[])
{
    if (argc == 1)
    {
        printf("./example/redrabbit/app_redrabbit -gate gate@0 -broker tcp://127.0.0.1:10241 -gate_listen tcp://*:10242 -python_path /home/evan/work/ffrpc -scene scene@0\n");
        return 0;
    }
    arg_helper_t arg_helper(argc, argv);
    
    //! 美丽的日志组件，shell输出是彩色滴！！
    LOG.start("-log_path ./log -log_filename log -log_class XX,BROKER,FFRPC,FFGATE,FFSCENE,FFSCENE_PYTHON,FFNET -log_print_screen true -log_print_file true -log_level 6");

    ffbroker_t ffbroker;
    ffgate_t ffgate;
    ffscene_python_t ffscene_python;

    if (arg_helper.is_enable_option("-d"))
    {
        daemon_tool_t::daemon();
    }

    //! 启动broker，负责网络相关的操作，如消息转发，节点注册，重连等
    ffbroker.open(arg_helper);
    
    try
    {
        if (ffgate.open(arg_helper))
        {
            printf("gate open error!\n");
            sleep(1);
            return 0;
        }
        
        if (ffscene_python.open(arg_helper))
        {
            sleep(1);
            printf("scene open error!\n");
            return 0;
        }
    }
    catch(exception& e_)
    {
        printf("exception=%s\n", e_.what());
    }
    signal_helper_t::wait();

    ffgate.close();
    ffscene_python.close();
    ffbroker.close();
    net_factory_t::stop();
    usleep(500);
    return 0;
}
