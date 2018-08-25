#include "PeerConnectivityChecker.h"

#include <webrtc/rtc_base/messagequeue.h>
#include <webrtc/rtc_base/thread.h>

#include "logging.h"

namespace faf {

PeerConnectivityChecker::PeerConnectivityChecker(rtc::scoped_refptr<webrtc::DataChannelInterface> dc, ConnectivityLostCallback cb) :
    _dataChannel(dc), _cb(cb)
{
  _timerStartTime = std::chrono::steady_clock::now();
  _connectivityCheckTimer.start(_connectionCheckIntervalMs, std::bind(&PeerConnectivityChecker::_checkConnectivity, this));
  _pingStartDelayTimer.singleShot(_connectionPingStartDelayTimeMs, std::bind(&PeerConnectivityChecker::_startPing, this));
}

bool PeerConnectivityChecker::handleMessageFromPeer(const uint8_t* data, std::size_t size)
{
  if (size == sizeof(PongMessage)
      && std::equal(data, data + sizeof(PongMessage), PongMessage))
  {
    _lastReceivedPongTime = std::chrono::steady_clock::now();
    return true;
  }
  _lastReceivedDataTime = std::chrono::steady_clock::now();
  return false;
}

void PeerConnectivityChecker::_startPing()
{
  FAF_LOG_INFO << "PeerConnectivityChecker: pingTimer start";
  _pingTimer.start(_connectionPingIntervalMs, std::bind(&PeerConnectivityChecker::_sendPing, this));
}

void PeerConnectivityChecker::_sendPing()
{
  _dataChannel->Send(webrtc::DataBuffer(rtc::CopyOnWriteBuffer(PingMessage, sizeof(PingMessage)), true));
  _lastSentPingTime = std::chrono::steady_clock::now();
}

void PeerConnectivityChecker::_checkConnectivity()
{
  auto connectionLostAssumptionTime = std::chrono::steady_clock::now()
      - std::chrono::milliseconds(_connectionTimeoutMs);

  bool assumeConnectivityLost = true;

  /*
   * check for uninitialized time values right after the connectivityChecker
   * is started.
   * call the callback, when after connectionLostAssumptionTime milliseconds,
   * if the timer values where not set yet.
   */
  if (!_lastReceivedDataTime &&
      !_lastReceivedPongTime &&
      _timerStartTime > connectionLostAssumptionTime)
  {
    assumeConnectivityLost = false;
  }

  /*
   * check the time of the last received data:
   * if data was received after connectionLostAssumptionTime,
   * no connection loss is assumed.
   */
  if (_lastReceivedDataTime &&
      _lastReceivedDataTime > connectionLostAssumptionTime)
  {
    assumeConnectivityLost = false;
  }

  /*
   * ... the same applies for explicit ping pong messages.
   */
  if (_lastReceivedPongTime &&
      _lastReceivedPongTime > connectionLostAssumptionTime)
  {
    assumeConnectivityLost = false;
  }

  if (assumeConnectivityLost)
  {
    FAF_LOG_INFO << "PeerConnectivityChecker: connectivity probably lost";
    _cb();
  }

}

} // namespace faf
