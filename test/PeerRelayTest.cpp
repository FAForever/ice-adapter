
#include <iostream>
#include <unordered_map>
#include <memory>

#include <webrtc/rtc_base/ssladapter.h>
#include <webrtc/rtc_base/messagedigest.h>
#include <webrtc/rtc_base/base64.h>
#include <webrtc/media/engine/webrtcmediaengine.h>
#include <third_party/json/json.h>

#include "Timer.h"
#include "PeerRelay.h"
#include "Pingtracker.h"
#include "logging.h"


class PeerRelayTest : public sigslot::has_slots<>, public rtc::MessageHandler
{
  enum class Message : uint32_t
  {
    RestartRelays
  };

public:

  PeerRelayTest();

protected:
  void  _restartRelays();
  void _onIceMsgPr1(Json::Value iceMsg);
  void _onIceMsgPr2(Json::Value iceMsg);
  void _onStatePr1(std::string state);
  void _onStatePr2(std::string state);
  void _onConnectedPr1(bool connected);
  void _onConnectedPr2(bool connected);
  virtual void OnMessage(rtc::Message* msg) override;
  void _onPeerdataToGame(rtc::AsyncSocket* socket);

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _pcfactory;
  std::unique_ptr<faf::PeerRelay> _pr1;
  std::unique_ptr<faf::PeerRelay> _pr2;
  std::unique_ptr<faf::Pingtracker> _pt1;
  std::unique_ptr<faf::Pingtracker> _pt2;
  std::unique_ptr<rtc::AsyncSocket> _lobbySocket;
  std::array<char, 2048> _readBuffer;
  bool _proxyOnly;
};

PeerRelayTest::PeerRelayTest():
  _proxyOnly(false)
{
  _pcfactory = webrtc::CreateModularPeerConnectionFactory(nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr);

  _lobbySocket.reset(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM));
  _lobbySocket->SignalReadEvent.connect(this, &PeerRelayTest::_onPeerdataToGame);
  _lobbySocket->Bind(rtc::SocketAddress("127.0.0.1", 54321));

  rtc::Thread::Current()->Post(RTC_FROM_HERE, this, static_cast<uint32_t>(Message::RestartRelays));
}

void  PeerRelayTest::_restartRelays()
{
  _pr1.reset();
  _pr2.reset();
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
  webrtc::PeerConnectionInterface::IceServer iceServer;
  iceServer.urls.push_back("turn:vmrbg145.informatik.tu-muenchen.de?transport=tcp");
  iceServer.urls.push_back("turn:vmrbg145.informatik.tu-muenchen.de?transport=udp");
  iceServer.urls.push_back("stun:vmrbg145.informatik.tu-muenchen.de");
  iceServer.password = authToken;
  iceServer.username = authUsername;

  faf::PeerRelay::Callbacks callbacks1;
  callbacks1.iceMessageCallback = std::bind(&PeerRelayTest::_onIceMsgPr1, this, std::placeholders::_1);
  callbacks1.stateCallback = std::bind(&PeerRelayTest::_onStatePr1, this, std::placeholders::_1);
  callbacks1.connectedCallback = std::bind(&PeerRelayTest::_onConnectedPr1, this, std::placeholders::_1);

  faf::PeerRelay::Options options1;
  options1.remotePlayerId = 2;
  options1.remotePlayerLogin = "Player2";
  options1.isOfferer = true;
  options1.gameUdpPort = 54321;
  options1.iceServers.push_back(iceServer);

  _pr1 = std::make_unique<faf::PeerRelay>(options1, callbacks1, _pcfactory);

  faf::PeerRelay::Callbacks callbacks2;
  callbacks2.iceMessageCallback = std::bind(&PeerRelayTest::_onIceMsgPr2, this, std::placeholders::_1);
  callbacks2.stateCallback = std::bind(&PeerRelayTest::_onStatePr2, this, std::placeholders::_1);
  callbacks2.connectedCallback = std::bind(&PeerRelayTest::_onConnectedPr2, this, std::placeholders::_1);

  faf::PeerRelay::Options options2;
  options2.remotePlayerId = 1;
  options2.remotePlayerLogin = "Player1";
  options2.isOfferer = false;
  options2.gameUdpPort = 54321;
  options2.iceServers.push_back(iceServer);

  _pr2 = std::make_unique<faf::PeerRelay>(options2, callbacks2, _pcfactory);
}

void PeerRelayTest::_onIceMsgPr1(Json::Value iceMsg)
{
  auto candidateString = iceMsg["candidate"]["candidate"].asString();
  if (_proxyOnly &&
      iceMsg["type"].asString() == "candidate" &&
      iceMsg["candidate"]["candidate"].asString().find("relay") == std::string::npos)
  {
    return;
  }
  std::cout << "ICE msg 1->2: " << iceMsg.toStyledString() << std::endl;
  if (_pr2)
  {
    _pr2->addIceMessage(iceMsg);
  }
}

void PeerRelayTest::_onIceMsgPr2(Json::Value iceMsg)
{
  auto candidateString = iceMsg["candidate"]["candidate"].asString();
  if (_proxyOnly &&
      iceMsg["type"].asString() == "candidate" &&
      iceMsg["candidate"]["candidate"].asString().find("relay") == std::string::npos)
  {
    return;
  }
  std::cout << "ICE msg 2->1: " << iceMsg.toStyledString() << std::endl;
  if (_pr1)
  {
    _pr1->addIceMessage(iceMsg);
  }
}

void PeerRelayTest::_onStatePr1(std::string state)
{
  std::cout << "PR1 state " << state << std::endl;
}

void PeerRelayTest::_onStatePr2(std::string state)
{
  std::cout << "PR2 state " << state << std::endl;
}

void PeerRelayTest::_onConnectedPr1(bool connected)
{
  std::cout << "PR1 connected " << connected << std::endl;
  if (_pr1 && _pr1->isConnected() &&
      _pr2 && _pr2->isConnected())
  {
    rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 1000, this, static_cast<uint32_t>(Message::RestartRelays));
  }
  _pt1.reset(new faf::Pingtracker(1, 2, _lobbySocket.get(), rtc::SocketAddress("127.0.0.1", _pr1->localUdpSocketPort())));
}

void PeerRelayTest::_onConnectedPr2(bool connected)
{
  std::cout << "PR2 connected " << connected << std::endl;
  if (_pr1 && _pr1->isConnected() &&
      _pr2 && _pr2->isConnected())
  {
    rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 1000, this, static_cast<uint32_t>(Message::RestartRelays));
  }
  _pt2.reset(new faf::Pingtracker(2, 1, _lobbySocket.get(), rtc::SocketAddress("127.0.0.1", _pr2->localUdpSocketPort())));
}

void PeerRelayTest::OnMessage(rtc::Message* msg)
{
  switch (static_cast<Message>(msg->message_id))
  {
    case Message::RestartRelays:
      _restartRelays();
      break;
  }
}

void PeerRelayTest::_onPeerdataToGame(rtc::AsyncSocket* socket)
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
    if (pingPacket->senderId == 1 &&
        _pt2)
    {
      _pt2->onPingPacket(pingPacket);
    }
    if (pingPacket->senderId == 2 &&
        _pt1)
    {
      _pt1->onPingPacket(pingPacket);
    }
  }
  else
  {
    if (pingPacket->senderId == 1 &&
        _pt1)
    {
      _pt1->onPingPacket(pingPacket);
      if (_pt1->successfulPings() % 10 ==0)
      {
        std::cout << "PING count 1 -> 2: " << _pt1->successfulPings() << std::endl;
      }
    }
    if (pingPacket->senderId == 2 &&
        _pt2)
    {
      _pt2->onPingPacket(pingPacket);
      if (_pt2->successfulPings() % 10 ==0)
      {
        std::cout << "PING count 2 -> 1: " << _pt2->successfulPings() << std::endl;
      }
    }
  }
}

int main(int argc, char *argv[])
{
  faf::logging_init("info");
  if (!rtc::InitializeSSL())
  {
    std::cerr << "Error in InitializeSSL()";
    std::exit(1);
  }

  PeerRelayTest test;

  rtc::Thread::Current()->Run();
  return 0;
}
