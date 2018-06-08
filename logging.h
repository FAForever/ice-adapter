#pragma once

#include <string>

#include <webrtc/rtc_base/logging.h>

namespace faf
{

void logging_init(std::string const& verbosity);
void logging_init_log_dir(std::string const& verbosity,
                          std::string const& log_directory);

#define FAF_LOG_TRACE RTC_LOG(LS_SENSITIVE) << "[trace] FAF: "
#define FAF_LOG_DEBUG RTC_LOG(LS_VERBOSE)   << "[debug] FAF: "
#define FAF_LOG_INFO  RTC_LOG(LS_INFO)      << "[info] FAF: "
#define FAF_LOG_WARN  RTC_LOG(LS_WARNING)   << "[warn] FAF: "
#define FAF_LOG_ERROR RTC_LOG(LS_ERROR)     << "[error] FAF: "

}
