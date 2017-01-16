#pragma once

struct icem;

namespace faf {

class IceStream
{
public:
  IceStream();
protected:
  icem* mIceStreamHandle;
};

} // namespace faf
