#include "JsonRpcQTcpServer.h"
#include "JsonRpcQTcpSocket.h"

namespace faf {

JsonRpcQTcpServer::JsonRpcQTcpServer(int port, QObject *parent):
  QTcpServer(parent)
{
  QTcpServer::listen(QHostAddress::Any, port);

  connect(this, &QTcpServer::newConnection, [this]()
  {
    auto qsocket = QTcpServer::nextPendingConnection();
    auto socket = new JsonRpcQTcpSocket(qsocket, this);
    for (auto it = mCallbacks.cbegin(), end = mCallbacks.cend(); it != end; ++it)
    {
      socket->setRpcCallback(it->first,
                             it->second);
    }
    QObject::connect(qsocket,
                     &QAbstractSocket::disconnected,
                     [this, socket]()
    {
      emit disconnected(socket);
      mSockets.remove(socket);
      delete socket;
    });
    mSockets.insert(socket);

    emit newConnection(socket);
  });
}

void JsonRpcQTcpServer::setRpcCallback(std::string const& method,
                                       RpcCallback cb)
{
  for(auto socket : mSockets)
  {
    socket->setRpcCallback(method,
                           cb);
  }
  JsonRpcProtocol::setRpcCallback(method,
                                  cb);
}

bool JsonRpcQTcpServer::sendJson(Socket* socket, std::string const& message)
{
  auto s = dynamic_cast<JsonRpcQTcpSocket*>(socket);
  if (mSockets.contains(s))
  {
    s->socket()->write(message.c_str(), message.size());
  }
  else
  {
    for(auto sock : mSockets)
    {
      sock->socket()->write(message.c_str(), message.size());
    }
  }
  return true;
}

} // namespace faf
