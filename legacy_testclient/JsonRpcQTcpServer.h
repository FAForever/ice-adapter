#pragma once

#include <vector>

#include <QtCore/QSet>
#include <QtNetwork/QTcpServer>

#include "JsonRpcProtocol.h"
#include "Socket.h"

namespace faf {

class JsonRpcQTcpSocket;

class JsonRpcQTcpServer : public QTcpServer, public JsonRpcProtocol
{
  Q_OBJECT
public:
  explicit JsonRpcQTcpServer(int port, QObject *parent = nullptr);

  virtual void setRpcCallback(std::string const& method,
                              RpcCallback cb) override;
signals:
  void newConnection(JsonRpcQTcpSocket* socket);
  void disconnected(JsonRpcQTcpSocket* socket);
protected:
  virtual bool sendJson(Socket* socket, std::string const& message) override;

  QSet<JsonRpcQTcpSocket*> mSockets;
};

} // namespace faf
