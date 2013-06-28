# -*- coding: utf-8 -*-
import time
import ffext
def GetNowTime():
    return time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(time.time()))
@ffext.session_call(1)
def process_chat(session_id, msg):
    print("process_chat", session_id, msg[0])
    ret = '[%s %s]:%s'%(session_id, GetNowTime(), msg[0])
    print(str(ret))
    ffext.broadcast_msg_session(1, ret)

    ffext.reload('main')

@ffext.session_verify_callback
def my_session_verify(session_key, online_time, ip, gate_name):
    ret = [session_key, "恭喜%s登陆成功"%(session_key)]
    print(str(ret))
    ffext.reload('main')
    return ret


print("loading.......")