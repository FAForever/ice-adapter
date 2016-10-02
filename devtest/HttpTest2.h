#pragma once

#include <boost/network/protocol/http/server.hpp>
#include <iostream>
#include "my_async_server.hpp"

namespace http = boost::network::http;

/*<< Defines the server. >>*/
struct Handler;
typedef http::my_server<Handler> server;

struct Handler
{
public:
  void operator() (server::request const &request,
                   server::connection_ptr connection)
  {
    server::string_type ip = source(request);
    unsigned int port = request.source_port;
    std::ostringstream data;
    data << "Hello, " << ip << ':' << port << '!';
    connection->write(data.str());
  }
};

class HttpServer
{
public:
  HttpServer();
  void run();
protected:
  Handler mHandler;
  server mServer;
};
