#pragma once

#include <string>
#include <vector>
#include <json/json.h>

struct GPGNetMessage
{
  std::string header; /*!< Message type like "CreateLobby" or "ConnectToPeer" */
  std::vector<Json::Value> chunks; /*!< parameters */
};
