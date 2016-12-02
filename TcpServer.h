#pragma once

#include <array>
#include <vector>
#include <string>
#include <functional>

#include <giomm.h>

namespace faf {

class TcpServer;

class TcpSession
{
public:
  TcpSession(TcpServer* server,
             Glib::RefPtr<Gio::Socket> socket);
  virtual ~TcpSession();

  void send(std::string const& msg);
protected:
  bool onRead(Glib::IOCondition condition);

  Glib::RefPtr<Gio::Socket> mSocket;
  std::array<char, 4096> mReadBuffer;
  std::vector<char> mMessage;
  TcpServer* mServer;
};

class TcpServer
{
public:
  TcpServer(int port);
  virtual ~TcpServer();

  int listenPort() const;

protected:
  virtual bool parseMessage(std::vector<char>& msgBuffer) = 0;

  void onCloseSession(TcpSession* session);

  virtual void onConnected();
  virtual void onDisconnected();

  Glib::RefPtr<Gio::Socket> mListenSocket;
  std::vector<std::shared_ptr<TcpSession>> mSessions;

  int mListenPort;

  friend TcpSession;
};

} // namespace faf
