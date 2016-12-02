#include "GPGNetConnection.h"

#include "GPGNetServer.h"
#include "logging.h"

namespace faf
{

GPGNetConnection::GPGNetConnection(GPGNetServer* server,
                                   Glib::RefPtr<Gio::Socket> socket)
  : mSocket(socket),
    mServer(server)
{
  FAF_LOG_TRACE << "GPGNetConnection()";

  try
  {
    mSocket->set_blocking(false);

      Gio::signal_socket().connect(
            sigc::mem_fun(this, &GPGNetConnection::onRead),
            mSocket,
            Glib::IO_IN);
  }
  catch (std::exception& e)
  {
    FAF_LOG_ERROR << "error in GPGNetConnection: " << e.what();
  }
}

GPGNetConnection::~GPGNetConnection()
{
  FAF_LOG_TRACE << "~GPGNetConnection()";
}

void GPGNetConnection::sendMessage(GPGNetMessage const& msg)
{
  debugOutputMessage(msg);
  int32_t headerLength = msg.header.size();
  int32_t chunkCount = msg.chunks.size();
  mSocket->send(reinterpret_cast<char*>(&headerLength), sizeof(headerLength));
  mSocket->send(msg.header.c_str(), msg.header.size());
  mSocket->send(reinterpret_cast<char*>(&chunkCount), sizeof(chunkCount));

  for(const auto chunk : msg.chunks)
  {
    int8_t typeCode;
    switch (chunk.type())
    {
      case Json::intValue:
      case Json::uintValue:
      case Json::booleanValue:
      {
        typeCode = 0;
        mSocket->send(reinterpret_cast<char*>(&typeCode), sizeof(typeCode));
        int32_t value = chunk.asInt();
        mSocket->send(reinterpret_cast<char*>(&value), sizeof(value));
      }
        break;
      case Json::stringValue:
      {
        typeCode = 1;
        mSocket->send(reinterpret_cast<char*>(&typeCode), sizeof(typeCode));
        auto string = chunk.asString();
        int32_t stringLength = string.size();
        mSocket->send(reinterpret_cast<char*>(&stringLength), sizeof(stringLength));
        mSocket->send(string.c_str(), string.size());
      }
        break;
      default:
        FAF_LOG_ERROR << "Unsupported Json chunk type " << chunk.type();
        break;
    }
  }
}

bool GPGNetConnection::onRead(Glib::IOCondition /*condition*/)
{
  auto doRead = [this]()
  {
    try
    {
      auto receiveCount = mSocket->receive(mReadBuffer.data(),
                                           mReadBuffer.size());
      mMessage.insert(mMessage.end(), mReadBuffer.begin(), mReadBuffer.begin() + receiveCount);
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
      //FAF_LOG_ERROR << "receiveCount == 0";
      mServer->onCloseConnection(this);
      return false;
    }
    while (receiveCount == mReadBuffer.size())
    {
      receiveCount = doRead();
    }

    FAF_LOG_TRACE << "received " << mMessage.size() << " GPGNet bytes";

    parseMessages();
  }
  catch (std::exception& e)
  {
    FAF_LOG_ERROR << "error in receive: " << e.what();
    return true;
  }

  return true;
}

void GPGNetConnection::parseMessages()
{
  GPGNetMessage::parse(mMessage, [this](GPGNetMessage const& msg)
  {
    debugOutputMessage(msg);
    mServer->onGPGNetMessage(msg);
  });
}

void GPGNetConnection::debugOutputMessage(GPGNetMessage const& msg)
{
  FAF_LOG_TRACE << "GPGNetMessage " <<
                              msg.header <<
                              " containing " <<
                              msg.chunks.size() <<
                              " chunks:";
  for(auto const& chunk : msg.chunks)
  {
    FAF_LOG_TRACE << "\t" << chunk;
  }
}

}
