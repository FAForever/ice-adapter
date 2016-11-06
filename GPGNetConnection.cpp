#include "GPGNetConnection.h"

#include "GPGNetServer.h"
#include "logging.h"

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
  auto it = mMessage.begin();

  auto bytesLeft = [this, it]()
  {
    return mMessage.end() - it;
  };

  while(it != mMessage.end())
  {
    int32_t headerLength;
    if (bytesLeft() <= sizeof(int32_t))
    {
      return;
    }
    headerLength = *reinterpret_cast<int32_t*>(&*it);
    it += sizeof(int32_t);
    if (bytesLeft() < headerLength)
    {
      return;
    }

    GPGNetMessage message;
    message.header = std::string(&*it, headerLength);
    it += headerLength;

    int32_t chunkCount;
    if (bytesLeft() < sizeof(int32_t))
    {
      return;
    }
    chunkCount = *reinterpret_cast<int32_t*>(&*it);
    it += sizeof (int32_t);
    message.chunks.resize(chunkCount);

    for (int chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
    {
      int8_t type;
      if (bytesLeft() < sizeof(int8_t))
      {
        return;
      }
      type = *reinterpret_cast<int8_t*>(&*it);
      it += sizeof(int8_t);
      int32_t length;
      if (bytesLeft() < sizeof(int32_t))
      {
        return;
      }
      length = *reinterpret_cast<int32_t*>(&*it);
      it += sizeof(int32_t);

      // Special-case for int (which uses the length field to hold the payload).
      if (type == 0)
      {
        message.chunks[chunkIndex] = length;
        continue;
      }

      if (type != 1)
      {
        FAF_LOG_ERROR << "GPGNetMessage type " << static_cast<int>(type) << " not supported";
        return;
      }

      if (bytesLeft() < length)
      {
        return;
      }
      message.chunks[chunkIndex] = std::string(&*it, length);
      it += length;
    }

    debugOutputMessage(message);
    mServer->onGPGNetMessage(message);

#if 0
    /* TODO: stop copying the buffer to pos 0,
     * and move the start pos */
#endif
  }
  /* Now move the remaining bytes in the buffer to the start
   * of the buffer */
  FAF_LOG_TRACE << "erasing " << it - mMessage.begin() << " bytes just read";
  mMessage.erase(mMessage.begin(), it);
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
