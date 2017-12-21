
#include <iostream>
#include <memory>
#include <random>

#include <webrtc/rtc_base/messagehandler.h>
#include <webrtc/rtc_base/thread.h>

#include "GPGNetServer.h"
#include "GPGNetClient.h"
#include "Timer.h"
#include "logging.h"

static int counter = 0;

class GPGNetTest : public sigslot::has_slots<>, public rtc::MessageHandler
{
  std::unique_ptr<faf::GPGNetClient> _client1;
  std::unique_ptr<faf::GPGNetClient> _client2;
  faf::GPGNetServer _server;

  enum class Message : uint32_t
  {
    StartClient1,
    StartClient2
  };
public:
  faf::Timer testMessageTimer;

  GPGNetTest()
  {
    _server.listen(0);
    _server.SignalNewGPGNetMessage.connect(this, &GPGNetTest::_onServerGpgnetMsg);
    _server.SignalClientConnected.connect(this, &GPGNetTest::_onServerClientConnected);
    _server.SignalClientDisconnected.connect(this, &GPGNetTest::_onServerClientDisconnected);

    /* create some dummy GPGNet load */
    testMessageTimer.start(100, [this]()
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
      if (_client1)
      {
        _client1->sendMessage(dummy);
      }
      if (_client2)
      {
        _client2->sendMessage(dummy);
      }
    });

    rtc::Thread::Current()->Post(RTC_FROM_HERE, this, static_cast<uint32_t>(Message::StartClient1));
  }

  void startClient1()
  {
    _client1.reset(new faf::GPGNetClient());
    _client1->SignalConnected.connect(this, &GPGNetTest::_onClient1Connected);
    _client1->connect("127.0.0.1", _server.listenPort());
    _client1->setCallback([this](faf::GPGNetMessage const& msg)
    {
      std::cout << "GPGNetMessage from server: " << msg.toDebug() << std::endl;
    });
  }

  void startClient2()
  {
    _client2.reset(new faf::GPGNetClient());
    _client2->SignalConnected.connect(this, &GPGNetTest::_onClient2Connected);
    _client2->connect("127.0.0.1", _server.listenPort());
    _client2->setCallback([this](faf::GPGNetMessage const& msg)
    {
      std::cout << "GPGNetMessage from server: " << msg.toDebug() << std::endl;
    });
  }

protected:
  void _onServerGpgnetMsg(faf::GPGNetMessage msg)
  {
    //std::cout << "GPGNetMessage from client: " << msg.toDebug() << std::endl;
  }

  void _onServerClientConnected()
  {
    std::cout << counter++ << " GPGNetServer: client connected" << std::endl;
  }

  void _onServerClientDisconnected()
  {
    std::cout << counter++ << " GPGNetServer: client disconnected" << std::endl;
  }

  void _onClient1Connected(rtc::AsyncSocket* socket)
  {
    std::cout << counter++ << " GPGNetClient: connected" << std::endl;
    rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 100, this, static_cast<uint32_t>(Message::StartClient2));
  }

  void _onClient2Connected(rtc::AsyncSocket* socket)
  {
    std::cout << counter++ << " GPGNetClient: connected" << std::endl;
    rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 100, this, static_cast<uint32_t>(Message::StartClient1));
  }

  virtual void OnMessage(rtc::Message* msg) override
  {
    switch (static_cast<Message>(msg->message_id))
    {
      case Message::StartClient1:
        startClient1();
        break;
      case Message::StartClient2:
        startClient2();
        break;
    }
  }
};

int main(int argc, char *argv[])
{
  GPGNetTest t;
  rtc::Thread::Current()->Run();
  return 0;
}
