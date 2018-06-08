#pragma once

#include <string>
#include <functional>
#include <webrtc/third_party/jsoncpp/source/include/json/json.h>

namespace faf
{

struct GPGNetMessage
{
  std::string header; /*!< Message type like "CreateLobby" or "ConnectToPeer" */
  std::vector<Json::Value> chunks; /*!< parameters */

  std::string toBinary() const;
  std::string toDebug() const;

  /** \brief Parse a GPGNetMessage from char*
       \param start: Start of the buffer
       \param maxSize: End of the buffer
       \param result: The result on success
       \returns The number of bytes read from the buffer on success
                Or 0 on failure
      */
  static void parse(std::string& msgBuffer, std::function<void (GPGNetMessage const&)> cb);
};

}
