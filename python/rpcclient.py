import bjsonrpc
from bjsonrpc.handlers import BaseHandler

class MyHandler(BaseHandler):
  def notify(self, text):
    print("Notify:\n%s" % text)

conn = bjsonrpc.connect(host="127.0.0.1", port=5362, handler_factory=MyHandler)

conn.call.ping('Hello World 2342fdswf w ')