#pragma once

#include <array>
#include <memory>
#include <map>
#include <functional>

#include <webrtc/base/asyncsocket.h>
#include <third_party/json/json.h>

namespace faf {

class JsonRpc
{
public:
  JsonRpc();
  virtual ~JsonRpc();

  typedef std::function<void (Json::Value const& paramsArray,
                              Json::Value & result,
                              Json::Value & error,
                              rtc::AsyncSocket* socket)> RpcCallback;
  void setRpcCallback(std::string const& method,
                      RpcCallback cb);

  typedef std::function<void (Json::Value const& result,
                              Json::Value const& error)> RpcRequestResult;
  void sendRequest(std::string const& method,
                   Json::Value const& paramsArray = Json::Value(Json::arrayValue),
                   rtc::AsyncSocket* socket = nullptr,
                   RpcRequestResult resultCb = RpcRequestResult());

protected:
  void _read(rtc::AsyncSocket* socket);
  Json::Value _parseJsonFromMsgBuffer(std::string& msgBuffer);
  void _processJsonMessage(Json::Value const& jsonMessage, rtc::AsyncSocket* socket);
  Json::Value _processRequest(Json::Value const& request, rtc::AsyncSocket* socket);

  virtual bool _sendMessage(std::string const& message, rtc::AsyncSocket* socket) = 0;

  std::array<char, 2048> _readBuffer;
  std::map<rtc::AsyncSocket*, std::string> _currentMsgs;
  std::map<int, RpcRequestResult> _currentRequests;
  std::map<std::string, RpcCallback> _callbacks;
  int _currentId;

};

} // namespace faf
