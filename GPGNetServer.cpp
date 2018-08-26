#include "GPGNetServer.h"

#include <iostream>

#include <webrtc/rtc_base/nethelpers.h>
#include <webrtc/rtc_base/asynctcpsocket.h>

#include "logging.h"

namespace faf {

GPGNetConnectionHandler::GPGNetConnectionHandler(rtc::AsyncSocket* socket):
  _socket(socket)
{
  rtc::SocketAddress accept_addr;
  _socket->SignalReadEvent.connect(this, &GPGNetConnectionHandler::_onRead);
  _socket->SignalCloseEvent.connect(this, &GPGNetConnectionHandler::_onClientDisconnect);
  FAF_LOG_DEBUG << "GPGNetServer client connected from " << accept_addr.ToString();
}

void GPGNetConnectionHandler::send(std::string const& msg)
{
  _socket->Send(msg.c_str(), msg.length());
}

void GPGNetConnectionHandler::_onClientDisconnect(rtc::AsyncSocket* socket, int _whatsThis_)
{
  if (socket == _socket)
  {
    SignalClientDisconnected.emit(this);
  }
}

void GPGNetConnectionHandler::_onRead(rtc::AsyncSocket* socket)
{
  int msgLength = 0;
  do
  {
    msgLength = socket->Recv(_readBuffer.data(), _readBuffer.size(), nullptr);

    if (msgLength > 0)
    {
      _currentMsg.append(_readBuffer.data(), std::size_t(msgLength));
      GPGNetMessage::parse(_currentMsg, [this](GPGNetMessage const& msg)
      {
        FAF_LOG_TRACE << "GPGNetServer received " << msg.toDebug();
        SignalNewGPGNetMessage.emit(msg);
      });
    }
  }
  while(msgLength > 0);
}

GPGNetServer::GPGNetServer():
  _server(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(AF_INET, SOCK_STREAM))
{
}

void GPGNetServer::listen(int port)
{
  _server->SignalReadEvent.connect(this, &GPGNetServer::_onNewClient);
  _server->Bind(rtc::SocketAddress("127.0.0.1", port));
  _server->Listen(5);
  //_server->SignalWriteEvent.connect(this, &GPGNetServer::_onConnect);
  //_server->SignalCloseEvent.connect(this, &GPGNetServer::_onNewClient);
  FAF_LOG_INFO << "GPGNetServer listening on port " << _server->GetLocalAddress().port();
}

int GPGNetServer::listenPort() const
{
  return _server->GetLocalAddress().port();
}

bool GPGNetServer::hasConnectedClient() const
{
  return !_connectedSockets.empty();
}

void GPGNetServer::sendMessage(GPGNetMessage const& msg)
{
  auto msgString = msg.toBinary();
  FAF_LOG_INFO << "GPGNetServer::sendMessage: " << msg.toDebug();
  for(auto it = _connectedSockets.begin(), end = _connectedSockets.end(); it != end; ++it)
  {
    (*it)->send(msgString);
  }
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
  if (socket != _server.get())
  {
    FAF_LOG_ERROR << "??";
    return;
  }
  rtc::SocketAddress accept_addr;

  auto handler = new GPGNetConnectionHandler(_server->Accept(&accept_addr));
  _connectedSockets.insert(handler);
  handler->SignalNewGPGNetMessage.connect(this, &GPGNetServer::_onClientMessage);
  handler->SignalClientDisconnected.connect(this, &GPGNetServer::_onClientDisconnect);
  SignalClientConnected.emit();
}

void GPGNetServer::_onClientDisconnect(GPGNetConnectionHandler* handler)
{
  rtc::Thread::Current()->Post(RTC_FROM_HERE, this, 0, new rtc::TypedMessageData<GPGNetConnectionHandler*>(handler));
}

void GPGNetServer::_onClientMessage(GPGNetMessage msg)
{
  SignalNewGPGNetMessage.emit(msg);
}

void GPGNetServer::OnMessage(rtc::Message* msg)
{
  auto handler = static_cast<rtc::TypedMessageData<GPGNetConnectionHandler*>*>(msg->pdata)->data();
  delete handler;
  _connectedSockets.erase(handler);
  SignalClientDisconnected.emit();
}

} // namespace faf
