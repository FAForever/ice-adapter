#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include <array>

#include <webrtc/rtc_base/asyncsocket.h>

#include "Timer.h"

namespace faf {

#if defined(_MSC_VER)
#  pragma pack(push, 1)
#endif
struct PingPacket
{
  enum {
    PING = 0x01ce,
    PONG = 0x1ce0
  } type;
  uint32_t senderId;
  uint32_t answererId;
  uint32_t pingId;
  uint8_t bloat[512];
}
#if !defined(_MSC_VER)
__attribute__((packed));
#else
;
#  pragma pack(pop)
#endif

class Pingtracker : public sigslot::has_slots<>
{
public:
  Pingtracker(int localPeerId,
              int remotePeerId,
              rtc::AsyncSocket* gameLobbySocket,
              rtc::SocketAddress const& peerAddress);
  void onPingPacket(PingPacket const* p);
  float currentPing() const;
  uint16_t port() const;
  int pendingPings() const;
  int lostPings() const;
  int successfulPings() const;
protected:
  void _update();
  void _sendPing();
  void _onData(rtc::AsyncSocket* socket);

  int _localPeerId;
  int _remotePeerId;
  uint16_t _port;
  std::map<int, int64_t> _pendingPings;
  int _lostPings;
  int _successfulPings;
  std::vector<int64_t> _pingHistory;
  int _currentPingId;
  rtc::AsyncSocket* _gameLobbySocket;
  rtc::SocketAddress _peerAddress;
  Timer _pingTimer;
  Timer _updateTimer;
};

} // namespace faf
