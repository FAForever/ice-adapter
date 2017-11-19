
import sys

from PyQt5 import QtCore

from JsonRpcTcpClient import JsonRpcTcpClient

app = QtCore.QCoreApplication(sys.argv)


timer = QtCore.QTimer
client = JsonRpcTcpClient(None)

def makePlayersQuit(playerList):
  for player in playerList:
    client.call("sendToPlayer", [player["id"], "quit", []])
  timer.singleShot(100, app.quit)

client.connect("localhost", 54321)
client.socket.waitForConnected(1000)
client.call("master")
client.call("players", [], makePlayersQuit)

app.exec_()