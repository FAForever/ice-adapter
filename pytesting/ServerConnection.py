from base.JsonRpcTcpClient import JsonRpcTcpClient

class ServerConnection:
  def __init__(self, masterEventCallback):
    self.client = JsonRpcTcpClient(self)
    self.client.connect("localhost", 54321)
    self.client.socket.waitForConnected(1000)
    self.client.call("master")
    self.masterEventCallback = masterEventCallback

  def onMasterEvent(self, event, clientId, *args):
    self.masterEventCallback(event, clientId, args)
