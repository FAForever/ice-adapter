#include "JsonRpcServer.h"

#include <iostream>

#include <boost/log/trivial.hpp>

JsonRpcSession::JsonRpcSession(JsonRpcServer* server,
                               tcp::socket socket)
  : mSocket(std::move(socket)),
    mBufferPos(0),
    mServer(server)
{
  BOOST_LOG_TRIVIAL(trace) << "JsonRpcSession()";
}

JsonRpcSession::~JsonRpcSession()
{
  BOOST_LOG_TRIVIAL(trace) << "~JsonRpcSession()";
  boost::system::error_code ignored_ec;
  mSocket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
}

void JsonRpcSession::start()
{
  do_read();
}

void JsonRpcSession::do_read()
{
  auto self(shared_from_this());
  mSocket.async_read_some(boost::asio::buffer(mBuffer.data() + mBufferPos,
                                              mBuffer.size() - mBufferPos),
      [self](boost::system::error_code ec, std::size_t length)
      {
        if (!ec)
        {
          self->mBufferPos += length;
          Json::Value request;
          Json::Reader r;
          if (r.parse(self->mBuffer.data(),
                      self->mBuffer.data() + self->mBufferPos,
                      request))
          {
            self->mBufferPos = 0;
            Json::Value id;
            Json::Value error;
            Json::Value response = self->processRequest(request,
                                                        error,
                                                        id);

            std::string responseString = Json::FastWriter().write(response);
            self->mSocket.write_some(boost::asio::buffer(responseString));

          }
          else
          {
            std::cout << "problems parsing" << std::endl;
          }
          if (self->mBufferPos >= self->mBuffer.size())
          {
            std::cerr << "buffer full!" << std::endl;
            self->mBufferPos = 0;
          }
          self->do_read();
        }
        else
        {
          self->mServer->onCloseSession(self);
        }
      });
}

Json::Value JsonRpcSession::processRequest(Json::Value const& request,
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

JsonRpcServer::JsonRpcServer(boost::asio::io_service& io_service, short port)
  : mIoService(io_service),
    mAcceptor(io_service, tcp::endpoint(tcp::v6(), port)),
    mSocket(io_service)
{
  do_accept();

  setRpcCallback("quit",
                 [this](Json::Value const& paramsArray,
                    Json::Value & result,
                    Json::Value & error)
      {
        //TODO: am I doing this right?
        result = "ok";
        BOOST_LOG_TRIVIAL(trace) << "JsonRpcServer: quit";
        mIoService.post([this]()
        {
          BOOST_LOG_TRIVIAL(trace) << "closing socket";
          mSessions.clear();
          mCallbacks.clear();
          mSocket.close();
          mIoService.stop();
        });
      });
  BOOST_LOG_TRIVIAL(trace) << "JsonRpcServer()";
}

JsonRpcServer::~JsonRpcServer()
{
  BOOST_LOG_TRIVIAL(trace) << "~JsonRpcServer()";
}

void JsonRpcServer::setRpcCallback(std::string const& method,
                                   RpcCallback cb)
{
  /* We allow only one callback, because there's only one result of the RPC call */
  if (mCallbacks.find(method) == mCallbacks.end())
  {
    mCallbacks.insert(std::make_pair(method, cb));
  }
  else
  {
    BOOST_LOG_TRIVIAL(error) << "RPC callback for method '" << method << "' already registered";
  }
}

void JsonRpcServer::do_accept()
{
  mAcceptor.async_accept(mSocket,
      [this](boost::system::error_code ec)
      {
        if (!ec)
        {
          auto session = std::make_shared<JsonRpcSession>(this,
                                                          std::move(mSocket));
          session->start();
          mSessions.push_back(session);
        }

        do_accept();
      });
}

void JsonRpcServer::onRpc(std::string const& method,
                          Json::Value const& paramsArray,
                          Json::Value & result,
                          Json::Value & error)
{
  auto it = mCallbacks.find(method);
  if (it != mCallbacks.end())
  {
    it->second(paramsArray, result, error);
  }
}
void JsonRpcServer::onCloseSession(std::shared_ptr<JsonRpcSession> session)
{
  mSessions.erase(std::remove(mSessions.begin(), mSessions.end(), session), mSessions.end());
}
