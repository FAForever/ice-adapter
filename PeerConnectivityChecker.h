#pragma once

#include <chrono>
#include <optional>
#include <functional>

#include <webrtc/api/datachannelinterface.h>
#include <webrtc/rtc_base/messagehandler.h>

#include "Timer.h"

namespace faf {

class PeerConnectivityChecker : public rtc::MessageHandler
{
public:
  typedef std::function<void()> ConnectivityLostCallback;
  PeerConnectivityChecker(rtc::scoped_refptr<webrtc::DataChannelInterface> dc,
                          ConnectivityLostCallback cb);

  virtual ~PeerConnectivityChecker();

  bool handleMessageFromPeer(const uint8_t* data, std::size_t size);

  static constexpr uint8_t PingMessage[] = "ICEADAPTERPING";
  static constexpr uint8_t PongMessage[] = "ICEADAPTERPONG";
protected:
  void OnMessage(rtc::Message* msg) override;

  rtc::scoped_refptr<webrtc::DataChannelInterface> _dataChannel;
  ConnectivityLostCallback _cb;
  std::optional<std::chrono::steady_clock::time_point> _lastSentPingTime;
  std::optional<std::chrono::steady_clock::time_point> _lastReceivedPongTime;
  unsigned int _missedPings{0};
  int _connectionCheckIntervalMs{7000};
};

} // namespace faf
