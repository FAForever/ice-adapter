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
#include "Pingtracker.h"
#include "IceCandidate.h"

class QUdpSocket;

namespace faf {

namespace Ui {
class Testclient;
}

class Testclient : public QMainWindow
{
  Q_OBJECT
public:
  explicit Testclient(QWidget *parent = 0);
  ~Testclient();

protected slots:
  void on_pushButton_connect_clicked();
  void on_pushButton_disconnect_clicked();
  void on_pushButton_hostGame_clicked();
  void on_pushButton_leave_clicked();
  void on_listWidget_games_itemClicked(QListWidgetItem *item);
  void on_pushButton_savelogs_clicked();
  void on_pushButton_refresh_clicked();
  void on_pushButton_iceadapter_connect_clicked();
  void on_pushButton_iceadapter_start_clicked();
  void onPingStats(int peerId, float ping, int pendPings, int lostPings, int succPings);
  void on_tableWidget_peers_itemSelectionChanged();

protected:
  void connectRpcMethods();
  void updateStatus(std::function<void()> finishedCallback = std::function<void()>());
  void startGpgnetClient();
  void onIceOutput();
  void onIceError();
  void changeEvent(QEvent *e);
  void onGPGNetMessageFromIceAdapter(GPGNetMessage const& msg);
  void onLobbyReadyRead();
  void onIceMessage(int peerId, Json::Value const& msg);
  void updateGamelist(Json::Value const& gamelist);
  int selectedPeer() const;
  QTableWidgetItem* peerItem(int peerId, int column);
  QString peerLogin(int peerId) const;
  void updatePeerInfo();
  void addCandidate(IceCandidate const& c, bool kept);

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
  QSet<int> mPeersReady;
  QMap<int, IceCandidateVector> mOmittedCandidates;
  QMap<int, IceCandidateVector> mKeptCandidates;
  QMap<int, int> mPeerRow;
  Json::Value mCurrentStatus;
};


} // namespace faf
