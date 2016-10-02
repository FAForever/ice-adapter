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
  typedef std::function<void ()> CreateGameCallback;
  void setCreateGameCallback(CreateGameCallback cb);
protected:
  JoinGameCallback mJoinGameCallback;
  CreateGameCallback mCreateGameCallback;
  SoupServer *mServer;

  friend void handler_cb(SoupServer*,
                         SoupMessage*,
                         const char*,
                         GHashTable*,
                         SoupClientContext*,
                         void*);
};
