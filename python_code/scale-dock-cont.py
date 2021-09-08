#!/usr/bin/python
import sys
import requests
import requests_unixsocket

if len(sys.argv) != 4:
    print "ERROR: Expecting three parameters"
    print "\t1) Container ID"
    print "\t2) CPU set for container (e.g. 0-3)"
    print "\t3) Memory to scale to, in Bytes (e.g. 536870912 for 512 MB)"
    sys.exit(1)
else:
    CONT_ID = sys.argv[1]
    CPUSET = sys.argv[2]
    TARGET_MEM = sys.argv[3]

requests_unixsocket.monkeypatch()

UPDATE_API = "/containers/%s/update"

UPDATE_BODY = "{\
    \"CpusetCpus\": \"%s\",\
    \"Memory\": %s\
}"

url = "http+unix://%2Fvar%2Frun%2Fdocker.sock" + (UPDATE_API % CONT_ID)
headers = {'Content-Type': 'application/json'}
res = requests.post(url, headers = headers, data = UPDATE_BODY % (CPUSET, TARGET_MEM))
#assert res.status_code == 200, "ERROR: %s" % res.status_code

print "Status: %s" % res.status_code
print(res.text)
