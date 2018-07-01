from ServerConnection import ServerConnection
from base.Client import Client
from base.GPGNet import GPGNet
from base.IceAdapter import IceAdapter
from base.P2PConnection import P2PConnection

class Runner:
  def __init__(self,
               clientClass = Client,
               gpgnetClass = GPGNet,
               iceAdapterClass = IceAdapter,
               p2pConnectionClass = P2PConnection,
               ):
    self.clientClass = clientClass
    self.gpgnetClass = gpgnetClass
    self.iceAdapterClass = iceAdapterClass
    self.p2pConnectionClass = p2pConnectionClass

    self.clients = {}
    self.ice_adapters = {}
    self.gpgnets = {}
    self.host_id = 0
    self.host_login = "?"

    self.peers_in_lobby = []
    self.p2p_connections = {}

  def peerReady(self, id):
    for p in self.peers_in_lobby:
      self.p2p_connections[(id, p)] = P2PConnection(self.clients[id],
                                                    id,
                                                    p,
                                                    self.clients[p].login,
                                                    self.host_id)
      self.p2p_connections[(p, id)] = P2PConnection(self.clients[p],
                                                    p,
                                                    id,
                                                    self.clients[id].login,
                                                    self.host_id)
    self.peers_in_lobby.append(id)

  def initIceAdapterForPlayer(self, clientState):
    playerId = clientState['id']
    ice_adapter = self.iceAdapterClass(self.clients[playerId])
    self.ice_adapters[playerId] = ice_adapter

    gpg_net = self.gpgnetClass(self.clients[playerId], host_id, host_login)
    self.gpgnets[playerId] = gpg_net

    ice_adapter._machine.on_enter_ice_adapter_ready(gpg_net.trigger)
    ice_adapter._machine.on_enter_no_ice_servers(gpg_net.callStatus)
    gpg_net._machine.on_enter_hosting(lambda id=playerId: self.peerReady(id))
    gpg_net._machine.on_enter_joining(lambda id=playerId: self.peerReady(id))

  def onMasterEvent(self, event, playerId, args):
    if event == 'onGpgNetMsgFromIceAdapter':
      self.gpgnets[playerId].processMessage(args[0], args[1])
    elif event == 'onIceAdapterOutput':
      pass
      # print("onIceAdapterOutput {}: {}".format(playerId, args))
    elif event == 'onIceOnConnected':
      srcId, destId, connected = args[0]
      self.p2p_connections[(destId, srcId)].onConnected(connected)
      print("onIceOnConnected {}: {}".format(playerId, args))
    elif event == 'onIceOnGpgNetMessageReceived':
      print("onIceOnGpgNetMessageReceived {}: {}".format(playerId, args))
    elif event == 'onIceOnIceMsg':
      srcId, destId, message = args[0]
      if (destId, srcId) in self.p2p_connections:
        self.p2p_connections[(destId, srcId)].onIceMessage(message)
    else:
      print("unhandled master event: {}".format((event, playerId, args)))

  def startPlayers(self, playerList):
    global host_id, host_login
    print("{} players ready".format(len(playerList)))
    for i, player in enumerate(playerList):
      if i == 0:
        host_id = player["id"]
        host_login = player["login"]
        self.clients[player["id"]] = self.clientClass(self.serverConnection, player["id"], player["login"])
        self.clients[player["id"]].call("status", callback_result=self.initIceAdapterForPlayer)

  def run(self):
    self.serverConnection = ServerConnection(self.onMasterEvent)
    self.serverConnection.client.call("players", [], self.startPlayers)
