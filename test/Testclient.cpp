#include "Testclient.h"
#include "ui_Testclient.h"

#include <set>

#include <QtCore/QVariant>
#include <QtCore/QTimer>
#include <QtNetwork/QTcpServer>

#include <json/json.h>

#include "logging.h"

namespace faf {

Testclient::Testclient(QWidget *parent) :
  QMainWindow(parent),
  mUi(new Ui::Testclient)
{
  mUi->setupUi(this);

  connectRpcMethods();

  mServerClient.connectToHost("sikoragmbh.my-router.de", 54321);

  connect(&mIceAdapterProcess,
          &QProcess::readyReadStandardError,
          this,
          &Testclient::onIceError);
  connect(&mIceAdapterProcess,
          &QProcess::readyReadStandardOutput,
          this,
          &Testclient::onIceOutput);
  connect(&mGpgClient,
          &GPGNetClient::onGPGNetMessage,
          this,
          &Testclient::onGPGNetMessageFromIceAdapter);
}

Testclient::~Testclient()
{
  mIceAdapterProcess.close();
  delete mUi;
}

void Testclient::on_pushButton_hostGame_clicked(bool startHosting)
{
  if (startHosting)
  {
    Json::Value params(Json::arrayValue);
    params.append(mPlayerLogin.toStdString());
    mServerClient.sendRequest("hostGame", params);
    mUi->listWidget_games->setEnabled(false);
  }
  else
  {
    FAF_LOG_ERROR << "implementme";
  }
}

void Testclient::on_listWidget_games_itemClicked(QListWidgetItem *item)
{
  if (item)
  {
    Json::Value params(Json::arrayValue);
    params.append(item->text().toStdString());
    params.append(item->data(Qt::UserRole).toInt());
    mServerClient.sendRequest("joinGame", params);
  }
}

void Testclient::connectRpcMethods()
{
  mServerClient.setRpcCallback("onLogin",
                               [this](Json::Value const& paramsArray,
                                      Json::Value & result,
                                      Json::Value & error,
                                      Socket* socket)
  {
    if (paramsArray.size() < 1)
    {
      error = "Need 1 parameter: playerId (int)";
      return;
    }
    mPlayerId = paramsArray[0].asInt();
    mPlayerLogin = QString("Player%1").arg(mPlayerId);
    mUi->label_myId->setText(mPlayerLogin);
    FAF_LOG_INFO << "logged in as " << mPlayerLogin.toStdString() << " (" << mPlayerId << ")";
    startIceAdapter();
  });
  mServerClient.setRpcCallback("sendToIceAdapter", [this](Json::Value const& paramsArray,
                               Json::Value & result,
                               Json::Value & error,
                               Socket* socket)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 1 parameter: method (string), params (array)";
      return;
    }
    mIceClient.sendRequest(paramsArray[0].asString(),
                           paramsArray[1]);
  });
  mServerClient.setRpcCallback("onGamelist", [this](Json::Value const& paramsArray,
                               Json::Value & result,
                               Json::Value & error,
                               Socket* socket)
  {
    if (paramsArray.size() != 1)
    {
      error = "Need 1 parameter: gameObject";
      return;
    }
    mUi->listWidget_games->clear();
    auto members = paramsArray[0].getMemberNames();
    for (auto it = members.begin(), end = members.end(); it != end; ++it)
    {
      auto item = new QListWidgetItem(QString::fromStdString(*it));
      item->setData(Qt::UserRole, paramsArray[0][*it].asInt());
      mUi->listWidget_games->addItem(item);
    }
  });

  for(auto event: {"onConnectionStateChanged",
                   "onGpgNetMessageReceived",
                   "onPeerStateChanged",
                   "onNeedSdp"})
  {
    mIceClient.setRpcCallback(event, [this](Json::Value const&,
                               Json::Value &,
                               Json::Value &,
                               Socket*)
    {
      updateStatus();
    });
  }

  mIceClient.setRpcCallback("onSdpMessage", [this](Json::Value const& paramsArray,
                            Json::Value &,
                            Json::Value &,
                            Socket*)
  {
    mServerClient.sendRequest("sendSdpMessage",
                              paramsArray);
  });
}

void Testclient::updateStatus()
{
  mIceClient.sendRequest("status",
                         Json::Value(Json::arrayValue),
                         nullptr,
                         [this] (Json::Value const& result,
                                 Json::Value const& error)
  {
    if (mGpgClient.state() != QAbstractSocket::ConnectedState)
    {
      FAF_LOG_DEBUG << "connecting GPGNetClient to port " << result["gpgnet"]["local_port"].asInt();
      mGpgClient.connectToHost("localhost",
                               result["gpgnet"]["local_port"].asInt());
    }

    mUi->label_gameConnected->setText(result["gpgnet"]["connected"].asBool() ? "true" : "false");
    mUi->label_gameState->setText(QString::fromStdString(result["gpgnet"]["game_state"].asString()));

    mUi->tableWidget_players->setRowCount(result["relays"].size());
    int row = 0;
    for(Json::Value const& relayInfo: result["relays"])
    {
      mUi->tableWidget_players->setItem(row, ColumnPlayer,
       new QTableWidgetItem(QString::fromStdString(relayInfo["remote_player_login"].asString())));
      mUi->tableWidget_players->setItem(row, ColumnState,
        new QTableWidgetItem(QString::fromStdString(relayInfo["ice_agent"]["state"].asString())));
      mUi->tableWidget_players->setItem(row, ColumnLocalCand,
        new QTableWidgetItem(QString::fromStdString(relayInfo["ice_agent"]["local_candidate"].asString())));
      mUi->tableWidget_players->setItem(row, ColumnRemoteCand,
        new QTableWidgetItem(QString::fromStdString(relayInfo["ice_agent"]["remote_candidate"].asString())));
      mUi->tableWidget_players->setItem(row, ColumnLocalSdp,
        new QTableWidgetItem(QString::fromStdString(relayInfo["local_sdp"]["remote_candidate"].asString())));
      mUi->tableWidget_players->setItem(row, ColumnRemoteSdp,
        new QTableWidgetItem(QString::fromStdString(relayInfo["remote_sdp"]["remote_candidate"].asString())));


      ++row;
    }

  });

}

void Testclient::startIceAdapter()
{
  int rpcPort = 0;
  {
    QTcpServer server;
    server.listen(QHostAddress::LocalHost, 0);
    rpcPort = server.serverPort();
  }
  QString exeName = "./faf-ice-adapter";
#if defined(__MINGW32__)
    exeName = "faf-ice-adapter.exe";
#endif
  mIceAdapterProcess.start("./faf-ice-adapter",
                           QStringList()
                           << "--id" << QString::number(mPlayerId)
                           << "--login" << mPlayerLogin
                           << "--rpc-port" << QString::number(rpcPort)
                           << "--gpgnet-port" << "0"
                           );
  mIceAdapterProcess.waitForReadyRead(1000);
  auto timer = new QTimer(this);
  timer->setSingleShot(true);
  connect(timer,
          &QTimer::timeout,
          [this, rpcPort]()
  {
    mIceClient.connectToHost("localhost", rpcPort);
    connect(&mIceClient,
            &QTcpSocket::connected,
            [this]()
    {
      updateStatus();
    });
  });
  timer->start(500);
}

void Testclient::onIceOutput()
{
  auto stdOut = mIceAdapterProcess.readAllStandardOutput();
  FAF_LOG_DEBUG << "ICE: " << stdOut.toStdString();
}

void Testclient::onIceError()
{
  auto stdErr = mIceAdapterProcess.readAllStandardError();
  FAF_LOG_ERROR << "ICE: " << stdErr.toStdString();
}

void Testclient::changeEvent(QEvent *e)
{
  QMainWindow::changeEvent(e);
  switch (e->type()) {
    case QEvent::LanguageChange:
      mUi->retranslateUi(this);
      break;
    default:
      break;
  }
}

void Testclient::onGPGNetMessageFromIceAdapter(GPGNetMessage const& msg)
{
  FAF_LOG_TRACE << "received GPGNetMessage from ICEAdapter: " << msg.toDebug();
  if (msg.header == "CreateLobby")
  {
    GPGNetMessage msg;
    msg.header = "GameState";
    msg.chunks.push_back("Lobby");
    auto binMsg = msg.toBinary();
    mGpgClient.write(binMsg.c_str(), binMsg.size());
  }
}

} // namespace faf

#include <QtWidgets/QApplication>
#include <QtCore/QtPlugin>

int main(int argc, char *argv[])
{
#if defined(__MINGW32__)
  Q_IMPORT_PLUGIN (QWindowsIntegrationPlugin);
#endif
  faf::logging_init();
  faf::logging_init_log_file("faf-ice-testclient.log");
  QApplication a(argc, argv);
  faf::Testclient c;
  c.show();
  return a.exec();
}
