#include "Process.h"

#include <iostream>
#include <cstdio>
#include <iostream>
#include <string>

#include "logging.h"

namespace faf {

Process::Process()
{
}

void Process::open(std::string const& executable,
                    std::vector<std::string> arguments)
{
  if (!_procThread)
  {
    std::string exeAndArgs = executable;
    for(auto arg: arguments)
    {
      exeAndArgs += " ";
      exeAndArgs += arg;
    }
    exeAndArgs += " 2>&1";
    _procThread.reset(new std::thread([this, exeAndArgs]()
    {
      auto procPipe = popen(exeAndArgs.c_str(), "r");
      std::array<char, 4096> buffer;
      while (fgets(buffer.data(), 4096, procPipe) != nullptr)
      {
        std::unique_lock<std::shared_mutex> lock(_outputBufferLock);
        _outputBuffer.push_back(std::string(buffer.data()));
      }
    }));
  }
}

bool Process::isOpen() const
{
  return static_cast<bool>(_procThread);
}

std::vector<std::string> Process::checkOutput()
{
  std::shared_lock<std::shared_mutex> lock(_outputBufferLock);
  std::vector<std::string> result(_outputBuffer);
  _outputBuffer.clear();
  return result;
}

} // namespace faf
