#include "IceAdapter.h"

#include <gio/gnetworking.h>

IceAdapter::IceAdapter(std::string const& playerId,
                       std::string const& httpBaseUri):
  mMainLoop(g_main_loop_new(NULL, FALSE)),
  mIceAgent(mMainLoop),
  mHttpClient(httpBaseUri),
  mPlayerId(playerId)
{
  mHttpServer.setJoinGameCallback(
        std::bind(&IceAdapter::onJoinGame, this, std::placeholders::_1)
      );
}

IceAdapter::~IceAdapter()
{
  if (mMainLoop)
  {
    g_main_loop_unref(mMainLoop);
    mMainLoop = nullptr;
  }
}

void IceAdapter::run()
{
  g_main_loop_run(mMainLoop);
}

void IceAdapter::onJoinGame(std::string const& gameId)
{
  auto jsonResponse = mHttpClient.joinGame(gameId);
}
