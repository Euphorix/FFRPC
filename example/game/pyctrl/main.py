# -*- coding: utf-8 -*-
import ffext
from msg_def import ttypes
import random
import os

def init():
    print('init......')
    return 0
    

def cleanup():
    print('cleanup.....')
    return 0


@ffext.ff_reg_scene_interfacee(ttypes.echo_thrift_in_t, ttypes.echo_thrift_out_t)
def process_echo(ffreq):
    print('process_echo', ffreq.msg)
    ret_msg = ttypes.echo_thrift_out_t('TODO')
    ffreq.response(ret_msg)

@ffext.ff_reg_scene_interfacee(ttypes.dump_file_req_t, ttypes.dump_file_ret_t)
def process_dump(ffreq):
    print('process_dump', ffreq.msg)
    ret_msg = ttypes.dump_file_ret_t(0, '', [])
    ret_msg.flag = 1 #dir =1， file=2
    
    prefix = './'
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
