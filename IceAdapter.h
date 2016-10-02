#pragma once

#include "HttpServer.h"
#include "HttpClient.h"
#include "IceAgent.h"

typedef struct _GMainLoop  GMainLoop;
class IceStream;

class IceAdapter
{
public:
  IceAdapter(std::string const& playerId,
             std::string const& httpBaseUri,
             std::string const& stunTurnHost,
             unsigned int httpPort);
  virtual ~IceAdapter();
  void run();

  void onJoinGame(std::string const& gameId);
  void onCreateGame();
  void onSdp(IceStream* stream, std::string const& sdp);
protected:
  GMainLoop*  mMainLoop;
  IceAgent    mIceAgent;
  HttpServer  mHttpServer;
  HttpClient  mHttpClient;
  std::string mPlayerId;
  std::map<IceStream*, std::string> mStreamRemoteplayerMap;
  std::string mGameId;
};
