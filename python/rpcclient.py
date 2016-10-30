import bjsonrpc
from bjsonrpc.handlers import BaseHandler
from typing import Tuple

class MyHandler(BaseHandler):
  def onConnectionStateChanged(self, state: str):
    if state == "Connected":
        self._conn.call.hostGame('monument_valley.v0001')

conn = bjsonrpc.connect(host="127.0.0.1", port=7236, handler_factory=MyHandler)
conn.serve()