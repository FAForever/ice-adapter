
#include <webrtc/rtc_base/ssladapter.h>

#include "IceAdapter.h"
#include "IceAdapterOptions.h"
#include "logging.h"

#if defined(WEBRTC_POSIX)
#  include <stdio.h>
#  include <execinfo.h>
#  include <signal.h>
#  include <stdlib.h>
#  include <unistd.h>

void exception_handler(int sig)
{
  void *array[32];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 32);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

#endif
int main(int argc, char *argv[])
{
#if defined(WEBRTC_POSIX)
  signal(SIGSEGV, exception_handler);   // install our exception handler
#endif
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
