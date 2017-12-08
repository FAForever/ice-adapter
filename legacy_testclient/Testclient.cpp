#include "Testclient.h"
#include "ui_Testclient.h"

#include <set>
#include <iostream>

#if defined(Q_OS_LINUX)
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#elif defined(Q_OS_WIN)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

#include <QtCore/QVariant>
#include <QtCore/QTimer>
#include <QtCore/QSettings>
#include <QtCore/QDateTime>
#include <QtCore/QTextStream>
#include <QtCore/QMessageAuthenticationCode>
#include <QtWidgets/QFileDialog>
#include <QtNetwork/QTcpServer>

#include <json/json.h>

namespace faf
{

enum
{
  PeerColumnLogin = 0,
  PeerColumnIceState,
  PeerColumnConn,
  PeerColumnPing
};

enum
{
  IceColumnType = 0,
  IceColumnProtocol,
  IceColumnAddress
};

static QTableWidget* logWidget = nullptr;

enum Column_t
{
  ColumnTime,
  ColumnSeverity,
  ColumnCode,
  ColumMessage
};

void myMessageHandler(QtMsgType type, const QMessageLogContext & ctx, const QString & msg)
{
  auto timeItem = new QTableWidgetItem(QDateTime::currentDateTime().toString());
  {
    int row = logWidget->rowCount();
    logWidget->insertRow(row);
    logWidget->setItem(row, ColumnTime, timeItem);
    timeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  }

  {
    auto sevItem = new QTableWidgetItem;
    switch (type)
    {
      case QtDebugMsg:
        sevItem->setText("debug");
        sevItem->setBackgroundColor(QColor::fromRgbF(0.8, 1, 0.8));
        break;
      case QtInfoMsg:
        sevItem->setText("info");
        sevItem->setBackgroundColor(QColor::fromRgbF(0.8, 0.8, 1));
        break;
      case QtWarningMsg:
        sevItem->setText("warning");
        sevItem->setBackgroundColor(QColor::fromRgbF(1, 1, 0.5));
        break;
      case QtSystemMsg:
      case QtFatalMsg:
        sevItem->setText("error");
        sevItem->setBackgroundColor(QColor::fromRgbF(1, 0.5, 0.5));
        break;

    }
    logWidget->setItem(timeItem->row(), ColumnSeverity, sevItem);
    sevItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  }

  {
    auto codeString = QString(ctx.function) + " (" +ctx.file + ":" + QString::number(ctx.line) + ")";
    auto codeItem = new QTableWidgetItem(ctx.function);
    logWidget->setItem(timeItem->row(), ColumnCode, codeItem);
    codeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  }
  {
    auto msgItem = new QTableWidgetItem(msg);
    logWidget->setItem(timeItem->row(), ColumMessage, msgItem);
    msgItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
  }
}


Testclient::Testclient(QWidget *parent) :
  QMainWindow(parent),
  mUi(new Ui::Testclient),
  mLobbyServerHost("fafsdp.erreich.bar"),
  mGpgnetPort(0),
  mIcePort(0),
  mKeepServerConnection(false)
{
  mUi->setupUi(this);

  logWidget = mUi->tableWidget_clientLog;

  qInstallMessageHandler(myMessageHandler);

  QSettings s;
  this->restoreGeometry(s.value("mainGeometry").toByteArray());
  this->restoreState(s.value("mainState").toByteArray());
  mUi->lineEdit_login->setText(s.value("login", "Player").toString());
  mUi->tableWidget_clientLog->horizontalHeader()->restoreState(s.value("clientLogHeaderState").toByteArray());
  mUi->tableWidget_iceLog->horizontalHeader()->restoreState(s.value("iceLogHeaderState").toByteArray());
  mUi->checkBox_c_udp_host->setChecked(s.value("c_udp_host", true).toBool());
  mUi->checkBox_c_udp_srflx->setChecked(s.value("c_udp_srflx", true).toBool());
  mUi->checkBox_c_udp_relay->setChecked(s.value("c_udp_relay", true).toBool());
  mUi->checkBox_c_tcp_host->setChecked(s.value("c_tcp_host", true).toBool());
  mUi->checkBox_c_tcp_srflx->setChecked(s.value("c_tcp_srflx", true).toBool());
  mUi->checkBox_c_udp_relay->setChecked(s.value("c_tcp_relay", true).toBool());

  mUi->pushButton_leave->setEnabled(false);
  mUi->pushButton_disconnect->setEnabled(false);
  mUi->groupBox_ice->setEnabled(false);
  mUi->groupBox_lobby->setEnabled(false);
  mUi->widget_details->setEnabled(false);

  connectRpcMethods();

  connect(&mIceAdapterProcess,
          &QProcess::readyReadStandardError,
          this,
          &Testclient::onIceError);
  connect(&mIceAdapterProcess,
          &QProcess::readyReadStandardOutput,
          this,
          &Testclient::onIceOutput);
  connect(&mIceAdapterProcess,
          &QProcess::readyReadStandardOutput,
          [this]()
  {
    if (mIcePort > 0 &&
        mIceClient.socket()->state() == QAbstractSocket::UnconnectedState)
    {
      mIceClient.socket()->connectToHost("localhost", mIcePort);
    }
  });
  connect(&mGpgClient,
          &GPGNetClient::onGPGNetMessage,
          this,
          &Testclient::onGPGNetMessageFromIceAdapter);
  connect(mIceClient.socket(),
          &QTcpSocket::connected,
          [this]()
  {
    qInfo() << "GPGnet client connected";
    mUi->groupBox_lobby->setEnabled(true);
    mUi->pushButton_iceadapter_connect->setEnabled(false);
    mUi->pushButton_iceadapter_start->setEnabled(false);
    updateStatus();
    sendIceServers();
  });
  connect(mIceClient.socket(),
          &QTcpSocket::disconnected,
          [this]()
  {
    qInfo() << "GPGnet client disconnected";
    if (mUi)
    {
      mUi->groupBox_lobby->setEnabled(false);
      mUi->pushButton_iceadapter_connect->setEnabled(true);
      mUi->pushButton_iceadapter_start->setEnabled(true);
    }
  });

  /*
  auto updateTimer = new QTimer(this);
  connect(updateTimer,
          &QTimer::timeout,
          [this]()
  {
    updateStatus();
  });
  updateTimer->start(1000);
  */

  mLobbySocket.bind();
  mUi->label_lobbyport->setText(QString::number(mLobbySocket.localPort()));
  connect(&mLobbySocket,
          &QUdpSocket::readyRead,
          this,
          &Testclient::onLobbyReadyRead);

  if (mIcePort == 0)
  {
    QTcpServer server;
    server.listen(QHostAddress::LocalHost, 0);
    mIcePort = server.serverPort();
    mUi->label_rpcport->setText(QString::number(mIcePort));
  }

  connect(mServerClient.socket(),
          &QTcpSocket::connected,
          [this] ()
  {
#if defined(Q_OS_LINUX)
    {
      int fd = mServerClient.socket()->socketDescriptor();
      int keepalive = 1;
      int keepcnt = 1;
      int keepidle = 3;
      int keepintvl = 5;
      setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
      setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
      setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
      setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    }
#elif defined(Q_OS_WIN)
    {
      int fd = mServerClient.socket()->socketDescriptor();
      DWORD dwBytesRet=0;
      struct tcp_keepalive alive;
      alive.onoff = TRUE;
      alive.keepalivetime = 1000;
      alive.keepaliveinterval = 1000;

      if (WSAIoctl(fd,
                   SIO_KEEPALIVE_VALS,
                   &alive,
                   sizeof(alive),
                   NULL,
                   0,
                   &dwBytesRet,
                   NULL,
                   NULL) == SOCKET_ERROR)
      {
        qCritical() << "WSAIotcl(SIO_KEEPALIVE_VALS) failed: " << WSAGetLastError();
      }
      //int keepalive = 1;
      //setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepalive), sizeof(keepalive));
    }
#endif
    mUi->label_connected->setText("True");
    if (!mUi->pushButton_disconnect->isEnabled())
    {
      mUi->pushButton_disconnect->setEnabled(true);
      mUi->pushButton_connect->setEnabled(false);
      mUi->lineEdit_login->setReadOnly(true);
      mUi->groupBox_ice->setEnabled(true);
      Json::Value params(Json::arrayValue);
      params.append(mUi->lineEdit_login->text().toStdString());
      mServerClient.sendRequest("login", params, nullptr,
                                [this](Json::Value const& result, Json::Value const& error)
      {
        mPlayerId = result["id"].asInt();
        mPlayerLogin = QString::fromStdString(result["login"].asString());
        mUi->label_playerid->setText(QString::number(mPlayerId));
        mUi->label_playerlogin->setText(mPlayerLogin);
        qInfo() << "logged in as " << mPlayerLogin << " (" << mPlayerId << ")";
        mUi->groupBox_ice->setEnabled(true);
        updateGamelist(result["games"]);
      });
    }
    else
    {
      Json::Value params(Json::arrayValue);
      params.append(mPlayerId);
      mServerClient.sendRequest("reconnect", params, nullptr,
                                [this](Json::Value const& result, Json::Value const& error)
      {
        if (error.isNull())
        {
          qInfo() << "reconnected";
        }
      });
    }

  });

  connect(mServerClient.socket(),
          &QTcpSocket::disconnected,
          [this] ()
  {
    if (mUi)
    {
      mUi->label_connected->setText("False");
      if (!mKeepServerConnection)
      {
        mUi->pushButton_disconnect->setEnabled(false);
        mUi->pushButton_connect->setEnabled(true);
        mUi->lineEdit_login->setReadOnly(false);
        mUi->groupBox_ice->setEnabled(false);
        mUi->groupBox_lobby->setEnabled(false);
      }
    }
  });


  auto reconnectTimer = new QTimer(this);
  connect(reconnectTimer,
          &QTimer::timeout,
          [this]()
  {
    if (mKeepServerConnection &&
        mServerClient.socket()->state() != QAbstractSocket::ConnectedState)
    {
      mServerClient.socket()->connectToHost(mLobbyServerHost, 54321);
    }
  });
  reconnectTimer->start(1000);
}

Testclient::~Testclient()
{
  mIceAdapterProcess.close();

  QSettings s;
  s.setValue("iceLogHeaderState", mUi->tableWidget_iceLog->horizontalHeader()->saveState());
  s.setValue("clientLogHeaderState", mUi->tableWidget_clientLog->horizontalHeader()->saveState());
  s.setValue("mainState", this->saveState());
  s.setValue("mainGeometry", this->saveGeometry());
  s.setValue("login", mUi->lineEdit_login->text());
  s.setValue("c_udp_host", mUi->checkBox_c_udp_host->isChecked());
  s.setValue("c_udp_srflx", mUi->checkBox_c_udp_srflx->isChecked());
  s.setValue("c_udp_relay", mUi->checkBox_c_udp_relay->isChecked());
  s.setValue("c_tcp_host", mUi->checkBox_c_tcp_host->isChecked());
  s.setValue("c_tcp_srflx", mUi->checkBox_c_tcp_srflx->isChecked());
  s.setValue("c_tcp_relay", mUi->checkBox_c_tcp_relay->isChecked());

  delete mUi;
  mUi = nullptr;
}

void Testclient::on_pushButton_connect_clicked()
{
  mKeepServerConnection = true;
  mServerClient.socket()->connectToHost(mLobbyServerHost, 54321);
}

void Testclient::on_pushButton_disconnect_clicked()
{
  mKeepServerConnection = false;
  mServerClient.socket()->disconnectFromHost();
}

void Testclient::on_pushButton_hostGame_clicked()
{
  if (mIceClient.socket()->state() == QAbstractSocket::UnconnectedState)
  {
    return;
  }
  startGpgnetClient();
  mServerClient.sendRequest("hostGame", Json::Value(Json::arrayValue));
  mUi->listWidget_games->setEnabled(false);
  mUi->pushButton_leave->setEnabled(true);
  mUi->pushButton_hostGame->setEnabled(false);
  mGameId = mPlayerId;
}

void Testclient::on_pushButton_leave_clicked()
{
  mGpgClient.disconnectFromHost();
  mServerClient.sendRequest("leaveGame", Json::Value(Json::arrayValue));
  mUi->listWidget_games->setEnabled(true);
  mUi->pushButton_hostGame->setEnabled(true);
  mUi->tableWidget_peers->setRowCount(0);
  mPeerRow.clear();
  mGameId = -1;
  mPeerIdPingtrackers.clear();
  mPeersReady.clear();
  mOmittedCandidates.clear();
  mKeptCandidates.clear();
  updatePeerInfo();
}

void Testclient::on_listWidget_games_itemClicked(QListWidgetItem *item)
{
  if (item)
  {
    startGpgnetClient();
    Json::Value params(Json::arrayValue);
    params.append(item->data(Qt::UserRole).toInt());
    mGameId = item->data(Qt::UserRole).toInt();
    mServerClient.sendRequest("joinGame", params);
    mUi->listWidget_games->setEnabled(false);
    mUi->pushButton_leave->setEnabled(true);
    mUi->pushButton_hostGame->setEnabled(false);
  }
}

void saveTablewidgetToFile(QTableWidget* table, QIODevice* file)
{
  QTextStream s(file);
  for (int row = 0; row < table->rowCount(); ++row)
  {
    for (int col = 0; col < table->columnCount(); ++col)
    {
      auto item = table->item(row, col);
      if (item)
      {
        s << item->text() << "\t";
      }
    }
    s << "\n";
  }
}

void Testclient::on_pushButton_savelogs_clicked()
{
  QString iceLogFilename = QFileDialog::getSaveFileName(this,
                                                        "Save ICE log",
                                                        "ice.log",
                                                        "Logs (*.log)");
  if (!iceLogFilename.isEmpty())
  {
    QFile f(iceLogFilename);
    if (f.open(QIODevice::WriteOnly))
    {
      saveTablewidgetToFile(mUi->tableWidget_iceLog, &f);
    }
  }
  QString clientLogFilename = QFileDialog::getSaveFileName(this,
                                                           "Save Client log",
                                                           "client.log",
                                                           "Logs (*.log)");
  if (!clientLogFilename.isEmpty())
  {
    QFile f(clientLogFilename);
    if (f.open(QIODevice::WriteOnly))
    {
      saveTablewidgetToFile(mUi->tableWidget_clientLog, &f);
    }
  }
}

void Testclient::on_pushButton_iceadapter_connect_clicked()
{
    if (mIcePort > 0 &&
        mIceClient.socket()->state() == QAbstractSocket::UnconnectedState)
    {
      mIceClient.socket()->connectToHost("localhost", mIcePort);
    }
}

void Testclient::on_pushButton_iceadapter_start_clicked()
{
  auto args = QStringList()
              << "--id" << QString::number(mPlayerId)
              << "--login" << mPlayerLogin
              << "--rpc-port" << QString::number(mIcePort)
              << "--gpgnet-port" << "0"
              << "--lobby-port" << QString::number(mLobbySocket.localPort());
  qInfo() << "going to start ice-adapter with arguments " << args.join(" ");
  mIceAdapterProcess.start("faf-ice-adapter", args);
}

void Testclient::onPingStats(int peerId, float ping, int pendPings, int lostPings, int succPings)
{
  if (peerItem(peerId, PeerColumnPing))
  {
    peerItem(peerId, PeerColumnPing)->setText(QString("%1 ms").arg(ping));
  }
  if (selectedPeer() == peerId)
  {
    mUi->label_det_succ_pings->setText(QString::number(succPings));
    mUi->label_det_pend_pings->setText(QString::number(pendPings));
    mUi->label_det_lost_pings->setText(QString::number(lostPings));
  }
}

void Testclient::on_tableWidget_peers_itemSelectionChanged()
{
  updatePeerInfo();
}

void Testclient::connectRpcMethods()
{
  mServerClient.setRpcCallback("onLogin",
                               [this](Json::Value const& paramsArray,
                                      Json::Value & result,
                                      Json::Value & error,
                                      Socket* socket)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 2 parameters: playerId (int), playerLogin (string)";
      return;
    }
    mPlayerId = paramsArray[0].asInt();
    mPlayerLogin = QString::fromStdString(paramsArray[1].asString());
    mUi->label_playerid->setText(QString::number(mPlayerId));
    mUi->label_playerlogin->setText(mPlayerLogin);
    qInfo() << "logged in as " << mPlayerLogin << " (" << mPlayerId << ")";
  });
  mServerClient.setRpcCallback("onHostLeft",
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
    qInfo() << "onHostLeft " << paramsArray[0].asInt();
    if (mGameId == paramsArray[0].asInt())
    {
      on_pushButton_leave_clicked();
    }
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
    if (paramsArray[0].asString() == "iceMsg")
    {
      if (paramsArray[1].size() != 2)
      {
        error = "Need 2 parameters: peer (int), msg (object)";
        return;
      }
      onIceMessage(paramsArray[1][0].asInt(),
                   paramsArray[1][1]);
    }
    else
    {
      mIceClient.sendRequest(paramsArray[0].asString(),
                             paramsArray[1]);
    }
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
    updateGamelist(paramsArray[0]);
  });

  for(auto event: {"onConnectionStateChanged",
                   "onGpgNetMessageReceived",
                   "onIceConnectionStateChanged"
                  })
  {
    mIceClient.setRpcCallback(event, [this](Json::Value const&,
                               Json::Value &,
                               Json::Value &,
                               Socket*)
    {
      updateStatus();
    });
  }

  mIceClient.setRpcCallback("onIceMsg", [this](Json::Value const& paramsArray,
                            Json::Value &,
                            Json::Value &,
                            Socket*)
  {
    mServerClient.sendRequest("sendIceMsg",
                              paramsArray);
    updateStatus();
  });

  mIceClient.setRpcCallback("onConnected", [this](Json::Value const& paramsArray,
                            Json::Value & result,
                            Json::Value & error,
                            Socket*)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 3 parameters.";
      return;
    }
    int peerId = paramsArray[1].asInt();
    bool connected = paramsArray[2].asBool();
    if (connected)
    {
      mPeersReady.insert(peerId);
      if (mPeerIdPingtrackers.contains(peerId))
      {
        mPeerIdPingtrackers.value(peerId)->start();
      }
      if (peerItem(peerId, PeerColumnConn))
      {
        peerItem(peerId, PeerColumnConn)->setBackgroundColor(Qt::green);
      }
    }
    qDebug() << "onConnected for peer " << peerId<< ": " << connected;
    updateStatus();
  });
}

void Testclient::updateStatus(std::function<void()> finishedCallback)
{
  if (mIceClient.socket()->state() != QAbstractSocket::ConnectedState)
  {
    return;
  }
  mIceClient.sendRequest("status",
                         Json::Value(Json::arrayValue),
                         nullptr,
                         [this, finishedCallback] (Json::Value const& result,
                                                   Json::Value const& error)
  {
    mCurrentStatus = result;
    if (mGpgnetPort == 0)
    {
      mGpgnetPort = result["gpgnet"]["local_port"].asInt();
    }

    mUi->label_gameConnected->setText(result["gpgnet"]["connected"].asBool() ? "true" : "false");
    mUi->label_gameState->setText(QString::fromStdString(result["gpgnet"]["game_state"].asString()));

    for(Json::Value const& relayInfo: result["relays"])
    {
      auto id = relayInfo["remote_player_id"].asInt();
      if (!mPeerRow.contains(id))
      {
        int row = mUi->tableWidget_peers->rowCount();
        mUi->tableWidget_peers->setRowCount(row + 1);
        mPeerRow[id] = row;
        mUi->tableWidget_peers->setItem(row, PeerColumnLogin, new QTableWidgetItem);
        mUi->tableWidget_peers->setItem(row, PeerColumnIceState, new QTableWidgetItem);
        mUi->tableWidget_peers->setItem(row, PeerColumnConn, new QTableWidgetItem);
        mUi->tableWidget_peers->setItem(row, PeerColumnPing, new QTableWidgetItem);
      }
      peerItem(id, PeerColumnLogin)->setText(QString::fromStdString(relayInfo["remote_player_login"].asString()));
      peerItem(id, PeerColumnIceState)->setText(QString::fromStdString(relayInfo["ice_agent"]["state"].asString()));
      peerItem(id, PeerColumnConn)->setText(QString("%1 s").arg(relayInfo["ice_agent"]["time_to_connected"].asDouble()));
      if (relayInfo["ice_agent"]["time_to_connected"].asDouble() > 0)
      {
        peerItem(id, PeerColumnConn)->setBackgroundColor(Qt::green);
      }
    }

    if (mUi->tableWidget_peers->rowCount() > 0)
    {
      if (selectedPeer() < 0)
      {
        mUi->tableWidget_peers->selectRow(0);
      }
      else
      {
        updatePeerInfo();
      }
    }

    if (finishedCallback)
    {
      finishedCallback();
    }
  });
}

void Testclient::startGpgnetClient()
{
  updateStatus([this]()
  {
    if (mGpgnetPort == 0)
    {
      qCritical() << "mGpgnetPort == 0";
    }
    else
    {
      mGpgClient.connectToHost("localhost",
                               mGpgnetPort);
    }
  });
}

void showIceOutput(QByteArray const& output,
                   QTableWidget* widget,
                   bool error)
{
  for(auto const& line: output.split('\n'))
  {
    if (!line.trimmed().isEmpty())
    {
      auto msg = QString(line.trimmed());

      /*
      if (msg.contains("<trace>"))
      {
        continue;
      }
      */

      auto item = new QTableWidgetItem(msg);
      int row = widget->rowCount();
      widget->insertRow(row);
      widget->setItem(row, 0, item);
      item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
      if (error)
      {
        if (msg.contains("warning"))
        {
          item->setBackgroundColor(QColor::fromRgbF(1, 1, 0.5));
        }
        else
        {
          item->setBackgroundColor(QColor::fromRgbF(1, 0.5, 0.5));
        }
      }
      else if (msg.contains("debug"))
      {
        item->setBackgroundColor(QColor::fromRgbF(0.8, 1, 0.8));
      }
      else if (msg.contains("info"))
      {
        item->setBackgroundColor(QColor::fromRgbF(0.8, 0.8, 1));
      }
    }
  }
}

void Testclient::onIceOutput()
{
  showIceOutput(mIceAdapterProcess.readAllStandardOutput(),
                mUi->tableWidget_iceLog,
                false);
}

void Testclient::onIceError()
{
  showIceOutput(mIceAdapterProcess.readAllStandardError(),
                mUi->tableWidget_iceLog,
                true);
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
  qInfo() << "received GPGNetMessage from ICEAdapter: " << QString::fromStdString(msg.toDebug());

  auto extractPort = [](std::string const& peerAddress)
  {
    auto colonPos = peerAddress.find(":");
    if (colonPos != std::string::npos)
    {
      return std::stoi(peerAddress.substr(colonPos + 1));
    }
    qCritical() << "error parsing peerAddress " << QString::fromStdString(peerAddress);
    return 0;
  };

  if (msg.header == "CreateLobby")
  {
    GPGNetMessage msg;
    msg.header = "GameState";
    msg.chunks.push_back("Lobby");
    auto binMsg = msg.toBinary();
    mGpgClient.write(binMsg.c_str(), binMsg.size());
  }
  else if (msg.header == "ConnectToPeer" ||
           msg.header == "JoinGame")
  {
    if (msg.chunks.size() != 3)
    {
      qCritical() << "ConnectToPeer/JoinGame expected 3 chunks";
      return;
    }
    auto peerId = msg.chunks.at(2).asInt();
    auto peerPort = extractPort(msg.chunks.at(0).asString());
    auto pingtracker = std::make_shared<Pingtracker>(mPlayerId,
                                                     peerId,
                                                     peerPort,
                                                     mLobbySocket);
    mPeerIdPingtrackers[peerId] = pingtracker;
    if (mPeersReady.contains(peerId))
    {
      pingtracker->start();
    }
    connect(pingtracker.get(),
            &Pingtracker::pingStats,
            this,
            &Testclient::onPingStats);
    qDebug() << "Pingtracker for peer " << peerId << " port " << peerPort << " created";
  }
  else if (msg.header == "DisconnectFromPeer")
  {
    if (msg.chunks.size() != 1)
    {
      qCritical() << "DisconnectFromPeer expected 1 chunks";
      return;
    }
    int peerId = msg.chunks.at(0).asInt();
    mPeerIdPingtrackers.remove(peerId);
    mPeersReady.remove(peerId);
    mOmittedCandidates.remove(peerId);
    mKeptCandidates.remove(peerId);

    mUi->tableWidget_peers->setRowCount(0);
    mPeerRow.clear();

    updateStatus();
  }
}

void Testclient::onLobbyReadyRead()
{
  while (mLobbySocket.hasPendingDatagrams())
  {
    QByteArray datagram;
    datagram.resize(mLobbySocket.pendingDatagramSize());
    QHostAddress sender;
    quint16 senderPort;
    mLobbySocket.readDatagram(datagram.data(),
                              datagram.size(),
                              &sender,
                              &senderPort);
    if (datagram.size() != sizeof(PingPacket))
    {
      qCritical() << "wrong PING datagram size: " << datagram.size();
    }
    else
    {
      auto p = reinterpret_cast<PingPacket const*>(datagram.constData());
      quint32 peerId = p->type == PingPacket::PING ? p->senderId : p->answererId;
      if (!mPeerIdPingtrackers.contains(peerId))
      {
        qCritical() << "!mPeerIdPingtrackers.contains(peerId): " << peerId;
        continue;
      }
      mPeerIdPingtrackers[peerId]->onPingPacket(p);
    }
  }
}


void Testclient::onIceMessage(int peerId, Json::Value const& msg)
{
  if (msg.isMember("type") && msg["type"].asString() == "candidate")
  {
    bool omitCandidate = false;
    auto candidateString = QString::fromStdString(msg["candidate"]["candidate"].asString());
    auto candidate = IceCandidate::fromString(candidateString);
    if (candidateString.startsWith("candidate"))
    {
      if (!mUi->checkBox_c_udp_host->isChecked() &&
          candidate.protocol == "udp" &&
          candidate.type == "host")
      {
        omitCandidate = true;
      }
      if (!mUi->checkBox_c_udp_srflx->isChecked() &&
          candidate.protocol == "udp" &&
          candidate.type == "srflx")
      {
        omitCandidate = true;
      }
      if (!mUi->checkBox_c_udp_relay->isChecked() &&
          candidate.protocol == "udp" &&
          candidate.type == "relay")
      {
        omitCandidate = true;
      }
      if (!mUi->checkBox_c_tcp_host->isChecked() &&
          candidate.protocol == "tcp" &&
          candidate.type == "host")
      {
        omitCandidate = true;
      }
      if (!mUi->checkBox_c_tcp_srflx->isChecked() &&
          candidate.protocol == "tcp" &&
          candidate.type == "srflx")
      {
        omitCandidate = true;
      }
      if (!mUi->checkBox_c_tcp_relay->isChecked() &&
          candidate.protocol == "tcp" &&
          candidate.type == "relay")
      {
        omitCandidate = true;
      }
    }
    if (omitCandidate)
    {
      mOmittedCandidates[peerId].push_back(candidate);
      return;
    }
    else
    {
      mKeptCandidates[peerId].push_back(candidate);
    }
  }
  Json::Value params(Json::arrayValue);
  params.append(peerId);
  params.append(msg);
  mIceClient.sendRequest("iceMsg",
                         params);
}

void Testclient::updateGamelist(Json::Value const& gamelist)
{
  mUi->listWidget_games->clear();
  auto members = gamelist.getMemberNames();
  for (auto it = members.begin(), end = members.end(); it != end; ++it)
  {
    auto item = new QListWidgetItem(QString::fromStdString(*it));
    item->setData(Qt::UserRole, gamelist[*it].asInt());
    mUi->listWidget_games->addItem(item);
  }
}

int Testclient::selectedPeer() const
{
  auto list = mUi->tableWidget_peers->selectedItems();
  if (list.empty())
  {
    return -1;
  }
  return mPeerRow.key(list.first()->row());
}

QTableWidgetItem* Testclient::peerItem(int peerId, int column)
{
  if (!mPeerRow.contains(peerId))
  {
    return nullptr;
  }
  return mUi->tableWidget_peers->item(mPeerRow.value(peerId), column);
}

QString Testclient::peerLogin(int peerId) const
{
  for(Json::Value const& relayInfo: mCurrentStatus["relays"])
  {
    if (peerId == relayInfo["remote_player_id"].asInt())
    {
      return QString::fromStdString(relayInfo["remote_player_login"].asString());
    }
  }
  return QString();
}

void Testclient::updatePeerInfo()
{
  int peerId = selectedPeer();
  if (peerId >= 0)
  {
    mUi->widget_details->setEnabled(true);
    for(Json::Value const& relayInfo: mCurrentStatus["relays"])
    {
      auto id = relayInfo["remote_player_id"].asInt();
      if (id == selectedPeer())
      {
        mUi->label_det_id->setText(QString::number(id));
        mUi->label_det_loccand->setText(QString::fromStdString(relayInfo["ice_agent"]["loc_cand_addr"].asString()));
        mUi->label_det_remcand->setText(QString::fromStdString(relayInfo["ice_agent"]["rem_cand_addr"].asString()));
      }
    }
    mUi->tableWidget_det_cands->setRowCount(0);

    for(IceCandidate const& cand : mKeptCandidates[peerId])
    {
      addCandidate(cand, true);
    }
    for(IceCandidate const& cand : mOmittedCandidates[peerId])
    {
      addCandidate(cand, false);
    }
  }
  else
  {
    mUi->widget_details->setEnabled(false);
  }
}

void Testclient::addCandidate(IceCandidate const& c, bool kept)
{
  int row = mUi->tableWidget_det_cands->rowCount();
  mUi->tableWidget_det_cands->setRowCount(row + 1);
  mUi->tableWidget_det_cands->setItem(row, IceColumnType, new QTableWidgetItem(c.type));
  mUi->tableWidget_det_cands->setItem(row, IceColumnProtocol, new QTableWidgetItem(c.protocol));
  mUi->tableWidget_det_cands->setItem(row, IceColumnAddress, new QTableWidgetItem(c.address + ":" + c.port));
  if (!kept)
  {
    mUi->tableWidget_det_cands->item(row, IceColumnType)->setBackgroundColor(Qt::red);
  }
}

void Testclient::sendIceServers()
{
  QString coturnHost = "vmrbg145.informatik.tu-muenchen.de";
  QByteArray coturnKey = "banana";

  auto message = QString("%1:%2@faforever.com")
                 .arg(QDateTime::currentDateTime().addDays(1).toSecsSinceEpoch())
                 .arg(mUi->label_playerlogin->text());

  QMessageAuthenticationCode coturnCrendentials(QCryptographicHash::Sha1);
  coturnCrendentials.setKey(coturnKey);
  coturnCrendentials.addData(message.toLocal8Bit());

  if (mIceClient.socket()->state() != QAbstractSocket::ConnectedState)
  {
    return;
  }
  Json::Value iceServers(Json::arrayValue);
  Json::Value fafServerStun;
  Json::Value fafServerStunUrls(Json::arrayValue);
  fafServerStunUrls.append(QString("stun:%1").arg(coturnHost).toStdString());
  fafServerStun["urls"] = fafServerStunUrls;
  iceServers.append(fafServerStun);
  Json::Value fafServerTurn;
  Json::Value fafServerTurnUrls(Json::arrayValue);
  fafServerTurnUrls.append(QString("turn:%1?transport=tcp").arg(coturnHost).toStdString());
  fafServerTurnUrls.append(QString("turn:%1?transport=tcp").arg(coturnHost).toStdString());
  fafServerTurn["urls"] = fafServerTurnUrls;
  fafServerTurn["credential"] = coturnCrendentials.result().toBase64().toStdString();
  fafServerTurn["username"] = message.toStdString();
  iceServers.append(fafServerTurn);
  Json::Value setIceServersParams(Json::arrayValue);
  setIceServersParams.append(iceServers);

  mIceClient.sendRequest("setIceServers",
                         setIceServersParams);
}

} // namespace faf

#include <QtWidgets/QApplication>
#include <QtCore/QtPlugin>

int main(int argc, char *argv[])
{
//#if defined(Q_OS_WIN)
//  Q_IMPORT_PLUGIN (QWindowsIntegrationPlugin);
//#endif
  QApplication a(argc, argv);
  a.setApplicationName("faf-ice-testclient");
  a.setOrganizationName("FAForever");
  faf::Testclient c;
  c.show();
  return a.exec();
}
