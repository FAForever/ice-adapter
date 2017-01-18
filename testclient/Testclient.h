#pragma once

#include <memory>
#include <functional>

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QGroupBox>
#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostInfo>
#include <QtCore/QProcess>
#include <QtCore/QList>

#include "JsonRpcQTcpSocket.h"
#include "GPGNetClient.h"
#include "PeerWidget.h"
#include "Pingtracker.h"

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

protected slots:
  void on_pushButton_hostGame_clicked();
  void on_pushButton_leave_clicked();
  void on_listWidget_games_itemClicked(QListWidgetItem *item);
  void on_pushButton_savelogs_clicked();
  void on_pushButton_refresh_clicked();
  void on_pushButton_iceadapter_connect_clicked();
  void on_pushButton_iceadapter_start_clicked();
  void onPingStats(int peerId, float ping, int pendPings, int lostPings, int succPings);

protected:
  void connectRpcMethods();
  void updateStatus(std::function<void()> finishedCallback = std::function<void()>());
  void startGpgnetClient();
  void onIceOutput();
  void onIceError();
  void changeEvent(QEvent *e);
  void onGPGNetMessageFromIceAdapter(GPGNetMessage const& msg);
  void onLobbyReadyRead();
  void onAddSdp(int peerId, std::string const& sdp);

private:
  Ui::Testclient *mUi;
  JsonRpcQTcpSocket mServerClient;
  JsonRpcQTcpSocket mIceClient;
  int mPlayerId;
  QString mPlayerLogin;
  int mGameId;
  QProcess mIceAdapterProcess;
  GPGNetClient mGpgClient;
  Json::Value mStatus;
  int mGpgnetPort;
  int mIcePort;
  QUdpSocket mLobbySocket;
  QMap<int, std::shared_ptr<Pingtracker>> mPeerIdPingtrackers;
  QMap<int, PeerWidget*> mPeerWidgets;
  QSet<int> mPeersReady;
  QMap<int, QStringList> mSdpCache;
};


} // namespace faf
