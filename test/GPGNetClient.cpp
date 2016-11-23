#include "GPGNetClient.h"

GPGNetClient::GPGNetClient():
  mSocket(Gio::Socket::create(Gio::SOCKET_FAMILY_IPV4,
                              Gio::SOCKET_TYPE_STREAM,
                              Gio::SOCKET_PROTOCOL_DEFAULT))
{
  mSocket->set_blocking(false);
  Gio::signal_socket().connect(
        sigc::mem_fun(this, &GPGNetClient::onRead),
        mSocket,
        Glib::IO_IN);
}

void GPGNetClient::connect(int port)
{
  mSocket->connect(Gio::InetSocketAddress::create(
                     Gio::InetAddress::create_loopback(Gio::SOCKET_FAMILY_IPV4), port));
}

bool GPGNetClient::onRead(Glib::IOCondition)
{

}
