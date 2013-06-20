# -*- coding: utf-8 -*-
import json

g_session_verify_callback  = None
g_session_enter_callback   = None
g_session_offline_callback = None
g_session_logic_callback_dict = {}#cmd -> func callback

def session_verify_callback(func_):
    global g_session_verify_callback
    g_session_verify_callback = func_
def session_enter_callback(func_):
    global g_session_enter_callback
    g_session_enter_callback = func_
def session_offline_callback(func_):
    global g_session_offline_callback
    g_session_offline_callback = func_
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
    ret= []
    if session_verify_callback != None:
       session_verify_callback(session_key, online_time, gate_name) 
    return ret

def ff_session_enter(session_id, from_scene, extra_data):
    '''
    session_id Ϊclient id
    from_scene Ϊ���ĸ�scene�����ģ���Ϊ�գ����ʾ��һ�ν���
    extra_data ��from_scene��������������
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
    info = g_session_logic_callback_dict[cmd]
    arg  = info[0](body)
    return info[1](session_id, arg)

def to_str(msg):
    if hasattr(msg, 'SerializeToString2'):
        return msg.SerializeToString()
    else:
        return str(msg)

def send_msg_session(session_id, cmd_, body):
    return ffext.ffscene_obj.send_msg_session(session_id, cmd_, to_str(body))
def send_msg_session(session_id_list, cmd_, body):
    return ffext.ffscene_obj.multicast_msg_session(session_id_list, cmd_, to_str(body))
def broadcast_msg_session(cmd_, body):
    return ffext.ffscene_obj.broadcast_msg_session(cmd_, to_str(body))
def broadcast_msg_gate(gate_name_, cmd_, body):
    return ffext.ffscene_obj.broadcast_msg_gate(gate_name_, cmd_, to_str(body))
def send_msg_session(session_id):
    return ffext.ffscene_obj.close_session(session_id)

#@session_call(100)
#def test(session_id, msg):
#    print("test", session_id, msg)
#@session_call(200)
#def test2(session_id, msg):
#    print("test2", session_id, msg)
#ff_session_logic('tttt', 100, '[1,2,3]')
#ff_session_logic('tttt', 200, '[1,2,3]')


