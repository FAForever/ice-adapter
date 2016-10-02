#include "IceAdapter.h"

#include <iostream>

#include <glib.h>

#include <json/json.h>

#include "IceStream.h"

IceAdapter::IceAdapter(std::string const& playerId,
                       std::string const& httpBaseUri,
                       std::string const& stunTurnHost,
                       unsigned int httpPort):
  mMainLoop(g_main_loop_new(NULL, FALSE)),
  mIceAgent(mMainLoop, stunTurnHost),
  mHttpServer(httpPort),
  mHttpClient(httpBaseUri),
  mPlayerId(playerId)
{
  mHttpServer.setJoinGameCallback(
        std::bind(&IceAdapter::onJoinGame, this, std::placeholders::_1)
      );
  mHttpServer.setCreateGameCallback(
        std::bind(&IceAdapter::onCreateGame, this)
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
  std::cout << "IceAdapter::onJoinGame " << gameId << std::endl;
  auto jsonResponse = mHttpClient.getPlayers(gameId);
  Json::Value root;
  Json::Reader r;
  if (!r.parse(jsonResponse, root))
  {
    std::cerr << "Invalid response from createGame: " << jsonResponse;
    return;
  }
  mGameId = gameId;
  std::cout << "IceAdapter::onJoinGame players " << jsonResponse << std::endl;
  if (root.isMember(mPlayerId))
  {
    std::cerr << "I'm already in the game" << std::endl;
    return;
  }
  for (auto it = root.begin(), end = root.end(); it != end; ++it)
  {
    IceStream* iceStream = mIceAgent.createStream();
    mStreamRemoteplayerMap.insert(std::make_pair(iceStream, it.key().asString()));
    iceStream->setCandidateGatheringDoneCallback(
          std::bind(&IceAdapter::onSdp,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2)
        );
    iceStream->gatherCandidates();
  }
}

void IceAdapter::onCreateGame()
{
  std::cout << "IceAdapter::onCreateGame " << std::endl;
  auto jsonResponse = mHttpClient.createGame(mPlayerId);
  Json::Value root;
  Json::Reader r;
  if (!r.parse(jsonResponse, root))
  {
    std::cerr << "Invalid response from createGame: " << jsonResponse;
    return;
  }
  if (!root.isMember("id"))
  {
    std::cerr << "missing ID in createGame response" << jsonResponse << std::endl;
    return;
  }
  mGameId = root["id"].asString();
  std::cout << "created game " << mGameId << std::endl;
}

void IceAdapter::onSdp(IceStream* stream, std::string const& sdp)
{
  auto remotePlayerIt = mStreamRemoteplayerMap.find(stream);
  if (remotePlayerIt == mStreamRemoteplayerMap.end())
  {
    std::cerr << "stream not found" << std::endl;
    return;
  }
  std::cout << "received SDP for remote ID " << remotePlayerIt->second << std::endl;
  if (mGameId.size() == 0)
  {
    std::cerr << "no game" << std::endl;
    return;
  }
  mHttpClient.setSdp(mGameId,
                     mPlayerId,
                     remotePlayerIt->second,
                     sdp);

}
