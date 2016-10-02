#include "HttpClient.h"

#include <libsoup/soup.h>

HttpClient::HttpClient(std::string const& baseUri):
  mSession(soup_session_new()),
  mBaseUri(baseUri)
{
}

HttpClient::~HttpClient()
{
  if (mSession)
  {
    g_object_unref(mSession);
    mSession = nullptr;
  }
}

std::string HttpClient::joinGame(std::string const& gameId)
{
  SoupMessage *msg = soup_message_new("GET", (mBaseUri + "join_game").c_str());
  soup_session_send_message (mSession, msg);
  return std::string(msg->response_body->data,
                     msg->response_body->length);
}
