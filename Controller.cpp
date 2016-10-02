#include "Controller.h"

#include <cstring>
#include <iostream>

#include "HttpServer.h"
#include "IceAgent.h"

Controller::Controller(std::string const& playerId,
                       HttpServer* httpServer,
                       IceAgent* iceAgent):
  mPlayerId(playerId),
  mHttpServer(httpServer),
  mIceAgent(iceAgent)
{
}

Controller::~Controller()
{
}

void Controller::onJoinGame(std::string const& gameId)
{
}
