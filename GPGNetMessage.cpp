#include "GPGNetMessage.h"

#include "logging.h"

namespace faf
{

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
        FAF_LOG_ERROR << "GPGNetMessage type " << static_cast<int>(type) << " not supported";
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
