#include "Testclient.h"
#include "ui_Testclient.h"

#include <set>
#include <iostream>

#include <QtCore/QVariant>
#include <QtCore/QTimer>
#include <QtCore/QSettings>
#include <QtCore/QDateTime>
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
  mIcePort(0),
  mPingId(0)
{
  mUi->setupUi(this);

  QSettings s;
  this->restoreGeometry(s.value("mainGeometry").toByteArray());
  this->restoreState(s.value("mainState").toByteArray());
  mUi->tableWidget_clientLog->horizontalHeader()->restoreState(s.value("clientLogHeaderState").toByteArray());
  mUi->tableWidget_iceLog->horizontalHeader()->restoreState(s.value("iceLogHeaderState").toByteArray());

  mUi->pushButton_leave->setEnabled(false);

  typedef boost::log::sinks::synchronous_sink<LogSink> sink_t;
  boost::shared_ptr<LogSink> backend(new LogSink(mUi->tableWidget_clientLog));
  boost::shared_ptr< sink_t > sink(new sink_t(backend));
  boost::log::core::get()->add_sink(sink);

  connectRpcMethods();

  mServerClient.connectToHost("zentrale.my-router.de", 54321);

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
        mIceClient.state() == QAbstractSocket::UnconnectedState)
    {
      mIceClient.connectToHost("localhost", mIcePort);
    }
  });
  connect(&mGpgClient,
          &GPGNetClient::onGPGNetMessage,
          this,
          &Testclient::onGPGNetMessageFromIceAdapter);
  connect(&mIceClient,
          &QTcpSocket::connected,
          this,
          &Testclient::updateStatus);

  auto updateTimer = new QTimer(this);
  connect(updateTimer,
          &QTimer::timeout,
          [this]()
  {
    updateStatus();
  });
  updateTimer->start(1000);

  mLobbySocket.bind();
  connect(&mLobbySocket,
          &QUdpSocket::readyRead,
          this,
          &Testclient::onLobbyReadyRead);
}

Testclient::~Testclient()
{
  mIceAdapterProcess.close();

  QSettings s;
  s.setValue("iceLogHeaderState", mUi->tableWidget_iceLog->horizontalHeader()->saveState());
  s.setValue("clientLogHeaderState", mUi->tableWidget_clientLog->horizontalHeader()->saveState());
  s.setValue("mainState", this->saveState());
  s.setValue("mainGeometry", this->saveGeometry());

  delete mUi;
}

void Testclient::on_pushButton_hostGame_clicked()
{
  if (mIceClient.state() == QAbstractSocket::UnconnectedState)
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
  mGameId = -1;
  for (QUdpSocket* s: mPeerPingers.values())
  {
    delete s;
  }
  mPeerPorts.clear();
  mPeerPingers.clear();
  mPingHistory.clear();
  mPendingPings.clear();
  mPeersReady.clear();
  mPingId = 0;
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

  mIceClient.setRpcCallback("onCandidateSelected", [this](Json::Value const& paramsArray,
                            Json::Value & result,
                            Json::Value & error,
                            Socket*)
  {
    if (paramsArray.size() < 4)
    {
      error = "Need 4 parameter.";
      return;
    }
    FAF_LOG_INFO << "onCandidateSelected: " << paramsArray[2].asString() << " <-> " << paramsArray[3].asString();
    mPeersReady.insert(paramsArray[1].asInt());
    createPinger(paramsArray[1].asInt());
  });
}

void Testclient::updateStatus()
{
  if (mIceClient.state() != QAbstractSocket::ConnectedState)
  {
    return;
  }
  mIceClient.sendRequest("status",
                         Json::Value(Json::arrayValue),
                         nullptr,
                         [this] (Json::Value const& result,
                                 Json::Value const& error)
  {
    if (mGpgnetPort == 0)
    {
      mGpgnetPort = result["gpgnet"]["local_port"].asInt();
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

      QString pingText = "none";
      int pid = relayInfo["remote_player_id"].asInt();
      //if (!mPingHistory.contains(pid))
      //{
      //  FAF_LOG_ERROR << "!mPingHistory.contains " << pid;
      //}
      if (mPingHistory.contains(pid) &&
          mPingHistory.value(pid).size() > 0)
      {
        float ping = 0;
        for (int time : mPingHistory.value(pid))
        {
          ping += time;
        }
        ping /= mPingHistory.value(pid).size();
        pingText = QString("%1 ms").arg(ping);
      }
      mUi->tableWidget_players->setItem(row, ColumnPing,
        new QTableWidgetItem(pingText));

      mUi->tableWidget_players->setItem(row, ColumnConntime,
        new QTableWidgetItem(QString("%1 s").arg(relayInfo["ice_agent"]["time_to_connected"].asDouble())));
      mUi->tableWidget_players->setItem(row, ColumnLocalCand,
        new QTableWidgetItem(QString::fromStdString(relayInfo["ice_agent"]["local_candidate"].asString())));
      mUi->tableWidget_players->setItem(row, ColumnRemoteCand,
        new QTableWidgetItem(QString::fromStdString(relayInfo["ice_agent"]["remote_candidate"].asString())));
      mUi->tableWidget_players->setItem(row, ColumnRemoteSdp,
        new QTableWidgetItem(QString::fromStdString(relayInfo["ice_agent"]["remote_sdp"].asString())));
      ++row;
    }

  });

}

void Testclient::startIceAdapter()
{
  if (mIcePort == 0)
  {
    QTcpServer server;
    server.listen(QHostAddress::LocalHost, 0);
    mIcePort = server.serverPort();
  }
  //QString exeName = "./faf-ice-adapter";
//#if defined(__MINGW32__)
//    exeName = "faf-ice-adapter.exe";
//#endif
//    G_MESSAGES_DEBUG=all
  auto env = QProcessEnvironment::systemEnvironment();
  env.insert("G_MESSAGES_DEBUG", "all");
  env.insert("NICE_DEBUG", "all");
  mIceAdapterProcess.setProcessEnvironment(env);
  mIceAdapterProcess.start("./faf-ice-adapter",
                           QStringList()
                           << "--id" << QString::number(mPlayerId)
                           << "--login" << mPlayerLogin
                           << "--rpc-port" << QString::number(mIcePort)
                           << "--gpgnet-port" << "0"
                           << "--lobby-port" << QString::number(mLobbySocket.localPort())
                           //<< "--stun-host" << "chat.erreich.bar"
                           //<< "--turn-host" << "chat.erreich.bar"
                           << "--stun-host" << "dev.faforever.com"
                           << "--turn-host" << "dev.faforever.com"
                           //<< "--stun-host" << "numb.viagenie.ca"
                           //<< "--turn-host" << "numb.viagenie.ca"
                           //<< "--turn-user" << "mm+viagenie.ca@netlair.de"
                           //<< "--turn-pass" << "asdf"
                           );
}

void Testclient::startGpgnetClient()
{
  updateStatus();
  if (mGpgnetPort == 0)
  {
    FAF_LOG_ERROR << "mGpgnetPort == 0";
  }
  mGpgClient.connectToHost("localhost",
                           mGpgnetPort);
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
        if (msg.contains("<warning>"))
        {
          item->setBackgroundColor(QColor::fromRgbF(1, 1, 0.5));
        }
        else
        {
          item->setBackgroundColor(QColor::fromRgbF(1, 0.5, 0.5));
        }
      }
      else if (msg.contains("<debug>"))
      {
        item->setBackgroundColor(QColor::fromRgbF(0.8, 1, 0.8));
      }
      else if (msg.contains("<info>"))
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
    mPeerPorts[msg.chunks.at(2).asInt()] = extractPort(msg.chunks.at(0).asString());
    mPortPeers[extractPort(msg.chunks.at(0).asString())] = msg.chunks.at(2).asInt();
    createPinger(msg.chunks.at(2).asInt());
  }
  else if (msg.header == "DisconnectFromPeer")
  {
    if (msg.chunks.size() != 1)
    {
      FAF_LOG_ERROR << "DisconnectFromPeer expected 1 chunks";
      return;
    }
    if (mPeerPingers.contains(msg.chunks.at(0).asInt()))
    {
      delete mPeerPingers.value(msg.chunks.at(0).asInt());
    }
    mPeerPingers.remove(msg.chunks.at(0).asInt());
    mPortPeers.remove(mPeerPorts.value(msg.chunks.at(0).asInt()));
    mPeerPorts.remove(msg.chunks.at(0).asInt());
  }
}

void Testclient::createPinger(int remotePeerId)
{
  if (mPeerPingers.contains(remotePeerId))
  {
    return;
  }

  if (!mPeerPorts.contains(remotePeerId))
  {
    return;
  }

  if (!mPeersReady.contains(remotePeerId))
  {
    return;
  }

  auto pinger = new QUdpSocket(this);
  mPeerPingers[remotePeerId] = pinger;
  //pinger->bind();
  pinger->connectToHost(QHostAddress::LocalHost,
                        mPeerPorts.value(remotePeerId));
  connect(pinger,
          &QUdpSocket::readyRead,
          [this, pinger, remotePeerId]()
  {
  });
  auto pingTimer = new QTimer(pinger);
  connect(pingTimer,
          &QTimer::timeout,
          [this, pinger, remotePeerId]()
  {
    QByteArray pingDatagram;
    pingDatagram.append("PING");
    pingDatagram.append(reinterpret_cast<const char *>(&mPingId), sizeof(mPingId));
    pinger->writeDatagram(pingDatagram,
                          QHostAddress::LocalHost,
                          mPeerPorts.value(remotePeerId));
    mPendingPings[mPingId] = QDateTime::currentMSecsSinceEpoch();
    ++mPingId;

    if (mPendingPings.size() > 20)
    {
      FAF_LOG_WARN << "mPendingPings.size() > 20";
    }
  });
  pingTimer->start(100);
}

void Testclient::onLobbyReadyRead()
{
  //FAF_LOG_DEBUG << "mLobbySocket readyRead";
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
    if (datagram.startsWith("PING"))
    {
      //FAF_LOG_TRACE << "ping from " << senderPort;
      mLobbySocket.writeDatagram(datagram.replace(0,4,"PONG"),
                                 sender,
                                 senderPort);
    }
    else if (datagram.startsWith("PONG"))
    {
      //FAF_LOG_TRACE << "pong from " << senderPort;
      quint32 pingId = *reinterpret_cast<quint32*>(datagram.data() + 4);
      if (!mPendingPings.contains(pingId))
      {
        FAF_LOG_ERROR << "!mPendingPings.contains(pingId)";
        return;
      }
      auto ping = QDateTime::currentMSecsSinceEpoch() - mPendingPings.value(pingId);
      mPendingPings.remove(pingId);
      if (!mPortPeers.contains(senderPort))
      {
        FAF_LOG_ERROR << "!mPortPeers.contains(senderPort)";
        return;
      }
      auto remotePeerId = mPortPeers.value(senderPort);
      mPingHistory[remotePeerId].push_back(ping);
      while (mPingHistory[remotePeerId].size() > 50)
      {
        mPingHistory[remotePeerId].pop_front();
      }
      //FAF_LOG_TRACE << "ping to " << remotePeerId << ": " << ping << " ms";
    }
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
