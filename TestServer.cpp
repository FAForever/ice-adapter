#include "TestServer.h"

#include <chrono>

#include <webrtc/base/thread.h>

#include "logging.h"

namespace faf {

TestServer::TestServer():
  _currentPlayerId(0)
{
  _server.SignalClientConnected.connect(this, &TestServer::_onPlayerConnected);
  _server.SignalClientDisconnected.connect(this, &TestServer::_onPlayerDisconnected);

  using namespace std::placeholders;

  _server.setRpcCallback("login", std::bind(&TestServer::_rpcLogin, this, _1, _2, _3, _4));
  _server.setRpcCallback("reconnect", std::bind(&TestServer::_rpcReconnect, this, _1, _2, _3, _4));
  _server.setRpcCallback("master", std::bind(&TestServer::_rpcMaster, this, _1, _2, _3, _4));
  _server.setRpcCallbackAsync("sendToPlayer", std::bind(&TestServer::_rpcSendToPlayer, this, _1, _2, _3, _4));
  //_server.setRpcCallback("hostGame", std::bind(&TestServer::_rpcHostGame, this, _1, _2, _3, _4));
  //_server.setRpcCallback("leaveGame", std::bind(&TestServer::_rpcLeaveGame, this, _1, _2, _3, _4));
  //_server.setRpcCallback("joinGame", std::bind(&TestServer::_rpcJoinGame, this, _1, _2, _3, _4));
  //_server.setRpcCallback("sendIceMsg", std::bind(&TestServer::_rpcSendIceMsg, this, _1, _2, _3, _4));
  _server.setRpcCallback("players", std::bind(&TestServer::_rpcPlayers, this, _1, _2, _3, _4));
  _server.setRpcCallback("onMasterEvent", std::bind(&TestServer::_rpcOnMasterEvent, this, _1, _2, _3, _4));
  _server.listen(54321, "0.0.0.0");
}

void TestServer::_onPlayerConnected(rtc::AsyncSocket* socket)
{
  FAF_LOG_DEBUG << "got connection";
}

void TestServer::_onPlayerDisconnected(rtc::AsyncSocket* socket)
{
  auto it = _socketPlayers.find(socket);
  if (it != _socketPlayers.end())
  {
    PlayerIdType leavingPlayerId = it->second;
    FAF_LOG_INFO << "logout " << leavingPlayerId;
    _disconnectPlayerFromGame(leavingPlayerId);
    _playerSockets.erase(leavingPlayerId);
    _socketPlayers.erase(it);
    _playersLogins.erase(leavingPlayerId);
  }
  _masterSockets.erase(socket);
}

void TestServer::_rpcLogin(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  if (_socketPlayers.find(socket) != _socketPlayers.end())
  {
    error = "Already logged in";
    return;
  }
  if (paramsArray.size() < 1)
  {
    error = "Need 1 parameters: login (string)";
    return;
  }

  auto loginExists = [this](std::string const& login)
  {
    for (auto it = _playersLogins.cbegin(), end = _playersLogins.cend(); it != end; ++it)
    {
      if (login == it->second)
      {
        return true;
      }
    }
    return false;
  };

  auto login = paramsArray[0].asString();
  if (loginExists(login))
  {
    int suffix = 1;
    while (true)
    {
      auto newLogin = login + std::to_string(suffix++);
      if (!loginExists(newLogin))
      {
        login = newLogin;
        break;
      }
    }
  }

  FAF_LOG_INFO << "login " << _currentPlayerId;
  _playerSockets.insert(std::make_pair(_currentPlayerId, socket));
  _socketPlayers.insert(std::make_pair(socket, _currentPlayerId));
  _playersLogins.insert(std::make_pair(_currentPlayerId, login));

  result["id"] = _currentPlayerId;
  result["login"] = login;
  result["games"] = _gamelistJson();
  ++_currentPlayerId;
}

void TestServer::_rpcReconnect(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  if (_socketPlayers.find(socket) != _socketPlayers.end())
  {
    error = "Already logged in";
    return;
  }
  if (paramsArray.size() < 1)
  {
    error = "Need 1 parameters: id (int)";
    return;
  }

  auto id = paramsArray[0].asInt();
  auto socketIt = _playerSockets.find(id);
  if (socketIt == _playerSockets.end())
  {
    error = "id not found";
    return;
  }

  _socketPlayers.erase(socketIt->second);
  _playerSockets.erase(id);
  _playerSockets.insert(std::make_pair(id, socket));
  _socketPlayers.insert(std::make_pair(socket, id));
  result = "ok";
}

void TestServer::_rpcMaster(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  _masterSockets.insert(socket);
}

void TestServer::_rpcSendToPlayer(Json::Value const& paramsArray, JsonRpc::ResponseCallback result, JsonRpc::ResponseCallback error, rtc::AsyncSocket* socket)
{
  if (_masterSockets.find(socket) == _masterSockets.end())
  {
    error("unprivileged");
    return;
  }
  if (paramsArray.size() < 3)
  {
    error("Need 3 parameters: remotePlayerId (int), commandName (string), commandArgs (array)");
    return;
  }
  auto id = paramsArray[0].asInt();
  auto playerIt = _playerSockets.find(id);
  //FAF_LOG_TRACE << "sending request " << paramsArray[1].asString() << " to player " << id;
  if (playerIt != _playerSockets.end())
  {
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    _server.sendRequest(paramsArray[1].asString(),
        paramsArray[2],
        playerIt->second,
        [result, error](Json::Value const& resultFromPlayer,
            Json::Value const& errorFromPlayer)
    {
      //FAF_LOG_TRACE << "got response for request " << paramsArray[1].asString() << " for player " << id;
      if (!resultFromPlayer.isNull())
      {
        result(resultFromPlayer);
      }
      else
      {
        error(errorFromPlayer);
      }
    });
  }
  else
  {
    error("player id not found");
  }
}

void TestServer::_rpcHostGame(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  auto it = _socketPlayers.find(socket);
  if (it != _socketPlayers.end())
  {
    PlayerIdType hostingPlayerId = it->second;
    _disconnectPlayerFromGame(hostingPlayerId);
    _games[hostingPlayerId].insert(hostingPlayerId);
    _playersGames[hostingPlayerId] = hostingPlayerId;
    {
      Json::Value params(Json::arrayValue);
      params.append("hostGame");
      Json::Value hostGameParams(Json::arrayValue);
      hostGameParams.append("testmap");
      _server.sendRequest("sendToIceAdapter",
                          params,
                          socket);
    }
    _sendGamelist();
  }
}

void TestServer::_rpcLeaveGame(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  auto it = _socketPlayers.find(socket);
  if (it != _socketPlayers.end())
  {
    _disconnectPlayerFromGame(it->second);
  }
}

void TestServer::_rpcJoinGame(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* joiningPlayerSocket)
{
  if (paramsArray.size() < 1)
  {
    error = "Need 1 parameters: remotePlayerId (int)";
    return;
  }
  auto joiningPlayerIt = _socketPlayers.find(joiningPlayerSocket);
  if (joiningPlayerIt == _socketPlayers.end())
  {
    error = "playerId not found";
    return;
  }

  PlayerIdType joiningPlayerId = joiningPlayerIt->second;
  _disconnectPlayerFromGame(joiningPlayerId);

  PlayerIdType hostingPlayerId = paramsArray[0].asInt();
  auto gameIt = _games.find(hostingPlayerId);
  if (gameIt == _games.end())
  {
    error = "game not found";
    return;
  }
  gameIt->second.insert(joiningPlayerId);
  _playersGames[joiningPlayerId] = hostingPlayerId;

  /* send "connectToPeer" to host */
  {
    Json::Value params(Json::arrayValue);
    params.append("connectToPeer");
    Json::Value connectToPeerParams(Json::arrayValue);
    connectToPeerParams.append(_playersLogins.at(joiningPlayerId));
    connectToPeerParams.append(joiningPlayerId);
    connectToPeerParams.append(true);
    params.append(connectToPeerParams);
    _server.sendRequest("sendToIceAdapter",
                        params,
                        _playerSockets[hostingPlayerId]);
  }

  /* Send "joinGame" to joining player */
  Json::Value joinGamesParams(Json::arrayValue);
  Json::Value joinGamesParamsParams(Json::arrayValue);
  joinGamesParamsParams.append(_playersLogins[hostingPlayerId]);
  joinGamesParamsParams.append(hostingPlayerId);
  joinGamesParams.append("joinGame");
  joinGamesParams.append(joinGamesParamsParams);
  _server.sendRequest("sendToIceAdapter",
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
    /* Send "connectToPeer" to fellow players to connect to joining player */
    {
      Json::Value params(Json::arrayValue);
      params.append("connectToPeer");
      Json::Value connectToPeerParams(Json::arrayValue);
      connectToPeerParams.append(_playersLogins.at(joiningPlayerId));
      connectToPeerParams.append(joiningPlayerId);
      connectToPeerParams.append(true);
      params.append(connectToPeerParams);
      _server.sendRequest("sendToIceAdapter",
                          params,
                          _playerSockets[existingPlayerId]);
    }
    /* Send "connectToPeer" to joining player to connect to fellow players   */
    {
      Json::Value params(Json::arrayValue);
      params.append("connectToPeer");
      Json::Value connectToPeerParams(Json::arrayValue);
      connectToPeerParams.append(_playersLogins.at(existingPlayerId));
      connectToPeerParams.append(existingPlayerId);
      connectToPeerParams.append(false);
      params.append(connectToPeerParams);
      _server.sendRequest("sendToIceAdapter",
                          params,
                          joiningPlayerSocket);
    }
  }
}

void TestServer::_rpcSendIceMsg(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  if (paramsArray.size() < 3)
  {
    error = "Need 3 parameters: playerId (int), remotePlayerId (int), sdp (string)";
    return;
  }
  auto remoteId = paramsArray[1].asInt();
  auto remoteIt = _playerSockets.find(remoteId);
  if (remoteIt == _playerSockets.end())
  {
    error = "remotePlayerId not found";
    return;
  }
  Json::Value params(Json::arrayValue);
  params.append("iceMsg");
  Json::Value iceMsgParams(Json::arrayValue);
  iceMsgParams.append(_socketPlayers[socket]);
  iceMsgParams.append(paramsArray[2]);
  params.append(iceMsgParams);
  _server.sendRequest("sendToIceAdapter",
                      params,
                      remoteIt->second);
}

void TestServer::_rpcPlayers(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  for (auto it = _playersLogins.begin(), end = _playersLogins.end(); it != end; ++it)
  {
    Json::Value pInfo;
    pInfo["id"] = it->first;
    pInfo["login"] = it->second;
    result.append(pInfo);
  }
}

void TestServer::_rpcOnMasterEvent(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  for (auto socket: _masterSockets)
  {
    _server.sendRequest("onMasterEvent", paramsArray, socket);
  }
}

Json::Value TestServer::_gamelistJson() const
{
  Json::Value result(Json::objectValue);
  for (auto it = _games.cbegin(), end = _games.cend(); it != end; ++it)
  {
    result[_playersLogins.at(it->first)] = it->first;
  }
  return result;
}

void TestServer::_sendGamelist(rtc::AsyncSocket* s)
{
  Json::Value params(Json::arrayValue);
  params.append(_gamelistJson());
  _server.sendRequest("onGamelist",
                      params,
                      s);
}

void TestServer::_disconnectPlayerFromGame(PlayerIdType leavingPlayerId)
{
  /* first check if the player hosts a game */
  auto hostingGameIt = _games.find(leavingPlayerId);
  if (hostingGameIt != _games.end())
  {
    for(PlayerIdType const& fellowPlayerId: hostingGameIt->second)
    {
      Json::Value params(Json::arrayValue);
      params.append(leavingPlayerId);
      _server.sendRequest("onHostLeft",
                          params,
                          _playerSockets[fellowPlayerId]);

    }
    _games.erase(hostingGameIt);
    _sendGamelist();
  }
  auto leavinPlayersGameIt = _playersGames.find(leavingPlayerId);
  if (leavinPlayersGameIt != _playersGames.end())
  {
    auto gameId = leavinPlayersGameIt->second;
    auto gameIt = _games.find(gameId);
    if (gameIt != _games.end())
    {
      /* disconnect all fellow players from the game */
      for(PlayerIdType const& fellowPlayerId: gameIt->second)
      {
        if (fellowPlayerId != leavingPlayerId)
        {
          {
            Json::Value disconnectParams(Json::arrayValue);
            disconnectParams.append(leavingPlayerId);
            Json::Value params(Json::arrayValue);
            params.append("disconnectFromPeer");
            params.append(disconnectParams);
            _server.sendRequest("sendToIceAdapter",
                                params,
                                _playerSockets[fellowPlayerId]);
          }
          {
            Json::Value disconnectParams(Json::arrayValue);
            disconnectParams.append(fellowPlayerId);
            Json::Value params(Json::arrayValue);
            params.append("disconnectFromPeer");
            params.append(disconnectParams);
            _server.sendRequest("sendToIceAdapter",
                                params,
                                _playerSockets[leavingPlayerId]);
          }
        }
      }
      gameIt->second.erase(leavingPlayerId);
    }
    _playersGames.erase(leavinPlayersGameIt);
  }
}

} // namespace faf

int main(int argc, char *argv[])
{
  faf::logging_init("debug");

  faf::TestServer server;

  rtc::Thread::Current()->Run();

  return 0;
}
