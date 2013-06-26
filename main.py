# -*- coding: utf-8 -*-
import json

import ffext
@ffext.session_call(1)
def process_chat(session_id, msg):
    print("process_chat", session_id, msg)
    ffext.broadcast_msg_gate('gate@0', 1, msg)
    def cb():
        ffext.broadcast_msg_gate('gate@0', 1, '定时器2')
    ffext.once_timer(1000, cb)
    ffext.reload('main')

@ffext.session_verify_callback
def my_session_verify(session_key, online_time, ip, gate_name):
    ret = [session_key, "恭喜%s登陆成功"%(session_key)]
    print(str(ret))
    ffext.reload('main')
    return ret


print("loading.......")