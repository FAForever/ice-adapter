#pragma once

#include <memory>

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QTableWidget>
#include <QtNetwork/QUdpSocket>
#include <QtCore/QProcess>
#include <QtCore/QList>

#include "test/JsonRpcClient.h"
#include "test/GPGNetClient.h"
#include "IceAdapter.h"

class QUdpSocket;

namespace faf {

namespace Ui {
class Testclient;
}

class Testclient : public QMainWindow
{
  Q_OBJECT
  enum Column_t
  {
    ColumnPlayer,
    ColumnState,
    ColumnPing,
    ColumnConntime,
    ColumnLocalCand,
    ColumnRemoteCand,
    ColumnRemoteSdp
  };

public:
  explicit Testclient(QWidget *parent = 0);
  ~Testclient();

protected Q_SLOTS:
  void on_pushButton_hostGame_clicked();
  void on_pushButton_leave_clicked();
  void on_listWidget_games_itemClicked(QListWidgetItem *item);

protected:
  void connectRpcMethods();
  void updateStatus();
  void startIceAdapter();
  void startGpgnetClient();
  void onIceOutput();
  void onIceError();
  void changeEvent(QEvent *e);
  void onGPGNetMessageFromIceAdapter(GPGNetMessage const& msg);
  void createPinger(int remotePeerId);
  void onLobbyReadyRead();

private:
  Ui::Testclient *mUi;
  JsonRpcClient mServerClient;
  JsonRpcClient mIceClient;
  int mPlayerId;
  QString mPlayerLogin;
  int mGameId;
  QProcess mIceAdapterProcess;
  GPGNetClient mGpgClient;
  Json::Value mStatus;
  int mGpgnetPort;
  int mIcePort;
  QMap<int, quint16> mPeerPorts;
  QMap<quint16, int> mPortPeers;
  QMap<int, QUdpSocket*> mPeerPingers;
  QMap<int, QList<qint64>> mPingHistory;
  QSet<int> mPeersReady;
  quint32 mPingId;
  QMap<quint32, qint64> mPendingPings;
  QUdpSocket mLobbySocket;
};


} // namespace faf
