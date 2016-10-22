#pragma once

#include <string>
#include <array>

#include <giomm.h>

#include <json/json.h>

class JsonRpcTcpServer;

class JsonRpcTcpSession
{
public:
  JsonRpcTcpSession(JsonRpcTcpServer* server,
                    Glib::RefPtr<Gio::Socket> socket);
  virtual ~JsonRpcTcpSession();

  void sendRequest(std::string const& method,
                   Json::Value const& paramsArray,
                   int id = -1);
protected:
  bool onRead(Glib::IOCondition /*condition*/);
  Json::Value processRequest(Json::Value const& request);
  void processResponse(Json::Value const& response);
  void send(Json::Value const& result,
            Json::Value const& error,
            Json::Value id);

  Glib::RefPtr<Gio::Socket> mSocket;
  std::array<char, 4096> mBuffer;
  unsigned int mBufferEnd;
  JsonRpcTcpServer* mServer;
};
