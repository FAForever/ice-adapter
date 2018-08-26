#include "JsonRpcClient.h"

#include <webrtc/rtc_base/thread.h>
#include <webrtc/rtc_base/physicalsocketserver.h>

#include "logging.h"

namespace faf {

JsonRpcClient::JsonRpcClient()
{
}

void JsonRpcClient::connect(std::string const& host, int port)
{
  _socket.reset(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(AF_INET, SOCK_STREAM));
  _socket->SignalConnectEvent.connect(this, &JsonRpcClient::_onConnected);
  _socket->SignalReadEvent.connect(this, &JsonRpcClient::_onRead);
  _socket->SignalCloseEvent.connect(this, &JsonRpcClient::_onDisconnected);
  _socket->Connect(rtc::SocketAddress(host, port));
}

void JsonRpcClient::disconnect()
{
  if (_socket)
  {
    _socket->Close();
  }
  _socket.reset(nullptr);
}

bool JsonRpcClient::isConnected() const
{
  return _socket && _socket->GetState() == rtc::AsyncSocket::CS_CONNECTED;
}

#if defined(WEBRTC_WIN)
SOCKET JsonRpcClient::GetDescriptor()
{
  auto d = static_cast<rtc::SocketDispatcher*>(_socket.get());
  if (d)
  {
    return d->GetSocket();
  }
  return 0;
}
#elif defined(WEBRTC_POSIX)
int JsonRpcClient::GetDescriptor()
{
  if (!_socket)
  {
    return 0;
  }
  auto d =static_cast<rtc::SocketDispatcher*>(_socket.get());
  if (d)
  {
    return d->GetDescriptor();
  }
  return 0;
}
#endif

bool JsonRpcClient::_sendMessage(std::string const& message, rtc::AsyncSocket* socket)
{
  if (!_socket)
  {
    return false;
  }
  if (socket &&
      socket != _socket.get())
  {
    return false;
  }
  if (_socket->GetState() == rtc::AsyncSocket::CS_CONNECTED)
  {
    _socket->Send(message.c_str(), message.size());
    return true;
  }
  return false;
}

void JsonRpcClient::_onConnected(rtc::AsyncSocket* socket)
{
  SignalConnected.emit(_socket.get());
}

void JsonRpcClient::_onRead(rtc::AsyncSocket* socket)
{
  JsonRpc::_read(socket);
}

void JsonRpcClient::_onDisconnected(rtc::AsyncSocket* socket, int)
{
  SignalDisconnected.emit(_socket.get());
}

} // namespace faf
