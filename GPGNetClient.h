#pragma once

#include <memory>
#include <functional>

#include <webrtc/base/asyncsocket.h>

namespace faf {

struct GPGNetMessage;

class GPGNetClient : public sigslot::has_slots<>
{
public:
  GPGNetClient();

  void connect(std::string const& host, int port);
  void disconnect();

  bool isConnected() const;

  typedef std::function<void (GPGNetMessage const&)> Callback;
  void setCallback(Callback cb);

  void sendMessage(GPGNetMessage const& msg);

  sigslot::signal1<rtc::AsyncSocket*, sigslot::multi_threaded_local> SignalConnected;
  sigslot::signal1<rtc::AsyncSocket*, sigslot::multi_threaded_local> SignalDisconnected;
protected:

  void _onConnected(rtc::AsyncSocket* socket);
  void _onRead(rtc::AsyncSocket* socket);
  void _onDisconnected(rtc::AsyncSocket* socket, int);

  std::unique_ptr<rtc::AsyncSocket> _socket;
  Callback _cb;
  std::string _messageBuffer;
};

} // namespace faf
