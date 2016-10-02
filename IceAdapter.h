#pragma once

#include "HttpServer.h"
#include "HttpClient.h"
#include "IceAgent.h"

typedef struct _GMainLoop GMainLoop;

class IceAdapter
{
public:
  IceAdapter(std::string const& playerId,
             std::string const& httpBaseUri);
  virtual ~IceAdapter();
  void run();

  void onJoinGame(std::string const& gameId);
protected:
  GMainLoop*  mMainLoop;
  IceAgent    mIceAgent;
  HttpServer  mHttpServer;
  HttpClient  mHttpClient;
  std::string mPlayerId;
};
