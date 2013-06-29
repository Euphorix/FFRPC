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
    session_key 为client发过来的验证key，可能包括账号密码
    online_time 为上线时间
    gate_name 为从哪个gate登陆的
    '''
    ret= [session_key]
    if g_session_verify_callback != None:
       ret = g_session_verify_callback(session_key, online_time, ip, gate_name) 
    return ret

def ff_session_enter(session_id, from_scene, extra_data):
    '''
    session_id 为client id
    from_scene 为从哪个scene过来的，若为空，则表示第一次进入
    extra_data 从from_scene附带过来的数据
    '''
    if g_session_enter_callback != None:
       return g_session_enter_callback(session_id, from_scene, extra_data)

def ff_session_offline(session_id, online_time):
    '''
    session_id 为client id
    online_time 为上线时间
    '''
    if g_session_offline_callback != None:
       return g_session_offline_callback(session_id, online_time)

def ff_session_logic(session_id, cmd, body):
    '''
    session_id 为client id
    body 为请求的消息
    '''
    print('ff_session_logic', session_id, cmd, body)
    info = g_session_logic_callback_dict[cmd]
    arg  = info[0](body)
    return info[1](session_id, arg)

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
    elif isinstance(msg, unicode):
        return msg.encode('utf-8')
    elif isinstance(msg, str):
        return msg
    else:
        return json.dumps(msg, ensure_ascii=False)

def send_msg_session(session_id, cmd_, body):
    ffext.ffscene_obj.send_msg_session(session_id, cmd_, to_str(body))
def multi_send_msg_session(session_id_list, cmd_, body):
    return ffext.ffscene_obj.multicast_msg_session(session_id_list, cmd_, to_str(body))
def broadcast_msg_session(cmd_, body):
    return ffext.ffscene_obj.broadcast_msg_session(cmd_, to_str(body))
def broadcast_msg_gate(gate_name_, cmd_, body):
    return ffext.ffscene_obj.broadcast_msg_gate(gate_name_, cmd_, body)
def close_session(session_id):
    return ffext.ffscene_obj.close_session(session_id)

def reload(name_):
    return ffext.ffscene_obj.reload(name_)

singleton_register_dict = {}
def singleton(type_):
    try:
	return type_._singleton
    except:
        global singleton_register_dict
        name = type(type_)
        obj  = singleton_register_dict.get(name)
        if obj == None:
            obj  = type_()
            singleton_register_dict[name] = obj
        type_._singleton = obj
        return obj


