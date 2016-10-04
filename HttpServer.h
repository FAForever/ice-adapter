#pragma once

#include <functional>

typedef struct _SoupServer SoupServer;
typedef struct _GHashTable GHashTable;
typedef struct _SoupMessage SoupMessage;
typedef struct SoupClientContext SoupClientContext;

class HttpServer
{
public:
  HttpServer(unsigned int httpPort);
  virtual ~HttpServer();

  typedef std::function<void (std::string const&)> JoinGameCallback;
  void setJoinGameCallback(JoinGameCallback cb);

  typedef std::function<void (std::string const&)> CreateJoinGameCallback;
  void setCreateJoinGameCallback(CreateJoinGameCallback cb);

  typedef std::function<std::string ()> StatusCallback;
  void setStatusCallback(StatusCallback cb);

  typedef std::function<void (std::string const&)> SetPlayerIdCallback;
  void setSetPlayerIdCallback(SetPlayerIdCallback cb);
protected:
  JoinGameCallback mJoinGameCallback;
  CreateJoinGameCallback mCreateJoinGameCallback;
  StatusCallback mStatusCallback;
  SetPlayerIdCallback mSetPlayerIdCallback;
  SoupServer *mServer;

  friend void handler_cb(SoupServer*,
                         SoupMessage*,
                         const char*,
                         GHashTable*,
                         SoupClientContext*,
                         void*);
};
