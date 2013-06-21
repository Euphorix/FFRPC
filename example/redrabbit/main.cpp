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

using namespace ff;
//./example/redrabbit/app_redrabbit -gate gate@0 -broker tcp://127.0.0.1:10241 -gate_listen tcp://127.0.0.1:10242 -python_path /home/evan/work/ffrpc -scene scene@0

int main(int argc, char* argv[])
{
    //! 美丽的日志组件，shell输出是彩色滴！！
    LOG.start("-log_path ./log -log_filename log -log_class XX,BROKER,FFRPC,FFGATE,FFSCENE,FFSCENE_PYTHON -log_print_screen true -log_print_file true -log_level 6");

    //! 启动broker，负责网络相关的操作，如消息转发，节点注册，重连等
    ffbroker_t ffbroker;
    ffbroker.open("-l tcp://127.0.0.1:10241");

    ffgate_t ffgate;
    ffscene_python_t ffscene_python;
    
    arg_helper_t arg_helper(argc, argv);
    try
    {
        sleep(1);

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
    sleep(300);
    ffbroker.close();
    return 0;
}
