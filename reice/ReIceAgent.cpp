#include "ReIceAgent.h"

#include <cstdint>
#include <re.h>

namespace faf {

IceAgent::IceAgent():
  mIceHandle(nullptr)
{
  auto err = ice_alloc(&mIceHandle,
                       ICE_MODE_FULL,
                       true);
  if (err)
  {
    mIceHandle = nullptr;
  }

}

IceAgent::~IceAgent()
{
  if (mIceHandle)
  {
    mem_deref(mIceHandle);
  }
}

}
