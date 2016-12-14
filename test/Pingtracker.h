#pragma once

#include <QtCore/QMap>
#include <QtCore/QTimer>

#include <QtNetwork/QUdpSocket>

namespace faf {

class Pingtracker
{
public:
  Pingtracker(int localPeerId,
              int remotePeerId,
              quint16 port,
              QUdpSocket& lobbySocket);
  void start();
  void onDatagram(QByteArray const& datagram);
  float currentPing() const;
  quint16 port() const;
  int pendingPings() const;
  int lostPings() const;
  int successfulPings() const;
protected:
  quint32 mLocalPeerId;
  quint32 mRemotePeerId;
  quint16 mPort;
  QMap<quint32, qint64> mPendingPings;
  int mLostPings;
  int mSuccessfulPings;
  QList<qint64> mPingHistory;
  quint32 mCurrentPingId;
  QUdpSocket& mLobbySocket;
  QTimer mPingTimer;
};

} // namespace faf
