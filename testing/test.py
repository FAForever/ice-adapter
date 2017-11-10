
import sys
import time
import hmac
import base64
from hashlib import sha1

from PyQt5 import QtCore

from JsonRpcTcpClient import JsonRpcTcpClient

class RemoteClient:
  def __init__(self, dispatcher, id, login):
    self.dispatcher = dispatcher
    self.id = id
    self.login = login
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
    print("onClientStatus", status)
    #self.dispatcher.call("sendToPlayer", id, "startIceAdapter", [[]])

  def onClientIceStatus(self):
    self.dispatcher.call("sendToPlayer", id, "startIceAdapter", [[]])

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
    self.dispatcher.call("sendToPlayer", [id, "setIceServers", [ice_servers]])

class Dispatcher:
  def __init__(self):
    self.client = JsonRpcTcpClient(self)
    self.client.connect("localhost", 54321)
    self.client.socket.waitForConnected(1000)
    self.client.call("master")
    self.client.call("players", [], self.playersResult)
    self.remotes = []
  def playersResult(self, playerList):
    for player in playerList:
      remote = RemoteClient(self, player["id"], player["login"])
      self.remotes.append(remote)
  def onIceAdapterOutput(self, id, output):
    print(id, output)

app = QtCore.QCoreApplication(sys.argv)

d = Dispatcher()

timer = QtCore.QTimer
#timer.singleShot(10000, app.quit)

app.exec_()
