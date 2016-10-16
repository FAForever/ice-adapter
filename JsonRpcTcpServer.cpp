#include "JsonRpcTcpServer.h"

#include <sigc++/sigc++.h>
#include <boost/log/trivial.hpp>

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

JsonRpcTcpSession::JsonRpcTcpSession(JsonRpcTcpServer* server,
                                     Glib::RefPtr<Gio::Socket> socket)
  : mSocket(socket),
    mBufferPos(0),
    mServer(server)
{
  BOOST_LOG_TRIVIAL(trace) << "JsonRpcSession()";

  mSocket->set_blocking(false);

  Gio::signal_socket().connect(
        sigc::mem_fun(this, &JsonRpcTcpSession::onRead),
        mSocket,
        Glib::IO_IN);
}

JsonRpcTcpSession::~JsonRpcTcpSession()
{
  BOOST_LOG_TRIVIAL(trace) << "~JsonRpcSession()";
}

bool JsonRpcTcpSession::onRead(Glib::IOCondition condition)
{
  auto numReceive = mSocket->receive(mBuffer.data() + mBufferPos,
                                     mBuffer.size() - mBufferPos);

  if (numReceive == 0)
  {
    BOOST_LOG_TRIVIAL(error) << "numReceive == 0";
    mServer->onCloseSession(this);
    return false;
  }
  BOOST_LOG_TRIVIAL(trace) << "received:" << std::string(mBuffer.data() + mBufferPos,
                                                         numReceive);
  mBufferPos += numReceive;
  Json::Value request;
  Json::Reader r;
  if (r.parse(mBuffer.data(),
              mBuffer.data() + mBufferPos,
              request))
  {
    numReceive = 0;
    Json::Value id;
    Json::Value error;
    Json::Value response = processRequest(request,
                                          error,
                                          id);

    std::string responseString = Json::FastWriter().write(response);
    BOOST_LOG_TRIVIAL(trace) << "sending response:" << responseString;
    auto numSent = mSocket->send(responseString.c_str(),
                                 responseString.size());
    BOOST_LOG_TRIVIAL(trace) << numSent << " bytes sent";
    mBufferPos = 0;
  }
  else
  {
    BOOST_LOG_TRIVIAL(warning) << "problems parsing";
  }
  if (mBufferPos >= mBuffer.size())
  {
    BOOST_LOG_TRIVIAL(error) << "buffer full!";
    mBufferPos = 0;
  }
  return true;
}

Json::Value JsonRpcTcpSession::processRequest(Json::Value const& request,
                                              Json::Value & error,
                                              Json::Value & id)
{
  if (!request.isMember("id"))
  {
    error["code"] = -1;
    error["message"] = "missing methods parameter";
    return Json::Value::null;
  }
  id = request["id"];
  if (!request.isMember("method"))
  {
    error["code"] = -1;
    error["message"] = "missing 'method' parameter";
    return Json::Value::null;
  }
  if (!request["method"].isString())
  {
    error["code"] = -1;
    error["message"] = "'method' parameter must be a string";
    return Json::Value::null;
  }

  Json::Value params(Json::arrayValue);
  if (request.isMember("params") &&
      request["params"].isArray())
  {
    params = request["params"];
  }

  Json::Value result;
  mServer->onRpc(request["method"].asString(),
                 params,
                 result,
                 error);
  Json::Value response;
  response["jsonrpc"] = "2.0";

  /* TODO: Better check for valid error/result combination */
  if (!result.isNull())
  {
    response["result"] = result;
  }
  else
  {
    response["error"] = error;
  }
  response["id"] = id;

  return response;
}


JsonRpcTcpServer::JsonRpcTcpServer(short port)
{
  mListenSocket = Gio::Socket::create(Gio::SOCKET_FAMILY_IPV4,
                                      Gio::SOCKET_TYPE_STREAM,
                                      Gio::SOCKET_PROTOCOL_DEFAULT);
  mListenSocket->set_blocking(false);

  auto srcAddress =
    Gio::InetSocketAddress::create(Gio::InetAddress::create_any(Gio::SOCKET_FAMILY_IPV4),
                                   port);
  mListenSocket->bind(srcAddress, false);
  mListenSocket->listen();
  BOOST_LOG_TRIVIAL(trace) << "JsonRpcTcpServer listening on port " << port;

  Gio::signal_socket().connect([this](Glib::IOCondition condition)
  {
    auto newSocket = mListenSocket->accept();
    auto session = std::make_shared<JsonRpcTcpSession>(this,
                                                       newSocket);
    mSessions.push_back(session);
    BOOST_LOG_TRIVIAL(trace) << "new JsonRpcTcpSession created";
    return true;
  }, mListenSocket, Glib::IO_IN);
}

JsonRpcTcpServer::~JsonRpcTcpServer()
{
  BOOST_LOG_TRIVIAL(trace) << "~JsonRpcTcpServer()";
}

void JsonRpcTcpServer::setRpcCallback(std::string const& method,
                                   RpcCallback cb)
{
  /* We allow only one callback, because there's only one result of the RPC call */
  if (mCallbacks.find(method) == mCallbacks.end())
  {
    mCallbacks.insert(std::make_pair(method, cb));
    BOOST_LOG_TRIVIAL(trace) << "callback for " << method << " registed";
  }
  else
  {
    BOOST_LOG_TRIVIAL(error) << "RPC callback for method '" << method << "' already registered";
  }
}

void JsonRpcTcpServer::onRpc(std::string const& method,
                             Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error)
{
  auto it = mCallbacks.find(method);
  if (it != mCallbacks.end())
  {
    it->second(paramsArray, result, error);
  }
  else
  {
    BOOST_LOG_TRIVIAL(error) << "RPC callback for method '" << method << "' not found";
    error = std::string("RPC callback for method '") + method + "' not found";
  }
}

void JsonRpcTcpServer::onCloseSession(JsonRpcTcpSession* session)
//void JsonRpcTcpServer::onCloseSession(std::shared_ptr<JsonRpcTcpSession> session)
{
  for (auto it = mSessions.begin(), end = mSessions.end(); it != end; ++it)
  {
    BOOST_LOG_TRIVIAL(trace) << "iterating";
    if (it->get() == session)
    {
      mSessions.erase(it);
      BOOST_LOG_TRIVIAL(trace) << "session removed";
      //delete session;
      return;
    }
  }
  //mSessions.erase(std::remove(mSessions.begin(), mSessions.end(), session), mSessions.end());
}

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>

int main(int argc, char *argv[])
{
  boost::log::core::get()->set_filter (
      boost::log::trivial::severity >= boost::log::trivial::trace
      );


  Gio::init();

  auto loop = Glib::MainLoop::create();
  JsonRpcTcpServer server(5362);

  server.setRpcCallback("ping", [](Json::Value const& paramsArray,
                                   Json::Value & result,
                                   Json::Value & error)
  {
    BOOST_LOG_TRIVIAL(trace) << "ping: " << paramsArray;
    result = "pong";
  });

  server.setRpcCallback("quit", [loop](Json::Value const& paramsArray,
                                       Json::Value & result,
                                       Json::Value & error)
  {
    BOOST_LOG_TRIVIAL(trace) << "onquit starts" << paramsArray;
    Glib::signal_idle().connect_once([loop]
    {
      BOOST_LOG_TRIVIAL(trace) << "quitting for real bro";
//      loop->quit();
    });
    result = "ok";
    loop->quit();
    BOOST_LOG_TRIVIAL(trace) << "onquit ends" << paramsArray;
  });

  loop->run();
  return 0;
}
