#include "JsonRpcProtocol.h"

#include "Socket.h"
#include "logging.h"

namespace faf {

JsonRpcProtocol::JsonRpcProtocol():
  mCurrentId(0)
{
  FAF_LOG_TRACE << "JsonRpcProtocol()";
}

JsonRpcProtocol::~JsonRpcProtocol()
{
  FAF_LOG_TRACE << "~JsonRpcProtocol()";
}

void JsonRpcProtocol::setRpcCallback(std::string const& method,
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

void JsonRpcProtocol::sendRequest(std::string const& method,
                                   Json::Value const& paramsArray,
                                   Socket* socket,
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

  if (!this->sendJson(socket, requestString))
  {
    Json::Value error = "send failed";
    if (resultCb)
    {
      resultCb(Json::Value(),
               error);
    }
  }
}

/* https://github.com/joncol/jcon-cpp/blob/master/src/jcon/json_rpc_endpoint.cpp#L107 */
QByteArray JsonRpcProtocol::processBuffer(Socket* socket, QByteArray const& msgBuffer)
{
  if (msgBuffer.isEmpty())
  {
    return QByteArray();
  }
  QByteArray buf(msgBuffer);

  if (buf[0] != '{')
  {
    FAF_LOG_ERROR << "buf[0] == '{' expected: " << msgBuffer.toStdString();
    return QByteArray();
  }

  bool in_string = false;
  int brace_nesting_level = 0;

  int i = 0;
  while (i < buf.size() )
  {
    const char curr_ch = buf[i++];

    if (curr_ch == '"')
        in_string = !in_string;

    if (!in_string)
    {
      if (curr_ch == '{')
          ++brace_nesting_level;

      if (curr_ch == '}')
      {
        --brace_nesting_level;
        if (brace_nesting_level < 0)
        {
          FAF_LOG_ERROR << "error parsing " << msgBuffer.toStdString() << ": brace_nesting_level < 0";
          return QByteArray();
        }

        if (brace_nesting_level == 0) {
          Json::Value jsonMessage;
          Json::Reader r;
          FAF_LOG_TRACE << "parsing JSON:" << std::string(buf.data(), i);
          if(!r.parse(buf.data(),
                      buf.data() + i,
                      jsonMessage))
          {
            return buf;
          }
          if (jsonMessage.isMember("method"))
          {
            /* this message is a request */
            Json::Value response = processRequest(jsonMessage, socket);

            /* we don't need to respond to notifications */
            if (jsonMessage.isMember("id"))
            {
              std::string responseString = Json::FastWriter().write(response);
              FAF_LOG_TRACE << "sending response:" << responseString;
              socket->send(responseString);
            }
          }
          else if (jsonMessage.isMember("error") ||
                   jsonMessage.isMember("result"))
          {
            /* this message is a response */
            if (jsonMessage.isMember("id"))
            {
              if (jsonMessage["id"].isInt())
              {
                auto reqIt = mCurrentRequests.find(jsonMessage["id"].asInt());
                if (reqIt != mCurrentRequests.end())
                {
                  try
                  {
                    reqIt->second(jsonMessage.isMember("result") ? jsonMessage["result"] : Json::Value(),
                                  jsonMessage.isMember("error") ? jsonMessage["error"] : Json::Value());
                  }
                  catch (std::exception& e)
                  {
                    FAF_LOG_ERROR << "exception in request handler for id " << jsonMessage["id"].asInt() << ": " << e.what() << "\nmessage was: " << std::string(buf.data(), i);
                  }
                  mCurrentRequests.erase(reqIt);
                }
              }
            }
          }
          buf = buf.mid(i).trimmed();
          i = 0;
          continue;
        }
      }
    }
  }
  return buf;
}

Json::Value JsonRpcProtocol::processRequest(Json::Value const& request, Socket* socket)
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

  FAF_LOG_TRACE << "dispatching JSRONRPC method '" << request["method"].asString() << "'";

  Json::Value params(Json::arrayValue);
  if (request.isMember("params") &&
      request["params"].isArray())
  {
    params = request["params"];
  }

  Json::Value result;
  Json::Value error;

  auto it = mCallbacks.find(request["method"].asString());
  if (it != mCallbacks.end())
  {
    try
    {
      it->second(params, result, error, socket);
    }
    catch (std::exception& e)
    {
      FAF_LOG_ERROR << "exception in callback for method '" << request["method"].asString() << "': " << e.what();
    }
  }
  else
  {
    FAF_LOG_ERROR << "RPC callback for method '" << request["method"].asString() << "' not found";
    error = std::string("RPC callback for method '") + request["method"].asString() + "' not found";
  }

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


} // namespace faf
