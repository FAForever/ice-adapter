#include "GPGNetServer.h"

#include <type_traits>

#include <sigc++/sigc++.h>
#include <boost/log/trivial.hpp>

#include "GPGNetConnection.h"

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

GPGNetServer::GPGNetServer(short port)
{
  mListenSocket = Gio::Socket::create(Gio::SOCKET_FAMILY_IPV4,
                                      Gio::SOCKET_TYPE_STREAM,
                                      Gio::SOCKET_PROTOCOL_DEFAULT);
  mListenSocket->set_blocking(false);

  auto srcAddress =
    Gio::InetSocketAddress::create(Gio::InetAddress::create_loopback(Gio::SOCKET_FAMILY_IPV4),
                                   port);
  mListenSocket->bind(srcAddress, false);
  mListenSocket->listen();

  BOOST_LOG_TRIVIAL(trace) << "GPGNetServer listening on port " << port;

  Gio::signal_socket().connect([this](Glib::IOCondition condition)
  {
    auto newSocket = mListenSocket->accept();
    auto connection = std::make_shared<GPGNetConnection>(this,
                                                         newSocket);
    mConnections.push_back(connection);
    BOOST_LOG_TRIVIAL(trace) << "new GPGNetConnection created";

    if (mConnections.size() > 1)
    {
      BOOST_LOG_TRIVIAL(error) << mConnections.size() << " GPGNetConnections simultaneous are not supported";
    }
    return true;
  }, mListenSocket, Glib::IO_IN);
}

GPGNetServer::~GPGNetServer()
{
  BOOST_LOG_TRIVIAL(trace) << "~GPGNetServer()";
}

void GPGNetServer::sendMessage(GPGNetMessage const& msg)
{
  if (mConnections.size() != 1)
  {
    BOOST_LOG_TRIVIAL(error) << mConnections.size() << " GPGNetConnections";
  }
  for (auto it = mConnections.begin(), end = mConnections.end(); it != end; ++it)
  {
    (*it)->sendMessage(msg);
  }
}

void GPGNetServer::sendCreateLobby(InitMode initMode,
                                   int port,
                                   std::string const& login,
                                   int playerId,
                                   int natTraversalProvider)
{
  GPGNetMessage msg;
  msg.header = "CreateLobby";
  msg.chunks = {
    static_cast<std::underlying_type<InitMode>::type>(initMode),
    port,
    login,
    playerId,
    natTraversalProvider
  };
  sendMessage(msg);
}

void GPGNetServer::sendConnectToPeer(std::string const& addressAndPort,
                                     std::string const& playerName,
                                     int playerId)
{
  GPGNetMessage msg;
  msg.header = "ConnectToPeer";
  msg.chunks = {
    addressAndPort,
    playerName,
    playerId
  };
  sendMessage(msg);
}

void GPGNetServer::sendJoinGame(std::string const& addressAndPort,
                                std::string const& remotePlayerName,
                                int remotePlayerId)
{
  GPGNetMessage msg;
  msg.header = "JoinGame";
  msg.chunks = {
    addressAndPort,
    remotePlayerName,
    remotePlayerId
  };
  sendMessage(msg);
}

void GPGNetServer::sendHostGame(std::string const& map)
{
  GPGNetMessage msg;
  msg.header = "HostGame";
  msg.chunks = {
    map
  };
  sendMessage(msg);
}

void GPGNetServer::sendSendNatPacket(std::string const& addressAndPort,
                                     std::string const& message)
{
  GPGNetMessage msg;
  msg.header = "SendNatPacket";
  msg.chunks = {
    addressAndPort,
    message
  };
  sendMessage(msg);
}

void GPGNetServer::sendDisconnectFromPeer(int remotePlayerId)
{
  GPGNetMessage msg;
  msg.header = "DisconnectFromPeer";
  msg.chunks = {
    remotePlayerId
  };
  sendMessage(msg);
}

void GPGNetServer::sendPing()
{
  GPGNetMessage msg;
  msg.header = "ping";
  sendMessage(msg);
}

void GPGNetServer::setGpgNetCallback(std::string const& header,
                                     GpgNetCallback cb)
{
  mCallbacks.insert(std::make_pair(header, cb));
}

void GPGNetServer::onCloseConnection(GPGNetConnection* connection)
{
  for (auto it = mConnections.begin(), end = mConnections.end(); it != end; ++it)
  {
    if (it->get() == connection)
    {
      mConnections.erase(it);
      BOOST_LOG_TRIVIAL(trace) << "connection removed";
      return;
    }
  }
  mGameState = GameState::NotConnected;
  BOOST_LOG_TRIVIAL(trace) << "GameState changed to 'NotConnected'";
}

void GPGNetServer::onGPGNetMessage(GPGNetMessage const& msg)
{
  if (msg.header == "GameState")
  {
    if (msg.chunks.size() == 1)
    {
      if (msg.chunks.at(0).asString() == "Idle")
      {
        mGameState = GameState::Idle;
        BOOST_LOG_TRIVIAL(trace) << "GameState changed to 'Idle'";

      }
      else if (msg.chunks.at(0).asString() == "Lobby")
      {
        mGameState = GameState::Lobby;
        BOOST_LOG_TRIVIAL(trace) << "GameState changed to 'Lobby'";
      }
    }
  }

  for (auto it = mCallbacks.find(msg.header), end = mCallbacks.end(); it != end; ++it)
  {
    it->second(msg.chunks);
  }
}
