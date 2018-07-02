#pragma once

#include <chrono>
#include <optional>
#include <functional>

#include <webrtc/api/datachannelinterface.h>

#include "Timer.h"

namespace faf {

class PeerConnectivityChecker
{
public:
  typedef std::function<void()> ConnectivityLostCallback;
  PeerConnectivityChecker(rtc::scoped_refptr<webrtc::DataChannelInterface> dc,
                          ConnectivityLostCallback cb);

  bool handleMessageFromPeer(const uint8_t* data, std::size_t size);

  static constexpr uint8_t PingMessage[] = "ICEADAPTERPING";
  static constexpr uint8_t PongMessage[] = "ICEADAPTERPONG";
protected:
  void _sendPing();

  rtc::scoped_refptr<webrtc::DataChannelInterface> _dataChannel;
  ConnectivityLostCallback _cb;
  Timer _pingTimer;
  std::optional<std::chrono::steady_clock::time_point> _lastSentPingTime;
  std::optional<std::chrono::steady_clock::time_point> _lastReceivedPongTime;
  int _connectionCheckIntervalMs{6000};
};

} // namespace faf
