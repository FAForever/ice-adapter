#include "HttpTest2.h"

#include <iostream>


HttpServer::HttpServer():
  mServer(server::options(mHandler).address("0.0.0.0").port("8080"))
{
  mServer.listen();
}

void HttpServer::run()
{
  mServer.run();
}
