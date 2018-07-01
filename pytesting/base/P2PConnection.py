from transitions import Machine

from PyQt5 import QtCore

class P2PConnection(object):
  states = [
    'initial',
    'connecting',
    'connected',
    'pinging'
  ]

  def __init__(self, client, peerId, remotePeerId, remotePeerLogin, hostId):
    self._client = client
    self._clientState = None
    self._peerId = peerId
    self._remotePeerId = remotePeerId
    self._remotePeerLogin = remotePeerLogin
    self._hostId = hostId
    self._machine = Machine(model=self, states=P2PConnection.states, initial='initial')
    self._machine.add_transition(trigger='connect_sent', source='initial', dest='connecting', after='showstate')
    self.connected = False

    if peerId == hostId:
      client.call("sendToIceAdapter", ["connectToPeer", [remotePeerLogin, remotePeerId, True]])
    elif remotePeerId != hostId:
      client.call("sendToIceAdapter", ["connectToPeer", [remotePeerLogin, remotePeerId, peerId < remotePeerId]])

  def showstate(self):
    self._client.log("P2PConnection adapter state changed to {}".format(self.state))

  def onIceMessage(self, message):
    self._client.call("sendToIceAdapter", ["iceMsg", [self._remotePeerId, message]])

  def onConnected(self, isConnected):
    #todo: start pinging
    self.connected = isConnected

