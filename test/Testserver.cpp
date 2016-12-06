#include "Testserver.h"

#include "TcpSession.h"
#include "logging.h"

namespace faf {

Testserver::Testserver():
  mServer(54321),
  mCurrentPlayerId(0)
{
  mServer.connectionChanged.connect([this](TcpSession* socket, ConnectionState c)
  {
    if (c == ConnectionState::Connected)
    {
      FAF_LOG_TRACE << "login " << mCurrentPlayerId;
      mPlayerSockets.insert(std::make_pair(mCurrentPlayerId, socket));
      mSocketPlayers.insert(std::make_pair(socket, mCurrentPlayerId));

      Json::Value params(Json::arrayValue);
      params.append(mCurrentPlayerId);
      mServer.sendRequest("onLogin",
                          params,
                          socket);
      ++mCurrentPlayerId;
      sendGamelist(socket);
    }
    else
    {
      auto it = mSocketPlayers.find(socket);
      if (it != mSocketPlayers.end())
      {
        FAF_LOG_TRACE << "logout " << it->second;
        mPlayerSockets.erase(it->second);
        mSocketPlayers.erase(it);
        mPlayersHostedgames.erase(it->second);
      }
    }
  });

  mServer.setRpcCallback("hostGame",
                         [this](Json::Value const& paramsArray,
                         Json::Value & result,
                         Json::Value & error,
                         Socket* socket)
  {
    if (paramsArray.size() < 1)
    {
      error = "Need 1 parameter: gameName (string)";
      return;
    }
    auto it = mSocketPlayers.find(socket);
    if (it != mSocketPlayers.end())
    {
      mGames[paramsArray[0].asString()].insert(it->second);
      mPlayersHostedgames[it->second] = paramsArray[0].asString();

      {
        Json::Value params(Json::arrayValue);
        params.append("hostGame");
        params.append(paramsArray[0].asString());
        mServer.sendRequest("sendToIceAdapter",
                            params,
                            socket);
      }

      sendGamelist();
    }
  });

  mServer.setRpcCallback("joinGame",
                         [this](Json::Value const& paramsArray,
                         Json::Value & result,
                         Json::Value & error,
                         Socket* socket)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 2 parameters: remotePlayerLogin (string), remotePlayerId (int)";
      return;
    }
    auto playerIt = mSocketPlayers.find(socket);
    if (playerIt == mSocketPlayers.end())
    {
      error = "playerId not found";
      return;
    }
    auto gameNameIt = mPlayersHostedgames.find(paramsArray[1].asInt());
    if (gameNameIt == mPlayersHostedgames.end())
    {
      error = "game not found";
      return;
    }
    auto gameIt = mGames.find(gameNameIt->second);
    if (gameIt->second.count(playerIt->second) > 0)
    {
      error = "already joined game";
      return;
    }
    Json::Value params(Json::arrayValue);
    params.append("joinGame");
    params.append(paramsArray);
    mServer.sendRequest("sendToIceAdapter",
                        params,
                        socket);
    for(PlayerIdType const& player: mGames[gameNameIt->second])
    {
      {
        Json::Value params(Json::arrayValue);
        params.append("connectToPeer");
        params.append(std::string("Player") + std::to_string(playerIt->second));
        params.append(playerIt->second);
        params.append(true);
        mServer.sendRequest("sendToIceAdapter",
                            params,
                            mPlayerSockets[player]);
      }
      {
        Json::Value params(Json::arrayValue);
        params.append("connectToPeer");
        params.append(std::string("Player") + std::to_string(player));
        params.append(player);
        params.append(false);
        mServer.sendRequest("sendToIceAdapter",
                            params,
                            socket);
      }
    }
    gameIt->second.insert(playerIt->second);
  });
}

void Testserver::sendGamelist(Socket* s)
{
  Json::Value params(Json::arrayValue);
  Json::Value gameObject(Json::objectValue);
  for (auto it = mPlayersHostedgames.cbegin(), end = mPlayersHostedgames.cend(); it != end; ++it)
  {
    gameObject[it->second] = it->first;
  }
  params.append(gameObject);
  mServer.sendRequest("onGamelist",
                      params,
                      s);
}

} // namespace faf

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

int main(int argc, char *argv[])
{
  try
  {
    Gio::init();

    faf::logging_init();

    auto loop = Glib::MainLoop::create();

    faf::Testserver server;
    loop->run();
  }
  catch (const Gio::Error& e)
  {
    FAF_LOG_ERROR << "Glib error: " << e.what();
    return 1;
  }
  catch (const std::exception& e)
  {
    FAF_LOG_ERROR << "error: " << e.what();
    return 1;
  }
  catch (...)
  {
    FAF_LOG_ERROR << "unknown error occured";
    return 1;
  }

  return 0;
}
