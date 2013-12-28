




print('main.lua loading....')

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
    print('process_task', name, json_encode(args), from_name, callback_id)
    ffscene:call_service('', from_name, 'test', json_encode(args), 0)
end
