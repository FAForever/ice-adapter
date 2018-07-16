#include "PeerConnectivityChecker.h"

#include <webrtc/rtc_base/messagequeue.h>
#include <webrtc/rtc_base/thread.h>

#include "logging.h"

namespace faf {

PeerConnectivityChecker::PeerConnectivityChecker(
    rtc::scoped_refptr<webrtc::DataChannelInterface> dc,
    ConnectivityLostCallback cb) :
    _dataChannel(dc), _cb(cb) {
  _pingTimer.start(_connectionPingIntervalMs,
      std::bind(&PeerConnectivityChecker::_sendPing, this));
  _timerStartTime = std::chrono::steady_clock::now();
  _connectivityCheckTimer.start(_connectionCheckIntervalMs,
      std::bind(&PeerConnectivityChecker::_checkConnectivity, this));
}

bool PeerConnectivityChecker::handleMessageFromPeer(const uint8_t* data,
    std::size_t size) {
  if (size == sizeof(PongMessage)
      && std::equal(data, data + sizeof(PongMessage), PongMessage)) {
    _lastReceivedPongTime = std::chrono::steady_clock::now();
    return true;
  }
  _lastReceivedData = std::chrono::steady_clock::now();
  return false;
}

void PeerConnectivityChecker::_sendPing() {
  _dataChannel->Send(
      webrtc::DataBuffer(
          rtc::CopyOnWriteBuffer(PingMessage, sizeof(PingMessage)), true));
  _lastSentPingTime = std::chrono::steady_clock::now();
}

/*
 * todo: remove redundancy in code
 */
void PeerConnectivityChecker::_checkConnectivity() {
  auto connectionLostAssumptionTime = std::chrono::steady_clock::now()
      - std::chrono::milliseconds(_connectionTimeoutMs);

  /*
   * check for uninitialized times right after the connection is estalished
   * call callback, when after _connectionTimeoutMs milliseconds, if the
   * timers where not yet set.
   */
  if (!_lastReceivedData || !_lastReceivedPongTime) {
    if (_timerStartTime < connectionLostAssumptionTime) {
      FAF_LOG_INFO << "PeerConnectivityChecker: connectivity probably lost";
      _cb();
    }
    return;
  }

  if (_lastReceivedData < connectionLostAssumptionTime
      || _lastReceivedPongTime < connectionLostAssumptionTime) {
    FAF_LOG_INFO << "PeerConnectivityChecker: connectivity probably lost";
    _cb();
  }
}

} // namespace faf
