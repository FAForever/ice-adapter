#pragma once

#include "PjIceGlobal.h"

class PjIceStreamTransport
{
public:
  PjIceStreamTransport(PjIceGlobal* global);

  pj_ice_strans* iceTransport;
protected:
  void onRxData(pj_ice_strans *ice_st,
                unsigned comp_id,
                void *pkt,
                pj_size_t size,
                const pj_sockaddr_t *src_addr,
                unsigned src_addr_len);
  void onComplete(pj_ice_strans *ice_st,
                  pj_ice_strans_op op,
                  pj_status_t status);
  PjIceGlobal* mGlobal;

  friend void rxDataCallback(pj_ice_strans *,
                             unsigned,
                             void *,
                             pj_size_t,
                             const pj_sockaddr_t *,
                             unsigned);
  friend void iceCompleteCallback(pj_ice_strans *,
                                  pj_ice_strans_op,
                                  pj_status_t);
};
