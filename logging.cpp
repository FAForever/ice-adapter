#include "logging.h"

#include <iostream>
#include <string>
#include <experimental/filesystem>

#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/logsinks.h"

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

class LogSink : public ::rtc::LogSink
{
public:
  virtual void OnLogMessage(const std::string& message) override
  {
    std::cout << message << std::flush;
  }
};

void logging_init(std::string const& verbosity)
{
  static ::faf::LogSink ls;

  rtc::LogMessage::LogTimestamps();
  rtc::LogMessage::LogThreads();
  rtc::LogMessage::SetLogToStderr(true);
  if (verbosity == "error")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_ERROR);
    //rtc::LogMessage::AddLogToStream(&ls, rtc::LS_ERROR);
  }
  else if (verbosity == "warn")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_WARNING);
    //rtc::LogMessage::AddLogToStream(&ls, rtc::LS_WARNING);
  }
  else if (verbosity == "info")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);
    //rtc::LogMessage::AddLogToStream(&ls, rtc::LS_INFO);
  }
  else if (verbosity == "verbose")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
    //rtc::LogMessage::AddLogToStream(&ls, rtc::LS_VERBOSE);
  }
  else if (verbosity == "debug")
  {
    rtc::LogMessage::LogToDebug(rtc::LS_SENSITIVE);
    //rtc::LogMessage::AddLogToStream(&ls, rtc::LS_SENSITIVE);
  }
}

void logging_init_log_dir(std::string const& verbosity,
                          std::string const& log_directory)
{
  static rtc::FileRotatingLogSink sink(log_directory,
                                       "ice_adapter",
                                       1024*1024,
                                       2);
  sink.Init();
  if (verbosity == "error")
  {
    rtc::LogMessage::AddLogToStream(&sink, rtc::LS_ERROR);
  }
  else if (verbosity == "warn")
  {
    rtc::LogMessage::AddLogToStream(&sink, rtc::LS_WARNING);
  }
  else if (verbosity == "info")
  {
    rtc::LogMessage::AddLogToStream(&sink, rtc::LS_INFO);
  }
  else if (verbosity == "verbose")
  {
    rtc::LogMessage::AddLogToStream(&sink, rtc::LS_VERBOSE);
  }
  else if (verbosity == "debug")
  {
    rtc::LogMessage::AddLogToStream(&sink, rtc::LS_SENSITIVE);
  }
}

}
