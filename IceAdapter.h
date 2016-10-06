#pragma once

#include <map>

#include "HttpServer.h"
#include "HttpClient.h"

typedef struct _GMainLoop  GMainLoop;
class IceAgent;

class IceAdapter
{
public:
  IceAdapter(std::string const& playerId,
             std::string const& httpBaseUri,
             std::string const& stunHost,
             std::string const& turnHost,
             std::string const& turnUser,
             std::string const& turnPassword,
             unsigned int httpPort,
             int joinGameId);
  virtual ~IceAdapter();
  void run();

protected:
  void onJoinGame(std::string const& gameId);
  void onSdp(IceAgent* stream, std::string const& sdp);
  void onReceive(IceAgent* stream, std::string const& msg);
  void onConnectPlayers();
  void onPingPlayers();
  void onSetPlayerId(std::string const& playerId);
  std::string onStatus();

  void parseGameInfoAndConnectPlayers(std::string const& jsonGameInfo);

  GMainLoop*   mMainLoop;
  HttpServer   mHttpServer;
  HttpClient   mHttpClient;
  std::string  mPlayerId;
  std::map<IceAgent*, std::string> mAgentRemoteplayerMap;
  std::map<std::string, IceAgent*> mRemoteplayerAgentMap;
  std::map<IceAgent*, unsigned long long> mSentPings;
  std::map<IceAgent*, unsigned long long> mReceivedPings;
  std::map<IceAgent*, unsigned long long> mReceivedPongs;
  std::string  mGameId;
  std::string  mTurnHost;
  std::string  mTurnUser;
  std::string  mTurnPassword;
  unsigned int mConnectPlayersTimer;
  unsigned int mPingPlayersTimer;
  char* mStunIp;
  char* mTurnIp;

  int mJoinGameId;

  friend int connect_players_timeout(void*);
  friend int ping_players_timeout(void*);
};
