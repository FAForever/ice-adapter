#include "trim.h"

namespace faf {

std::string trim_whitespace(std::string const& in)
{
  const std::string whitespace = " \t\f\v\n\r";
  auto start = in.find_first_not_of(whitespace);
  auto end = in.find_last_not_of(whitespace);
  if (start == std::string::npos ||
      end == std::string::npos)
  {
    return std::string();
  }
  return in.substr(start, end + 1);
}

} // namespace faf
