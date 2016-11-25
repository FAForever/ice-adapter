#include "JsonRpcTcpSession.h"

#include <sstream>
#include <streambuf>

#include "JsonRpcTcpServer.h"
#include "logging.h"


JsonRpcTcpSession::JsonRpcTcpSession(JsonRpcTcpServer* server,
                                     Glib::RefPtr<Gio::Socket> socket)
  : mSocket(socket),
    mServer(server)
{
  FAF_LOG_TRACE << "JsonRpcSession()";

  mSocket->set_blocking(false);

  Gio::signal_socket().connect(
        sigc::mem_fun(this, &JsonRpcTcpSession::onRead),
        mSocket,
        Glib::IO_IN);
}

JsonRpcTcpSession::~JsonRpcTcpSession()
{
  FAF_LOG_TRACE << "~JsonRpcSession()";
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
    FAF_LOG_ERROR << "mSocket->send: " << e.code() << ": " << e.what();
    return false;
  }
  return true;
}

bool JsonRpcTcpSession::onRead(Glib::IOCondition condition)
{
  auto doRead = [this]()
  {
    try
    {
      auto receiveCount = mSocket->receive(mReadBuffer.data(),
                                           mReadBuffer.size());
      //mMessage.insert(mMessage.end(), mReadBuffer.begin(), mReadBuffer.begin() + receiveCount);
      mMessage.append(mReadBuffer.data(),
                      receiveCount);
      return receiveCount;
    }
    catch (const Glib::Error& e)
    {
      FAF_LOG_ERROR << "mSocket->receive: " << e.code() << ": " << e.what();
      return gssize(0);
    }
  };

  try
  {
    auto receiveCount = doRead();
    if (receiveCount == 0)
    {
      //FAF_LOG_ERROR << "receiveCount == 0";
      mServer->onCloseSession(this);
      return false;
    }
    while (receiveCount == mReadBuffer.size())
    {
      receiveCount = doRead();
    }

    Json::Value jsonMessage;
    Json::Reader r;

    std::istringstream is(mMessage);
    mMessage.clear();
    while (true)
    {
      std::string doc;
      std::getline(is, doc, '\n');
      if (doc.empty())
      {
        break;
      }
      FAF_LOG_TRACE << "parsing JSON:" << doc;
      if(!r.parse(doc, jsonMessage))
      {
        FAF_LOG_TRACE << "storing doc:" << doc;
        mMessage = doc;
        break;
      }
      if (jsonMessage.isMember("method"))
      {
        Json::Value response = processRequest(jsonMessage);

        std::string responseString = Json::FastWriter().write(response);
        FAF_LOG_TRACE << "sending response:" << responseString;
        auto numSent = mSocket->send(responseString.c_str(),
                                     responseString.size());
      }
      else if (jsonMessage.isMember("error") ||
               jsonMessage.isMember("result"))
      {
        processResponse(jsonMessage);
      }
    }
  }
  catch (std::exception& e)
  {
    FAF_LOG_ERROR << "error in receive: " << e.what();
    return true;
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

  FAF_LOG_DEBUG << "dispatching JSRONRPC method '" << request["method"].asString() << "'";

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
