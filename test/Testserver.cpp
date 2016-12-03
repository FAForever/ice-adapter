#include "Testserver.h"

#include "logging.h"

namespace faf {

Testserver::Testserver():
  mServer(54321),
  mCurrentPlayerId(0)
{
  mServer.connectionChanged.connect([this](TcpSession* session, ConnectionState c)
  {
    if (c == ConnectionState::Connected)
    {
      FAF_LOG_TRACE << "login " << mCurrentPlayerId;
      mPlayerSessions.insert(std::make_pair(mCurrentPlayerId, session));
      mSessionPlayers.insert(std::make_pair(session, mCurrentPlayerId));

      Json::Value params(Json::arrayValue);
      params.append(mCurrentPlayerId);
      mServer.sendRequest("onLogin",
                          params,
                          session);
      ++mCurrentPlayerId;
    }
    else
    {
      auto it = mSessionPlayers.find(session);
      if (it != mSessionPlayers.end())
      {
        mPlayerSessions.erase(it->second);
        mSessionPlayers.erase(it);
      }
    }
  });

  mServer.setRpcCallback("hostGame",
                         [this](Json::Value const& paramsArray,
                         Json::Value & result,
                         Json::Value & error,
                         TcpSession* session)
  {
    if (paramsArray.size() < 1)
    {
      error = "Need 1 parameter: gameName (string)";
      return;
    }
    auto it = mSessionPlayers.find(session);
    if (it != mSessionPlayers.end())
    {
      mGames[paramsArray[0].asString()].insert(it->second);

      Json::Value params(Json::arrayValue);
      params.append("hostGame");
      params.append(paramsArray[0].asString());
      mServer.sendRequest("sendToIceAdapter",
                          params,
                          session);
    }
  });

  mServer.setRpcCallback("joinGame",
                         [this](Json::Value const& paramsArray,
                         Json::Value & result,
                         Json::Value & error,
                         TcpSession* session)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 2 parameters: remotePlayerLogin (string), remotePlayerId (int)";
      return;
    }
    auto it = mSessionPlayers.find(session);
    if (it != mSessionPlayers.end())
    {
      mGames[paramsArray[0].asString()].insert(it->second);

      Json::Value params(Json::arrayValue);
      params.append("hostGame");
      params.append(paramsArray[0].asString());
      mServer.sendRequest("sendToIceAdapter",
                          params,
                          session);
    }
  });
}

} // namespace faf
