#pragma once

#include <string>

namespace faf {

class Socket
{
public:
  Socket();
  virtual ~Socket();

  virtual bool send(std::string const& msg) = 0;
};

} // namespace faf
