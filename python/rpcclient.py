import bjsonrpc
from bjsonrpc.handlers import BaseHandler
from typing import Tuple

class MyHandler(BaseHandler):
  def rpcNeedSdp(self, localId: int, remoteId: int):
    print("rpcNeedSdp: %i %i" % (localId, remoteId))
  def rpcGatheredSdp(self, localId: int, remoteId: int, sdp: str):
    print("rpcNeedSdp: %i %i %s" % (localId, remoteId, sdp))
  def rpcGPGNetMessageReceived(self, header: str, chunks: Tuple):
    if header == "GameState" and len(chunks) == 1:
      print("rpcGamestateChanged: %s" % chunks[0])
      #if newState == "Idle":
      #  self._conn.call.hostGame('4v4isis.v0001')
      if chunks[0] == "Idle":
        self._conn.call.joinGame("testplayer", 12345)

conn = bjsonrpc.connect(host="127.0.0.1", port=7236, handler_factory=MyHandler)
conn.serve()