#!/usr/bin/python
# Original code from: https://stackoverflow.com/questions/17377426/shared-variable-in-pythons-multiprocessing
#
# This snippet just illustates differences between Value and Manager.Value
# Also illustrates thread safety (or lack thereof)
#   Manager.Value currently does not support the lock mechanism, not process safe
#   Value itself does, but needs to explicitly call acquire() and release()
#
import time
from multiprocessing import Process, Manager, Value

def foo(data, name=''):
    print(type(data), data.value, name)
    for j in range(1000):
        #data.acquire()
        data.value += 1
        #data.release()

if __name__ == "__main__":
    manager = Manager()
    x = manager.Value('i', 0, lock = True)
    y = Value('i', 0, lock = True)

    for i in range(5):
        Process(target=foo, args=(x, 'x')).start()
        Process(target=foo, args=(y, 'y')).start()

    print('Before waiting: ')
    print('x = {0}'.format(x.value))
    print('y = {0}'.format(y.value))

    time.sleep(5.0)
    print('After waiting: ')
    print('x = {0}'.format(x.value))
    print('y = {0}'.format(y.value))

