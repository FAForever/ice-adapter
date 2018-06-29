from ServerConnection import ServerConnection
from TestClient import TestClient
from IceAdapter import IceAdapter
from GPGNet import GPGNet
from P2PConnection import P2PConnection

import sys

from PyQt5 import QtCore

app = QtCore.QCoreApplication(sys.argv)

clients = {}
ice_adapters = {}
gpgnets = {}
host_id = 0
host_login = "?"

peers_in_lobby = []
p2p_connections = {}

def peerReady(id):
  for p in peers_in_lobby:
    p2p_connections[(id, p)] = P2PConnection(clients[id],
                                             id,
                                             p,
                                             clients[p].login,
                                             host_id)
    p2p_connections[(p, id)] = P2PConnection(clients[p],
                                             p,
                                             id,
                                             clients[id].login,
                                             host_id)
  peers_in_lobby.append(id)

def initIceAdapterForPlayer(clientState):
  playerId = clientState['id']
  ice_adapter = IceAdapter(clients[playerId])
  ice_adapters[playerId] = ice_adapter

  gpg_net = GPGNet(clients[playerId], host_id, host_login)
  gpgnets[playerId] = gpg_net

  ice_adapter._machine.on_enter_ice_adapter_ready(gpg_net.trigger)
  ice_adapter._machine.on_enter_no_ice_servers(gpg_net.callStatus)
  gpg_net._machine.on_enter_hosting(lambda id=playerId: peerReady(id))
  gpg_net._machine.on_enter_joining(lambda id=playerId: peerReady(id))


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
    srcId, destId, message = args[0]
    if (destId, srcId) in p2p_connections:
      p2p_connections[(destId, srcId)].onIceMessage(message)
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
    clients[player["id"]].call("status", callback_result=initIceAdapterForPlayer)

c = ServerConnection(onMasterEvent)
c.client.call("players", [], startPlayers)

app.exec_()
