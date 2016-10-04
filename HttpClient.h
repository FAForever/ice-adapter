#pragma once

#include <string>

typedef struct _SoupSession SoupSession;

class HttpClient
{
public:
  HttpClient(std::string const& baseUri);
  virtual ~HttpClient();

  std::string getPlayers(std::string const& gameId);
  std::string createGame(std::string const& gameId);
  std::string joinGame(std::string const& gameId,
                       std::string const& playerId);
  std::string setSdp(std::string const& gameId,
                     std::string const& playerId,
                     std::string const& remotePlayer,
                     std::string const& sdp);
protected:
  SoupSession *mSession;
  std::string mBaseUri;
};
