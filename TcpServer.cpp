#include "TcpServer.h"

#include "TcpSession.h"
#include "logging.h"

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

namespace faf {



TcpServer::TcpServer(int port,
                     bool loopback)
{
  mListenSocket = Gio::Socket::create(Gio::SOCKET_FAMILY_IPV4,
                                      Gio::SOCKET_TYPE_STREAM,
                                      Gio::SOCKET_PROTOCOL_DEFAULT);
  mListenSocket->set_blocking(false);

  Glib::RefPtr<Gio::InetAddress> listenAddress;
  if (loopback)
  {
    listenAddress = Gio::InetAddress::create_loopback(Gio::SOCKET_FAMILY_IPV4);
  }
  else
  {
    listenAddress = Gio::InetAddress::create_any(Gio::SOCKET_FAMILY_IPV4);
  }
  auto srcAddress =
    Gio::InetSocketAddress::create(listenAddress, static_cast<guint16>(port));

  mListenSocket->bind(srcAddress, false);

  auto isockaddr = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(mListenSocket->get_local_address());
  if (isockaddr)
  {
    mListenPort = isockaddr->get_port();
    FAF_LOG_TRACE << "TcpServer listening bound to port " << mListenPort;
  }
  else
  {
    FAF_LOG_ERROR << "!isockaddr";
  }

  Gio::signal_socket().connect([this](Glib::IOCondition condition)
  {
    auto newSocket = mListenSocket->accept();
    auto session = std::make_shared<TcpSession>(this, newSocket);
    mSessions.push_back(session);
    FAF_LOG_TRACE << "new TcpSession created";
    this->connectionChanged.emit(session.get(), ConnectionState::Connected);
    return true;
  }, mListenSocket, Glib::IO_IN);
}

TcpServer::~TcpServer()
{
  FAF_LOG_TRACE << "~TcpServer()";
}

void TcpServer::listen()
{
  mListenSocket->listen();
}

int TcpServer::listenPort() const
{
  return mListenPort;
}

int TcpServer::sessionCount() const
{
  return mSessions.size();
}

void TcpServer::onCloseSession(TcpSession* session)
{
  mSessions.erase(
        std::remove_if(
          mSessions.begin(),
          mSessions.end(),
          [session](std::shared_ptr<TcpSession> const& s){ return s.get() == session; }),
      mSessions.end());
  this->connectionChanged.emit(session, ConnectionState::Disconnected);
  FAF_LOG_INFO << "TcpSession removed " << mSessions.size();
}


} // namespace faf
