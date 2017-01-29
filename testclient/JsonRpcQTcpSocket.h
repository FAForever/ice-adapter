#pragma once

#include "Socket.h"
#include "JsonRpcProtocol.h"

#include <QtNetwork/QTcpSocket>

namespace faf {

class JsonRpcQTcpSocket : public QObject, public JsonRpcProtocol, public Socket
{
  Q_OBJECT
public:
  JsonRpcQTcpSocket(QObject* parent = nullptr);
  JsonRpcQTcpSocket(QTcpSocket* socket, QObject* parent = nullptr);
  virtual ~JsonRpcQTcpSocket();
  QTcpSocket* socket() const;
protected:
  void init();
  virtual bool send(std::string const& msg) override;
  virtual bool sendJson(Socket* socket, std::string const& message) override;

  QByteArray mBuffer;
  QTcpSocket* mSocket;
  bool mOwnSocket;
};

} // namespace faf
