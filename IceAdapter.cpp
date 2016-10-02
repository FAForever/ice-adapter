#include "IceAdapter.h"

#include <iostream>

#include <glib.h>

#include <json/json.h>

#include "IceStream.h"


int connect_players_timeout(gpointer data)
{
  static_cast<IceAdapter*>(data)->onConnectPlayers();
  return 1;
}

IceAdapter::IceAdapter(std::string const& playerId,
                       std::string const& httpBaseUri,
                       std::string const& stunHost,
                       std::string const& turnHost,
                       std::string const& turnUser,
                       std::string const& turnPassword,
                       unsigned int httpPort):
  mMainLoop(g_main_loop_new(NULL, FALSE)),
  mIceAgent(mMainLoop,
            stunHost,
            turnHost,
            turnUser,
            turnPassword),
  mHttpServer(httpPort),
  mHttpClient(httpBaseUri),
  mPlayerId(playerId),
  mTurnHost(turnHost),
  mTurnUser(turnUser),
  mTurnPassword(turnPassword)
{
  mHttpServer.setJoinGameCallback(
        std::bind(&IceAdapter::onJoinGame, this, std::placeholders::_1)
      );
  mHttpServer.setCreateGameCallback(
        std::bind(&IceAdapter::onCreateGame, this)
        );

  mConnectPlayersTimer = g_timeout_add(100, connect_players_timeout, this);
}

IceAdapter::~IceAdapter()
{
  g_source_remove(mConnectPlayersTimer);
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
  mGameId = gameId;
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

void IceAdapter::onConnectPlayers()
{
  if (mGameId.size() == 0)
  {
    return;
  }
  auto jsonResponse = mHttpClient.getPlayers(mGameId);
  Json::Value players;
  Json::Reader r;
  if (!r.parse(jsonResponse, players))
  {
    std::cerr << "Invalid response from getPlayers: " << jsonResponse;
    return;
  }
  for (auto it = players.begin(), end = players.end(); it != end; ++it)
  {
    auto const& remotePlayerId = it.key().asString();
    if (remotePlayerId == mPlayerId)
    {
      continue;
    }
    /* if there's no stream for this remotePlayer, create one */
    if (mRemoteplayerStreamMap.find(remotePlayerId) == mRemoteplayerStreamMap.end())
    {
      IceStream* iceStream = mIceAgent.createStream();
      mStreamRemoteplayerMap.insert(std::make_pair(iceStream, remotePlayerId));
      mRemoteplayerStreamMap.insert(std::make_pair(remotePlayerId, iceStream));
      iceStream->setCandidateGatheringDoneCallback(
            std::bind(&IceAdapter::onSdp,
                      this,
                      std::placeholders::_1,
                      std::placeholders::_2)
          );
      iceStream->gatherCandidates();
    }

    /* check if the remote player already has a SDP for us */
    for (auto remotePlayerSdpsIt = it->begin(), end = it->end();
         remotePlayerSdpsIt != end; ++remotePlayerSdpsIt)
    {
      if (it.key() == mPlayerId)
      {
        std::cout << "remote player " << remotePlayerId << " has SDP for us:" << it->asString() << std::endl;
      }
    }
  }

}
