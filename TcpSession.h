#pragma once

#include <array>

#include <giomm.h>

#include "Socket.h"

namespace faf {

class TcpServer;

class TcpSession : public Socket
{
public:
  TcpSession(TcpServer* server,
             Glib::RefPtr<Gio::Socket> socket);
  virtual ~TcpSession();

  bool send(std::string const& msg) override;
protected:
  bool onRead(Glib::IOCondition condition);

  Glib::RefPtr<Gio::Socket> mSocket;
  std::array<char, 4096> mReadBuffer;
  std::vector<char> mMessage;
  TcpServer* mServer;
};


} // namespace faf
