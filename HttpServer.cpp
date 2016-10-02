#include "HttpServer.h"

#include <cstring>
#include <iostream>

#include <libsoup/soup.h>

void handler_cb(SoupServer         *server,
                SoupMessage        *msg,
                const char         *path,
                GHashTable         *query,
                SoupClientContext  *client,
                gpointer            user_data)
{
  auto httpServer = static_cast<HttpServer*>(user_data);

  if (strcmp(path, "/join_game") == 0 &&
      httpServer->mJoinGameCallback)
  {
    auto game_id = g_hash_table_lookup(query, "game_id");
    if (!game_id)
    {
      soup_message_set_status (msg, SOUP_STATUS_BAD_REQUEST);
      return;
    }
    else
    {
      httpServer->mJoinGameCallback(static_cast<char*>(game_id));
      soup_message_set_status(msg, SOUP_STATUS_OK);
    }
  }
  else if (strcmp(path, "/create_game") == 0 &&
      httpServer->mCreateGameCallback)
  {
    httpServer->mCreateGameCallback();
    soup_message_set_status(msg, SOUP_STATUS_OK);
  }
  else
  {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
  }
}

HttpServer::HttpServer(unsigned int httpPort):
  mServer(nullptr)
{
  GError *error = NULL;

  mServer = soup_server_new(SOUP_SERVER_SERVER_HEADER,
                            "faf-ice-control ",
                            nullptr);
  soup_server_listen_local(mServer,
                           httpPort,
                           static_cast<SoupServerListenOptions>(0),
                           &error);

  soup_server_add_handler(mServer,
                          nullptr,
                          handler_cb,
                          this,
                          NULL);

  std::cout << "Internal HTTP server listening at http://127.0.0.1:" << httpPort << std::endl;
}

HttpServer::~HttpServer()
{
  g_object_unref(mServer);
}

void HttpServer::setJoinGameCallback(JoinGameCallback cb)
{
  mJoinGameCallback = cb;
}

void HttpServer::setCreateGameCallback(CreateGameCallback cb)
{
  mCreateGameCallback = cb;
}
