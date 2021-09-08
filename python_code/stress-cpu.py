#!/usr/bin/python3
# Based off of https://superuser.com/questions/443406/how-can-i-produce-high-cpu-load-on-a-linux-server
# Added signal handler to properly kill the workers
import signal
import sys
import multiprocessing

if len(sys.argv) != 2:
    print("ERROR: Expecting one parameter (number of CPUs to stress)")
    sys.exit(1)
else:
    NUM_CPUS = int(sys.argv[1])
    POOL = None

# Define program signal handler
def signal_handler(sig, frame):
    print("Signal %s captured!" % sig)
    if type(POOL) is multiprocessing.pool.Pool:
        POOL.terminate()

    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

# Function to stress CPUs
def f(x):
    # Put any cpu (only) consuming operation here
    x = x * 3.14159 + 1.414
    while True:
        x * x / x

print("Stressing %s CPUs..." % NUM_CPUS)
POOL = multiprocessing.Pool(processes=NUM_CPUS)
POOL.map(f, range(NUM_CPUS))
