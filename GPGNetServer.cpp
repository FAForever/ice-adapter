#include "GPGNetServer.h"

#include <type_traits>

#include <sigc++/sigc++.h>
#include <boost/log/trivial.hpp>

#include "GPGNetConnection.h"

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

GPGNetServer::GPGNetServer(short port):
  mPort(port)
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

  auto isockaddr = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(mListenSocket->get_local_address());
  if (isockaddr)
  {
    mPort = isockaddr->get_port();
    BOOST_LOG_TRIVIAL(trace) << "GPGNetServer listening on port " << mPort;
  }
  else
  {
    BOOST_LOG_TRIVIAL(error) << "!isockaddr";
  }

  Gio::signal_socket().connect([this](Glib::IOCondition condition)
  {
    if (mConnection)
    {
      BOOST_LOG_TRIVIAL(error) << "simultaneous GPGNetConnections are not supported";
      return true;
    }
    auto newSocket = mListenSocket->accept();
    mConnection = std::make_shared<GPGNetConnection>(this,
                                                     newSocket);
    BOOST_LOG_TRIVIAL(trace) << "new GPGNetConnection created";

    for (auto cb : mConnectionStateCallbacks)
    {
      cb(ConnectionState::Connected);
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
  if (!mConnection)
  {
    BOOST_LOG_TRIVIAL(error) << "no GPGNetConnection";
    return;
  }
  mConnection->sendMessage(msg);
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

void GPGNetServer::addGpgMessageCallback(GpgMessageCallback cb)
{
  mGPGNetMessageCallbacks.push_back(cb);
}

void GPGNetServer::addConnectionStateCallback(ConnectionStateCallback cb)
{
  mConnectionStateCallbacks.push_back(cb);
}

ConnectionState GPGNetServer::connectionState() const
{
  if (mConnection)
  {
    return ConnectionState::Connected;
  }
  else
  {
    return ConnectionState::Disconnected;
  }
}

guint16 GPGNetServer::port() const
{
  return mPort;
}

void GPGNetServer::onCloseConnection(GPGNetConnection* connection)
{
  mConnection.reset();
  for (auto cb : mConnectionStateCallbacks)
  {
    cb(ConnectionState::Disconnected);
  }
}


void GPGNetServer::onGPGNetMessage(GPGNetMessage const& msg)
{
  for (auto cb : mGPGNetMessageCallbacks)
  {
    cb(msg);
  }
}
