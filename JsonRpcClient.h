#pragma once

#include <memory>

#include <webrtc/base/asyncsocket.h>

#include "JsonRpc.h"

namespace faf {

class JsonRpcClient : public sigslot::has_slots<>, public JsonRpc
{
public:
  JsonRpcClient();
  void connect(std::string const& host, int port);
  void disconnect();

  bool isConnected() const;

  sigslot::signal1<rtc::AsyncSocket*, sigslot::multi_threaded_local> SignalConnected;
  sigslot::signal1<rtc::AsyncSocket*, sigslot::multi_threaded_local> SignalDisconnected;

#if defined(WEBRTC_WIN)
  SOCKET GetDescriptor();
#elif defined(WEBRTC_POSIX)
  int GetDescriptor();
#endif

protected:
  virtual bool _sendMessage(std::string const& message, rtc::AsyncSocket* socket) override;

  void _onConnected(rtc::AsyncSocket* socket);
  void _onRead(rtc::AsyncSocket* socket);
  void _onDisconnected(rtc::AsyncSocket* socket, int);

  std::unique_ptr<rtc::AsyncSocket> _socket;
};

} // namespace faf
