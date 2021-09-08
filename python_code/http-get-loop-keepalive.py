#!/home/ubuntu/k8s-python/bin/python
import sys
import time

from requests import Request, Session

if len(sys.argv) != 3:
    print "ERROR: Expecting two parameters"
    print "\t1) Interval between requests (seconds)"
    print "\t2) The URL to 'GET' from"
    sys.exit(1)
else:
    INTERVAL = float(sys.argv[1])
    TARGET_URL = sys.argv[2]

s = Session()
req = Request('GET', TARGET_URL)

prepped = s.prepare_request(req)

while True:
    resp = s.send(prepped)

    if resp.status_code == 200:
        print resp.text
    else:
        print "ERROR: Returned status code %s" % resp.status_code

    time.sleep(INTERVAL)
