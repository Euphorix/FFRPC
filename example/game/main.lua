
print('main.lua loading....')

ffext = {
    id = 0,
    rpc_callback_cache_func = {},
}

function ffext:ff_rpc_bridge_call(self, name_space_, service_name, msg_name, msg_data, cb)
    data = args
    if type(args) == 'table' then
        data = json_encode(args)
    end
    cb_id = 0
    if cb ~= nil then
        ffext.id = ffext.id + 1
        cb_id = ffext.id
        ffext.rpc_callback_cache_func[cb_id..''] = function() print('XCVVCXVXCVXCVXCV') end
    end
    return ffscene:call_service(name_space_, service_name, msg_name, data, cb_id)
end

function ffext:ff_rpc_call(self, service_name, msg_name, args, cb)
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
    return ffscene:call_service('', service_name, msg_name, data, cb_id)
end

function ff_rpc_call_return_msg(cb_id, body, err)
   --print('ff_rpc_call_return_msg', cb_id, type(cb_id))
   cb = ffext.rpc_callback_cache_func[cb_id..'']
   ffext.rpc_callback_cache_func[cb_id] = nil
   cb(body, err) 
   return 0
end

function init()
	print('main.lua init execute....')

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


function process_task(name, args, from_name, callback_id)
    print('luaprocess_task', name, json_encode(args), from_name, callback_id)
    ffext:ff_rpc_call('', from_name, 'test', args, function() print('TTTTT') end)
    
    ffscene:task_callback(from_name, args, callback_id)
end
