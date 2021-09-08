#!/usr/bin/python
import sys
import requests
import traceback

# Makes HTTP request, assumes returned body is JSON
# Deserializes JSON into dictionary
def httpReqJson(url, method = 'GET'):
    assert type(url) in (str, unicode)
    assert type(method) in (str, unicode)

    req = requests.get(url)

    if (req.status_code != 200):
        print "ERROR: Request to %s failed" % url
        print "\tReturn status was %d" % req.status_code
        traceback.print_stack()
        return None

    req.close()
    try:
        return req.json()
    except:
        # How to handle? Raise for now
        raise


##### EXAMPLE K8S USAGE SCENARIO BELOW #####
K8S_API_ENDPOINT = 'http://localhost:8001'

def kubectl_GetEndpoints(service, namespace = 'default'):
    url = K8S_API_ENDPOINT + '/api/v1/namespaces/%s/endpoints/%s' % (namespace, service)
    res_body = httpReqJson(url)
    assert type(res_body) is dict

    endpoints = res_body['subsets'][0]['addresses']
    port = res_body['subsets'][0]['ports'][0]['port']
    proto = res_body['subsets'][0]['ports'][0]['protocol']

    print "Endpoints are:"
    for ep in endpoints:
        print "%s %s:%s on host %s" % (proto, ep['ip'], port, ep['nodeName'])

kubectl_GetEndpoints('guids')

def kubectl_GetNodes():
    url = K8S_API_ENDPOINT + "/api/v1/nodes"
    res_body = httpReqJson(url)

    nodeList = res_body['items']
    for node in nodeList:
        hostname = node['metadata']['name']
        addrList = node['status']['addresses']
        for addr in addrList:
            if addr['type'] == 'InternalIP':
                ipAddr = addr['address']
                break

        print "%s has underlay address: %s" % (hostname, ipAddr)

kubectl_GetNodes()
