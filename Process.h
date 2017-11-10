#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <thread>
#include <mutex>
#include <shared_mutex>

#if defined(WEBRTC_WIN)
#elif defined(WEBRTC_POSIX)
#include "pstream.h"
#endif

namespace faf {

class Process
{
public:
  Process();

  void open(std::string const& executable,
            std::vector<std::string> arguments);

  bool isOpen() const;

  std::vector<std::string> checkOutput();

protected:
#if defined(WEBRTC_WIN)
#elif defined(WEBRTC_POSIX)
  std::unique_ptr<std::thread> _procThread;
#endif
  std::vector<std::string> _outputBuffer;
  mutable std::shared_mutex _outputBufferLock;
};

} // namespace faf
