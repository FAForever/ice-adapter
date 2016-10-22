#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <string>

#include <giomm.h>

#include <json/json.h>

class JsonRpcTcpSession;

class JsonRpcTcpServer
{
public:
  JsonRpcTcpServer(int port);
  virtual ~JsonRpcTcpServer();

  typedef std::function<void (Json::Value const& paramsArray,
                              Json::Value & result,
                              Json::Value & error)> RpcCallback;
  void setRpcCallback(std::string const& method,
                      RpcCallback cb);

  typedef std::function<void (Json::Value const& result,
                              Json::Value const& error)> RpcRequestResult;
  void sendRequest(std::string const& method,
                   Json::Value const& paramsArray = Json::Value(Json::arrayValue),
                   RpcRequestResult resultCb = RpcRequestResult());
protected:
  void onRpcRequest(std::string const& method,
                    Json::Value const& paramsArray,
                    Json::Value & result,
                    Json::Value & error);
  void onRpcResponse(Json::Value const& id,
                     Json::Value const& result,
                     Json::Value const& error);
  void onCloseSession(JsonRpcTcpSession* session);

  friend JsonRpcTcpSession;

  Glib::RefPtr<Gio::Socket> mListenSocket;
  std::map<std::string, RpcCallback> mCallbacks;
  std::vector<std::shared_ptr<JsonRpcTcpSession>> mSessions;
  std::map<unsigned int, RpcRequestResult> mCurrentRequests;
  int mCurrentId;
};
