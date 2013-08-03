# -*- coding: utf-8 -*-

import ff

import thrift.Thrift as Thrift
import thrift.protocol.TBinaryProtocol as TBinaryProtocol
import thrift.protocol.TCompactProtocol as TCompactProtocol
import thrift.transport.TTransport as TTransport

import msg_def.ttypes as msg_def

def send_msg(cmd_, msg_):
    data = to_str(msg_)
    print(__name__, len(data))
    ff.send_msg(cmd_, data)

def to_str(msg):
    if hasattr(msg, 'thrift_spec'):
        g_WriteTMemoryBuffer   = TTransport.TMemoryBuffer()
        g_WriteTBinaryProtocol = TBinaryProtocol.TBinaryProtocol(g_WriteTMemoryBuffer)
        g_WriteTMemoryBuffer.cstringio_buf.truncate()
        g_WriteTMemoryBuffer.cstringio_buf.seek(0)
        msg.write(g_WriteTBinaryProtocol)
        return g_WriteTMemoryBuffer.getvalue()
    return msg

def decode_buff(dest, val_):
    global g_ReadTMemoryBuffer, g_ReadTBinaryProtocol
    g_ReadTMemoryBuffer.cstringio_buf.truncate()
    g_ReadTMemoryBuffer.cstringio_buf.seek(0)
    g_ReadTMemoryBuffer.cstringio_buf.write(val_)
    g_ReadTMemoryBuffer.cstringio_buf.seek(0)
    dest.read(g_ReadTBinaryProtocol)
    return dest

def process_connect():
    account = msg_def.account_t()
    account.nick_name = 'cs7'
    account.password  = 'cs123'
    account.register_flag = True
    send_msg(0, account)


g_cmd_reg_dict = {}
def reg_callback(cmd_, msg_type_):
    def wrapper(func_):
        global g_cmd_reg_dict
        def tmp_func(str_msg_):
            dest = msg_type_()
            decode_buff(dest, str_msg_)
            func_(dest)
        g_cmd_reg_dict[cmd_] = tmp_func
        return func_
    return wrapper

#处理输入
def process_gm(cmd_):
    print("process_gm", "*"*10, cmd_)
    return 0

#处理消息到来
def process_recv(cmd_, msg_):
    return 0

@reg_callback(1, msg_def.account_t)
def handle_login_ret(msg_):
    return 0
