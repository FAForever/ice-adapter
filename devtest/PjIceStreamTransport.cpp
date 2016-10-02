#include "PjIceStreamTransport.h"

#include <iostream>
#include <map>

typedef std::map<pj_ice_strans*, PjIceStreamTransport*> transports_map;
transports_map& transports()
{
  static transports_map result;
  return result;
}

void rxDataCallback(pj_ice_strans *ice_st,
                    unsigned comp_id,
                    void *pkt,
                    pj_size_t size,
                    const pj_sockaddr_t *src_addr,
                    unsigned src_addr_len)
{
  if (transports().find(ice_st) != transports().end())
  {
    transports().at(ice_st)->onRxData(ice_st,
                                      comp_id,
                                      pkt,
                                      size,
                                      src_addr,
                                      src_addr_len);
  }
}

void iceCompleteCallback(pj_ice_strans *ice_st,
                         pj_ice_strans_op op,
                         pj_status_t status)
{
  if (transports().find(ice_st) != transports().end())
  {
    transports().at(ice_st)->onComplete(ice_st,
                                      op,
                                      status);
  }

}

PjIceStreamTransport::PjIceStreamTransport(PjIceGlobal* global)
{
  pj_ice_strans_cb icecb;
  pj_bzero(&icecb, sizeof(icecb));
  icecb.on_rx_data = rxDataCallback;
  icecb.on_ice_complete = iceCompleteCallback;

  /* create the instance */
  if(pj_ice_strans_create("icestreamtransport", /* object name  */
      &global->iceConfig,	    /* settings	    */
      2,	    /* comp_cnt	    */
      this,			    /* user data    */
      &icecb,			    /* callback	    */
      &iceTransport)		    /* instance ptr */
      != PJ_SUCCESS)
  {
    throw std::runtime_error("error in pj_ice_strans_create()");
  }

  if (pj_ice_strans_init_ice(iceTransport,
                             PJ_ICE_SESS_ROLE_CONTROLLING,
                             NULL,
                             NULL)
      != PJ_SUCCESS)
  {
    throw std::runtime_error("error in pj_ice_strans_init_ice()");
  }
}

void PjIceStreamTransport::onRxData(pj_ice_strans *ice_st,
                                    unsigned comp_id,
                                    void *pkt,
                                    pj_size_t size,
                                    const pj_sockaddr_t *src_addr,
                                    unsigned src_addr_len)
{
}

void PjIceStreamTransport::onComplete(pj_ice_strans *ice_st,
                                      pj_ice_strans_op op,
                                      pj_status_t status)
{
  std::cout << "onComplete" << (int)op << " " << status << std::endl;
}
