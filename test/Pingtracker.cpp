#include "Pingtracker.h"

#include <chrono>
#include <iostream>

#include <webrtc/rtc_base/thread.h>
#include <webrtc/api/datachannelinterface.h>

#include "logging.h"

namespace faf {

Pingtracker::Pingtracker(int localPeerId,
                         int remotePeerId,
                         rtc::AsyncSocket* gameLobbySocket,
                         rtc::SocketAddress const& peerAddress):
  _localPeerId(localPeerId),
  _remotePeerId(remotePeerId),
  _lostPings(0),
  _successfulPings(0),
  _currentPingId(0),
  _gameLobbySocket(gameLobbySocket),
  _peerAddress(peerAddress)
{
  _updateTimer.start(1000, std::bind(&Pingtracker::_update, this));
  _pingTimer.start(20, std::bind(&Pingtracker::_sendPing, this));
  std::cout << localPeerId << " -> " << remotePeerId << " PINGTRACKER STARTED " << std::endl;
}

void Pingtracker::onPingPacket(PingPacket const* p)
{
  if (p->type == PingPacket::PING)
  {
    if (p->answererId != _localPeerId)
    {
      FAF_LOG_ERROR << "PING receiver mismatch";
      return;
    }
    PingPacket response(*p);
    response.type = PingPacket::PONG;
    _gameLobbySocket->SendTo(reinterpret_cast<const char *>(&response),
                             sizeof(PingPacket),
                             _peerAddress);
  }
  else if (p->type == PingPacket::PONG)
  {
    if (p->senderId != _localPeerId)
    {
      FAF_LOG_ERROR << "PING sender mismatch";
      return;
    }
    auto pingIt = _pendingPings.find(p->pingId);
    if (pingIt == _pendingPings.end())
    {
      FAF_LOG_ERROR << "!_pendingPings.contains(p->pingId)";
      return;
    }
    auto now = std::chrono::duration_cast< std::chrono::milliseconds >(
                 std::chrono::system_clock::now().time_since_epoch()).count();
    auto ping = now - pingIt->second;
    _pendingPings.erase(pingIt);
    ++_successfulPings;
    _currentPing = (_currentPing * 9 + ping) / 10;
  }
}

float Pingtracker::currentPing() const
{
  return _currentPing;
}

uint16_t Pingtracker::port() const
{
  return _port;
}

int Pingtracker::pendingPings() const
{
  return _pendingPings.size();
}

int Pingtracker::lostPings() const
{
  return _lostPings;
}

int Pingtracker::successfulPings() const
{
  return _successfulPings;
}

void Pingtracker::_update()
{
  auto currentTime = std::chrono::duration_cast< std::chrono::milliseconds >(
                       std::chrono::system_clock::now().time_since_epoch()).count();
  while (_pendingPings.size() > 0)
  {
    auto it = _pendingPings.begin();
    if ((currentTime - it->second) > 5000)
    {
      _pendingPings.erase(it);
      ++_lostPings;
    }
    else
    {
      break;
    }
  }
  if (_pendingPings.size() > 60)
  {
    FAF_LOG_WARN << "mPendingPings.size() > 60";
  }
}

void Pingtracker::_sendPing()
{
  PingPacket p = {PingPacket::PING,
                  static_cast<uint32_t>(_localPeerId),
                  static_cast<uint32_t>(_remotePeerId),
                  static_cast<uint32_t>(_currentPingId)};
  _gameLobbySocket->SendTo(reinterpret_cast<const char *>(&p),
                           sizeof(PingPacket),
                           _peerAddress);
  auto now = std::chrono::duration_cast< std::chrono::milliseconds >(
                       std::chrono::system_clock::now().time_since_epoch()).count();
  _pendingPings[_currentPingId] = now;
  ++_currentPingId;
}

} // namespace faf
