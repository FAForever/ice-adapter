
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
#include <webrtc/third_party/jsoncpp/source/include/json/json.h>

#include "IceAdapterOptions.h"
#include "IceAdapter.h"
#include "Timer.h"
#include "logging.h"
#include "test/GPGNetClient.h"
#include "test/JsonRpcClient.h"
#include "test/Pingtracker.h"

class GPGNetHandler;
class IceAdapterHandler;

static class Test : public sigslot::has_slots<>, public rtc::MessageHandler
{
public:
  /* number of simulated peers. Note that the number of PeerConnections will be
   * 2 * (N-1)^2, so don't overdo it */
  static constexpr std::size_t numPeers = 2;
  typedef std::size_t PeerId;

  std::unordered_map<PeerId, std::unique_ptr<faf::IceAdapter>> iceAdapters;

  std::unordered_map<PeerId, std::unique_ptr<GPGNetHandler>> gpgnetClients;

  std::unordered_map<PeerId, std::unique_ptr<IceAdapterHandler>> clients;

  std::unordered_map<PeerId, std::unique_ptr<rtc::AsyncSocket>> lobbySockets;

  std::unordered_map<PeerId, std::unordered_set<PeerId>> offers;
  struct PeerIdPairHash {
  public:
    std::size_t operator()(const std::pair<PeerId, PeerId> &x) const
    {
      return std::hash<PeerId>()(x.first) ^ std::hash<PeerId>()(x.second);
    }
  };
  std::unordered_map<std::pair<PeerId, PeerId>, std::unique_ptr<faf::Pingtracker>, PeerIdPairHash> directedPingTrackers;
  std::unordered_map<std::pair<PeerId, PeerId>, rtc::SocketAddress, PeerIdPairHash> directedPeerAddresses;

  void restartP2PSessions();
  void restart();
  virtual void OnMessage(rtc::Message* msg) override
  {

  }
} test;

class IceAdapterHandler : public sigslot::has_slots<>
{
  std::unique_ptr<faf::JsonRpcClient> _client;

public:
  bool hosting;
  Test::PeerId localId;
  faf::Timer statusTimer;

  IceAdapterHandler(int port, bool hosting):
    _client(std::make_unique<faf::JsonRpcClient>()),
    hosting(hosting)
  {
    _client->SignalConnected.connect(this, &IceAdapterHandler::_onClientConnected);
    _client->connect("127.0.0.1", port);
    _client->setRpcCallback("onGpgNetMessageReceived",
                            [this](Json::Value const& paramsArray,
                                   Json::Value&,
                                   Json::Value&,
                                   rtc::AsyncSocket*)
    {
      if (paramsArray[0].asString() == "GameState" &&
          paramsArray[1][0].asString() == "Lobby")
      {
        if (this->hosting)
        {
          Json::Value rpcParams(Json::arrayValue);
          rpcParams.append("monument_valley");
          _client->sendRequest("hostGame", rpcParams);
        }
        else
        {
          Json::Value rpcParams(Json::arrayValue);
          rpcParams.append(test.iceAdapters.at(0)->options().localPlayerLogin);
          rpcParams.append(test.iceAdapters.at(0)->options().localPlayerId);
          _client->sendRequest("joinGame", rpcParams);
        }
      }
      else
      {
        //std::cout << "unhandled GPGNet msg " << paramsArray[0].asString() << " chunksize: " << paramsArray[1].size() << std::endl;
      }
    });
    _client->setRpcCallback("onIceMsg",
                            [this](Json::Value const& paramsArray,
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
      test.iceAdapters.at(paramsArray[1].asInt())->iceMsg(localId, paramsArray[2]);
    });
    _client->setRpcCallback("onIceConnectionStateChanged",
                            [this](Json::Value const& paramsArray,
                            Json::Value&,
                            Json::Value&,
                            rtc::AsyncSocket*)
    {
      std::cout << "ICE connection " << paramsArray[0].asInt() << " -> " << paramsArray[1].asInt() << " changed to " << paramsArray[2].asString() << std::endl;
    });
    _client->setRpcCallback("onConnected",
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
    statusTimer.start(100, [this]()
    {
      _client->sendRequest("status");
    });
  }

  void connectPeers()
  {
    for (Test::PeerId remoteId = 0; remoteId < test.numPeers; ++remoteId)
    {
      if (localId != remoteId &&
          remoteId != 0)
      {
        bool isOfferer = test.offers.count(localId) ? test.offers.at(localId).count(remoteId) == 0 : true;
        test.offers[localId].insert(remoteId);
        test.offers[remoteId].insert(localId);

        Json::Value rpcParams(Json::arrayValue);
        rpcParams.append(test.iceAdapters.at(remoteId)->options().localPlayerLogin);
        rpcParams.append(test.iceAdapters.at(remoteId)->options().localPlayerId);
        rpcParams.append(isOfferer);
        _client->sendRequest("connectToPeer", rpcParams);
      }
    }
  }
protected:
  void _onClientConnected(rtc::AsyncSocket* socket)
  {
    _client->sendRequest("status", Json::Value(Json::arrayValue), nullptr, [this] (Json::Value const& result, Json::Value const&)
    {
      localId = result["options"]["player_id"].asInt();
    });

    char digest[64];
    std::time_t authValidUntil = std::time(nullptr) + 3600*24;
    std::string authUsername = std::to_string(authValidUntil) + ":mylogin";
    auto digestSize = rtc::ComputeHmac(rtc::DIGEST_SHA_1,
                                       "banana",
                                       sizeof("banana"),
                                       authUsername.c_str(),
                                       authUsername.length(),
                                       digest,
                                       sizeof(digest));
    auto authToken = rtc::Base64::Encode(std::string(digest, digestSize));

    Json::Value iceServer;
    iceServer["urls"].append("turn:vmrbg145.informatik.tu-muenchen.de?transport=tcp");
    iceServer["urls"].append("turn:vmrbg145.informatik.tu-muenchen.de?transport=udp");
    iceServer["urls"].append("stun:vmrbg145.informatik.tu-muenchen.de");
    iceServer["username"] = authUsername;
    iceServer["credential"] = authToken;
    iceServer["credentialType"] = "token";
    Json::Value iceServers(Json::arrayValue);
    iceServers.append(iceServer);
    Json::Value rpcParams(Json::arrayValue);
    rpcParams.append(iceServers);
    _client->sendRequest("setIceServers", rpcParams);
  }
};

static class LobbyDataReceiver : public sigslot::has_slots<>
{
protected:
  std::array<char, 2048> _readBuffer;
public:
  void onPeerdataToGame(rtc::AsyncSocket* socket)
  {
    auto msgLength = socket->Recv(_readBuffer.data(), _readBuffer.size(), nullptr);
    if (msgLength != sizeof (faf::PingPacket))
    {
      std::cerr << "msgLength != sizeof (PingPacket)" << std::endl;
      return;
    }
    auto pingPacket = reinterpret_cast<faf::PingPacket*>(_readBuffer.data());
    if (pingPacket->type == faf::PingPacket::PING)
    {
      auto ptIt = test.directedPingTrackers.find({pingPacket->answererId, pingPacket->senderId});
      if (ptIt == test.directedPingTrackers.end())
      {
        std::cerr << "no pingtracker for sender id " << pingPacket->senderId << std::endl;
      }
      else
      {
        ptIt->second->onPingPacket(pingPacket);
      }
    }
    else
    {
      auto ptIt = test.directedPingTrackers.find({pingPacket->senderId, pingPacket->answererId});
      if (ptIt == test.directedPingTrackers.end())
      {
        std::cerr << "no pingtracker for answerer id " << pingPacket->answererId;
      }
      else
      {
        ptIt->second->onPingPacket(pingPacket);
        if (ptIt->second->successfulPings() % 10 ==0)
        {
          std::cout << "PING count " << pingPacket->senderId << "->" << pingPacket->answererId << ": " << ptIt->second->successfulPings() << std::endl;
        }
      }
    }
  }
} ldr;


class GPGNetHandler : public sigslot::has_slots<>
{
  std::unique_ptr<faf::GPGNetClient> _client;
public:
  Test::PeerId localId;
  faf::Timer dummyTimer;

  GPGNetHandler(int port):
    _client(std::make_unique<faf::GPGNetClient>())
  {
    _client->SignalConnected.connect(this, &GPGNetHandler::_onClientConnected);
    _client->connect("127.0.0.1", port);
    _client->setCallback([this](faf::GPGNetMessage const& msg)
    {
      if (msg.header == "CreateLobby")
      {
        auto lobbyPort = msg.chunks.at(1).asInt();
        localId = msg.chunks.at(3).asInt();
        _client->sendMessage({"GameState", {"Lobby"}});
        test.lobbySockets.insert({localId, std::unique_ptr<rtc::AsyncSocket>(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM))});
        test.lobbySockets.at(localId)->SignalReadEvent.connect(&ldr, &LobbyDataReceiver::onPeerdataToGame);
        test.lobbySockets.at(localId)->Bind(rtc::SocketAddress("127.0.0.1", lobbyPort));
      }
      else if (msg.header == "HostGame")
      {
        test.clients.at(localId)->connectPeers();
      }
      else if (msg.header == "JoinGame")
      {
        test.clients.at(localId)->connectPeers();

        rtc::SocketAddress addr;
        addr.FromString(msg.chunks[0].asString());
        test.directedPeerAddresses.insert({{localId, msg.chunks[2].asInt()}, addr});
      }
      else if (msg.header == "ConnectToPeer")
      {
        rtc::SocketAddress addr;
        addr.FromString(msg.chunks[0].asString());
        test.directedPeerAddresses.insert({{localId, msg.chunks[2].asInt()}, addr});
      }
    });


    /* create some dummy GPGNet load */
    dummyTimer.start(100, [this]()
    {
      std::random_device rd;
      std::mt19937 gen(rd());
      faf::GPGNetMessage dummy;
      dummy.header = "DUMMY";
      std::uniform_int_distribution<> dis(10, 100);
      for (int i = 0; i < dis(gen); ++i)
      {
        char dummyString[128];
        dummy.chunks.push_back(dis(gen));
        dummy.chunks.push_back(dummyString);
      }
      _client->sendMessage(dummy);
    });
  }
protected:
  void _onClientConnected(rtc::AsyncSocket* socket)
  {
    _client->sendMessage({"GameState", {"Idle"}});
  }
};

void Test::restartP2PSessions()
{
  FAF_LOG_DEBUG << "restarting P2P sessions";
  gpgnetClients.clear();
  lobbySockets.clear();
  directedPeerAddresses.clear();
  directedPingTrackers.clear();
  for (PeerId localId = 0; localId < numPeers; ++localId)
  {
    gpgnetClients.insert({localId, std::make_unique<GPGNetHandler>(iceAdapters.at(localId)->options().gpgNetPort)});
  }
}

void Test::restart()
{
  FAF_LOG_DEBUG << "restarting test";
  iceAdapters.clear();
  gpgnetClients.clear();
  clients.clear();
  lobbySockets.clear();
  offers.clear();
  directedPingTrackers.clear();
  directedPeerAddresses.clear();

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

    iceAdapters.insert({localId, std::make_unique<faf::IceAdapter>(options)});
    clients.insert({localId, std::make_unique<IceAdapterHandler>(rpcPort, localId == 0)});
  }
  restartP2PSessions();
}

int main(int argc, char *argv[])
{
  faf::logging_init("verbose");
  if (!rtc::InitializeSSL())
  {
    std::cerr << "Error in InitializeSSL()";
    std::exit(1);
  }

  test.restart();

  rtc::Thread::Current()->Run();

  rtc::CleanupSSL();

  return 0;
}
