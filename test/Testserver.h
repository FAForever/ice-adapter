#pragma once

#include <map>
#include <set>
#include <string>

#include "JsonRpcTcpServer.h"

namespace faf {

typedef int PlayerIdType;

class Testserver
{
public:
  Testserver();
protected:
  JsonRpcTcpServer mServer;
  PlayerIdType mCurrentPlayerId;
  std::map<PlayerIdType, TcpSession*> mPlayerSessions;
  std::map<TcpSession*, PlayerIdType> mSessionPlayers;
  std::map<PlayerIdType, std::string> mPlayersHostedgames;
  std::map<std::string, std::set<PlayerIdType>> mGames;
};

} // namespace faf
