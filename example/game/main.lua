




print('main.lua loading....')

function init()
	print('main execute....')
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

