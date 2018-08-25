#include "JsonRpcServer.h"

#if defined(WEBRTC_POSIX)
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <webrtc/rtc_base/physicalsocketserver.h>
#endif

#include <webrtc/rtc_base/thread.h>

#include "logging.h"

namespace faf {

JsonRpcServer::JsonRpcServer():
  _server(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(AF_INET, SOCK_STREAM))
{
}

JsonRpcServer::~JsonRpcServer()
{
}

void JsonRpcServer::listen(int port, std::string const& hostname)
{
  _server->SignalReadEvent.connect(this, &JsonRpcServer::_onNewClient);
  if (_server->Bind(rtc::SocketAddress(hostname, port)) != 0)
  {
    FAF_LOG_ERROR << "unable to bind to port " << port;
    std::exit(1);
  }
  _server->Listen(5);
  FAF_LOG_INFO << "JsonRpcServer listening on " << hostname << ":" << _server->GetLocalAddress().port();
}

int JsonRpcServer::listenPort() const
{
  return _server->GetLocalAddress().port();
}

void JsonRpcServer::_onNewClient(rtc::AsyncSocket* socket)
{
  rtc::SocketAddress accept_addr;
  auto newConnectedSocket = std::shared_ptr<rtc::AsyncSocket>(_server->Accept(&accept_addr));
#if defined(WEBRTC_POSIX)
  int fd = static_cast<rtc::SocketDispatcher*>(newConnectedSocket.get())->GetDescriptor();
  if (fd)
  {
    int keepalive = 1;
    int keepcnt = 1;
    int keepidle = 3;
    int keepintvl = 5;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
    setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
  }
#endif
  newConnectedSocket->SignalReadEvent.connect(this, &JsonRpcServer::_onRead);
  newConnectedSocket->SignalCloseEvent.connect(this, &JsonRpcServer::_onClientDisconnect);
  _connectedSockets.insert(std::make_pair(newConnectedSocket.get(), newConnectedSocket));
  FAF_LOG_DEBUG << "JsonRpcServer client connected from " << accept_addr.ToString();
  SignalClientConnected.emit(newConnectedSocket.get());
}

void JsonRpcServer::_onClientDisconnect(rtc::AsyncSocket* socket, int _whatsThis_)
{
  _currentMsgs.erase(socket);
  _connectedSockets.erase(socket);
  FAF_LOG_DEBUG << "JsonRpcServer client disonnected: " << _whatsThis_;
  SignalClientDisconnected.emit(socket);
}

void JsonRpcServer::_onRead(rtc::AsyncSocket* socket)
{
  if (_connectedSockets.find(socket) != _connectedSockets.end())
  {
    JsonRpc::_read(socket);
  }
}

bool JsonRpcServer::_sendMessage(std::string const& message, rtc::AsyncSocket* socket)
{
  if (_connectedSockets.empty())
  {
    FAF_LOG_ERROR << "_connectedSockets.empty()";
    return false;
  }
  for (auto it = _connectedSockets.begin(), end = _connectedSockets.end(); it != end; ++it)
  {
    if (socket)
    {
      if (it->second.get() != socket)
      {
        continue;
      }
    }
    //FAF_LOG_TRACE << "sending " << message;

    if (!it->second->Send(message.c_str(), message.size()))
    {
      _currentMsgs.erase(it->second.get());
      it = _connectedSockets.erase(it);
      FAF_LOG_ERROR << "sending " << message << " failed";
    }
    else
    {
      //FAF_LOG_TRACE << " done";
    }
  }
  return true;
}

} // namespace faf
