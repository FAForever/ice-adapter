#include "GPGNetServer.h"

#include <iostream>

#include <webrtc/base/nethelpers.h>
#include <webrtc/base/asynctcpsocket.h>

#include "logging.h"

namespace faf {


GPGNetServer::GPGNetServer():
  _server(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_STREAM))
{
}

void GPGNetServer::listen(int port)
{
  _server->SignalReadEvent.connect(this, &GPGNetServer::_onNewClient);
  _server->Bind(rtc::SocketAddress("127.0.0.1", port));
  _server->Listen(5);
  //_server->SignalWriteEvent.connect(this, &GPGNetServer::_onConnect);
  //_server->SignalCloseEvent.connect(this, &GPGNetServer::_onNewClient);
  FAF_LOG_DEBUG << "GPGNetServer listening on port " << port;
}

void GPGNetServer::sendMessage(GPGNetMessage const& msg)
{
  if (!_connectedSocket)
  {
    FAF_LOG_ERROR << "No GPGNetConnection. Wait for the game to connect before sending messages";
    return;
  }
  auto msgString = msg.toBinary();
  _connectedSocket->Send(msgString.c_str(), msgString.length());
  FAF_LOG_TRACE << "sending " << msg.toDebug();
}

void GPGNetServer::sendCreateLobby(InitMode initMode,
                                   int port,
                                   std::string const& login,
                                   int playerId,
                                   int natTraversalProvider)
{
  GPGNetMessage msg;
  msg.header = "CreateLobby";
  msg.chunks = {
    static_cast<std::underlying_type<InitMode>::type>(initMode),
    port,
    login,
    playerId,
    natTraversalProvider
  };
  sendMessage(msg);
}

void GPGNetServer::sendConnectToPeer(std::string const& addressAndPort,
                                     std::string const& playerName,
                                     int playerId)
{
  GPGNetMessage msg;
  msg.header = "ConnectToPeer";
  msg.chunks = {
    addressAndPort,
    playerName,
    playerId
  };
  sendMessage(msg);
  FAF_LOG_INFO << "sending ConnectToPeer " << playerId << " " << playerName << " " << addressAndPort;
}

void GPGNetServer::sendJoinGame(std::string const& addressAndPort,
                                std::string const& remotePlayerName,
                                int remotePlayerId)
{
  GPGNetMessage msg;
  msg.header = "JoinGame";
  msg.chunks = {
    addressAndPort,
    remotePlayerName,
    remotePlayerId
  };
  sendMessage(msg);
}

void GPGNetServer::sendHostGame(std::string const& map)
{
  GPGNetMessage msg;
  msg.header = "HostGame";
  msg.chunks = {
    map
  };
  sendMessage(msg);
}

void GPGNetServer::sendSendNatPacket(std::string const& addressAndPort,
                                     std::string const& message)
{
  GPGNetMessage msg;
  msg.header = "SendNatPacket";
  msg.chunks = {
    addressAndPort,
    message
  };
  sendMessage(msg);
}

void GPGNetServer::sendDisconnectFromPeer(int remotePlayerId)
{
  GPGNetMessage msg;
  msg.header = "DisconnectFromPeer";
  msg.chunks = {
    remotePlayerId
  };
  sendMessage(msg);
}

void GPGNetServer::sendPing()
{
  GPGNetMessage msg;
  msg.header = "ping";
  sendMessage(msg);
}

void GPGNetServer::_onNewClient(rtc::AsyncSocket* socket)
{
  rtc::SocketAddress accept_addr;
  _connectedSocket = std::unique_ptr<rtc::AsyncSocket>(_server->Accept(&accept_addr));
  _connectedSocket->SignalReadEvent.connect(this, &GPGNetServer::_onRead);
  _connectedSocket->SignalCloseEvent.connect(this, &GPGNetServer::_onClientDisconnect);
  FAF_LOG_DEBUG << "GPGNetServer client connected from " << accept_addr;
  SignalClientConnected.emit();
}

void GPGNetServer::_onClientDisconnect(rtc::AsyncSocket* socket, int _whatsThis_)
{
  _connectedSocket.reset();
  FAF_LOG_DEBUG << "GPGNetServer client disonnected: " << _whatsThis_;
  SignalClientDisconnected.emit();
}

void GPGNetServer::_onRead(rtc::AsyncSocket* socket)
{
  int msgLength = 0;
  do
  {
    msgLength = _connectedSocket->Recv(_readBuffer.data(), _readBuffer.size(), nullptr);
    _currentMsg.append(_readBuffer.data(), msgLength);

    GPGNetMessage::parse(_currentMsg, [this](GPGNetMessage const& msg)
    {
      FAF_LOG_TRACE << "received " << msg.toDebug();
      SignalNewGPGNetMessage.emit(msg);
    });
  }
  while(msgLength > 0);
}

} // namespace faf
