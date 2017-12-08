from transitions import Machine

from PyQt5 import QtCore

class GPGNet(object):
  states = [
    'no_state',
    'not_connected', # the client must connect to the ice-adapters GPGNet server
    'none',          # the game is launching and didn't send GameState "Idle" yet
    'idle',          # the game send GameState "Idle"
    'lobby',
    'hosting',
    'joining'
  ]

  def __init__(self, client, host_id, host_login):
    self._client = client
    self._host_id = host_id
    self._host_login = host_login
    self._machine = Machine(model=self, states=GPGNet.states, initial='no_state')
    self._machine.add_transition(trigger='trigger', source='no_state', dest='not_connected',
                                 conditions=['gpgnet_is_not_connected', 'has_ice_state'],
                                 after='showstate')
    self._machine.add_transition(trigger='trigger', source='no_state', dest='none',
                                 conditions=['gpgnet_is_connected', 'has_ice_state'],
                                 after='showstate')
    self._machine.add_transition(trigger='trigger', source='not_connected', dest='none',
                                 conditions=['gpgnet_is_connected'],
                                 after='showstate')
    self._machine.add_transition(trigger='trigger', source='is_connected', dest='none',
                                 conditions=['is_state_none'],
                                 after='showstate')
    self._machine.add_transition(trigger='trigger', source='none', dest='idle',
                                 conditions=['is_state_idle'],
                                 after='showstate')
    self._machine.add_transition(trigger='trigger', source='idle', dest='lobby',
                                 conditions=['is_state_lobby'],
                                 after='showstate')
    self._machine.add_transition(trigger='received_hostgame_command', source='lobby', dest='hosting', after='showstate')
    self._machine.add_transition(trigger='received_joingame_command', source='lobby', dest='joining', after='showstate')

    self._clientState = None
    self._iceState = None
    self.callStatus()

  def showstate(self):
    self._client.log("GPGNet state changed to {}".format(self.state))

  def processMessage(self, header, chunks):
    if header == "CreateLobby":
      self._client.call("sendToGpgNet", ["GameState", ["Lobby"]])
      self.lobbyPort = chunks[1]
      self.callIceStatus()
    elif header == "HostGame":
      self.received_hostgame_command()
    elif header == "JoinGame":
      self.received_joingame_command()

  def callStatus(self):
    self._client.call("status", callback_result=self.setClientState)

  def setClientState(self, clientState):
    if clientState != self._clientState:
      self._clientState = clientState
      self.trigger()

  def has_client_state(self):
    if self._clientState is None:
      self.callStatus()
      return False
    else:
      return True

  def callIceStatus(self):
    self._client.call("sendToIceAdapter", ["status", []], callback_result=self.setIceStatus)

  def setIceStatus(self, iceState):
    if iceState != self._iceState:
      self._iceState = iceState
      self.trigger()

  def has_ice_state(self):
    if not self.has_client_state():
      return False
    if not self._clientState['ice_adapter_connected']:
      return False
    if self._iceState is None:
      self.callIceStatus()
      return False
    else:
      return True

  def gpgnet_is_not_connected(self):
    if self._clientState:
      return not self._clientState["gpgnet_connected"]
    else:
      return False

  def gpgnet_is_connected(self):
    if self._iceState:
      return self._iceState["gpgnet"]["connected"]
    else:
      return False

  def is_state_none(self):
    return self._iceState["gpgnet"]["game_state"] == "None"

  def is_state_idle(self):
    return self._iceState["gpgnet"]["game_state"] == "Idle"

  def is_state_lobby(self):
    return self._iceState["gpgnet"]["game_state"] == "Lobby"

  def is_state_hosting(self):
    return "Hosting" in self._iceState["gpgnet"]["task_string"]

  def is_state_joining(self):
    return "Joining" in self._iceState["gpgnet"]["task_string"]

  def on_enter_not_connected(self):
    self._client.call("connectToGPGNet", [self._iceState["gpgnet"]["local_port"]])
    QtCore.QTimer.singleShot(500, self.callIceStatus)

  def on_enter_none(self):
    self._client.call("sendToGpgNet", ["GameState", ["Idle"]])
    self.callIceStatus()

  def on_enter_idle(self):
    self.callIceStatus()

  def on_enter_lobby(self):
    if self._host_id == self._client.id:
      self._client.call("sendToIceAdapter", ["hostGame", ["testmap"]])
    else:
      self._client.call("sendToIceAdapter", ["joinGame", [self._host_login, self._host_id]])
