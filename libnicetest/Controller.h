#pragma once

#include <string>

class HttpServer;
class IceAgent;

class Controller
{
public:
  Controller(std::string const& playerId,
             HttpServer* httpServer,
             IceAgent* iceAgent);
  virtual ~Controller();
protected:
  void onJoinGame(std::string const& gameId);
  std::string mPlayerId;
  HttpServer* mHttpServer;
  IceAgent*   mIceAgent;
};
