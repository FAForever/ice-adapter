
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <array>
#include <random>
#include <ctime>

#include <webrtc/rtc_base/ssladapter.h>
#include <webrtc/rtc_base/thread.h>
#include <webrtc/rtc_base/messagedigest.h>
#include <webrtc/rtc_base/base64.h>
#include <third_party/json/json.h>

#include "IceAdapterOptions.h"
#include "IceAdapter.h"
#include "Timer.h"
#include "logging.h"
#include "test/GPGNetClient.h"
#include "test/JsonRpcClient.h"
#include "test/Pingtracker.h"

static class Test : public sigslot::has_slots<>, public rtc::MessageHandler
{
public:
  typedef std::size_t PeerId;
  static constexpr std::size_t numPeers = 2;
  std::unique_ptr<faf::IceAdapter> _iceAdapter[numPeers];

  std::unique_ptr<faf::JsonRpcClient> _iceAdapterClient[numPeers];
  std::unique_ptr<faf::Timer> _statusTimer[numPeers];

  std::unique_ptr<faf::GPGNetClient> _gpgClient[numPeers];

  std::unique_ptr<rtc::AsyncSocket> _lobbySocket[numPeers];

  std::unique_ptr<faf::Pingtracker> _pingTracker[numPeers];


  Test()
  {
  }

  void startIceAdaptersAndClients()
  {
    for (PeerId localId = 0; localId < numPeers; ++localId)
    {
      /* detect unused tcp server ports for the ice-adapter */
      int rpcPort;
      {
        auto serverSocket = rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_STREAM);
        if (serverSocket->Bind(rtc::SocketAddress("127.0.0.1", 0)) != 0)
        {
          FAF_LOG_ERROR << "unable to bind tcp server";
          std::exit(1);
        }
        serverSocket->Listen(5);
        rpcPort = serverSocket->GetLocalAddress().port();
        delete serverSocket;
      }
      int gpgnetPort;
      {
        auto serverSocket = rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_STREAM);
        if (serverSocket->Bind(rtc::SocketAddress("127.0.0.1", 0)) != 0)
        {
          FAF_LOG_ERROR << "unable to bind tcp server";
          std::exit(1);
        }
        serverSocket->Listen(5);
        gpgnetPort = serverSocket->GetLocalAddress().port();
        delete serverSocket;
      }

      auto options = faf::IceAdapterOptions::init(localId,
                                                  std::string("Player") + std::to_string(localId));
      options.rpcPort = rpcPort;
      options.gpgNetPort = gpgnetPort;

      _iceAdapters[localId] = std::make_unique<faf::IceAdapter>(options);
      _iceAdapterClient[localId] = std::make_unique<JsonRpcClient>();
      _iceAdapterClient[localId]->connect("127.0.0.1", rpcPort);

      _iceAdapterClient[localId]->setRpcCallback("onGpgNetMessageReceived",
                              [this, localId](Json::Value const& paramsArray,
                                              Json::Value&,
                                              Json::Value&,
                                              rtc::AsyncSocket*)
      {
        if (paramsArray[0].asString() == "GameState" &&
            paramsArray[1][0].asString() == "Lobby")
        {
          if (localId == 0)
          {
            Json::Value rpcParams(Json::arrayValue);
            rpcParams.append("monument_valley");
            _iceAdapterClient[localId]->sendRequest("hostGame", rpcParams);
          }
          else
          {
            Json::Value rpcParams(Json::arrayValue);
            rpcParams.append("PLayer0");
            rpcParams.append(0);
            _iceAdapterClient[localId]->sendRequest("joinGame", rpcParams);
          }
        }
        else
        {
          //std::cout << "unhandled GPGNet msg " << paramsArray[0].asString() << " chunksize: " << paramsArray[1].size() << std::endl;
        }
      });
      _iceAdapterClient[localId]->setRpcCallback("onIceMsg",
                              [this, localId](Json::Value const& paramsArray,
                                              Json::Value&,
                                              Json::Value&,
                                              rtc::AsyncSocket*)
      {
        auto iceMsg = paramsArray[2];
        auto candidateString = iceMsg["candidate"]["candidate"].asString();
        if (iceMsg["type"].asString() == "candidate" &&
            iceMsg["candidate"]["candidate"].asString().find("relay") != std::string::npos)
        {
          return;
        }
        _iceAdapters[paramsArray[1]]->iceMsg(localId, paramsArray[2]);
      });
      _iceAdapterClient[localId]->setRpcCallback("onIceConnectionStateChanged",
                              [this](Json::Value const& paramsArray,
                              Json::Value&,
                              Json::Value&,
                              rtc::AsyncSocket*)
      {
        std::cout << "ICE connection " << paramsArray[0].asInt() << " -> " << paramsArray[1].asInt() << " changed to " << paramsArray[2].asString() << std::endl;
      });
      _iceAdapterClient[localId]->setRpcCallback("onConnected",
                              [this](Json::Value const& paramsArray,
                              Json::Value&,
                              Json::Value&,
                              rtc::AsyncSocket*)
      {
        bool connected = paramsArray[2].asBool();
        if (connected)
        {
          auto locId = paramsArray[0].asInt();
          auto remId = paramsArray[1].asInt();
          if (test.lobbySockets.count(locId) == 0)
          {
            std::cerr << "missing lobbysocket for local Id " << locId << std::endl;
            return;
          }
          if (test.directedPeerAddresses.count({locId, remId}) == 0)
          {
            std::cerr << "missing peer address for " << locId << " -> " << remId << std::endl;
            return;
          }
          std::cout << paramsArray[0].asInt() << " -> " << paramsArray[1].asInt() << " PLAYERS CONNECTED " << std::endl;

          test.directedPingTrackers.insert(
          {
                std::make_pair<Test::PeerId, Test::PeerId>(Test::PeerId(locId), Test::PeerId(remId)),
                std::make_unique<faf::Pingtracker>(locId,
                                                   remId,
                                                   test.lobbySockets.at(locId).get(),
                                                   test.directedPeerAddresses.at({locId, remId}))
          });

        }
      });

      /* create some dummy JSONRPC load */
      _statusTimer[localId] = std::make_unique<faf::Timer>();
      _statusTimer[localId]->start(100, [this]()
      {
        if (_iceAdapterClient[localId])
        {
          _iceAdapterClient[localId]->sendRequest("status");
        }
      });
    }
  }

  void restartP2PSessions();
  void restart();
  virtual void OnMessage(rtc::Message* msg) override
  {

  }
} test;
