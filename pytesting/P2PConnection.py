from transitions import Machine

from PyQt5 import QtCore

class P2PConnection(object):
  states = [
    'initial',
    'connecting',
    'connected',
    'pinging'
  ]

  def __init__(self, client, peerId1, peerId2, hostId):
    self._client = client
    self._clientState = None
    self._peer_id_1 = peerId1
    self._peer_id_2 = peerId2
    self._host_id = hostId
    self._machine = Machine(model=self, states=P2PConnection.states, initial='initial')
    self._machine.add_transition(trigger='connect_sent', source='initial', dest='connecting', after='showstate')

  def showstate(self):
    self._client.log("P2PConnection adapter state changed to {}".format(self.state))
