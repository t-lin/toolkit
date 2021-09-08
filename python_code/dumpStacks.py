#!/usr/bin/python
import os
import sys
import traceback

# Credit: https://stackoverflow.com/questions/1032813/dump-stacktraces-of-all-active-threads
print >> sys.stderr, "\n*** STACKTRACE - START ***\n"
code = []
for threadId, stack in sys._current_frames().items():
    code.append("\n# ThreadID: %s\tPID: %s" % (threadId, os.getpid()))
    for filename, lineno, name, line in traceback.extract_stack(stack):
        code.append('  File: "%s", line %d, in %s' % (filename, lineno, name))
        if line:
            code.append("    %s" % (line.strip()))

for line in code:
    print >> sys.stderr, line
print >> sys.stderr, "\n*** STACKTRACE - END ***\n"


########## FOR DUMPING GREENTHREAD STACKS ##########
import gc

from greenlet import greenlet

# Partial credit: https://stackoverflow.com/questions/12510648/in-gevent-how-can-i-dump-stack-traces-of-all-running-greenlets
# Dump stack trace of all greenthreads
print "Dumping greenlet stacks...\n"
for ob in gc.get_objects():
    if not isinstance(ob, greenlet):
        continue
    if not ob:
        continue
    if ob.gr_frame and bool(ob):
        stack = traceback.format_stack(ob.gr_frame, 50)
        print ''.join(stack)
        if len(stack) == 50:
            print "UH OH, VERY LARGE OR INFINITE STACK"
    print "\n\n"
