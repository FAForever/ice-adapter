#include "TestClient.h"

#include <iostream>

#include <webrtc/base/thread.h>

#if defined(WEBRTC_WIN)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#elif defined(WEBRTC_POSIX)
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#endif
#include "logging.h"

namespace faf {

TestClient::TestClient(std::string const& login):
  _login(login),
  _id(-1)
{
  using namespace std::placeholders;

  _controlConnection.setRpcCallback("startIceAdapter", std::bind(&TestClient::_rpcStartIceAdapter, this, _1, _2, _3, _4));
  _controlConnection.setRpcCallback("connectToIceAdapter", std::bind(&TestClient::_rpcConnectToIceAdapter, this, _1, _2, _3, _4));
  _controlConnection.setRpcCallback("sendToIceAdapter", std::bind(&TestClient::_rpcSendToIceAdapter, this, _1, _2, _3, _4));
  _controlConnection.setRpcCallback("iceStatus", std::bind(&TestClient::_rpcIceStatus, this, _1, _2, _3, _4));
  _controlConnection.setRpcCallback("status", std::bind(&TestClient::_rpcStatus, this, _1, _2, _3, _4));

  _controlConnection.SignalConnected.connect(this, &TestClient::_onConnected);
  _controlConnection.SignalDisconnected.connect(this, &TestClient::_onDisconnected);

  _iceAdapaterOutputCheckTimer.start(100, std::bind(&TestClient::_onCheckIceAdapterOutput, this));
  _reconnectTimer.start(1000, std::bind(&TestClient::_onCheckConnection, this));

  /* detect unused tcp server port for the ice-adapter */
  {
    auto serverSocket = rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_STREAM);
    if (serverSocket->Bind(rtc::SocketAddress("127.0.0.1", 0)) != 0)
    {
      FAF_LOG_ERROR << "unable to bind tcp server";
      std::exit(1);
    }
    serverSocket->Listen(5);
    _iceAdapterPort = serverSocket->GetLocalAddress().port();
    delete serverSocket;
  }
}

void TestClient::_onConnected(rtc::AsyncSocket* socket)
{
  auto fd = _controlConnection.GetDescriptor();
  if (fd)
  {
#if defined(WEBRTC_WIN)
    DWORD dwBytesRet=0;
    struct tcp_keepalive alive;
    alive.onoff = TRUE;
    alive.keepalivetime = 1000;
    alive.keepaliveinterval = 1000;

    if (WSAIoctl(fd,
                 SIO_KEEPALIVE_VALS,
                 &alive,
                 sizeof(alive),
                 NULL,
                 0,
                 &dwBytesRet,
                 NULL,
                 NULL) == SOCKET_ERROR)
    {
      qCritical() << "WSAIotcl(SIO_KEEPALIVE_VALS) failed: " << WSAGetLastError();
    }
#elif defined(WEBRTC_POSIX)
    int keepalive = 1;
    int keepcnt = 1;
    int keepidle = 3;
    int keepintvl = 5;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
    setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
#endif
  }
  if (_id == -1)
  {
    Json::Value params(Json::arrayValue);
    params.append(_login);
    _controlConnection.sendRequest("login", params, nullptr,
                              [this](Json::Value const& result,
                                     Json::Value const& error)
    {
      _login = result["login"].asString();
      _id = result["id"].asInt();
      _games.clear();
      auto members = result["games"].getMemberNames();
      for (auto it = members.begin(), end = members.end(); it != end; ++it)
      {
        _games.push_back(result["games"][*it].asInt());
      }
      FAF_LOG_INFO << "connected";
    });
  }
  else
  {
    Json::Value params(Json::arrayValue);
    params.append(_id);
    _controlConnection.sendRequest("reconnect", params, nullptr,
                              [this](Json::Value const& result, Json::Value const& error)
    {
      if (error.isNull())
      {
        FAF_LOG_INFO << "reconnected";
      }
      else
      {
        FAF_LOG_INFO << "error in reconnect: " << error.asString();
        _controlConnection.disconnect();
        _id = -1;
      }
    });
  }
}

void TestClient::_onDisconnected(rtc::AsyncSocket* socket)
{
  FAF_LOG_INFO << "disconnected";
}

void TestClient::_rpcStartIceAdapter(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  if (_iceAdapterProcess.isOpen())
  {
    error = "ice-adapter already started";
    return;
  }
  if (paramsArray.size() < 1)
  {
    error = "Need 1 parameter: argument array";
    return;
  }
  std::vector<std::string> args = {"--login", _login,
                                   "--id", std::to_string(_id),
                                   "--rpc-port", std::to_string(_iceAdapterPort),
                                   "--gpgnet-port", "0",
                                   "--log-level" , "debug"};
  for (Json::Value const& arg: paramsArray[0])
  {
    args.push_back(arg.asString());
  }
  _iceAdapterProcess.open("faf-ice-adapter",
                          args);
  result = "ok";
}

void TestClient::_rpcConnectToIceAdapter(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  if (paramsArray.size() < 2)
  {
    error = "Need 2 parameters: hostname, port";
    return;
  }
  auto host = paramsArray[0].asString();
  auto port = paramsArray[1].asInt();
  _iceAdapterConnection.connect(host, port);
  result = "ok";
}

void TestClient::_rpcSendToIceAdapter(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  if (paramsArray.size() < 2)
  {
    error = "Need 1 parameter: method (string), params (array)";
    return;
  }
  if (paramsArray[0].asString() == "iceMsg")
  {
    if (paramsArray[1].size() != 2)
    {
      error = "Need 2 parameters: peer (int), msg (object)";
      return;
    }
    _processIceMessage(paramsArray[1][0].asInt(),
                       paramsArray[1][1]);
  }
  else
  {
    _iceAdapterConnection.sendRequest(paramsArray[0].asString(),
                                      paramsArray[1]);
  }
}

void TestClient::_rpcIceStatus(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  if (!_iceAdapterConnection.isConnected())
  {
    error = "not connected";
    return;
  }
  bool hasStatus = false;
  _iceAdapterConnection.sendRequest("status",
                                    Json::Value(Json::arrayValue),
                                    socket,
                                    [&](Json::Value const& iceResult,
                                        Json::Value const& iceError)
  {
    if (!error.isNull())
    {
      error = iceError;
    }
    else
    {
      result = iceResult;
    }
    hasStatus = true;
  });

  while (!hasStatus)
  {
    rtc::Thread::Current()->ProcessMessages(1);
  }
}

void TestClient::_rpcStatus(Json::Value const& paramsArray, Json::Value & result, Json::Value & error, rtc::AsyncSocket* socket)
{
  result["ice_adapter_open"] = _iceAdapterProcess.isOpen();
  result["ice_adapter_connected"] = _iceAdapterConnection.isConnected();
  result["login"] = _login;
  result["id"] = _id;
}

void TestClient::_onCheckConnection()
{
  if (!_controlConnection.isConnected())
  {
    _controlConnection.connect("localhost", 54321);
  }
}

void TestClient::_onCheckIceAdapterOutput()
{
  auto output = _iceAdapterProcess.checkOutput();
  Json:: Value jsonOutputArray(Json::arrayValue);
  for (auto line: output)
  {
    jsonOutputArray.append(line);
  }
  if (jsonOutputArray.size() > 0 && _controlConnection.isConnected())
  {
    _controlConnection.sendRequest("onIceAdapterOutput", jsonOutputArray);
  }
}

void TestClient::_processIceMessage(int peerId, Json::Value const& msg)
{
  /* TODO: filter message */
  Json::Value params(Json::arrayValue);
  params.append(peerId);
  params.append(msg);
  _iceAdapterConnection.sendRequest("iceMsg",
                                    params);
}

} // namespace faf

int main(int argc, char *argv[])
{
  faf::logging_init("debug");
  std::string login;
  if (argc > 1)
  {
    login = argv[1];
  }
  else
  {
    std::cout << "Please enter nickname: ";
    std::getline(std::cin, login);
  }
  faf::TestClient client(login);

  rtc::Thread::Current()->Run();

  return 0;
}
