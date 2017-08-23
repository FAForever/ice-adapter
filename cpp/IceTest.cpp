#include "IceTest.h"
#include "IceAdapterOptions.h"

#include <iostream>

#include <webrtc/base/scoped_ref_ptr.h>
#include <webrtc/base/ssladapter.h>
#include <webrtc/base/thread.h>
#include <webrtc/base/physicalsocketserver.h>
#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/pc/test/fakeaudiocapturemodule.h>

namespace faf {

IceTest::IceTest(int argc, char *argv[])
{
  auto options = faf::IceAdapterOptions::init(1, "Rhiza");

  bool result;
  result = rtc::InitializeSSL();

  auto audio_device_module = FakeAudioCaptureModule::Create();
  auto peer_connection_factory = webrtc::CreatePeerConnectionFactory(rtc::Thread::Current(),
                                                                     rtc::Thread::Current(),
                                                                     audio_device_module,
                                                                     nullptr,
                                                                     nullptr);
  if (peer_connection_factory.get() == nullptr)
  {
    std::cerr << "Error on CreatePeerConnectionFactory";
  }

}

IceTest::~IceTest()
{
  rtc::CleanupSSL();
}

void IceTest::Run()
{
  rtc::Thread::Current()->Run();
}

} // namespace faf
