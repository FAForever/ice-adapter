#include "IceCandidate.h"

namespace faf {

IceCandidate::IceCandidate()
{

}

IceCandidate IceCandidate::fromString(QString const& s)
{
  QString candidateStr("candidate:");
  auto pos = s.indexOf(candidateStr) + candidateStr.size();
  auto fields = s.mid(pos).split(' ');

  IceCandidate result;
  result.component = fields[1];
  result.type = fields[7];
  result.foundation = fields[0];
  result.protocol = fields[2];
  result.address = fields[4];
  result.port = fields[5];
  result.priority = fields[3];
  return result;
}

} // namespace faf
