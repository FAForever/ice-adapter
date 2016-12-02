#include "TcpServer.h"

#include "logging.h"

namespace faf {

TcpSession::TcpSession(TcpServer* server,
                       Glib::RefPtr<Gio::Socket> socket)
  : mSocket(socket),
    mServer(server)
{
  FAF_LOG_TRACE << "TcpSession()";

  mSocket->set_blocking(false);

  Gio::signal_socket().connect(
        sigc::mem_fun(this, &TcpSession::onRead),
        mSocket,
        Glib::IO_IN);
}

TcpSession::~TcpSession()
{
  FAF_LOG_TRACE << "~TcpSession()";
}

void TcpSession::send(std::string const& msg)
{
  int bytesSent = mSocket->send(msg.c_str(), msg.size());
  if (bytesSent != msg.size())
  {
    FAF_LOG_ERROR << "only " << bytesSent << " of " << msg.size() << " bytes sent";
  }
}

bool TcpSession::onRead(Glib::IOCondition condition)
{
  auto doRead = [this]()
  {
    try
    {
      auto receiveCount = mSocket->receive(mReadBuffer.data(),
                                           mReadBuffer.size());
      mMessage.insert(mMessage.end(),
                      mReadBuffer.begin(),
                      mReadBuffer.begin() + receiveCount);
      return receiveCount;
    }
    catch (const Glib::Error& e)
    {
      FAF_LOG_ERROR << "mSocket->receive: " << e.code() << ": " << e.what();
      return gssize(0);
    }
  };

  try
  {
    auto receiveCount = doRead();
    if (receiveCount == 0)
    {
      mServer->onCloseSession(this);
      return false;
    }
    while (receiveCount == mReadBuffer.size())
    {
      receiveCount = doRead();
    }
    while(mServer->parseMessage(mMessage)){};
  }
  catch (std::exception& e)
  {
    FAF_LOG_ERROR << "error in receive: " << e.what();
    return true;
  }
  return true;
}

TcpServer::TcpServer(int port)
{
  mListenSocket = Gio::Socket::create(Gio::SOCKET_FAMILY_IPV4,
                                      Gio::SOCKET_TYPE_STREAM,
                                      Gio::SOCKET_PROTOCOL_DEFAULT);
  mListenSocket->set_blocking(false);

  auto srcAddress =
    Gio::InetSocketAddress::create(Gio::InetAddress::create_loopback(Gio::SOCKET_FAMILY_IPV4),
                                   static_cast<guint16>(port));

  mListenSocket->bind(srcAddress, false);
  mListenSocket->listen();

  auto isockaddr = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(mListenSocket->get_local_address());
  if (isockaddr)
  {
    mListenPort = isockaddr->get_port();
    FAF_LOG_TRACE << "TcpServer listening on port " << mListenPort;
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
    return true;
  }, mListenSocket, Glib::IO_IN);
}

TcpServer::~TcpServer()
{
  FAF_LOG_TRACE << "~TcpServer()";
}

int TcpServer::listenPort() const
{
  return mListenPort;
}

void TcpServer::onCloseSession(TcpSession* session)
{
  mSessions.erase(
        std::remove_if(
          mSessions.begin(),
          mSessions.end(),
          [session](std::shared_ptr<TcpSession> const& s){ return s.get() == session;}),
      mSessions.end());
  FAF_LOG_TRACE << "TcpSession removed";
}

void TcpServer::onConnected()
{
}

void TcpServer::onDisconnected()
{
}

} // namespace faf
