#pragma once

#include <vector>
#include <string>
#include <memory>

#include <giomm.h>


namespace faf {

enum class ConnectionState
{
  Connected,
  Disconnected
};

class Socket;
class TcpSession;

class TcpServer
{
public:
  TcpServer(int port);
  virtual ~TcpServer();

  int listenPort() const;

  int sessionCount() const;

  sigc::signal<void, TcpSession*, ConnectionState> connectionChanged;
protected:
  virtual void parseMessage(Socket* socket, std::vector<char>& msgBuffer) = 0;

  void onCloseSession(TcpSession* socket);

  Glib::RefPtr<Gio::Socket> mListenSocket;
  std::vector<std::shared_ptr<TcpSession>> mSessions;

  int mListenPort;

  friend TcpSession;
};

} // namespace faf
