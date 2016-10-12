#include <iostream>
#include <boost/asio.hpp>
#include <json/json.h>

#include "JsonRpcServer.h"


int main(int argc, char *argv[])
{
  boost::asio::io_service io_service;
  auto server = std::make_shared<JsonRpcServer>(io_service, 5362);

  server->setRpcCallback("ping", [](Json::Value const& paramsArray,
                                    Json::Value & result,
                                    Json::Value & error)
  {
    std::cout << "ping: " << paramsArray << std::endl;
    result = "pong";
  });

  io_service.run();

}
