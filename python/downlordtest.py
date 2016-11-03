import bjsonrpc
from bjsonrpc.handlers import BaseHandler
from typing import Tuple

class IceAdapterHandler(BaseHandler):
  def _setup(self):
    print("_setup")
  def onGpgNetMessageReceived(self, header: str, chunks: tuple):
    print("from GPGNet: {}: {}".format(header, chunks))
    if header == "GameState":
      if len(chunks) >= 1:
        if chunks[0] == "Lobby":
          #self._conn.call.hostGame("africa.v0005")
          self._conn.call.joinGame("asdf", 1234)


iceconn = bjsonrpc.connect(host="127.0.0.1", port=7236, handler_factory=IceAdapterHandler)
iceconn.serve()