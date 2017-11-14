
import sys
import time
import hmac
import base64
from hashlib import sha1

from PyQt5 import QtCore

from JsonRpcTcpClient import JsonRpcTcpClient

class RemoteClient:
  def __init__(self, dispatcher, id, login, shouldHost):
    self.dispatcher = dispatcher
    self.id = id
    self.login = login
    self.shouldHost = shouldHost
    self.gpgnetserver_port = 0
    self.ice_gamestate = ""
    self.iceStateTimer = QtCore.QTimer()
    self.iceStateTimer.timeout.connect(self.callStatus)
    self.iceStateTimer.start(2000)
    self.callStatus()

  def call(self, method, args=[], callback_result=None, callback_error=None):
    self.dispatcher.client.call("sendToPlayer", [self.id, method, args], callback_result, callback_error)

  def callStatus(self):
    self.call("status", callback_result=self.onClientStatus)
    #self.dispatcher.call("sendToPlayer", [id, "status", []], self.onClientStatus)

  def onClientStatus(self, status):
    #print("onClientStatus", status)
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

  def onClientIceStatus(self, iceStatus):
    #print("onClientIceStatus", iceStatus)
    if iceStatus["ice_servers_size"] == 0:
      self.sendIceServers()
      self.call("sendToIceAdapter", ["setLobbyInitMode", ["normal"]])

    self.gpgnetserver_port = iceStatus["gpgnet"]["local_port"]
    self.ice_gamestate = iceStatus["gpgnet"]["game_state"]

    if self.ice_gamestate == "Lobby":
      if self.shouldHost:
        self.dispatcher.client.call("hostGame", [])

  def onGpgNetMsgFromIceAdapter(self, header, chunks):
    if self.ice_gamestate == "Idle" and header == "CreateLobby":
      self.call("sendToGpgNet", ["GameState", ["Lobby"]])

    print("onGpgNetMsgFromIceAdapter", header, chunks)

  def onIceAdapterOutput(self, lines):
    print("onIceAdapterOutput", lines)

  # These messages received the ice-adapter from the gpgnet client (the game)
  def onIceonGpgNetMessageReceived(self, *args):
    print("onIceonGpgNetMessageReceived", args)
    #print("onIceonGpgNetMessageReceived", header, chunks)

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
    #print(args)
    m(*args)

app = QtCore.QCoreApplication(sys.argv)

d = Dispatcher()

timer = QtCore.QTimer
#timer.singleShot(10000, app.quit)

app.exec_()
