#include "JsonRpcQTcpSocket.h"

#include "logging.h"

namespace faf {

JsonRpcQTcpSocket::JsonRpcQTcpSocket(QObject* parent):
  QObject(parent),
  mSocket(new QTcpSocket),
  mOwnSocket(true)
{
  QObject::connect(mSocket,
                   &QTcpSocket::readyRead,
                   [this]()
  {
    auto data = mSocket->readAll();
    mMessage.insert(mMessage.end(),
                    data.begin(),
                    data.begin() + data.size());
    JsonRpcProtocol::parseMessage(this, mMessage);
  });
}

JsonRpcQTcpSocket::JsonRpcQTcpSocket(QTcpSocket* socket, QObject* parent):
  QObject(parent),
  mSocket(socket),
  mOwnSocket(false)
{
  QObject::connect(mSocket,
                   &QTcpSocket::readyRead,
                   [this]()
  {
    auto data = mSocket->readAll();
    mMessage.insert(mMessage.end(),
                    data.begin(),
                    data.begin() + data.size());
    JsonRpcProtocol::parseMessage(this, mMessage);
  });
}

JsonRpcQTcpSocket::~JsonRpcQTcpSocket()
{
  if (mOwnSocket)
  {
    delete mSocket;
  }
}

QTcpSocket* JsonRpcQTcpSocket::socket() const
{
  return mSocket;
}

bool JsonRpcQTcpSocket::send(std::string const& msg)
{
  auto bytesSent = mSocket->write(msg.c_str(), msg.size());
  return bytesSent == msg.size();
}

bool JsonRpcQTcpSocket::sendJson(Socket* socket, std::string const& message)
{
  if (socket && this != socket)
  {
    FAF_LOG_ERROR << "this != socket";
    return false;
  }
  return send(message);
}


} // namespace faf
