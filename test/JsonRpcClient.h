#pragma once

#include "Socket.h"
#include "JsonRpcProtocol.h"

#include <QtNetwork/QTcpSocket>

namespace faf {

class JsonRpcClient : public QTcpSocket, public JsonRpcProtocol, public Socket
{
  Q_OBJECT
public:
  JsonRpcClient();
  virtual ~JsonRpcClient();
protected:
  void onReadyRead();
  virtual bool send(std::string const& msg) override;
  virtual bool sendJson(Socket* socket, std::string const& message) override;

  std::vector<char> mMessage;
};

} // namespace faf
