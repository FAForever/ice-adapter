import bjsonrpc
from bjsonrpc.handlers import BaseHandler

class MyServerHandler(BaseHandler):
    def hello(self, txt):
        response = "hello, %s!." % txt
        print("*", response)
        return response

s = bjsonrpc.createserver(handler_factory = MyServerHandler, port = 5362)
s.debug_socket(True)
s.serve()
