#pragma once

#include <vector>
#include <string>

#include "TcpServer.h"
#include "JsonRpcProtocol.h"

namespace faf
{

class JsonRpcServer : public TcpServer, public JsonRpcProtocol
{
public:
  JsonRpcServer(int port, bool loopback = true);
  virtual ~JsonRpcServer();

protected:
  virtual void parseMessage(Socket* socket, std::vector<char>& msgBuffer) override;
  virtual bool sendJson(Socket* socket, std::string const& message) override;
};

}
