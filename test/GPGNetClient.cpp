#include "GPGNetClient.h"

#include <webrtc/rtc_base/thread.h>

#include "GPGNetMessage.h"

namespace faf {

GPGNetClient::GPGNetClient()
{
}

void GPGNetClient::connect(std::string const& host, int port)
{
  _socket.reset(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_STREAM));
  _socket->SignalConnectEvent.connect(this, &GPGNetClient::_onConnected);
  _socket->SignalReadEvent.connect(this, &GPGNetClient::_onRead);
  _socket->SignalCloseEvent.connect(this, &GPGNetClient::_onDisconnected);
  _socket->Connect(rtc::SocketAddress(host, port));
}

void GPGNetClient::disconnect()
{
  if (_socket)
  {
    _socket->Close();
  }
  _socket.reset(nullptr);
}

bool GPGNetClient::isConnected() const
{
  return _socket && _socket->GetState() == rtc::AsyncSocket::CS_CONNECTED;
}

void GPGNetClient::setCallback(Callback cb)
{
  _cb = cb;
}

void GPGNetClient::sendMessage(GPGNetMessage const& msg)
{
  if (_socket)
  {
    auto msgBytes = msg.toBinary();
    _socket->Send(msgBytes.c_str(), msgBytes.size());
  }
}

void GPGNetClient::_onConnected(rtc::AsyncSocket* socket)
{
  SignalConnected.emit(_socket.get());
}

void GPGNetClient::_onRead(rtc::AsyncSocket* socket)
{
  static char buffer[2048];

  int msgLength = 0;
  do
  {
    msgLength = socket->Recv(buffer, 2048, nullptr);
    if (msgLength > 0)
    {
      _messageBuffer.append(buffer, std::size_t(msgLength));
    }
  }
  while (msgLength > 0);
  GPGNetMessage::parse(_messageBuffer, [&](const GPGNetMessage& msg)
  {
    if (_cb)
    {
      _cb(msg);
    }
  });
}

void GPGNetClient::_onDisconnected(rtc::AsyncSocket* socket, int)
{
  SignalDisconnected.emit(_socket.get());
}


} // namespace faf
