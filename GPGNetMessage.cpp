#include "GPGNetMessage.h"

#include <cstdint>

#include "logging.h"

namespace faf
{

std::string GPGNetMessage::toBinary() const
{
  std::string result;
  int32_t headerLength = this->header.size();
  int32_t chunkCount = this->chunks.size();
  char buf[sizeof(int32_t)];
  std::memcpy(buf, &headerLength, sizeof(headerLength));
  result.append(buf, sizeof(headerLength));
  result.append(this->header);
  std::memcpy(buf, &chunkCount, sizeof(chunkCount));
  result.append(buf, sizeof(chunkCount));

  for(const auto chunk : this->chunks)
  {
    int8_t typeCode;
    switch (chunk.type())
    {
      case Json::intValue:
      case Json::uintValue:
      case Json::booleanValue:
      {
        typeCode = 0;
        std::memcpy(buf, &typeCode, sizeof(typeCode));
        result.append(buf, sizeof(typeCode));
        int32_t value = chunk.asInt();
        std::memcpy(buf, &value, sizeof(value));
        result.append(buf, sizeof(value));
      }
        break;
      case Json::stringValue:
      {
        typeCode = 1;
        std::memcpy(buf, &typeCode, sizeof(typeCode));
        result.append(buf, sizeof(typeCode));
        auto string = chunk.asString();
        int32_t stringLength = string.size();
        std::memcpy(buf, &stringLength, sizeof(stringLength));
        result.append(buf, sizeof(stringLength));
        result.append(string);
      }
        break;
      default:
        FAF_LOG_ERROR << "Unsupported Json chunk type " << chunk.type();
        break;
    }
  }
  return result;
}

std::string GPGNetMessage::toDebug() const
{
  std::ostringstream os;
  os << "GPGNetMessage <" <<
        this->header <<
        "> [";
  for(auto const& chunk : this->chunks)
  {
    os<< chunk << ", ";
  }
  os << "]";
  return os.str();
}

void GPGNetMessage::parse(std::string& msgBuffer, std::function<void (GPGNetMessage const&)> cb)
{
  while(true)
  {
    auto it = msgBuffer.begin();

    int32_t headerLength;
    if ((msgBuffer.end() - it) <= sizeof(int32_t))
    {
      return;
    }
    std::memcpy(&headerLength, &*it, sizeof (headerLength));
    it += sizeof(int32_t);
    if ((msgBuffer.end() - it) < headerLength)
    {
      return;
    }

    GPGNetMessage message;
    message.header = std::string(&*it, headerLength);
    it += headerLength;

    int32_t chunkCount;
    if ((msgBuffer.end() - it) < sizeof(int32_t))
    {
      return;
    }
    std::memcpy(&chunkCount, &*it, sizeof (chunkCount));
    it += sizeof (int32_t);
    message.chunks.resize(chunkCount);

    for (int chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
    {
      int8_t type;
      if ((msgBuffer.end() - it) < sizeof(int8_t))
      {
        return;
      }
      std::memcpy(&type, &*it, sizeof (int8_t));
      it += sizeof(int8_t);
      int32_t length;
      if ((msgBuffer.end() - it) < sizeof(int32_t))
      {
        return;
      }
      std::memcpy(&length, &*it, sizeof (int32_t));
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

      if ((msgBuffer.end() - it) < length)
      {
        return;
      }
      message.chunks[chunkIndex] = std::string(&*it, length);
      it += length;
    }

    cb(message);
    /* Now move the remaining bytes in the buffer to the start
     * of the buffer */
    msgBuffer.erase(msgBuffer.begin(), it);
  }

}

}
