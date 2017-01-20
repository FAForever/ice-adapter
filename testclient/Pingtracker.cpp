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
  auto updateTimer = new QTimer(this);
  connect(updateTimer,
          &QTimer::timeout,
          this,
          &Pingtracker::update);
  updateTimer->start(1000);
}

void Pingtracker::start()
{
  FAF_LOG_DEBUG << "Pingtracker for peer " << mRemotePeerId << " started";
  if (mPingTimer.isActive())
  {
    return;
  }
  QObject::connect(&mPingTimer,
                   &QTimer::timeout,
                   [this]()
  {
    PingPacket p = {PingPacket::PING,
                    static_cast<quint32>(mLocalPeerId),
                    static_cast<quint32>(mRemotePeerId),
                    static_cast<quint32>(mCurrentPingId)};
    QByteArray pingDatagram(reinterpret_cast<const char *>(&p),
                            sizeof(PingPacket));
    mLobbySocket.writeDatagram(pingDatagram,
                          QHostAddress::LocalHost,
                          mPort);
    auto currentTime = QDateTime::currentMSecsSinceEpoch();
    mPendingPings[mCurrentPingId] = currentTime;
    ++mCurrentPingId;
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

void Pingtracker::update()
{
  while (mPendingPings.size() > 0)
  {
    auto it = mPendingPings.begin();
    auto currentTime = QDateTime::currentMSecsSinceEpoch();
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
  while (mPingHistory.size() > 50)
  {
    mPingHistory.pop_front();
  }
  Q_EMIT pingStats(mRemotePeerId,
                   currentPing(),
                   mPendingPings.size(),
                   mLostPings,
                   mSuccessfulPings);
}

} // namespace faf
