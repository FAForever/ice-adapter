#pragma once

#include <string>
#include <vector>
#include <json/json.h>

struct GPGNetMessage
{
  std::string header; /*!< Message type like "CreateLobby" or "ConnectToPeer" */
  std::vector<Json::Value> chunks; /*!< parameters */

  /** \brief Parse a GPGNetMessage from char*
       \param start: Start of the buffer
       \param maxSize: End of the buffer
       \param result: The result on success
       \returns The number of bytes read from the buffer on success
                Or 0 on failure
      */
  static int parse(char* start, int maxSize, GPGNetMessage& result);
};
