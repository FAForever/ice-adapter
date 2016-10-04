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
int ping_players_timeout(gpointer data)
{
  static_cast<IceAdapter*>(data)->onPingPlayers();
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
  mPingPlayersTimer    = g_timeout_add(10,  ping_players_timeout,    this);
}

IceAdapter::~IceAdapter()
{
  g_source_remove(mConnectPlayersTimer);
  g_source_remove(mPingPlayersTimer);
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

void IceAdapter::onReceive(IceStream* stream, std::string const& msg)
{
  if (msg == "PING")
  {
    stream->send("PONG");
    ++mReceivedPings[stream];
    if (mReceivedPings.at(stream) % 100 == 0)
    {
      std::cout << mStreamRemoteplayerMap.at(stream) << " PING " << mReceivedPings.at(stream) << std::endl;
    }
  }
  else if (msg == "PONG")
  {
    ++mReceivedPongs[stream];
    if (mReceivedPings.at(stream) % 100 == 0)
    {
      std::cout << mStreamRemoteplayerMap.at(stream) << " PONG " << mReceivedPings.at(stream) << std::endl;
    }
  }
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
    auto const& playerId = it.key().asString();
    if (playerId == mPlayerId)
    {
      continue;
    }
    /* if there's no stream for this remotePlayer, create one */
    if (mRemoteplayerStreamMap.find(playerId) == mRemoteplayerStreamMap.end())
    {
      IceStream* iceStream = mIceAgent.createStream();
      mStreamRemoteplayerMap.insert(std::make_pair(iceStream, playerId));
      mRemoteplayerStreamMap.insert(std::make_pair(playerId, iceStream));
      mReceivedPings.insert(std::make_pair(iceStream, 0));
      mReceivedPongs.insert(std::make_pair(iceStream, 0));
      iceStream->setCandidateGatheringDoneCallback(
            std::bind(&IceAdapter::onSdp,
                      this,
                      std::placeholders::_1,
                      std::placeholders::_2)
          );
      iceStream->setReceiveCallback(
            std::bind(&IceAdapter::onReceive,
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
      if (remotePlayerSdpsIt.key().isString() &&
          remotePlayerSdpsIt.key().asString() == mPlayerId &&
          remotePlayerSdpsIt->isString())
      {
        auto streamIt = mRemoteplayerStreamMap.find(playerId);
        if (streamIt != mRemoteplayerStreamMap.end() &&
            !streamIt->second->hasRemoteSdp())
        {
          streamIt->second->setRemoteSdp(remotePlayerSdpsIt->asString());
          std::cout << "starting negotiation with remote ID " << playerId << std::endl;
        }
      }
    }
  }
}

void IceAdapter::onPingPlayers()
{
  for (auto streamIt : mRemoteplayerStreamMap)
  {
    if (streamIt.second->isConnected())
    {
      streamIt.second->send("PING");
    }
  }
}
