#include "IceAdapter.h"

#include <iostream>
#include <sstream>

#include <glib.h>
#include <gio/gio.h>

#include <json/json.h>

#include "IceAgent.h"


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
                       unsigned int httpPort,
                       int joinGameId):
  mMainLoop(g_main_loop_new(NULL, FALSE)),
  mHttpServer(httpPort),
  mHttpClient(httpBaseUri),
  mPlayerId(playerId),
  mTurnHost(turnHost),
  mTurnUser(turnUser),
  mTurnPassword(turnPassword),
  mJoinGameId(joinGameId)
{
  mHttpServer.setJoinGameCallback(
        std::bind(&IceAdapter::onJoinGame, this, std::placeholders::_1)
      );
  mHttpServer.setStatusCallback(
        std::bind(&IceAdapter::onStatus, this)
        );

  mConnectPlayersTimer = g_timeout_add(1000, connect_players_timeout, this);
  mPingPlayersTimer    = g_timeout_add(10,  ping_players_timeout,    this);

  auto resolver = g_resolver_get_default();

  auto stunResults = g_resolver_lookup_by_name(resolver,
                                           stunHost.c_str(),
                                           nullptr,
                                           nullptr);
  auto stunAddr = G_INET_ADDRESS (g_object_ref (stunResults->data));
  mStunIp = g_inet_address_to_string(stunAddr);
  g_object_unref(stunAddr);

  g_resolver_free_addresses (stunResults);

  auto turnResults = g_resolver_lookup_by_name(resolver,
                                               turnHost.c_str(),
                                               nullptr,
                                               nullptr);
  auto turnAddr = G_INET_ADDRESS (g_object_ref (turnResults->data));
  mTurnIp = g_inet_address_to_string(turnAddr);
  g_object_unref(turnAddr);

  g_object_unref (resolver);
}

IceAdapter::~IceAdapter()
{
  g_source_remove(mConnectPlayersTimer);
  g_source_remove(mPingPlayersTimer);
  for (auto agentIt : mRemoteplayerAgentMap)
  {
    delete agentIt.second;
  }
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
  bool ok;
  std::string jsonGameInfo = mHttpClient.joinGame(gameId,
                                                  mPlayerId,
                                                  &ok);
  if (!ok)
  {
    mHttpClient.createGame(gameId);
    jsonGameInfo = mHttpClient.joinGame(gameId,
                                        mPlayerId,
                                        &ok);
    if (!ok)
    {
      throw std::runtime_error(std::string("joinGame failed: ") + jsonGameInfo);
    }
  }
  parseGameInfoAndConnectPlayers(jsonGameInfo);
}

void IceAdapter::onSdp(IceAgent* agent, std::string const& sdp)
{
  auto remotePlayerIt = mAgentRemoteplayerMap.find(agent);
  if (remotePlayerIt == mAgentRemoteplayerMap.end())
  {
    std::cerr << "agent not found" << std::endl;
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

void IceAdapter::onReceive(IceAgent* agent, std::string const& msg)
{
  if (msg == "PING")
  {
    agent->send("PONG");
    ++mReceivedPings[agent];
    /*
    if (mReceivedPings.at(stream) % 100 == 0)
    {
      std::cout << mStreamRemoteplayerMap.at(stream) << " PING " << mReceivedPings.at(stream) << std::endl;
    }
    */
  }
  else if (msg == "PONG")
  {
    ++mReceivedPongs[agent];
    /*
    if (mReceivedPings.at(stream) % 100 == 0)
    {
      std::cout << mStreamRemoteplayerMap.at(stream) << " PONG " << mReceivedPings.at(stream) << std::endl;
    }
    */
  }
}

void IceAdapter::onConnectPlayers()
{
  if (mGameId.size() == 0)
  {
    if (mJoinGameId >= 0)
    {
      onJoinGame(std::to_string(mJoinGameId));
    }
    return;
  }
  parseGameInfoAndConnectPlayers(mHttpClient.getPlayers(mGameId));
}

void IceAdapter::onPingPlayers()
{
  for (auto agentIt : mRemoteplayerAgentMap)
  {
    if (agentIt.second->isConnected())
    {
      agentIt.second->send("PING");
      ++mSentPings[agentIt.second];
    }
  }
}

std::string IceAdapter::onStatus()
{
  std::ostringstream os;
  os << "<!doctype html>";
  os << "<html lang=\"en\">";
  os << "<head>";
  os << "<meta charset=\"utf-8\">";
  os << "<title>FAF ICE Adapter</title>";
  os << "<style type=\"text/css\">";
  os << "td {border: 1px solid black;}";
  os << "</style>";
  os << "</head>";
  os << "<body>";
  os << "<h1>My player id: " << mPlayerId << "</h1>";
  if (mGameId.size() == 0)
  {
    os << "<form action=\"/join_game\" method=\"get\">";
    os << "<input type=\"number\" id=\"name\" name=\"game_id\" value=\"0\"/>";
    os << "<button type=\"submit\">Join Game</button>";
    os << "</form>";
  }
  else
  {
    os << "<h1>In game " << mGameId << "</h1>";
    os << "<table>";
    os << "<thead>";
    os << "<tr>";
    os << "<th>Remote player</th>";
    os << "<th>Status</th>";
    os << "<th>Connection Info</th>";
    os << "<th>Sent Pings</th>";
    os << "<th>Received Pongs</th>";
    os << "<th>Received Pings</th>";
    os << "</tr>";
    os << "</thead>";
    os << "<tbody>";
    for (auto streamIt : mRemoteplayerAgentMap)
    {
      if (streamIt.second->isConnected())
      {
        os << "<tr>";
        os << "<td>" << streamIt.first << "</td>";
        os << "<td>" << (streamIt.second->isConnected() ? "connected" : "not connected") << "</td>";
        os << "<td>" << (streamIt.second->isConnected() ? streamIt.second->localCandidateInfo() + " <-> " + streamIt.second->remoteCandidateInfo() : "N/A") << "</td>";
        os << "<td>" << mSentPings.at(streamIt.second) << "</td>";
        os << "<td>" << mReceivedPongs.at(streamIt.second) << "</td>";
        os << "<td>" << mReceivedPings.at(streamIt.second) << "</td>";
        os << "</tr>";
      }
    }
    os << "</tbody>";
    os << "</table>";
  }
  os << "</body>";
  os << "</html>";

  return os.str();
}

void IceAdapter::parseGameInfoAndConnectPlayers(std::string const& jsonGameInfo)
{
  Json::Value players;
  Json::Reader r;
  if (!r.parse(jsonGameInfo, players))
  {
    std::cerr << "Invalid game info: " << jsonGameInfo;
    return;
  }
  for (auto playersIt = players.begin(), end = players.end(); playersIt != end; ++playersIt)
  {
    auto const& playerId = playersIt.key().asString();
    if (playerId == mPlayerId)
    {
      continue;
    }
    /* if there's no stream for this remotePlayer, create one */
    if (mRemoteplayerAgentMap.find(playerId) == mRemoteplayerAgentMap.end())
    {
      bool controlling = true;
      if (playersIt->isObject() && playersIt->isMember(mPlayerId))
      {
        controlling = false;
      }
      std::cout << "creating " << (controlling ? "controlling" : "non-controlling") << " agent for player " << playerId;
      auto newAgent = new IceAgent(mMainLoop,
                                   controlling,
                                   mStunIp,
                                   mTurnIp,
                                   mTurnUser,
                                   mTurnPassword);

      mAgentRemoteplayerMap.insert(std::make_pair(newAgent, playerId));
      mRemoteplayerAgentMap.insert(std::make_pair(playerId, newAgent));
      mReceivedPings.insert(std::make_pair(newAgent, 0));
      mReceivedPongs.insert(std::make_pair(newAgent, 0));
      mSentPings.insert(std::make_pair(newAgent, 0));
      newAgent->setCandidateGatheringDoneCallback(
            std::bind(&IceAdapter::onSdp,
                      this,
                      std::placeholders::_1,
                      std::placeholders::_2)
          );
      newAgent->setReceiveCallback(
            std::bind(&IceAdapter::onReceive,
                      this,
                      std::placeholders::_1,
                      std::placeholders::_2)
            );
      newAgent->gatherCandidates();
    }

    /* check if the remote player already has a SDP for us */
    for (auto remotePlayerSdpsIt = playersIt->begin(), remotePlayerSdpsEnd = playersIt->end();
         remotePlayerSdpsIt != remotePlayerSdpsEnd; ++remotePlayerSdpsIt)
    {
      if (remotePlayerSdpsIt.key().isString() &&
          remotePlayerSdpsIt.key().asString() == mPlayerId &&
          remotePlayerSdpsIt->isString())
      {
        auto streamIt = mRemoteplayerAgentMap.find(playerId);
        if (streamIt != mRemoteplayerAgentMap.end() &&
            !streamIt->second->hasRemoteSdp())
        {
          try
          {
            streamIt->second->setRemoteSdp(remotePlayerSdpsIt->asString());
          }
          catch (std::runtime_error& e)
          {
            std::cerr << "Error setting remote SDP: " << remotePlayerSdpsIt->asString() << "\n";
            gsize sdp_len;
            char* sdp = reinterpret_cast<char*>(g_base64_decode (remotePlayerSdpsIt->asString().c_str(), &sdp_len));
            std::cerr << sdp << std::endl;
            g_free (sdp);
          }
          std::cout << "starting negotiation with remote ID " << playerId << std::endl;
        }
      }
    }
  }
}
