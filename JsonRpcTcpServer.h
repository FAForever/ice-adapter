#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <string>

#include <giomm.h>
#include <glibmm.h>

#include <json/json.h>

class JsonRpcTcpServer;

class JsonRpcTcpSession
{
public:
  JsonRpcTcpSession(JsonRpcTcpServer* server,
                    Glib::RefPtr<Gio::Socket> socket);
  virtual ~JsonRpcTcpSession();

protected:
  bool onRead(Glib::IOCondition /*condition*/);
  Json::Value processRequest(Json::Value const& request,
                             Json::Value & error,
                             Json::Value & id);
  void send(Json::Value const& result,
            Json::Value const& error,
            Json::Value id);

  Glib::RefPtr<Gio::Socket> mSocket;
  std::array<char, 4096> mBuffer;
  unsigned int mBufferPos;
  JsonRpcTcpServer* mServer;
};

class JsonRpcTcpServer
{
public:
  JsonRpcTcpServer(short port);
  virtual ~JsonRpcTcpServer();

  typedef std::function<void (Json::Value const& paramsArray,
                              Json::Value & result,
                              Json::Value & error)> RpcCallback;
  void setRpcCallback(std::string const& method,
                      RpcCallback cb);
protected:
  void onRpc(std::string const& method,
             Json::Value const& paramsArray,
             Json::Value & result,
             Json::Value & error);
  //void onCloseSession(std::shared_ptr<JsonRpcTcpSession> session);
  void onCloseSession(JsonRpcTcpSession* session);

  friend JsonRpcTcpSession;


  Glib::RefPtr<Gio::Socket> mListenSocket;
  std::map<std::string, RpcCallback> mCallbacks;
  std::vector<std::shared_ptr<JsonRpcTcpSession>> mSessions;

};
