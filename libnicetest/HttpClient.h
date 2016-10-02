#pragma once

#include <string>

typedef struct _SoupSession SoupSession;

class HttpClient
{
public:
  HttpClient(std::string const& baseUri);
  virtual ~HttpClient();

  std::string joinGame(std::string const& gameId);
protected:
  SoupSession *mSession;
  std::string mBaseUri;
};
