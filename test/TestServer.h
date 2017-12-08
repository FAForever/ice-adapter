#pragma once

#include <map>
#include <set>
#include <string>

#include <third_party/json/json.h>

#include "JsonRpcServer.h"

namespace faf {

typedef int PlayerIdType;

class TestServer : public sigslot::has_slots<>
{
public:
  TestServer();
protected:

  void _onPlayerConnected(rtc::AsyncSocket*);
  void _onPlayerDisconnected(rtc::AsyncSocket*);

  void _rpcLogin(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcReconnect(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcMaster(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcSendToPlayer(Json::Value const& paramsArray, JsonRpc::ResponseCallback, JsonRpc::ResponseCallback, rtc::AsyncSocket* socket);
  void _rpcHostGame(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcLeaveGame(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcJoinGame(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcSendIceMsg(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcPlayers(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcOnMasterEvent(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);

  Json::Value _gamelistJson() const;
  void _sendGamelist(rtc::AsyncSocket* = nullptr);
  void _disconnectPlayerFromGame(PlayerIdType leavingPlayerId);

  JsonRpcServer _server;

  PlayerIdType _currentPlayerId;
  std::map<PlayerIdType, rtc::AsyncSocket*> _playerSockets;
  std::map<rtc::AsyncSocket*, PlayerIdType> _socketPlayers;

  /* store logins of connected players */
  std::map<PlayerIdType, std::string> _playersLogins;

  /* store which players are joined which games */
  std::map<PlayerIdType, PlayerIdType> _playersGames;

  /* store which players are hosting which games */
  std::map<PlayerIdType, std::set<PlayerIdType>> _games;

  /* sockets which are allowed to use the "sendCommandToPlayer" command and get the ice-adapter output */
  std::set<rtc::AsyncSocket*> _masterSockets;

  RTC_DISALLOW_COPY_AND_ASSIGN(TestServer);
};

} // namespace faf
