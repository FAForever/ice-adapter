import bjsonrpc
from bjsonrpc.handlers import BaseHandler
from typing import Tuple

class SdpHandler(BaseHandler):
  def _setup(self):
    print("_setup")
  def onSdp(self, playerA: int, playerB: int, sdp: str):
    print("onSdp: {} -> {}: {}".format(playerA, playerB, sdp))
  def onGpgNetMessageReceived(self, header: str, chunks: tuple):
    print("from GPGNet: {}: {}".format(header, chunks))

sdpconn = bjsonrpc.connect(host="127.0.0.1", port=10123, handler_factory=SdpHandler)

class IceAdapterHostHandler(BaseHandler):
  def _setup(self):
    print("_setup")
  def onConnectionStateChanged(self, state: str):
    if state == "Connected":
        print("Connected")
        status = self._conn.call.status()
        print("status: {}".format(status))
        myId = status["options"]["player_id"]
        myLogin = status["options"]["player_login"]
        self.sdpconn.call.setId(myId)

        self._conn.call.hostGame('monument_valley.v0001')
        self.sdpconn.call.addHostedGame(myId, myLogin)
    elif state == "Disconnected":
        print("Disconnected")
        self._conn.call.quit()
  def onGpgNetMessageReceived(self, header: str, chunks: tuple):
    print("from GPGNet: {}: {}".format(header, chunks))
  def onNeedSdp(self, playerA: int, playerB: int):
    print("onNeedSdp: {} -> {}".format(playerA, playerB))
  def onSdpGathered(self, playerA: int, playerB: int, sdp: str):
    self.sdpconn.setSdp(playerA, playerB, sdp)
  def onPeerStateChanged(self, playerA: int, playerB: int):
    print("onNeedSdp: {} -> {}".format(playerA, playerB))

iceconnhost = bjsonrpc.connect(host="127.0.0.1", port=7236, handler_factory=IceAdapterHostHandler)
iceconnhost.handler.sdpconn = sdpconn

class IceAdapterJoinHandler(IceAdapterHostHandler):
  def onConnectionStateChanged(self, state: str):
    if state == "Connected":
        print("join Connected")
        status = self._conn.call.status()
        print("status: {}".format(status))
        myId = status["options"]["player_id"]
        myLogin = status["options"]["player_login"]
        self.sdpconn.call.setId(myId)
        games = self.sdpconn.call.hostedGames()
        print("games: {}".format(games))
        if len(games) > 0:
          self._conn.call.joinGame(games[0][0], games[0][1])
    elif state == "Disconnected":
        print("Disconnected")
        self._conn.call.quit()

iceconnjoin = bjsonrpc.connect(host="127.0.0.1", port=7336, handler_factory=IceAdapterHostHandler)
iceconnjoin.handler.sdpconn = sdpconn

iceconnhost.serve()