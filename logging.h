#pragma once

#include <string>

#include <webrtc/rtc_base/logging.h>

namespace faf
{

void logging_init(std::string const& verbosity);
void logging_init_log_dir(std::string const& verbosity,
                          std::string const& log_directory);

#define FAF_LOG_TRACE LOG(LS_SENSITIVE) << "[trace] FAF: "
#define FAF_LOG_DEBUG LOG(LS_VERBOSE)   << "[debug] FAF: "
#define FAF_LOG_INFO LOG(LS_INFO)       << "[info] FAF: "
#define FAF_LOG_WARN LOG(LS_WARNING)    << "[warn] FAF: "
#define FAF_LOG_ERROR LOG(LS_ERROR)     << "[error] FAF: "

}
