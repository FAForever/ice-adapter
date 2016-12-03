#include "JsonRpcTcpServer.h"

#include "logging.h"

namespace faf
{

JsonRpcTcpServer::JsonRpcTcpServer(int port):
  TcpServer(port),
  mCurrentId(0)
{
  FAF_LOG_TRACE << "JsonRpcTcpServer()";
}

JsonRpcTcpServer::~JsonRpcTcpServer()
{
  FAF_LOG_TRACE << "~JsonRpcTcpServer()";
}

void JsonRpcTcpServer::setRpcCallback(std::string const& method,
                                   RpcCallback cb)
{
  /* We allow only one callback, because there's only one result of the RPC call */
  if (mCallbacks.find(method) == mCallbacks.end())
  {
    mCallbacks.insert(std::make_pair(method, cb));
    FAF_LOG_TRACE << "callback for " << method << " registered";
  }
  else
  {
    FAF_LOG_ERROR << "RPC callback for method '" << method << "' already registered";
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

  Json::Value request;
  request["jsonrpc"] = "2.0";
  request["method"] = method;
  request["params"] = paramsArray;
  if (resultCb)
  {
    mCurrentRequests[mCurrentId] = resultCb;
    request["id"] = mCurrentId;
    ++mCurrentId;
  }
  std::string requestString = Json::FastWriter().write(request);

  for (auto it = mSessions.begin(), end = mSessions.end(); it != end; ++it)
  {
    FAF_LOG_TRACE << "sending " << requestString;

    if (!(*it)->send(requestString))
    {
      it = mSessions.erase(it);
      FAF_LOG_ERROR << "sending " << method << " failed";
    }
    else
    {
      FAF_LOG_TRACE << "done";
    }
  }
}

void JsonRpcTcpServer::parseMessage(TcpSession* session, std::vector<char>& msgBuffer)
{
  try
  {
    Json::Value jsonMessage;
    Json::Reader r;

    std::istringstream is(std::string(msgBuffer.data(), msgBuffer.size()));
    msgBuffer.clear();
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
        msgBuffer.insert(msgBuffer.end(),
                        doc.c_str(),
                        doc.c_str() + doc.size());
        break;
      }
      if (jsonMessage.isMember("method"))
      {
        Json::Value response = processRequest(jsonMessage);

        std::string responseString = Json::FastWriter().write(response);
        FAF_LOG_TRACE << "sending response:" << responseString;
        session->send(responseString);
      }
      else if (jsonMessage.isMember("error") ||
               jsonMessage.isMember("result"))
      {
        if (jsonMessage.isMember("id"))
        {
          onRpcResponse(jsonMessage["id"],
                        jsonMessage.isMember("result") ? jsonMessage["result"] : Json::Value(),
                        jsonMessage.isMember("error") ? jsonMessage["error"] : Json::Value());
        }
      }
    }
  }
  catch (std::exception& e)
  {
    FAF_LOG_ERROR << "error in receive: " << e.what();
  }
}

Json::Value JsonRpcTcpServer::processRequest(Json::Value const& request)
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
  onRpcRequest(request["method"].asString(),
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
    FAF_LOG_ERROR << "RPC callback for method '" << method << "' not found";
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

}
