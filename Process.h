#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <thread>
#include <mutex>
#include <shared_mutex>

namespace faf {

class Process
{
public:
  Process();

  void open(std::string const& executable,
            std::vector<std::string> arguments);

  void close();

  bool isOpen() const;

  std::vector<std::string> checkOutput();

protected:
  std::unique_ptr<std::thread> _procThread;
  std::vector<std::string> _outputBuffer;
  mutable std::shared_mutex _outputBufferLock;
};

} // namespace faf
