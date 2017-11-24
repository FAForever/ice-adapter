#pragma once

#include <string>
#include <map>
#include <memory>
#include <array>

#include <webrtc/rtc_base/asyncsocket.h>

#include "JsonRpcServer.h"
#include "JsonRpcClient.h"
#include "Process.h"
#include "Timer.h"
#include "GPGNetClient.h"
#include "Pingtracker.h"

namespace faf {

class TestClient : public sigslot::has_slots<>
{
public:
  TestClient(std::string const& login);

protected:
  void _reinit();

  void _onConnected(rtc::AsyncSocket*);
  void _onDisconnected(rtc::AsyncSocket*);

  void _rpcStartIceAdapter(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcConnectToIceAdapter(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcSendToIceAdapter(Json::Value const& paramsArray, JsonRpc::ResponseCallback result, JsonRpc::ResponseCallback error, rtc::AsyncSocket* socket);
  void _rpcSendToGpgNet(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcStatus(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcConnectToGPGNet(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcQuit(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcBindGameLobbySocket(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);
  void _rpcPingTracker(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket);

  void _onCheckConnection();
  void _onCheckIceAdapterOutput();
  void _onPeerData(rtc::AsyncSocket* socket);

  void _processIceMessage(int peerId, Json::Value const& msg);

  std::string _login;
  int _id;
  int _iceAdapterPort;
  std::vector<int> _games;
  JsonRpcClient _controlConnection;
  Process _iceAdapterProcess;
  JsonRpcClient _iceAdapterConnection;
  GPGNetClient _gpgNetClient;
  Timer _iceAdapaterOutputCheckTimer;
  Timer _reconnectTimer;
  std::unique_ptr<rtc::AsyncSocket> _gameLobbyUdpSocket;
  std::array<char, 2048> _readBuffer;
  std::map<int, std::shared_ptr<Pingtracker>> _peerIdPingtrackers;

  RTC_DISALLOW_COPY_AND_ASSIGN(TestClient);
};

} // namespace faf
