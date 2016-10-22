#include "GPGNetConnection.h"

#include <boost/log/trivial.hpp>

#include "GPGNetServer.h"

GPGNetConnection::GPGNetConnection(GPGNetServer* server,
                                   Glib::RefPtr<Gio::Socket> socket)
  : mSocket(socket),
    mBufferEnd(0),
    mServer(server)
{
  BOOST_LOG_TRIVIAL(trace) << "GPGNetConnection()";

  mSocket->set_blocking(false);

  Gio::signal_socket().connect(
        sigc::mem_fun(this, &GPGNetConnection::onRead),
        mSocket,
        Glib::IO_IN);
}

GPGNetConnection::~GPGNetConnection()
{
  BOOST_LOG_TRIVIAL(trace) << "~GPGNetConnection()";
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
        BOOST_LOG_TRIVIAL(error) << "Unsupported Json chunk type " << chunk.type();
        break;
    }
  }
}

bool GPGNetConnection::onRead(Glib::IOCondition /*condition*/)
{
  auto receiveCount = mSocket->receive(mBuffer.data() + mBufferEnd,
                                     mBuffer.size() - mBufferEnd);

  if (receiveCount == 0)
  {
    //BOOST_LOG_TRIVIAL(error) << "receiveCount == 0";
    mServer->onCloseConnection(this);
    return false;
  }
  BOOST_LOG_TRIVIAL(trace) << "received:" << std::string(mBuffer.data() + mBufferEnd,
                                                         receiveCount);
  mBufferEnd += receiveCount;

  parseMessages();

  if (mBufferEnd >= mBuffer.size())
  {
    BOOST_LOG_TRIVIAL(error) << "buffer full!";
    mBufferEnd = 0;
  }

  return true;
}

void GPGNetConnection::parseMessages()
{
  while(mBufferEnd > 0)
  {
    int bufferPos = 0;

    int32_t headerLength;
    if (mBufferEnd <= sizeof(int32_t))
    {
      return;
    }
    headerLength = *reinterpret_cast<int32_t*>(mBuffer.data() + bufferPos);
    bufferPos += sizeof(int32_t);
    if ((mBufferEnd - bufferPos) < headerLength)
    {
      return;
    }

    GPGNetMessage message;
    message.header = std::string(mBuffer.data() + bufferPos, headerLength);
    bufferPos += headerLength;

    int32_t chunkCount;
    if ((mBufferEnd - bufferPos) < sizeof(int32_t))
    {
      return;
    }
    chunkCount = *reinterpret_cast<int32_t*>(mBuffer.data() + bufferPos);
    bufferPos += sizeof (int32_t);
    message.chunks.resize(chunkCount);

    for (int chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
    {
      int8_t type;
      if ((mBufferEnd - bufferPos) < sizeof(int8_t))
      {
        return;
      }
      type = *reinterpret_cast<int8_t*>(mBuffer.data() + bufferPos);
      bufferPos += sizeof(int8_t);
      int32_t length;
      if ((mBufferEnd - bufferPos) < sizeof(int32_t))
      {
        return;
      }
      length = *reinterpret_cast<int32_t*>(mBuffer.data() + bufferPos);
      bufferPos += sizeof(int32_t);

      // Special-case for int (which uses the length field to hold the payload).
      if (type == 0)
      {
        message.chunks[chunkIndex] = length;
        continue;
      }

      if (type != 1)
      {
        BOOST_LOG_TRIVIAL(error) << "GPGNetMessage type " << static_cast<int>(type) << " not supported";
        return;
      }

      if ((mBufferEnd - bufferPos) < length)
      {
        return;
      }
      message.chunks[chunkIndex] = std::string(mBuffer.data() + bufferPos, length);
      bufferPos += length;
    }

    debugOutputMessage(message);
    mServer->onGPGNetMessage(message);

    std::copy(mBuffer.data() + bufferPos,
              mBuffer.data() + mBufferEnd,
              mBuffer.data());
    mBufferEnd -= bufferPos;
    if (mBufferEnd > 0)
    {
      BOOST_LOG_TRIVIAL(debug) << "mBufferEnd: " << mBufferEnd;
    }
  }
}

void GPGNetConnection::debugOutputMessage(GPGNetMessage const& msg)
{
  BOOST_LOG_TRIVIAL(trace) << "GPGNetMessage " <<
                              msg.header <<
                              " containing " <<
                              msg.chunks.size() <<
                              " chunks:";
  for(auto const& chunk : msg.chunks)
  {
    BOOST_LOG_TRIVIAL(trace) << "\t" << chunk;
  }
}
