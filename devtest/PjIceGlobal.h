#pragma once

#include <memory>

#include <pj/types.h>
#include <pj/pool.h>
#include <pjnath/ice_strans.h>

class PjIceStreamTransport;
typedef std::shared_ptr<PjIceStreamTransport>  PjIceStreamTransportPtr;

class PjIceGlobal
{
public:
  PjIceGlobal();
  void run();

  PjIceStreamTransportPtr createTransport();

  pj_caching_pool   cachingPool;
  pj_ice_strans_cfg iceConfig;
  pj_pool_t*        pool;
  unsigned int      pollCount;

protected:
  static void logCallback(int level, const char *data, int len);
  static int workerThread(void *unused);
};
