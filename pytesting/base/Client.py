
import sys

class Client:
  def __init__(self, dispatcher, id, login):
    self.dispatcher = dispatcher
    self.id = id
    self.login = login
    print("RemoteClient for id {} created".format(id))

  def call(self, method, args=[], callback_result=None, callback_error=None):
    if not callback_error:
      callback_error = lambda error, m=method: self.error("error from calling {}: {}".format(m, error))
    self.dispatcher.client.call("sendToPlayer", [self.id, method, args], callback_result, callback_error)
    #if not method is "status" and not method is "sendToIceAdapter":
    self.log("calling: {}({})".format(method, args))

  def log(self, msg):
    print("Player {} ({}): {}".format(self.login, self.id, msg))

  def error(self, msg):
    print("Player {} ({}): {}".format(self.login, self.id, msg), file=sys.stderr)
