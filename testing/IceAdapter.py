from transitions import Machine

import time
import hmac
import base64
from hashlib import sha1

from PyQt5 import QtCore

class IceAdapter(object):
  states = [
    'initial',
    'not_running',
    'not_connected',
    'no_ice_servers',
    'ice_adapter_ready'
  ]

  def __init__(self, client):
    self._client = client
    self._clientState = None
    self._machine = Machine(model=self, states=IceAdapter.states, initial='initial')
    self._machine.add_transition(trigger='trigger', source='initial', dest='no_ice_servers', conditions=['is_state_running', 'is_connected'], after='showstate')
    self._machine.add_transition(trigger='trigger', source='initial', dest='not_running', conditions=['is_state_not_running'], after='showstate')
    self._machine.add_transition(trigger='trigger', source='initial', dest='not_connected', conditions=['is_state_running'], after='showstate')
    self._machine.add_transition(trigger='trigger', source='not_running', dest='not_connected', conditions=['is_state_running'], after='showstate')
    self._machine.add_transition(trigger='trigger', source='not_connected', dest='no_ice_servers', conditions=['is_connected'], after='showstate')
    self._machine.add_transition(trigger='ice_servers_sent', source='no_ice_servers', dest='ice_adapter_ready', after='showstate')

    self.callStatus()

  def callStatus(self):
    self._client.call("status", callback_result=self.setClientState)

  def showstate(self):
    self._client.log("ICE adapter state changed to {}".format(self.state))

  def setClientState(self, clientState):
    if clientState != self._clientState:
      self._clientState = clientState
      self.trigger()

  def is_state_running(self):
    return self._clientState["ice_adapter_open"]

  def is_state_not_running(self):
    return not self._clientState["ice_adapter_open"]

  def is_connected(self):
    return self._clientState["ice_adapter_connected"]

  def on_enter_not_running(self):
    self._client.call("startIceAdapter")
    self.callStatus()

  def on_enter_not_connected(self):
    self._client.call("connectToIceAdapter")
    QtCore.QTimer.singleShot(300, self.callStatus)

  def on_enter_no_ice_servers(self):
    coturn_host = "vmrbg145.informatik.tu-muenchen.de"
    coturn_key = "banana"
    timestamp = int(time.time()) + 3600*24
    token_name = "{}:{}".format(timestamp, self._client.login)
    secret = hmac.new(coturn_key.encode(), str(token_name).encode(), sha1)
    auth_token = base64.b64encode(secret.digest()).decode()

    ice_servers = [dict(urls=["turn:{}?transport=tcp".format(coturn_host),
                              "turn:{}?transport=udp".format(coturn_host),
                              "stun:{}".format(coturn_host)],
                        username=token_name,
                        credential=auth_token,
                        credentialType="token")]
    self._client.call("sendToIceAdapter", ["setIceServers", [ice_servers]])
    self.ice_servers_sent()
