#include "TcpSession.h"

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

bool TcpSession::send(std::string const& msg)
{
  try
  {
    int bytesSent = mSocket->send(msg.c_str(), msg.size());
    if (bytesSent != msg.size())
    {
      FAF_LOG_ERROR << "only " << bytesSent << " of " << msg.size() << " bytes sent";
      return false;
    }
  }
  catch (const Glib::Exception& e)
  {
    FAF_LOG_ERROR << "error sending " << msg.size() << " bytes to TCP client: " << e.what();
    return false;
  }
  catch (const std::exception& e)
  {
    FAF_LOG_ERROR << "error sending " << msg.size() << " bytes to TCP client: " << e.what();
    return 1;
  }
  catch (...)
  {
    FAF_LOG_ERROR << "unknown error occured";
    return 1;
  }
  return true;
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
    mServer->parseMessage(this, mMessage);
  }
  catch (std::exception& e)
  {
    FAF_LOG_ERROR << "error in receive: " << e.what();
    return true;
  }
  return true;
}

} // namespace faf
