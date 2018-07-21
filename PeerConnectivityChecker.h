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
  void _checkConnectivity();

  rtc::scoped_refptr<webrtc::DataChannelInterface> _dataChannel;
  ConnectivityLostCallback _cb;
  Timer _pingTimer;
  Timer _connectivityCheckTimer;
  std::optional<std::chrono::steady_clock::time_point> _timerStartTime;
  std::optional<std::chrono::steady_clock::time_point> _lastSentPingTime;
  std::optional<std::chrono::steady_clock::time_point> _lastReceivedPongTime;
  std::optional<std::chrono::steady_clock::time_point> _lastReceivedData;

  int _connectionTimeoutMs{10000};
  int _connectionCheckIntervalMs{1000};
  int _connectionPingIntervalMs{500};
};

} // namespace faf
