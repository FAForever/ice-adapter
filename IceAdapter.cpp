#include "IceAdapter.h"

#include <iostream>
#include <sstream>

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
  mHttpServer.setCreateJoinGameCallback(
        std::bind(&IceAdapter::onCreateJoinGame, this, std::placeholders::_1)
        );
  mHttpServer.setStatusCallback(
        std::bind(&IceAdapter::onStatus, this)
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
  parseGameInfoAndConnectPlayers(
        mHttpClient.joinGame(gameId,
                             mPlayerId));
}

void IceAdapter::onCreateJoinGame(std::string const& gameId)
{
  std::cout << "IceAdapter::onCreateJoinGame " << gameId << std::endl;
  auto response = mHttpClient.createGame(gameId);
  if (response != "OK")
  {
    throw std::runtime_error(std::string("createGame failed: ") + response);
  }
  onJoinGame(gameId);
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
    /*
    if (mReceivedPings.at(stream) % 100 == 0)
    {
      std::cout << mStreamRemoteplayerMap.at(stream) << " PING " << mReceivedPings.at(stream) << std::endl;
    }
    */
  }
  else if (msg == "PONG")
  {
    ++mReceivedPongs[stream];
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
    return;
  }
  parseGameInfoAndConnectPlayers(mHttpClient.getPlayers(mGameId));
}

void IceAdapter::onPingPlayers()
{
  for (auto streamIt : mRemoteplayerStreamMap)
  {
    if (streamIt.second->isConnected())
    {
      streamIt.second->send("PING");
      ++mSentPings[streamIt.second];
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

    os << "<form action=\"/create_join_game\" method=\"get\">";
    os << "<input type=\"number\" id=\"name\" name=\"game_id\" value=\"0\"/>";
    os << "<button type=\"submit\">Create & Join Game</button>";
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
    for (auto streamIt : mRemoteplayerStreamMap)
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
      mSentPings.insert(std::make_pair(iceStream, 0));
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
    for (auto remotePlayerSdpsIt = it->begin(), remotePlayerSdpsEnd = it->end();
         remotePlayerSdpsIt != remotePlayerSdpsEnd; ++remotePlayerSdpsIt)
    {
      if (remotePlayerSdpsIt.key().isString() &&
          remotePlayerSdpsIt.key().asString() == mPlayerId &&
          remotePlayerSdpsIt->isString())
      {
        auto streamIt = mRemoteplayerStreamMap.find(playerId);
        if (streamIt != mRemoteplayerStreamMap.end() &&
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
