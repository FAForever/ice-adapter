#include <gio/gnetworking.h>

#include "IceAdapter.h"

char const* playerId = "1234";
char const* serverBaseUri = "http://localhost:5000/";
char const* stunTurnHost = "erreich.bar";
unsigned int httpApiPort = 8080;

static GOptionEntry entries[] =
{
  { "player-id",       'p', 0, G_OPTION_ARG_STRING, &playerId,      "ID of this player", nullptr},
  { "server-base-uri", 'b', 0, G_OPTION_ARG_STRING, &serverBaseUri, "URI of the HTTP SDP server", nullptr},
  { "stun-turn-host",  's', 0, G_OPTION_ARG_STRING, &stunTurnHost,  "Hostname of the STUN/TURN server", nullptr},
  { "http-port",       'h', 0, G_OPTION_ARG_INT,    &httpApiPort,   "Port of the internal HTTP API server", nullptr},
  { NULL }
};

int main(int argc, char *argv[])
{

  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("- test tree model performance");
  g_option_context_add_main_entries (context, entries, nullptr);
  if (!g_option_context_parse (context, &argc, &argv, &error))
  {
    g_print ("option parsing failed: %s\n", error->message);
    exit (1);
  }

  g_networking_init();

  IceAdapter a(playerId,
               serverBaseUri,
               stunTurnHost,
               httpApiPort);
  a.run();

  g_option_context_free (context);

  return 0;
}
