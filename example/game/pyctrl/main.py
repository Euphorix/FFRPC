# -*- coding: utf-8 -*-
import ffext

from msg.ffengine import ttypes 
#from msg_def import ttypes

import random
import os

def init():
    print('init......')
    return 0
    

def cleanup():
    print('cleanup.....')
    return 0


#@ffext.ff_reg_scene_interfacee(ttypes.echo_thrift_in_t, ttypes.echo_thrift_out_t)
def process_echo(ffreq):
    print('process_echo', ffreq.msg)
    ret_msg = ttypes.echo_thrift_out_t('TODO')
    ffreq.response(ret_msg)

@ffext.ff_reg_scene_interfacee(ttypes.process_cmd_req_t, ttypes.process_cmd_ret_t)
def process_echo(ffreq):
    print('process_echo', ffreq.msg, {})
    ret_msg = ttypes.process_cmd_ret_t(1, '暂未开放此功能')
    if ffreq.msg.cmd == '查看状态':
        ret_msg.output_msg = '关闭'
    elif ffreq.msg.cmd == '启动':
        config_name = gen_filename(ffreq.msg.process_name)
        app_name    = gen_app_name(ffreq.msg.process_name)
        f = open(config_name, 'w')
        if ffreq.msg.param != None:
            for k, v in iteritems(ffreq.msg.param):
                f.write('-' + k + ' ' + v + '\n')
        f.close()
        os.system(app_name)
        #print(app_name)
        if ps_app(ffreq.msg.process_name):
            ret_msg.output_msg = ffreq.msg.process_name +'启动成功'
        else:
            ret_msg.output_msg = ffreq.msg.process_name +'启动失败'
    elif ffreq.msg.cmd == '关闭':
        if kill_app(ffreq.msg.process_name):
            ret_msg.output_msg = ffreq.msg.process_name +'关闭成功'
        else:
            ret_msg.output_msg = ffreq.msg.process_name +'关闭失败'
    else:
        ret_msg.ret_code = 0
    
    
    ffreq.response(ret_msg)


def gen_filename(s):
    return './config/' + s + '.config'
def gen_app_name(s):
    return './app_game -f ./config/' + s + '.config -d'
def ps_app(s):
    cmd = 'ps aux|grep ' + s + '  > tmp.dump'
    os.system(cmd)
    f = open('tmp.dump')
    data = f.read()
    f.close()
    if data.find('app_game') >= 0:
        pid = 0
        pid_list = data.split('app_game')[0].split(' ')
        for k in pid_list:
            if k != '':
                if pid == 0:
                    pid = k
                else:
                    return k
        print(pid)
    return '0'
def kill_app(s):
    pid = ps_app(s)
    print('pid', pid)
    if pid == '0':
        return True
    cmd = 'kill ' + pid
    os.system(cmd)
    for k in range(0, 10):
        if '0' == ps_app(s):
            return True
    return False

#@ffext.ff_reg_scene_interfacee(ttypes.dump_file_req_t, ttypes.dump_file_ret_t)
def process_dump(ffreq):
    print('process_dump', ffreq.msg)
    ret_msg = ttypes.dump_file_ret_t(0, '', [])
    ret_msg.flag = 1 #dir =1， file=2
    
    prefix = "./"
    path_name = prefix + ffreq.msg.name
    print(path_name)
    raw_flag = False
    if path_name[-1:] == '@':
        raw_flag = True
        path_name = path_name[0:len(path_name)-1]
    if os.path.isdir(path_name):
        list_files = os.listdir(path_name)
        for k in list_files:
            if k != '' and k[0:1] != '.' and k[0:4] != 'app_' and k[-2:] != '.o':
                ret_msg.dir.append(k)
    elif os.path.isfile(path_name):
        ret_msg.flag = 2
        pre_code = ''
        end_code = ''
        f = open(path_name, 'r')
        
        #if path_name[-4:] == '.lua':
        #    pre_code = '<textarea  cols="80" rows="20" class="pane"><pre class=\'brush: lua\'>'
        #    end_code = '</pre></textarea>'
        #elif path_name[-3:] == '.py':
        #    pre_code = '<textarea  cols="80" rows="20" class="pane"><pre class=\'brush: python\'>'
        #    end_code = '</pre></textarea>'
        data = pre_code
        for line in f.readlines():
           if raw_flag:
               data += line
           else:
               data += line + "<br/>" 
        f.close()
        if data[-1:] == '\n':
            data = data[0:-1]
        data += end_code
        ret_msg.data = data
    else:
        ret_msg.flag = 0
	
    ffreq.response(ret_msg)
    print('process_dump ret_msg', ret_msg)
