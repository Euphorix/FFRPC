# -*- coding: utf-8 -*-
import json
import ff as ffext

g_session_verify_callback  = None
g_session_enter_callback   = None
g_session_offline_callback = None
g_session_logic_callback_dict = {}#cmd -> func callback
g_timer_callback_dict = {}

def session_verify_callback(func_):
    global g_session_verify_callback
    g_session_verify_callback = func_
def session_enter_callback(func_):
    global g_session_enter_callback
    g_session_enter_callback = func_
def session_offline_callback(func_):
    global g_session_offline_callback
    g_session_offline_callback = func_
GID = 0
def once_timer(timeout_, func_):
    global g_timer_callback_dict, GID
    GID += 1
    g_timer_callback_dict[GID] = func_
    ffext.ffscene_obj.once_timer(timeout_, GID);


def json_to_value(val_):
    return json.loads(val_)

def session_call(cmd_, protocol_type_ = 'json'):
    global g_session_logic_callback_dict
    def session_logic_callback(func_):
        if protocol_type_ == 'json':
            g_session_logic_callback_dict[cmd_] = (json_to_value, func_)
        else: #protobuf
            def protobuf_to_value(val_):
                dest = protocol_type_()
                dest.ParseFromString(val_)
                return dest
            g_session_logic_callback_dict[cmd_] = (protobuf_to_value, func_)
    return session_logic_callback

def ff_session_verify(session_key, online_time, ip, gate_name):
    '''
    session_key Ϊclient����������֤key�����ܰ����˺�����
    online_time Ϊ����ʱ��
    gate_name Ϊ���ĸ�gate��½��
    '''
    ret= [session_key]
    if g_session_verify_callback != None:
       ret = g_session_verify_callback(session_key, online_time, ip, gate_name) 
    return ret

def ff_session_enter(session_id, from_scene, extra_data):
    '''
    session_id Ϊclient id
    from_scene Ϊ���ĸ�scene�����ģ���Ϊ�գ����ʾ��һ�ν���
    extra_data ��from_scene������������
    '''
    if g_session_enter_callback != None:
       return g_session_enter_callback(session_id, from_scene, extra_data)

def ff_session_offline(session_id, online_time):
    '''
    session_id Ϊclient id
    online_time Ϊ����ʱ��
    '''
    if g_session_offline_callback != None:
       return g_session_offline_callback(session_id, online_time)

def ff_session_logic(session_id, cmd, body):
    '''
    session_id Ϊclient id
    body Ϊ�������Ϣ
    '''
    try:
        info = g_session_logic_callback_dict[cmd]
        arg  = info[0](body)
        return info[1](session_id, arg)
    except:
        return False

def ff_timer_callback(id):
    try:
        cb = g_timer_callback_dict[id]
        del g_timer_callback_dict[id]
        cb()
    except:
        return False

def to_str(msg):
    if hasattr(msg, 'SerializeToString2'):
        return msg.SerializeToString()
    else:
        return str(msg)

def send_msg_session(session_id, cmd_, body):
    ffext.ffscene_obj.send_msg_session(session_id, cmd_, to_str(body))
def multi_send_msg_session(session_id_list, cmd_, body):
    return ffext.ffscene_obj.multicast_msg_session(session_id_list, cmd_, to_str(body))
def broadcast_msg_session(cmd_, body):
    return ffext.ffscene_obj.broadcast_msg_session(cmd_, to_str(body))
def broadcast_msg_gate(gate_name_, cmd_, body):
    return ffext.ffscene_obj.broadcast_msg_gate(gate_name_, cmd_, to_str(body))
def close_session(session_id):
    return ffext.ffscene_obj.close_session(session_id)

num = 0
@session_call(1)
def test(session_id, msg):
    print("test", session_id, msg)
    global num
    num += 1
    #if num % 3 == 0:
        #close_session(session_id)
    #send_msg_session(session_id, 100, msg)
    #multi_send_msg_session([session_id, session_id], 100, msg)
    #broadcast_msg_session(100, msg)
    broadcast_msg_gate('gate@0', 100, msg)
    def cb():
        broadcast_msg_gate('gate@0', 100, msg)
    once_timer(1000, cb)

#@session_call(200)
#def test2(session_id, msg):
#    print("test2", session_id, msg)
#ff_session_logic('tttt', 100, '[1,2,3]')
#ff_session_logic('tttt', 200, '[1,2,3]')

@session_verify_callback
def my_session_verify(session_key, online_time, ip, gate_name):
    ret = [session_key, "恭喜%s登陆成功"%(session_key)]
    print(str(ret))
    return ret


print("loading.......")