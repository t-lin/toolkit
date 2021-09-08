#!/usr/bin/python3
import select
import sys
import re

from systemd import journal

def printEntry(entry):
    assert entry and type(entry) is dict
    try:
        print(str(entry['__REALTIME_TIMESTAMP']) + ': ' + entry['MESSAGE'])
    except Exception as e:
        print(entry)

j = journal.Reader()
#j.add_match(_SYSTEMD_UNIT="ssh.service")
#j.log_level(journal.LOG_INFO)
j.seek_tail()

# Need to call get_previous() after seek_tail()
#   See: https://bugs.freedesktop.org/show_bug.cgi?id=64614
# TODO: See if latest systemd is fixed
# NOTE: get_previous() acts as a seek back 1 and then reads 1
j.get_previous()

# ===== Similar to journalctl -xe -n 1000 =====
#ips = set()
#lastN = 1000
#for i in range(lastN):
#    if i == 0:
#        # NOTE: get_previous() seeks back lastN entries and fetches it, leaving 9 left
#        entry = j.get_previous(lastN)
#    else:
#        entry = j.get_next()
#
#    msg = entry.get('MESSAGE')
#    #if msg and ("Invalid user" in msg or "Failed password" in msg):
#    if msg and re.search('(invalid user|failed password)', msg, re.IGNORECASE):
#        #printEntry(entry)
#        res = re.search('[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+', msg)
#        ips.add(res.group())
#
#print(ips)
#
#sys.exit(0)

# ===== Similar to journalctl -xe -f =====
p = select.poll()
p.register(j, j.get_events())
while p.poll():
    if j.process() != journal.APPEND:
        continue

    # Your example code has too many get_next() (i.e, "while j.get_next()" and "for event in j") which cause skipping entry.
    # Since each iteration of a journal.Reader() object is equal to "get_next()", just do simple iteration.
    for entry in j:
        print("\n==========")
        if entry.get('MESSAGE'): # Make sure it exists and is not empty
            printEntry(entry)

