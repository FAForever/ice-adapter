#include "Testclient.h"
#include "ui_Testclient.h"

#include <set>
#include <iostream>

#include <QtCore/QVariant>
#include <QtCore/QTimer>
#include <QtCore/QSettings>
#include <QtCore/QDateTime>
#include <QtCore/QTextStream>
#include <QtWidgets/QFileDialog>
#include <QtNetwork/QTcpServer>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/basic_sink_backend.hpp>

#include <json/json.h>

#include "logging.h"

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

class LogSink : public boost::log::sinks::basic_formatted_sink_backend<char, boost::log::sinks::synchronized_feeding>
{
public:
  enum Column_t
  {
    ColumnTime,
    ColumnSeverity,
    ColumnCode,
    ColumMessage
  };

    LogSink(QTableWidget* widget)
      : mWidget(widget)
    {
    }

    void consume(const boost::log::record_view& rec, const string_type& fstring)
    {
      auto time = boost::log::extract<boost::posix_time::ptime>("TimeStamp", rec).get();
      auto timeItem = new QTableWidgetItem(QString::fromStdString(boost::posix_time::to_simple_string(time)));
      {
        int row = mWidget->rowCount();
        mWidget->insertRow(row);
        mWidget->setItem(row, ColumnTime, timeItem);
        timeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      }

      {
        std::ostringstream os;
        os << rec[boost::log::trivial::severity];
        auto sevItem = new QTableWidgetItem(QString::fromStdString(os.str()));
        mWidget->setItem(timeItem->row(), ColumnSeverity, sevItem);
        sevItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        if (sevItem->text() == "debug")
        {
          sevItem->setBackgroundColor(QColor::fromRgbF(0.8, 1, 0.8));
        }
        else if(sevItem->text() == "info")
        {
          sevItem->setBackgroundColor(QColor::fromRgbF(0.8, 0.8, 1));
        }
        else if(sevItem->text() == "warning")
        {
          sevItem->setBackgroundColor(QColor::fromRgbF(1, 1, 0.5));
        }
        else if(sevItem->text() == "error")
        {
          sevItem->setBackgroundColor(QColor::fromRgbF(1, 0.5, 0.5));
        }

      }

      {
        auto codeString = boost::log::extract<std::string>("Function", rec).get() +
                          "(" + boost::log::extract<std::string>("File", rec).get() + ":" +
                          std::to_string(boost::log::extract<int>("Line", rec).get()) + ")";
        auto codeItem = new QTableWidgetItem(QString::fromStdString(codeString));
        mWidget->setItem(timeItem->row(), ColumnCode, codeItem);
        codeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      }
      {
        auto msgItem = new QTableWidgetItem(QString::fromStdString(fstring));
        mWidget->setItem(timeItem->row(), ColumMessage, msgItem);
        msgItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
      }
    }

private:
    QTableWidget* mWidget;
};

Testclient::Testclient(QWidget *parent) :
  QMainWindow(parent),
  mUi(new Ui::Testclient),
  mGpgnetPort(0),
  mIcePort(0)
{
  mUi->setupUi(this);

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

  typedef boost::log::sinks::synchronous_sink<LogSink> sink_t;
  boost::shared_ptr<LogSink> backend(new LogSink(mUi->tableWidget_clientLog));
  boost::shared_ptr<sink_t> sink(new sink_t(backend));
  boost::log::core::get()->add_sink(sink);

  connectRpcMethods();

  //mServerClient.socket()->connectToHost("fafsdp.erreich.bar", 54321);
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
    FAF_LOG_INFO << "GPGnet client connected";
    mUi->groupBox_lobby->setEnabled(true);
    mUi->pushButton_iceadapter_connect->setEnabled(false);
    mUi->pushButton_iceadapter_start->setEnabled(false);
    updateStatus();
  });
  connect(mIceClient.socket(),
          &QTcpSocket::disconnected,
          [this]()
  {
    FAF_LOG_INFO << "GPGnet client disconnected";
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
      FAF_LOG_INFO << "logged in as " << mPlayerLogin.toStdString() << " (" << mPlayerId << ")";
      mUi->groupBox_ice->setEnabled(true);
      updateGamelist(result["games"]);
    });
  });

  connect(mServerClient.socket(),
          &QTcpSocket::disconnected,
          [this] ()
  {
    if (mUi)
    {
      mUi->pushButton_disconnect->setEnabled(false);
      mUi->pushButton_connect->setEnabled(true);
      mUi->lineEdit_login->setReadOnly(false);
      mUi->groupBox_ice->setEnabled(false);
      mUi->groupBox_lobby->setEnabled(false);
    }
  });
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
  mServerClient.socket()->connectToHost("fafsdp.erreich.bar", 54321);
}

void Testclient::on_pushButton_disconnect_clicked()
{
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

void Testclient::on_pushButton_refresh_clicked()
{
  updateStatus();
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
  QString adapterSrc = QDir::toNativeSeparators(QDir(qApp->applicationDirPath()).absoluteFilePath("faf-ice-adapter.js"));
  if (!QFile::exists(adapterSrc))
  {
    FAF_LOG_INFO << "ice-adapter app " << adapterSrc.toStdString() << " does not exist, using index.js";
    adapterSrc = QDir::toNativeSeparators(QDir(qApp->applicationDirPath()).absoluteFilePath("index.js"));
    if (!QFile::exists(adapterSrc))
    {
      FAF_LOG_INFO << "ice-adapter app " << adapterSrc.toStdString() << " does not exist, using /home/sws/projPriv/2017/faf-ice-adapter/index.js";
      adapterSrc = "/home/sws/projPriv/2017/faf-ice-adapter/index.js";
    }
  }
  auto args = QStringList()
              << adapterSrc
              << "--id" << QString::number(mPlayerId)
              << "--login" << mPlayerLogin
              << "--rpc_port" << QString::number(mIcePort)
              << "--gpgnet_port" << "0"
              << "--lobby_port" << QString::number(mLobbySocket.localPort());
  FAF_LOG_INFO << "going to start node with arguments " << args.join(" ").toStdString();
  mIceAdapterProcess.start("node", args);
}

void Testclient::onPingStats(int peerId, float ping, int pendPings, int lostPings, int succPings)
{
  peerItem(peerId, PeerColumnPing)->setText(QString("%1 ms").arg(ping));
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
    FAF_LOG_INFO << "logged in as " << mPlayerLogin.toStdString() << " (" << mPlayerId << ")";
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
    FAF_LOG_INFO << "onHostLeft " << paramsArray[0].asInt();
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

  mIceClient.setRpcCallback("onDatachannelOpen", [this](Json::Value const& paramsArray,
                            Json::Value & result,
                            Json::Value & error,
                            Socket*)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 2 parameters.";
      return;
    }
    int peerId = paramsArray[1].asInt();
    mPeersReady.insert(peerId);
    if (mPeerIdPingtrackers.contains(peerId))
    {
      mPeerIdPingtrackers.value(peerId)->start();
    }
    if (mPeerRow.contains(peerId))
    {
      peerItem(peerId, PeerColumnConn)->setBackgroundColor(Qt::green);
    }
    FAF_LOG_DEBUG << "onDatachannelOpen for peer " << peerId;
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
      FAF_LOG_ERROR << "mGpgnetPort == 0";
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
  FAF_LOG_INFO << "received GPGNetMessage from ICEAdapter: " << msg.toDebug();

  auto extractPort = [](std::string const& peerAddress)
  {
    auto colonPos = peerAddress.find(":");
    if (colonPos != std::string::npos)
    {
      return std::stoi(peerAddress.substr(colonPos + 1));
    }
    FAF_LOG_ERROR << "error parsing peerAddress " << peerAddress;
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
      FAF_LOG_ERROR << "ConnectToPeer/JoinGame expected 3 chunks";
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
    FAF_LOG_DEBUG << "Pingtracker for peer " << peerId << " port " << peerPort << " created";
  }
  else if (msg.header == "DisconnectFromPeer")
  {
    if (msg.chunks.size() != 1)
    {
      FAF_LOG_ERROR << "DisconnectFromPeer expected 1 chunks";
      return;
    }
    int peerId = msg.chunks.at(0).asInt();
    mPeerIdPingtrackers.remove(peerId);
    mPeersReady.remove(peerId);
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
      FAF_LOG_ERROR << "wrong PING datagram size: " << datagram.size();
    }
    else
    {
      auto p = reinterpret_cast<PingPacket const*>(datagram.constData());
      quint32 peerId = p->type == PingPacket::PING ? p->senderId : p->answererId;
      if (!mPeerIdPingtrackers.contains(peerId))
      {
        FAF_LOG_ERROR << "!mPeerIdPingtrackers.contains(peerId): " << peerId;
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
  a.setApplicationName("faf-ice-testclient");
  a.setOrganizationName("FAForever");
  faf::Testclient c;
  c.show();
  return a.exec();
}
