#pragma once

struct ice;

namespace faf {

class IceAgent
{
public:
  IceAgent();
  virtual ~IceAgent();
protected:
  ice* mIceHandle;
};

} // namespace faf
