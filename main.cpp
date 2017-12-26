
#include <webrtc/rtc_base/ssladapter.h>

#include "IceAdapter.h"
#include "IceAdapterOptions.h"
#include "logging.h"

int main(int argc, char *argv[])
{
  auto options = faf::IceAdapterOptions::init(argc, argv);

  faf::logging_init(options.logLevel);
  if (!options.logDirectory.empty())
  {
    faf::logging_init_log_dir(options.logLevel, options.logDirectory);
  }

  if (!rtc::InitializeSSL())
  {
    FAF_LOG_ERROR << "Error in InitializeSSL()";
    std::exit(1);
  }

  faf::IceAdapter iceAdapter(options);

  rtc::Thread::Current()->Run();

  rtc::CleanupSSL();

  return 0;
}
