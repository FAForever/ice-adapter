#include "JsonRpcTcpServer.h"

#include <sigc++/sigc++.h>
#include <boost/log/trivial.hpp>

#include "JsonRpcTcpSession.h"

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

JsonRpcTcpServer::JsonRpcTcpServer(int port):
  mCurrentId(0)
{
  mListenSocket = Gio::Socket::create(Gio::SOCKET_FAMILY_IPV4,
                                      Gio::SOCKET_TYPE_STREAM,
                                      Gio::SOCKET_PROTOCOL_DEFAULT);
  mListenSocket->set_blocking(false);

  auto srcAddress =
    Gio::InetSocketAddress::create(Gio::InetAddress::create_loopback(Gio::SOCKET_FAMILY_IPV4),
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
    BOOST_LOG_TRIVIAL(trace) << "callback for " << method << " registered";
  }
  else
  {
    BOOST_LOG_TRIVIAL(error) << "RPC callback for method '" << method << "' already registered";
  }
}

void JsonRpcTcpServer::sendRequest(std::string const& method,
                                   Json::Value const& paramsArray,
                                   RpcRequestResult resultCb)
{
  if (!paramsArray.isArray())
  {
    Json::Value error = "paramsArray MUST be an array";
    if (resultCb)
    {
      resultCb(Json::Value(),
               error);
    }
    return;
  }
  if (method.empty())
  {
    Json::Value error = "method MUST not be empty";
    if (resultCb)
    {
      resultCb(Json::Value(),
               error);
    }
    return;
  }
  if (mSessions.empty())
  {
    Json::Value error = "no sessions connected";
    if (resultCb)
    {
      resultCb(Json::Value(),
               error);
    }
    return;
  }

  int id = -1;
  if (resultCb)
  {
    id = mCurrentId++;
  }
  for(auto session: mSessions)
  {
    session->sendRequest(method,
                         paramsArray,
                         id);
    if (resultCb)
    {
      mCurrentRequests[id] = resultCb;
    }
  }
}

void JsonRpcTcpServer::onRpcRequest(std::string const& method,
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

void JsonRpcTcpServer::onRpcResponse(Json::Value const& id,
                                     Json::Value const& result,
                                     Json::Value const& error)
{
  if (id.isInt())
  {
    auto reqIt = mCurrentRequests.find(id.asInt());
    if (reqIt != mCurrentRequests.end())
    {
      reqIt->second(result, error);
      mCurrentRequests.erase(reqIt);
    }
  }
}

void JsonRpcTcpServer::onCloseSession(JsonRpcTcpSession* session)
//void JsonRpcTcpServer::onCloseSession(std::shared_ptr<JsonRpcTcpSession> session)
{
  for (auto it = mSessions.begin(), end = mSessions.end(); it != end; ++it)
  {
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
