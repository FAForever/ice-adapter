#include "Testserver.h"

#include "TcpSession.h"
#include "logging.h"

namespace faf {

Testserver::Testserver():
  mServer(54321, false),
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
        mHostingplayersLogins.erase(it->second);
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
      mHostingloginsGames[paramsArray[0].asString()].insert(it->second);
      mHostingplayersLogins[it->second] = paramsArray[0].asString();

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
                         Socket* joiningPlayerSocket)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 2 parameters: remotePlayerLogin (string), remotePlayerId (int)";
      return;
    }
    auto joiningPlayerIt = mSocketPlayers.find(joiningPlayerSocket);
    if (joiningPlayerIt == mSocketPlayers.end())
    {
      error = "playerId not found";
      return;
    }

    PlayerIdType joiningPlayerId = joiningPlayerIt->second;
    auto gameIt = mHostingloginsGames.find(paramsArray[0].asString());
    if (gameIt->second.count(joiningPlayerId) > 0)
    {
      error = "already joined game";
      return;
    }

    /* Send "connectToPeer" to game host */
    PlayerIdType hostingPlayerId = paramsArray[1].asInt();
    if (mHostingplayersLogins.find(hostingPlayerId) == mHostingplayersLogins.end())
    {
      error = "game not found";
      return;
    }
    {
      Json::Value params(Json::arrayValue);
      params.append("connectToPeer");
      Json::Value connectToPeerParams(Json::arrayValue);
      connectToPeerParams.append(std::string("Player") + std::to_string(joiningPlayerId));
      connectToPeerParams.append(joiningPlayerId);
      connectToPeerParams.append(true);
      params.append(connectToPeerParams);
      mServer.sendRequest("sendToIceAdapter",
                          params,
                          mPlayerSockets[hostingPlayerId]);
    }

    /* Send "joinGame" to joining player */
    Json::Value joinGamesParams(Json::arrayValue);
    joinGamesParams.append("joinGame");
    joinGamesParams.append(paramsArray);
    mServer.sendRequest("sendToIceAdapter",
                        joinGamesParams,
                        joiningPlayerSocket);
    for(PlayerIdType const& existingPlayerId: gameIt->second)
    {
      if (existingPlayerId == joiningPlayerId)
      {
        continue;
      }
      if (existingPlayerId == hostingPlayerId)
      {
        continue;
      }
      {
        Json::Value params(Json::arrayValue);
        params.append("connectToPeer");
        Json::Value connectToPeerParams(Json::arrayValue);
        connectToPeerParams.append(std::string("Player") + std::to_string(joiningPlayerId));
        connectToPeerParams.append(joiningPlayerId);
        connectToPeerParams.append(true);
        params.append(connectToPeerParams);
        mServer.sendRequest("sendToIceAdapter",
                            params,
                            mPlayerSockets[existingPlayerId]);
      }
      {
        Json::Value params(Json::arrayValue);
        params.append("connectToPeer");
        Json::Value connectToPeerParams(Json::arrayValue);
        connectToPeerParams.append(std::string("Player") + std::to_string(existingPlayerId));
        connectToPeerParams.append(existingPlayerId);
        connectToPeerParams.append(false);
        params.append(connectToPeerParams);
        mServer.sendRequest("sendToIceAdapter",
                            params,
                            joiningPlayerSocket);
      }
    }
    gameIt->second.insert(joiningPlayerIt->second);
  });
  mServer.setRpcCallback("sendSdpMessage",
                         [this](Json::Value const& paramsArray,
                         Json::Value & result,
                         Json::Value & error,
                         Socket* socket)
  {
    if (paramsArray.size() < 4)
    {
      error = "Need 2 parameters: playerId (int), remotePlayerId (int), type (string), message (string)";
      return;
    }
    auto remoteId = paramsArray[1].asInt();
    auto remoteIt = mPlayerSockets.find(remoteId);
    if (remoteIt == mPlayerSockets.end())
    {
      error = "remotePlayerId not found";
      return;
    }
    Json::Value params(Json::arrayValue);
    params.append("addSdpMessage");
    Json::Value setSdpParams(Json::arrayValue);
    setSdpParams.append(mSocketPlayers[socket]);
    setSdpParams.append(paramsArray[2]);
    setSdpParams.append(paramsArray[3]);
    params.append(setSdpParams);
    mServer.sendRequest("sendToIceAdapter",
                        params,
                        remoteIt->second);
  });
}

void Testserver::sendGamelist(Socket* s)
{
  Json::Value params(Json::arrayValue);
  Json::Value gameObject(Json::objectValue);
  for (auto it = mHostingplayersLogins.cbegin(), end = mHostingplayersLogins.cend(); it != end; ++it)
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
