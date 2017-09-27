#include "IceAdapter.h"

#include <iostream>

#include <webrtc/base/ssladapter.h>
#include <webrtc/pc/test/fakeaudiocapturemodule.h>
#include <webrtc/base/logging.h>
#include <webrtc/base/thread.h>
#include "logging.h"

namespace faf {

IceAdapter::IceAdapter(int argc, char *argv[]):
  _options(faf::IceAdapterOptions::init(argc, argv))
{

  logging_init(_options.logLevel);

  if (!rtc::InitializeSSL())
  {
    FAF_LOG_ERROR << "Error in InitializeSSL()";
    std::exit(1);
  }

  auto audio_device_module = FakeAudioCaptureModule::Create();
_pcfactory = webrtc::CreatePeerConnectionFactory(rtc::Thread::Current(),
                                                 rtc::Thread::Current(),
                                                 audio_device_module,
                                                 nullptr,
                                                 nullptr);
  if (!_pcfactory)
  {
    FAF_LOG_ERROR << "Error in CreatePeerConnectionFactory()";
    std::exit(1);
  }
}

IceAdapter::~IceAdapter()
{
  rtc::CleanupSSL();
}

void IceAdapter::run()
{
  _gpgnetServer.listen(_options.gpgNetPort);
  std::cout << _options.gpgNetPort << std::endl;
  rtc::Thread::Current()->Run();
}

} // namespace faf
