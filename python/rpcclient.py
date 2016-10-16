import bjsonrpc
from bjsonrpc.handlers import BaseHandler

class MyHandler(BaseHandler):
  def notify(self, text):
    print("Notify:\n%s" % text)

conn = bjsonrpc.connect(host="127.0.0.1", port=5362, handler_factory=MyHandler)
#conn = bjsonrpc.connect(host="::1", port=5362, handler_factory=MyHandler)

conn.call.ping('Hello World 2342fdswf w ')
print("1")
conn.call.ping('Hello World 2342fdswf w ')
print("3")
conn.call.ping('Hello World 2342fdswf w ')
print("4")
conn.call.quit('quit now!')
print("2")