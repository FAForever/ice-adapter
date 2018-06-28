#include "PeerConnectivityChecker.h"

#include <webrtc/rtc_base/messagequeue.h>
#include <webrtc/rtc_base/thread.h>

#include "logging.h"

namespace faf {

PeerConnectivityChecker::PeerConnectivityChecker(rtc::scoped_refptr<webrtc::DataChannelInterface> dc):
  _dataChannel(dc)
{
  rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, _connectionCheckIntervalMs, this);
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


void PeerConnectivityChecker::OnMessage(rtc::Message* msg)
{
  if (_lastSentPingTime &&
      !_lastReceivedPongTime)
  {
    ++_missedPings;
    if (_missedPings >= 2)
    {
      FAF_LOG_INFO << "PeerConnectivityChecker:" << _missedPings << " missed pings, connectivity lost";
      SignalConnectivityLost.emit();
      return;
    }
  }
  _dataChannel->Send(webrtc::DataBuffer(rtc::CopyOnWriteBuffer(PingMessage, sizeof(PingMessage)), true));
  _lastSentPingTime = std::chrono::steady_clock::now();
  _lastReceivedPongTime.reset();
  rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, _connectionCheckIntervalMs, this);
}

} // namespace faf
