
import base64
import hmac
import sys
import time
from hashlib import sha1

from PyQt5 import QtCore

from base.JsonRpcTcpClient import JsonRpcTcpClient


class RemoteClient:
  def __init__(self, dispatcher, id, login, should_host):
    self.dispatcher = dispatcher
    self.id = id
    self.login = login
    self.should_host = should_host
    self.gpgnetserver_port = 0
    self.ice_gamestate = "None"
    self.iceStateTimer = QtCore.QTimer()
    self.iceStateTimer.timeout.connect(self.callStatus)
    self.iceStateTimer.start(2000)
    self.callStatus()
    self.is_hosting = False
    self.is_joining = False
    self.pings = {}
    print("RemoteClient for id {} created (Master: {})".format(id, should_host))

  def call(self, method, args=[], callback_result=None, callback_error=None):
    self.dispatcher.client.call("sendToPlayer", [self.id, method, args], callback_result, callback_error)
    if not method is "status" and not method is "sendToIceAdapter":
      self.log("calling: {}({})".format(method, args))


  def callStatus(self):
    self.call("status", callback_result=self.onClientStatus)

  def log(self, msg):
    print("Player {} ({}): {}".format(self.login, self.id, msg))

  def onClientStatus(self, status):
    for tracker in status["ping_trackers"]:
      self.pings[tracker["remote_id"]] = tracker["ping"]
      self.log("ping to {}: {}".format(tracker["remote_id"], tracker["ping"]))

    if not status["ice_adapter_open"]:
      self.call("startIceAdapter", [[]])
    if status["ice_adapter_open"] and not status["ice_adapter_connected"]:
      self.call("connectToIceAdapter")
    if status["ice_adapter_connected"]:
      self.call("sendToIceAdapter", ["status", []], callback_result=self.onClientIceStatus)
    if not status["gpgnet_connected"] and self.gpgnetserver_port != 0:
      self.call("connectToGPGNet", [self.gpgnetserver_port])
    if status["gpgnet_connected"] and self.ice_gamestate == "None":
      self.call("sendToGpgNet", ["GameState", ["Idle"]])
      self.ice_gamestate = "Idle"

  def onClientIceStatus(self, iceStatus):
    #self.log("onClientIceStatus: {}".format(iceStatus))
    if iceStatus["ice_servers_size"] == 0:
      self.sendIceServers()
      self.call("sendToIceAdapter", ["setLobbyInitMode", ["normal"]])

    self.gpgnetserver_port = iceStatus["gpgnet"]["local_port"]
    #self.ice_gamestate = iceStatus["gpgnet"]["game_state"]

    if "Hosting" in iceStatus["gpgnet"]["task_string"]:
      self.is_hosting = True

    if self.ice_gamestate == "Lobby":
      if self.should_host:
        if not self.is_hosting:
          self.call("sendToIceAdapter", ["hostGame", ["testmap"]])
          self.is_hosting = True
      else:
        if not self.is_joining:
          host = self.dispatcher.getHostingPlayer()
          if host:
            self.call("sendToIceAdapter", ["joinGame", [host.login, host.id]])
            host.call("sendToIceAdapter", ["connectToPeer", [self.login, self.id, True]])
            self.is_joining = True

            for id, player in self.dispatcher.remotes.items():
              if (id != self.id) and (not player.is_hosting):
                self.call("sendToIceAdapter", ["connectToPeer", [player.login, id, False]])
                player.call("sendToIceAdapter", ["connectToPeer", [self.login, self.id, True]])
    #for relay in iceStatus['relays']:
    #  if relay['ice_agent']['connected'] and not relay['remote_player_id'] in self.pings:

  def onGpgNetMsgFromIceAdapter(self, header, chunks):
    if self.ice_gamestate == "Idle" and header == "CreateLobby":
      initMode, port, login, playerId, natTraversalProvider = chunks
      self.call("bindGameLobbySocket", [port])
      self.call("sendToGpgNet", ["GameState", ["Lobby"]])
      self.ice_gamestate = "Lobby"
    if self.ice_gamestate == "Lobby" and header == "HostGame":
      if not self.is_hosting:
        print("ERROR not hosting")
    if header == "JoinGame" or header == "ConnectToPeer":
      peerRelayAddress, remoteLogin, remoteId = chunks
      if not remoteId in self.pings:
        self.call("pingTracker", [peerRelayAddress, remoteId])
        self.pings[remoteId] = 0
        self.log("creating pingtracker for: {}".format(remoteId))

    #print("onGpgNetMsgFromIceAdapter", header, chunks)

  def onIceAdapterOutput(self, lines):
    for line in lines:
      if "FAF:" in line:
        #self.log("onIceAdapterOutput: {}".format(line.strip()))
        pass

  # These messages received the ice-adapter from the gpgnet client (the game)
  def onIceOnConnected(self, *args):
    self.log("onIceOnConnected: {}".format(args))

  # These messages received the ice-adapter from the gpgnet client (the game)
  def onIceOnGpgNetMessageReceived(self, *args):
    #print("onIceonGpgNetMessageReceived", args)
    #print("onIceonGpgNetMessageReceived", header, chunks)
    pass

  def onIceOnIceMsg(self, args):
    localId, remoteId, msg = args
    #self.log("onIceonIceMsg: {} {} {}".format(localId, remoteId, msg))
    self.dispatcher.remotes[remoteId].call("sendToIceAdapter", ["iceMsg", [localId, msg]])

  def sendIceServers(self):
    coturn_host = "vmrbg145.informatik.tu-muenchen.de"
    coturn_key = "banana"
    timestamp = int(time.time()) + 3600*24
    token_name = "{}:{}".format(timestamp, self.login)
    secret = hmac.new(coturn_key.encode(), str(token_name).encode(), sha1)
    auth_token = base64.b64encode(secret.digest()).decode()

    ice_servers = [dict(urls=["turn:{}?transport=tcp".format(coturn_host),
                              "turn:{}?transport=udp".format(coturn_host),
                              "stun:{}".format(coturn_host)],
                        username=token_name,
                        credential=auth_token,
                        credentialType="token")]
    self.call("sendToIceAdapter", ["setIceServers", [ice_servers]])

class Dispatcher:
  def __init__(self):
    self.client = JsonRpcTcpClient(self)
    self.client.connect("localhost", 54321)
    self.client.socket.waitForConnected(1000)
    self.client.call("master")
    self.client.call("players", [], self.playersResult)
    self.remotes = {}
  def playersResult(self, playerList):
    print("{} players ready".format(len(playerList)))
    for i, player in enumerate(playerList):
      self.remotes[player["id"]] = RemoteClient(self, player["id"], player["login"], i==0)

  def onMasterEvent(self, event, clientId, *args):
    m = getattr(self.remotes[clientId], event)
    #self.remotes[clientId].log("onMasterEvent: {}({}(".format(event, args))
    m(*args)

  def getHostingPlayer(self):
    for id, player in self.remotes.items():
      if player.is_hosting:
        return player
    return None

app = QtCore.QCoreApplication(sys.argv)

d = Dispatcher()

#timer = QtCore.QTimer
#timer.singleShot(10000, app.quit)

app.exec_()
