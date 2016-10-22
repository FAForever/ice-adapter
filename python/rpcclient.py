import bjsonrpc
from bjsonrpc.handlers import BaseHandler

class MyHandler(BaseHandler):
  def rpcNeedSdp(self, localId, remoteId):
    print("rpcNeedSdp: %i %i" % (localId, remoteId))
  def rpcGatheredSdp(self, localId, remoteId, sdp):
    print("rpcNeedSdp: %i %i %s" % (localId, remoteId, sdp))
  def rpcGamestateChanged(self, newState):
    print("rpcGamestateChanged: %s" % newState)
    #if newState == "Idle":
    #  self._conn.call.hostGame('4v4isis.v0001')
    if newState == "Idle":
      self._conn.call.joinGame("testplayer", 12345)

conn = bjsonrpc.connect(host="127.0.0.1", port=7236, handler_factory=MyHandler)
conn.serve()