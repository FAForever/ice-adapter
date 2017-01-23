#pragma once

#include <QtCore/QString>
#include <QtCore/QVector>

namespace faf {

class IceCandidate
{
public:
  IceCandidate();
  QString component;
  QString type;
  QString foundation;
  QString protocol;
  QString address;
  QString port;
  QString priority;
  static IceCandidate fromString(QString const& s);
};

typedef QVector<IceCandidate>  IceCandidateVector;

} // namespace faf
