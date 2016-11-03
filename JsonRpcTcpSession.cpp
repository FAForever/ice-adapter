#include "JsonRpcTcpSession.h"

#include <boost/log/trivial.hpp>

#include "JsonRpcTcpServer.h"


JsonRpcTcpSession::JsonRpcTcpSession(JsonRpcTcpServer* server,
                                     Glib::RefPtr<Gio::Socket> socket)
  : mSocket(socket),
    mBufferEnd(0),
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

bool JsonRpcTcpSession::sendRequest(std::string const& method,
                                    Json::Value const& paramsArray,
                                    int id)
{
  Json::Value request;
  request["jsonrpc"] = "2.0";
  request["method"] = method;
  request["params"] = paramsArray;
  if (id != -1)
  {
    request["id"] = id;
  }
  std::string requestString = Json::FastWriter().write(request);
  try
  {
    mSocket->send(requestString.c_str(),
                  requestString.size());

  }
  catch (const Glib::Error& e)
  {
    BOOST_LOG_TRIVIAL(error) << "mSocket->send: " << e.code() << ": " << e.what();
    return false;
  }
  return true;
}

bool JsonRpcTcpSession::onRead(Glib::IOCondition condition)
{
  auto numReceive = mSocket->receive(mBuffer.data() + mBufferEnd,
                                     mBuffer.size() - mBufferEnd);

  if (numReceive == 0)
  {
    BOOST_LOG_TRIVIAL(error) << "numReceive == 0";
    mServer->onCloseSession(this);
    return false;
  }
  BOOST_LOG_TRIVIAL(trace) << "received:" << std::string(mBuffer.data() + mBufferEnd,
                                                         numReceive);
  mBufferEnd += numReceive;
  Json::Value jsonMessage;
  Json::Reader r;
  if (r.parse(mBuffer.data(),
              mBuffer.data() + mBufferEnd,
              jsonMessage))
  {
    numReceive = 0;
    if (jsonMessage.isMember("method"))
    {
      Json::Value response = processRequest(jsonMessage);

      std::string responseString = Json::FastWriter().write(response);
      BOOST_LOG_TRIVIAL(trace) << "sending response:" << responseString;
      auto numSent = mSocket->send(responseString.c_str(),
                                   responseString.size());
      BOOST_LOG_TRIVIAL(trace) << numSent << " bytes sent";
    }
    else if (jsonMessage.isMember("error") ||
             jsonMessage.isMember("result"))
    {
      processResponse(jsonMessage);
    }
    mBufferEnd = 0;
  }
  else
  {
    BOOST_LOG_TRIVIAL(warning) << "problems parsing";
  }
  if (mBufferEnd >= mBuffer.size())
  {
    BOOST_LOG_TRIVIAL(error) << "buffer full!";
    mBufferEnd = 0;
  }
  return true;
}

Json::Value JsonRpcTcpSession::processRequest(Json::Value const& request)
{
  Json::Value response;
  response["jsonrpc"] = "2.0";

  if (request.isMember("id"))
  {
    response["id"] = request["id"];
  }
  if (!request.isMember("method"))
  {
    response["error"]["code"] = -1;
    response["error"]["message"] = "missing 'method' parameter";
    return response;
  }
  if (!request["method"].isString())
  {
    response["error"]["code"] = -1;
    response["error"]["message"] = "'method' parameter must be a string";
    return response;
  }

  Json::Value params(Json::arrayValue);
  if (request.isMember("params") &&
      request["params"].isArray())
  {
    params = request["params"];
  }

  Json::Value result;
  Json::Value error;
  mServer->onRpcRequest(request["method"].asString(),
                        params,
                        result,
                        error);

  /* TODO: Better check for valid error/result combination */
  if (!result.isNull())
  {
    response["result"] = result;
  }
  else
  {
    response["error"] = error;
  }

  return response;
}

void JsonRpcTcpSession::processResponse(Json::Value const& response)
{
  if (response.isMember("id"))
  {
    mServer->onRpcResponse(response["id"],
                           response.isMember("result") ? response["result"] : Json::Value(),
                           response.isMember("error") ? response["error"] : Json::Value());
  }

}
