#include "JsonRpcServer.h"

#include "logging.h"

namespace faf {

JsonRpcServer::JsonRpcServer()
{
}

void JsonRpcServer::listen(int port)
{
  _server->SignalReadEvent.connect(this, &JsonRpcServer::_onNewClient);
  _server->Bind(rtc::SocketAddress("127.0.0.1", port));
  _server->Listen(5);
  FAF_LOG_DEBUG << "JsonRpcServer listening on port " << port;
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
  do
  {
    auto msgLength = _connectedSocket->Recv(_readBuffer.data(), readBufferSize, nullptr);
    _currentMsg.append(_readBuffer.data(), msgLength);

    GPGNetMessage::parse(_currentMsg, [this](GPGNetMessage const& msg)
    {
      FAF_LOG_TRACE << "received " << msg.toDebug();
      SignalNewGPGNetMessage.emit(msg);
    });
  }
}

} // namespace faf
