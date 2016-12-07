#pragma once

#include <map>
#include <set>
#include <string>

#include "JsonRpcServer.h"

namespace faf {

typedef int PlayerIdType;

class Testserver
{
public:
  Testserver();
protected:
  void sendGamelist(Socket* s = nullptr);
  JsonRpcServer mServer;
  PlayerIdType mCurrentPlayerId;
  std::map<PlayerIdType, Socket*> mPlayerSockets;
  std::map<Socket*, PlayerIdType> mSocketPlayers;
  std::map<PlayerIdType, std::string> mHostingplayersLogins;
  std::map<std::string, std::set<PlayerIdType>> mHostingloginsGames;
};

} // namespace faf
