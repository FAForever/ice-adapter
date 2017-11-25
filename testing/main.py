from ServerConnection import ServerConnection
from TestClient import TestClient
from IceAdapter import IceAdapter
from GPGNet import GPGNet

import sys

from PyQt5 import QtCore

app = QtCore.QCoreApplication(sys.argv)

clients = {}
ice_adapters = {}
gpgnets = {}
host_id = 0
host_login = "?"

def createTestsForPlayer(clientState):
  playerId = clientState['id']
  ice_adapters[playerId] = IceAdapter(clients[playerId])
  gpgnets[playerId] = GPGNet(clients[playerId], host_id, host_login)


def onMasterEvent(event, playerId, args):
  if event == 'onGpgNetMsgFromIceAdapter':
    gpgnets[playerId].processMessage(args[0], args[1])
  elif event == 'onIceAdapterOutput':
    print("onIceAdapterOutput {}: {}".format(playerId, args))
  elif event == 'onIceOnConnected':
    print("onIceOnConnected {}: {}".format(playerId, args))
  elif event == 'onIceOnGpgNetMessageReceived':
    print("onIceOnGpgNetMessageReceived {}: {}".format(playerId, args))
  elif event == 'onIceOnIceMsg':
    print("onIceOnIceMsg {}: {}".format(playerId, args))
  else:
    print("unhandled master event: {}".format((event, playerId, args)))


def startPlayers(playerList):
  global host_id, host_login
  print("{} players ready".format(len(playerList)))
  for i, player in enumerate(playerList):
    if i == 0:
      host_id = player["id"]
      host_login = player["login"]
    clients[player["id"]] = TestClient(c, player["id"], player["login"], i==0)
    clients[player["id"]].call("status", callback_result=createTestsForPlayer)

c = ServerConnection(onMasterEvent)
c.client.call("players", [], startPlayers)

app.exec_()
