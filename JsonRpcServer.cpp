#include "JsonRpcServer.h"

#include "TcpSession.h"
#include "logging.h"

namespace faf
{

JsonRpcServer::JsonRpcServer(int port,
                             bool loopback):
  TcpServer(port, loopback)
{
  FAF_LOG_INFO << "JsonRpcServer() listening on port " << port;
}

JsonRpcServer::~JsonRpcServer()
{
  FAF_LOG_TRACE << "~JsonRpcServer()";
}

void JsonRpcServer::parseMessage(Socket* socket, std::vector<char>& msgBuffer)
{
  JsonRpcProtocol::parseMessage(socket, msgBuffer);
}

bool JsonRpcServer::sendJson(Socket* socket, std::string const& message)
{
  if (mSessions.empty())
  {
    FAF_LOG_ERROR << "mSessions.empty()";
    return false;
  }
  for (auto it = mSessions.begin(), end = mSessions.end(); it != end; ++it)
  {
    if (socket)
    {
      if (it->get() != socket)
      {
        continue;
      }
    }
    FAF_LOG_TRACE << "sending " << message;

    if (!(*it)->send(message))
    {
      it = mSessions.erase(it);
      FAF_LOG_ERROR << "sending " << message << " failed";
    }
    else
    {
      FAF_LOG_TRACE << "done";
    }
  }
  return true;
}

}
