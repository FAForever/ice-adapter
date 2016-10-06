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

std::string HttpClient::getPlayers(std::string const& gameId)
{
  SoupMessage *msg = soup_message_new("GET", (mBaseUri + "get_players/" + gameId).c_str());
  soup_session_send_message(mSession, msg);
  std::string result(msg->response_body->data,
                     static_cast<std::size_t>(msg->response_body->length));
  soup_message_finished(msg);
  return result;
}

std::string HttpClient::createGame(std::string const& gameId)
{
  SoupMessage *msg = soup_message_new("GET", (mBaseUri + "create_game/" + gameId).c_str());
  soup_session_send_message (mSession, msg);
  std::string result(msg->response_body->data,
                     static_cast<std::size_t>(msg->response_body->length));
  soup_message_finished(msg);
  return result;
}

std::string HttpClient::joinGame(std::string const& gameId,
                                 std::string const& playerId,
                                 bool* ok)
{
  SoupMessage *msg = soup_message_new("GET", (mBaseUri + "join_game/" + gameId + "/" + playerId).c_str());
  soup_session_send_message (mSession, msg);
  std::string result(msg->response_body->data,
                     static_cast<std::size_t>(msg->response_body->length));
  if (ok)
  {
    *ok = msg->status_code == 200;
  }
  soup_message_finished(msg);
  return result;
}

std::string HttpClient::setSdp(std::string const& gameId,
                               std::string const& playerId,
                               std::string const& remotePlayer,
                               std::string const& sdp)
{
  SoupMessage *msg = soup_message_new("GET", (mBaseUri +
                                              "set_sdp/" +
                                              gameId + "/" +
                                              playerId + "/" +
                                              remotePlayer + "/" +
                                              sdp).c_str());
  soup_session_send_message (mSession, msg);
  std::string result(msg->response_body->data,
                     static_cast<std::size_t>(msg->response_body->length));
  soup_message_finished(msg);
  return result;
}
