#include "GPGNetClient.h"

#include "logging.h"

namespace faf
{

GPGNetClient::GPGNetClient()
{
  connect(this,
          &QTcpSocket::readyRead,
          this,
          &GPGNetClient::onReadyRead);

  connect(this,
          &QTcpSocket::connected,
          this,
          &GPGNetClient::onConnected);
}

void GPGNetClient::onReadyRead()
{
  auto data = this->readAll();
  FAF_LOG_TRACE << "GPGNetClient received: " << data.toStdString();
  mMessage.insert(mMessage.end(),
                  data.begin(),
                  data.begin() + data.size());
  GPGNetMessage::parse(mMessage, [this](GPGNetMessage const& msg)
  {
    onGPGNetMessage(msg);
  });
}

void GPGNetClient::onConnected()
{
  GPGNetMessage msg;
  msg.header = "GameState";
  msg.chunks.push_back("Idle");

  auto binMsg = msg.toBinary();
  QTcpSocket::write(binMsg.c_str(), binMsg.size());
}

}
