# HTTP WSGI server handler
def application(env, response):
    #print env
    if env['REQUEST_METHOD'] == 'POST':
        postBody = env['wsgi.input'].read()
        #print "type of post body is: %s" % type(postBody)
        print postBody + '\n'
        response('200 OK', [('Content-Type', 'text/plain')])
        return "you sent me: %s" % postBody

    if env['PATH_INFO'] == '/hithere':
        response('200 OK', [('Content-Type', 'text/plain')])
        body = "hello world\n"
        return body
    else:
        response('404 Not Found', [('Content-Type', 'text/plain')])
        return b"Not Found\n"

