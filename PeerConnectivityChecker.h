#pragma once

#include <chrono>
#include <optional>

#include <webrtc/api/datachannelinterface.h>
#include <webrtc/rtc_base/messagehandler.h>
#include <webrtc/rtc_base/sigslot.h>

#include "Timer.h"

namespace faf {

class PeerConnectivityChecker : public rtc::MessageHandler
{
public:
  PeerConnectivityChecker(rtc::scoped_refptr<webrtc::DataChannelInterface> dc);

  bool handleMessageFromPeer(const uint8_t* data, std::size_t size);

  sigslot::signal0<sigslot::multi_threaded_local> SignalConnectivityLost;

  static constexpr uint8_t PingMessage[] = "ICEADAPTERPING";
  static constexpr uint8_t PongMessage[] = "ICEADAPTERPONG";
protected:
  void OnMessage(rtc::Message* msg) override;
  rtc::scoped_refptr<webrtc::DataChannelInterface> _dataChannel;
  std::optional<std::chrono::steady_clock::time_point> _lastSentPingTime;
  std::optional<std::chrono::steady_clock::time_point> _lastReceivedPongTime;
  unsigned int _missedPings{0};
  int _connectionCheckIntervalMs{7000};
};

} // namespace faf
