#include "Pingtracker.h"

#include <QtCore/QDateTime>

#include "logging.h"

namespace faf {

Pingtracker::Pingtracker(int localPeerId,
                         int remotePeerId,
                         quint16 port,
                         QUdpSocket& lobbySocket):
  mLocalPeerId(localPeerId),
  mRemotePeerId(remotePeerId),
  mPort(port),
  mLostPings(0),
  mSuccessfulPings(0),
  mLobbySocket(lobbySocket),
  mCurrentPingId(0)
{

}
void Pingtracker::start()
{
  if (mPingTimer.isActive())
  {
    return;
  }
  QObject::connect(&mPingTimer,
                   &QTimer::timeout,
                   [this]()
  {
    PingPacket p = {PingPacket::PING,
                    mLocalPeerId,
                    mRemotePeerId,
                    mCurrentPingId};
    QByteArray pingDatagram(reinterpret_cast<const char *>(&p),
                            sizeof(PingPacket));
    mLobbySocket.writeDatagram(pingDatagram,
                          QHostAddress::LocalHost,
                          mPort);
    auto currentTime = QDateTime::currentMSecsSinceEpoch();
    mPendingPings[mCurrentPingId] = currentTime;
    ++mCurrentPingId;

    int maxTime = 5000;
    while (mPendingPings.size() > 0)
    {
      auto it = mPendingPings.begin();
      if ((currentTime - it.value()) > 5000)
      {
        mPendingPings.erase(it);
        ++mLostPings;
      }
      else
      {
        break;
      }
    }
    if (mPendingPings.size() > 60)
    {
      FAF_LOG_WARN << "mPendingPings.size() > 60";
    }
  });
  mPingTimer.start(100);
}

void Pingtracker::onPingPacket(PingPacket const* p)
{
  if (p->type == PingPacket::PING)
  {
    PingPacket response(*p);
    response.type = PingPacket::PONG;
    mLobbySocket.writeDatagram(QByteArray(reinterpret_cast<const char *>(&response),
                                          sizeof(PingPacket)),
                               QHostAddress::LocalHost,
                               mPort);
  }
  else if (p->type == PingPacket::PONG)
  {
    if (!mPendingPings.contains(p->pingId))
    {
      FAF_LOG_ERROR << "!mPendingPings.contains(p->pingId)";
      return;
    }
    auto ping = QDateTime::currentMSecsSinceEpoch() - mPendingPings.value(p->pingId);
    mPendingPings.remove(p->pingId);
    ++mSuccessfulPings;
    mPingHistory.push_back(ping);
    while (mPingHistory.size() > 50)
    {
      mPingHistory.pop_front();
    }
    if (((mLostPings + mSuccessfulPings) % 10) == 0)
    {
      Q_EMIT pingStats(mRemotePeerId,
                       currentPing(),
                       mPendingPings.size(),
                       mLostPings,
                       mSuccessfulPings);
    }
  }
}

float Pingtracker::currentPing() const
{
  float result = 1e10;
  if (mPingHistory.size() > 0)
  {
    result = 0;
    for (int time : mPingHistory)
    {
      result += time;
    }
    result /= mPingHistory.size();
  }
  return result;
}

quint16 Pingtracker::port() const
{
  return mPort;
}

int Pingtracker::pendingPings() const
{
  return mPendingPings.size();
}

int Pingtracker::lostPings() const
{
  return mLostPings;
}

int Pingtracker::successfulPings() const
{
  return mSuccessfulPings;
}

} // namespace faf
