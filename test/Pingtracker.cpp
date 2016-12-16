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
    QByteArray pingDatagram;
    pingDatagram.append("PING");
    pingDatagram.append(reinterpret_cast<const char *>(&mLocalPeerId), sizeof(mLocalPeerId));
    pingDatagram.append(reinterpret_cast<const char *>(&mRemotePeerId), sizeof(mRemotePeerId));
    pingDatagram.append(reinterpret_cast<const char *>(&mCurrentPingId), sizeof(mCurrentPingId));
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

void Pingtracker::onDatagram(QByteArray const& datagram)
{
  if (datagram.startsWith("PING"))
  {
    mLobbySocket.writeDatagram(QByteArray(datagram).replace(0,4,"PONG"),
                               QHostAddress::LocalHost,
                               mPort);
  }
  else if (datagram.startsWith("PONG"))
  {
    quint32 pingId = *reinterpret_cast<quint32 const*>(datagram.constData() +
                                                       4 +
                                                       sizeof(mLocalPeerId) +
                                                       sizeof(mRemotePeerId));
    if (!mPendingPings.contains(pingId))
    {
      FAF_LOG_ERROR << "!mPendingPings.contains(pingId)";
      return;
    }
    auto ping = QDateTime::currentMSecsSinceEpoch() - mPendingPings.value(pingId);
    mPendingPings.remove(pingId);
    ++mSuccessfulPings;
    mPingHistory.push_back(ping);
    while (mPingHistory.size() > 50)
    {
      mPingHistory.pop_front();
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
