#pragma once

#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include <json/json.h>

using boost::asio::ip::tcp;

class JsonRpcServer;

class JsonRpcSession: public std::enable_shared_from_this<JsonRpcSession>
{
public:
  JsonRpcSession(JsonRpcServer* server,
                 tcp::socket socket);
  virtual ~JsonRpcSession();

  void start();

protected:
  void do_read();
  Json::Value processRequest(Json::Value const& request,
                             Json::Value & error,
                             Json::Value & id);
  void send(Json::Value const& result,
            Json::Value const& error,
            Json::Value id);

  tcp::socket mSocket;
  std::array<char, 4096> mBuffer;
  unsigned int mBufferPos;
  JsonRpcServer* mServer;
};

//----------------------------------------------------------------------

class JsonRpcServer
{
public:
  JsonRpcServer(boost::asio::io_service& io_service, short port);

  typedef std::function<void (Json::Value const& paramsArray,
                              Json::Value & result,
                              Json::Value & error)> RpcCallback;
  void setRpcCallback(std::string const& method,
                      RpcCallback cb);
protected:
  void do_accept();

  void onRpc(std::string const& method,
             Json::Value const& paramsArray,
             Json::Value & result,
             Json::Value & error);
  void onCloseSession(std::shared_ptr<JsonRpcSession> session);

  friend JsonRpcSession;

  tcp::acceptor mAcceptor;
  tcp::socket mSocket;

  std::map<std::string, RpcCallback> mCallbacks;
  std::vector<std::shared_ptr<JsonRpcSession>> mSessions;
};
