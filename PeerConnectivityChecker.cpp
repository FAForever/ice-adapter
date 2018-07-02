#include "PeerConnectivityChecker.h"

#include <webrtc/rtc_base/messagequeue.h>
#include <webrtc/rtc_base/thread.h>

#include "logging.h"

namespace faf {

PeerConnectivityChecker::PeerConnectivityChecker(rtc::scoped_refptr<webrtc::DataChannelInterface> dc,
                                                 ConnectivityLostCallback cb):
  _dataChannel(dc),
  _cb(cb)
{
  _pingTimer.start(_connectionCheckIntervalMs,
                   std::bind(&PeerConnectivityChecker::_sendPing, this));
}

bool PeerConnectivityChecker::handleMessageFromPeer(const uint8_t* data, std::size_t size)
{
  if (size == sizeof(PongMessage) &&
      std::equal(data,
                 data + sizeof(PongMessage),
                 PongMessage))
  {
    _lastReceivedPongTime = std::chrono::steady_clock::now();
    return true;
  }
  return false;
}


void PeerConnectivityChecker::_sendPing()
{
  if (_lastSentPingTime &&
      !_lastReceivedPongTime)
  {
    FAF_LOG_INFO << "PeerConnectivityChecker: missed ping, connectivity lost";
    _cb();
    return;
  }
  _dataChannel->Send(webrtc::DataBuffer(rtc::CopyOnWriteBuffer(PingMessage, sizeof(PingMessage)), true));
  _lastSentPingTime = std::chrono::steady_clock::now();
  _lastReceivedPongTime.reset();
}

} // namespace faf
