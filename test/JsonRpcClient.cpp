#include "JsonRpcClient.h"

#include "logging.h"

namespace faf {

JsonRpcClient::JsonRpcClient()
{
  connect(this,
          &QTcpSocket::readyRead,
          this,
          &JsonRpcClient::onReadyRead);
}

JsonRpcClient::~JsonRpcClient()
{
}

void JsonRpcClient::onReadyRead()
{
  auto data = this->readAll();
  mMessage.insert(mMessage.end(),
                  data.begin(),
                  data.begin() + data.size());
  JsonRpcProtocol::parseMessage(this, mMessage);
}

bool JsonRpcClient::send(std::string const& msg)
{
  auto bytesSent = QTcpSocket::write(msg.c_str(), msg.size());
  return bytesSent == msg.size();
}

bool JsonRpcClient::sendJson(Socket* socket, std::string const& message)
{
  if (socket && this != socket)
  {
    FAF_LOG_ERROR << "this != socket";
    return false;
  }
  return send(message);
}


} // namespace faf
