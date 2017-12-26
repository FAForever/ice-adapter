#include "logging.h"

#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/logsinks.h"

namespace faf
{

void logging_init(std::string const& verbosity)
{
  rtc::LogMessage::LogTimestamps();
  rtc::LogMessage::LogThreads();
  rtc::LogMessage::SetLogToStderr(true);
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
