#include "JsonRpcClient.h"

#include <webrtc/base/thread.h>
#include <webrtc/base/physicalsocketserver.h>

#include "logging.h"

namespace faf {

JsonRpcClient::JsonRpcClient():
  _socket(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_STREAM))
{
  _socket->SignalConnectEvent.connect(this, &JsonRpcClient::_onConnected);
  _socket->SignalReadEvent.connect(this, &JsonRpcClient::_onRead);
  _socket->SignalCloseEvent.connect(this, &JsonRpcClient::_onDisconnected);
}

void JsonRpcClient::connect(std::string const& host, int port)
{
  _socket->Connect(rtc::SocketAddress(host, port));
}

void JsonRpcClient::disconnect()
{
  _socket->Close();
}

bool JsonRpcClient::isConnected() const
{
  return _socket->GetState() == rtc::AsyncSocket::CS_CONNECTED;
}

#if defined(WEBRTC_WIN)
SOCKET JsonRpcClient::GetDescriptor()
{
  auto d = static_cast<rtc::SocketDispatcher*>(_socket.get());
  if (d)
  {
    return d->GetSocket();
  }
}
#elif defined(WEBRTC_POSIX)
int JsonRpcClient::GetDescriptor()
{
  auto d =static_cast<rtc::SocketDispatcher*>(_socket.get());
  if (d)
  {
    return d->GetDescriptor();
  }
}
#endif

bool JsonRpcClient::_sendMessage(std::string const& message, rtc::AsyncSocket* socket)
{
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
