#include <ctime>
#include <string>

#include <gio/gnetworking.h>

#include "IceAdapter.h"

char const* playerId      = "invalid";
char const* serverBaseUri = "http://localhost:5000/";
//char const* stunTurnHost  = "manchmal.erreich.bar";
//char const* stunTurnHost  = "dev.faforever.com";
char const* stunHost      = "dev.faforever.com";
char const* turnHost      = "numb.viagenie.ca";
char const* turnUser      = "mm+viagenie.ca@netlair.de";
char const* turnPassword  = "asdf";
unsigned int httpApiPort  = 8080;

static GOptionEntry entries[] =
{
  { "player-id",       'p', 0, G_OPTION_ARG_STRING, &playerId,      "ID of this player", nullptr},
  { "server-base-uri", 'b', 0, G_OPTION_ARG_STRING, &serverBaseUri, "URI of the HTTP SDP server", nullptr},
  { "stun-host",       's', 0, G_OPTION_ARG_STRING, &stunHost,      "Hostname of the STUN server", nullptr},
  { "turn-host",       't', 0, G_OPTION_ARG_STRING, &turnHost,      "Hostname of the TURN server", nullptr},
  { "turn-user",       'u', 0, G_OPTION_ARG_STRING, &turnUser,      "TURN credentials username", nullptr},
  { "turn-pass",       'u', 0, G_OPTION_ARG_STRING, &turnPassword,  "TURN credentials password", nullptr},
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

  std::string playerIdString = playerId;

  if (playerIdString == "invalid")
  {
    srand (time(NULL));
    playerIdString = std::to_string(rand() % 1000 + 1);
  }

  g_networking_init();

  IceAdapter a(playerIdString,
               serverBaseUri,
               stunHost,
               turnHost,
               turnUser,
               turnPassword,
               httpApiPort);
  a.run();

  g_option_context_free (context);

  return 0;
}
