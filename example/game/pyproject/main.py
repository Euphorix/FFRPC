import ffext
import ff
from user_data import ttypes

def init():
    print('init......')
    def cb():
        print('timer.....')
        def task_cb(data):
            print('task cb', data)
        ffext.post_task('scene@1', 'test_task', [1,2,3,4,5,6], task_cb)
        #ff.post_task('fflua@1', 'test', [1,2,3,4,5,6], 0)
        def foo(ffreq):
            print('foo', ffreq)
        #ffext.ff_rpc_call('scene@0', '[1,2,3,444,555]', foo, 'test')
        msg = ttypes.friend_t()
        #ffext.ff_rpc_call('scene@0', msg, ffext.ff_rpc_callback(foo, ttypes.friend_t))
        return
    ffext.once_timer(1000, cb)
    return 0


def cleanup():
    print('cleanup.....')
    return 0

@ffext.ff_reg_scene_interfacee('ff::test')
def process_test(ffreq):
    print('pyprocess_test', ffreq.msg)
    ffreq.response(ffreq.msg)

@ffext.ff_reg_scene_interfacee(ttypes.friend_t, ttypes.friend_t)
def process_friend(ffreq):
    print('process_friend', ffreq.msg)
    ffreq.response(ffreq.msg)


@ffext.bind_task('py_task')
def py_test_task(args, cb):
    print('py_test_task', args)
    cb(args)