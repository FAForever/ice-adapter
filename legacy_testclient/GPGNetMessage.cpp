#include "GPGNetMessage.h"

#include <cstdint>
#include <sstream>

#include <QtCore/QtDebug>

namespace faf
{

std::string GPGNetMessage::toBinary() const
{
  std::string result;
  int32_t headerLength = this->header.size();
  int32_t chunkCount = this->chunks.size();
  result.append(reinterpret_cast<char*>(&headerLength), sizeof(headerLength));
  result.append(this->header.c_str(), this->header.size());
  result.append(reinterpret_cast<char*>(&chunkCount), sizeof(chunkCount));

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
        result.append(reinterpret_cast<char*>(&typeCode), sizeof(typeCode));
        int32_t value = chunk.asInt();
        result.append(reinterpret_cast<char*>(&value), sizeof(value));
      }
        break;
      case Json::stringValue:
      {
        typeCode = 1;
        result.append(reinterpret_cast<char*>(&typeCode), sizeof(typeCode));
        auto string = chunk.asString();
        int32_t stringLength = string.size();
        result.append(reinterpret_cast<char*>(&stringLength), sizeof(stringLength));
        result.append(string.c_str(), string.size());
      }
        break;
      default:
        qCritical() << "Unsupported Json chunk type " << chunk.type();
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

void GPGNetMessage::parse(std::vector<char>& buffer, std::function<void (GPGNetMessage const&)> cb)
{
  while(true)
  {
    auto it = buffer.begin();

    int32_t headerLength;
    if ((buffer.end() - it) <= sizeof(int32_t))
    {
      return;
    }
    headerLength = *reinterpret_cast<int32_t*>(&*it);
    it += sizeof(int32_t);
    if ((buffer.end() - it) < headerLength)
    {
      return;
    }

    GPGNetMessage message;
    message.header = std::string(&*it, headerLength);
    it += headerLength;

    int32_t chunkCount;
    if ((buffer.end() - it) < sizeof(int32_t))
    {
      return;
    }
    chunkCount = *reinterpret_cast<int32_t*>(&*it);
    it += sizeof (int32_t);
    message.chunks.resize(chunkCount);

    for (int chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
    {
      int8_t type;
      if ((buffer.end() - it) < sizeof(int8_t))
      {
        return;
      }
      type = *reinterpret_cast<int8_t*>(&*it);
      it += sizeof(int8_t);
      int32_t length;
      if ((buffer.end() - it) < sizeof(int32_t))
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
        qCritical() << "GPGNetMessage type " << static_cast<int>(type) << " not supported";
        return;
      }

      if ((buffer.end() - it) < length)
      {
        return;
      }
      message.chunks[chunkIndex] = std::string(&*it, length);
      it += length;
    }

    cb(message);
    /* Now move the remaining bytes in the buffer to the start
     * of the buffer */
    buffer.erase(buffer.begin(), it);
  }

}

}
