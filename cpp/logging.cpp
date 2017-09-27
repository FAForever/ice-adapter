#include "logging.h"

#include <string>
#include <experimental/filesystem>

#include "webrtc/base/logging.h"
#include "webrtc/base/logsinks.h"

#if (defined(__GNUC__) && !defined(__MINGW32__))
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>


void segfault_handler(int sig)
{
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}
#endif

namespace faf
{

void logging_init(std::string const& verbosity)
{
  if (verbosity == "error")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_ERROR);
  }
  else if (verbosity == "warn")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_WARNING);
  }
  else if (verbosity == "info")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);
  }
  else if (verbosity == "verbose")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
  }
  else if (verbosity == "debug")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_SENSITIVE);
  }
}

void logging_init_log_file(std::string const& verbosity,
                           std::string const& log_directory)
{
  static rtc::FileRotatingLogSink sink(log_directory,
                                      "faf-ice-adapter",
                                      1024*1024,
                                      1);
}

}
