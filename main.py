# coding=utf-8
import time
import ffext
def GetNowTime():
    return time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(time.time()))
@ffext.session_call(1)
def process_chat(session_id, msg):
    content = msg[0]
    content = content.encode('utf-8')
    print("process_chat", session_id, content, type(content))

    ret = '[%s %s]:%s'%(session_id, GetNowTime(), content)
    ffext.broadcast_msg_session(1, ret)

    ffext.reload('main')

@ffext.session_verify_callback
def my_session_verify(session_key, online_time, ip, gate_name):
    ret = [session_key, "%s login"%(session_key)]
    print(str(ret))
    ffext.reload('main')
    content = "[" + (session_key).encode('utf-8') + " %s]login"%(GetNowTime())
    ffext.broadcast_msg_session(1, content)
    
    return ret


print("loading.......")

