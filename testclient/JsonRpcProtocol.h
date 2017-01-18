#pragma once

#include <functional>
#include <string>
#include <map>
#include <vector>

#include <json/json.h>

namespace faf {

class Socket;

class JsonRpcProtocol
{
public:
  JsonRpcProtocol();
  virtual ~JsonRpcProtocol();

  typedef std::function<void (Json::Value const& paramsArray,
                              Json::Value & result,
                              Json::Value & error,
                              Socket* socket)> RpcCallback;
  virtual void setRpcCallback(std::string const& method,
                              RpcCallback cb);

  typedef std::function<void (Json::Value const& result,
                              Json::Value const& error)> RpcRequestResult;
  void sendRequest(std::string const& method,
                   Json::Value const& paramsArray = Json::Value(Json::arrayValue),
                   Socket* socket = nullptr,
                   RpcRequestResult resultCb = RpcRequestResult());

protected:
  void parseMessage(Socket* socket, std::vector<char>& msgBuffer);
  virtual bool sendJson(Socket* socket, std::string const& message) = 0;
  Json::Value processRequest(Json::Value const& request, Socket* socket);

  std::map<std::string, RpcCallback> mCallbacks;
  std::map<unsigned int, RpcRequestResult> mCurrentRequests;
  int mCurrentId;
};

} // namespace faf
