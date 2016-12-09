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
  void disconnectPlayerFromGame(PlayerIdType leavingPlayerId);
  JsonRpcServer mServer;
  PlayerIdType mCurrentPlayerId;
  std::map<PlayerIdType, Socket*> mPlayerSockets;
  std::map<Socket*, PlayerIdType> mSocketPlayers;

  /* store logins of connected players */
  std::map<PlayerIdType, std::string> mPlayersLogins;

  /* store which players are joined which games */
  std::map<PlayerIdType, PlayerIdType> mPlayersGames;

  /* store which players are hosting which games */
  std::map<PlayerIdType, std::set<PlayerIdType>> mGames;
};

} // namespace faf
