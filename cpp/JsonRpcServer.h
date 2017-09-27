#pragma once

#include <memory>
#include <map>

#include <webrtc/base/asyncsocket.h>

namespace faf {

static const std::size_t readBufferSize = 2048;

class JsonRpcServer
{
public:
  JsonRpcServer();
  void listen(int port);
protected:
  void _onNewClient(rtc::AsyncSocket* socket);
  void _onClientDisconnect(rtc::AsyncSocket* socket, int);
  void _onRead(rtc::AsyncSocket* socket);
  std::unique_ptr<rtc::AsyncSocket> _server;
  std::map<rtc::AsyncSocket*, std::unique_ptr<rtc::AsyncSocket>> _connectedSockets;
  std::array<char, readBufferSize> _readBuffer;
  std::map<rtc::AsyncSocket*, std::string> _currentMsg;
};

} // namespace faf
