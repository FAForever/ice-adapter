#include "JsonRpc.h"

#include "logging.h"

namespace faf {

JsonRpc::JsonRpc():
  _currentId(0)
{
}

JsonRpc::~JsonRpc()
{
}

void JsonRpc::setRpcCallback(std::string const& method,
                             RpcCallback cb)
{
  _callbacks[method] = cb;
}

void JsonRpc::sendRequest(std::string const& method,
                          Json::Value const& paramsArray,
                          rtc::AsyncSocket* socket,
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
    _currentRequests[_currentId] = resultCb;
    request["id"] = _currentId;
    ++_currentId;
  }
  std::string requestString = Json::FastWriter().write(request);

  if (!_sendMessage(requestString, socket))
  {
    Json::Value error = "send failed";
    if (resultCb)
    {
      resultCb(Json::Value(),
               error);
    }
  }
}

Json::Value JsonRpc::_parseJsonFromMsgBuffer(std::string& msgBuffer)
{
  FAF_LOG_TRACE << "parsing JSON string: " << msgBuffer;
  Json::Value result;

  if (msgBuffer.empty())
  {
    return result;
  }
  if (msgBuffer.at(0) != '{')
  {
    msgBuffer.clear();
    FAF_LOG_ERROR << "invalid JSON msg";
    return result;
  }

  bool inString = false;
  int braceNestingLevel = 0;
  std::size_t msgPos = 0;

  for (; msgPos < msgBuffer.size(); ++msgPos)
  {
    const char& c = msgBuffer.at(msgPos);
    if (c == '"')
    {
      inString = !inString;
    }

    if (!inString)
    {
      if (c == '{')
      {
        ++braceNestingLevel;
      }

      if (c == '}')
      {
        --braceNestingLevel;
        if (braceNestingLevel < 0)
        {
          msgBuffer.clear();
          FAF_LOG_ERROR << "invalid JSON msg";
          return result;
        }

        /* parse msg */
        if (braceNestingLevel == 0)
        {
          Json::Reader reader;
          if (!reader.parse(std::string(msgBuffer.cbegin(),
                                        msgBuffer.cbegin() + static_cast<std::string::difference_type>(msgPos + 1)),
                            result))
          {
            FAF_LOG_ERROR << "error parsing JSON msg: " << reader.getFormatedErrorMessages();
            msgBuffer.clear();
            return result;
          }
          if (msgPos + 1 >= msgBuffer.size())
          {
            msgBuffer.clear();
          }
          else
          {
            msgBuffer = msgBuffer.substr(msgPos + 1);
          }
          return result;
        }
      }
    }
  }
  return result;
}

void JsonRpc::_processJsonMessage(Json::Value const& jsonMessage, rtc::AsyncSocket* socket)
{
  FAF_LOG_TRACE << "processing JSON msg: " << jsonMessage.toStyledString();
  if (jsonMessage.isMember("method"))
  {
    /* this message is a request */
    Json::Value response = _processRequest(jsonMessage, socket);

    /* we don't need to respond to notifications */
    if (jsonMessage.isMember("id"))
    {
      std::string responseString = Json::FastWriter().write(response);
      FAF_LOG_TRACE << "sending response:" << responseString;
      _sendMessage(responseString, socket);
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
        auto reqIt = _currentRequests.find(jsonMessage["id"].asInt());
        if (reqIt != _currentRequests.end())
        {
          try
          {
            reqIt->second(jsonMessage.isMember("result") ? jsonMessage["result"] : Json::Value(),
                          jsonMessage.isMember("error") ? jsonMessage["error"] : Json::Value());
          }
          catch (std::exception& e)
          {
            FAF_LOG_ERROR << "exception in request handler for id " << jsonMessage["id"].asInt() << ": " << e.what();
          }
          _currentRequests.erase(reqIt);
        }
      }
    }
  }
}

Json::Value JsonRpc::_processRequest(Json::Value const& request, rtc::AsyncSocket* socket)
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

  auto it = _callbacks.find(request["method"].asString());
  if (it != _callbacks.end())
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

std::string trim_whitespace(std::string const& in)
{
  const std::string whitespace = " \t\f\v\n\r";
  auto start = in.find_first_not_of(whitespace);
  auto end = in.find_last_not_of(whitespace);
  if (start == std::string::npos ||
      end == std::string::npos)
  {
    return std::string();
  }
  return in.substr(start, end + 1);
}

void JsonRpc::_read(rtc::AsyncSocket* socket)
{
  int msgLength = 0;
  std::string& msgBuffer = _currentMsgs[socket];
  do
  {
    msgLength = socket->Recv(_readBuffer.data(), _readBuffer.size(), nullptr);
    if (msgLength > 0)
    {
      msgBuffer.append(_readBuffer.data(), std::size_t(msgLength));
    }
  }
  while (msgLength > 0);
  while (true)
  {
    msgBuffer = trim_whitespace(msgBuffer);
    if (msgBuffer.empty())
    {
      break;
    }
    Json::Value json = _parseJsonFromMsgBuffer(msgBuffer);
    if (json.isNull())
    {
      break;
    }
    _processJsonMessage(json, socket);
  }
}

} // namespace faf
