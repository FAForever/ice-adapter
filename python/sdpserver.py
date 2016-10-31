import bjsonrpc
from bjsonrpc.handlers import BaseHandler
from typing import Tuple

conns = {}
games = []

class SdpServer(BaseHandler):
  def _setup(self):
    print("_setup")

  def setId(self, playerID: int):
    print("setId: {}".format(playerID))
    global conns
    conns[playerID] = self._conn

  def setSdp(self, playerA: int, playerB: int, sdp: str):
    print("setSdp: {} -> {}: {}".format(playerA, playerB, sdp))
    global conns
    if playerB in conns:
      conns[playerB].call.onSdp(playerA, playerB, sdp)
    else:
      print("remote player {} not connected".format(playerB))

  def addHostedGame(self, hostPlayerId: int, hostPlayerLogin: str):
    global games
    games.append((hostPlayerId, hostPlayerLogin))

  def hostedGames(self):
    global games
    return games


s = bjsonrpc.createserver(handler_factory=SdpServer, port = 10123, host = "127.0.0.1")
s.serve()