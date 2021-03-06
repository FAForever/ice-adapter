#pragma once

#include <array>

#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <QtCore/QObject>

#include <QtNetwork/QUdpSocket>

namespace faf {

struct PingPacket
{
  enum {
    PING = 0x01ce,
    PONG = 0x1ce0
  } type;
  quint32 senderId;
  quint32 answererId;
  quint32 pingId;
};

class Pingtracker : public QObject
{
  Q_OBJECT
public:
  Pingtracker(int localPeerId,
              int remotePeerId,
              quint16 port,
              QUdpSocket& lobbySocket);
  void start();
  void onPingPacket(PingPacket const* p);
  float currentPing() const;
  quint16 port() const;
  int pendingPings() const;
  int lostPings() const;
  int successfulPings() const;
signals:
  void pingStats(int peerId, float ping, int pendPings, int lostPings, int succPings);
protected:
  void update();
  int mLocalPeerId;
  int mRemotePeerId;
  quint16 mPort;
  QMap<int, qint64> mPendingPings;
  int mLostPings;
  int mSuccessfulPings;
  float _currentPing{0};
  int mCurrentPingId;
  QUdpSocket& mLobbySocket;
  QTimer mPingTimer;
};

} // namespace faf
