#pragma once

#include <memory>
#include <map>
#include <functional>

#include <webrtc/base/asyncsocket.h>
#include <third_party/json/json.h>

namespace faf {

class JsonRpcServer : public sigslot::has_slots<>
{
public:
  JsonRpcServer();
  void listen(int port);


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

  sigslot::signal0<sigslot::multi_threaded_local> SignalClientConnected;
  sigslot::signal0<sigslot::multi_threaded_local> SignalClientDisconnected;
protected:
  void _onNewClient(rtc::AsyncSocket* socket);
  void _onClientDisconnect(rtc::AsyncSocket* socket, int);
  void _onRead(rtc::AsyncSocket* socket);
  void _parseMessage(std::string& msgBuffer, rtc::AsyncSocket* socket);
  Json::Value _processRequest(Json::Value const& request, rtc::AsyncSocket* socket);
  bool _sendJson(std::string const& message, rtc::AsyncSocket* socket);

  std::unique_ptr<rtc::AsyncSocket> _server;
  std::map<rtc::AsyncSocket*, std::shared_ptr<rtc::AsyncSocket>> _connectedSockets;
  std::array<char, 2048> _readBuffer;
  std::map<rtc::AsyncSocket*, std::string> _currentMsgs;
  std::map<int, RpcRequestResult> _currentRequests;
  std::map<std::string, RpcCallback> _callbacks;
  int _currentId;
};

} // namespace faf
