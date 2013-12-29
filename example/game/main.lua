
print('main.lua loading....')

ffext = {
    id = 0,
    rpc_callback_cache_func = {},
    task_bind_func = {},
    task_callback_func = {},
}

-- 调用远程scene的接口
function ffext:ff_rpc_call(service_name, msg_name, args, cb, name_space_)
    name_space_ = name_space_ or ''
    data = args
    if type(args) == 'table' then
        data = json_encode(args)
    end
    cb_id = 0
    if cb ~= nil then
        ffext.id = ffext.id + 1
        cb_id = ffext.id
        ffext.rpc_callback_cache_func[cb_id..''] = function(data, err) cb(json_decode(data)) end
    end
    --print('ff_rpc_call', cb_id)
    return ffscene:call_service(name_space_, service_name, msg_name, data, cb_id)
end
function ffext:ff_rpc_bridge_call(name_space_, service_name, msg_name, msg_data, cb)
    return ffext:ff_rpc_call(service_name, msg_name, args, cb, name_space_)
end

-- 调用远程scene的接口后，返回的消息调用此接口通知s
function ff_rpc_call_return_msg(cb_id, body, err)
   --print('ff_rpc_call_return_msg', cb_id, type(cb_id))
   cb = ffext.rpc_callback_cache_func[cb_id..'']
   ffext.rpc_callback_cache_func[cb_id] = nil
   cb(body, err) 
   return 0
end
-- 注册处理任务的接口
function ffext:bind_task(task_name, func)
    ffext.task_bind_func[task_name] = func
end
function ffext:multi_bind_task(tbl)
    for task_name, func in pairs(tbl)
    do
        ffext.task_bind_func[task_name] = func
    end
end
-- 处理投递远程的任务消息
function ffext:post_task(name, task_name, args, cb)
    cb_id = 0
    if cb ~= nil then
        ffext.id = ffext.id + 1
        cb_id = ffext.id
        ffext.task_callback_func[cb_id..''] = cb
    end
    ffscene:post_task(name, task_name, args, cb_id)
end


-- c++调用，处理远程投递的任务消息
function process_task(name, args, from_name, callback_id)
    -- print('luaprocess_task', name, json_encode(args), from_name, callback_id)
    func = ffext.task_bind_func[name]
    if func ~= nil then
        func(args, function(ret_args)
            if callback_id ~= 0 and callback_id ~= '0' then
                return ffscene:task_callback(from_name, ret_args, callback_id)
            end
        end)
    else
        print('process_task', name, 'none task binder')
    end
    return 0 
end
-- 处理投递远程的task回调 消息
function process_task_callback(args, cb_id)
    cb_id = cb_id ..''
    cb = ffext.task_callback_func[cb_id]
    ffext.task_callback_func[cb_id] = nil
    cb(args)
end


function init()
	print('main.lua init execute....')
    ffext:bind_task('test_task', function(args, cb)
        print('lua_task_test', json_encode(args))
        cb(args)
        ffext:ff_rpc_call('scene@0', 'test', args, function(ret_args)
            print('ff_rpc_call callback TTTTT', json_encode(ret_args))
            ffext:post_task('scene@0', 'py_task', ret_args)
        end)
    end)
	return 0
end
function cleanup()
	print('main.lua cleanup execute....')
	return 0
end

function test(tb)
    print('test', tb, tb['foo'], tb['foo']['dumy'])
    data = json_encode(tb)
    --data = json_encode({"a", "b"})
    print(data)
    --print(json_encode(nil))
    tb = json_decode(data)
    print('test', tb, tb['foo'], tb['foo']['dumy'])
end

