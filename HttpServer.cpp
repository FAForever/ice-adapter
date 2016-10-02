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
  else
  {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
  }
}

HttpServer::HttpServer():
  mServer(nullptr)
{
  GError *error = NULL;

  mServer = soup_server_new(SOUP_SERVER_SERVER_HEADER,
                            "faf-ice-control ",
                            NULL);
  soup_server_listen_local(mServer,
                           8080,
                           static_cast<SoupServerListenOptions>(0),
                           &error);

  soup_server_add_handler(mServer,
                          "/join_game",
                          handler_cb,
                          this,
                          NULL);
}

HttpServer::~HttpServer()
{
  g_object_unref(mServer);
}

void HttpServer::setJoinGameCallback(JoinGameCallback cb)
{
  mJoinGameCallback = cb;
}
