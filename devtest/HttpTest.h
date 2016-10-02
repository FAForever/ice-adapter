#pragma once

#include <vector>
#include <cstddef>

#include "PjIceGlobal.h"
#include "PjIceStreamTransport.h"

class HttpTest
{
public:
  HttpTest(PjIceGlobal*);
  void run();
protected:
  int onConnection(struct MHD_Connection *connection,
                   const char *url,
                   const char *method,
                   const char *version,
                   const char *upload_data,
                   size_t *upload_data_size,
                   void **con_cls);
  friend int staticOnConnection(void *cls, struct MHD_Connection *,
                                const char *, const char *,
                                const char *, const char *,
                                size_t *, void **);
  struct MHD_Daemon * mDaemon;
  PjIceGlobal* mIce;
  std::vector<PjIceStreamTransportPtr> mTransports;
};
