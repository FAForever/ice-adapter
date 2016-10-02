#include "PjIceGlobal.h"

#include <iostream>
#include <stdexcept>

#include <pjlib.h>
#include <pjnath.h>

#include "PjIceStreamTransport.h"

PjIceGlobal::PjIceGlobal()
{
  pj_log_set_log_func(&PjIceGlobal::logCallback);

  /* Initialize the libraries before anything else */
  if( pj_init() != PJ_SUCCESS)
  {
    throw std::runtime_error("error in pj_init()");
  }
  if( pjlib_util_init() != PJ_SUCCESS)
  {
    throw std::runtime_error("error in pjlib_util_init()");
  }
  if( pjnath_init() != PJ_SUCCESS)
  {
    throw std::runtime_error("error in pjnath_init()");
  }

  /* Must create pool factory, where memory allocations come from */
  pj_caching_pool_init(&cachingPool, NULL, 0);

  /* Init our ICE settings with null values */
  pj_ice_strans_cfg_default(&iceConfig);

  iceConfig.stun_cfg.pf = &cachingPool.factory;

  /* Create application memory pool */
  pool = pj_pool_create(&cachingPool.factory,
                         "ice-adapter",
                         1024,
                         1024,
                         NULL);

  /* Create timer heap for timer stuff */
  if (pj_timer_heap_create(pool,
                           100,
                           &iceConfig.stun_cfg.timer_heap)
      != PJ_SUCCESS)
  {
    throw std::runtime_error("error in pj_timer_heap_create()");
  }

  if (pj_ioqueue_create(pool,
                        16,
                        &iceConfig.stun_cfg.ioqueue)
      != PJ_SUCCESS)
  {
    throw std::runtime_error("error in pj_ioqueue_create()");
  }

  iceConfig.af = pj_AF_INET();

  if (pj_dns_resolver_create(&cachingPool.factory,
                             "resolver",
                             0,
                             iceConfig.stun_cfg.timer_heap,
                             iceConfig.stun_cfg.ioqueue,
                             &iceConfig.resolver)
      != PJ_SUCCESS)
  {
    throw std::runtime_error("error in pj_dns_resolver_create()");
  }
  pj_str_t ns;
  pj_cstr(&ns, "8.8.8.8");
  if (pj_dns_resolver_set_ns(iceConfig.resolver,
                             1,
                             &ns,
                             NULL)
      != PJ_SUCCESS)
  {
    throw std::runtime_error("error in pj_dns_resolver_set_ns()");
  }

  /* -= Start initializing ICE stream transport config =- */

  /* Maximum number of host candidates */
  iceConfig.stun.max_host_cands = -1;

  /* Nomination strategy */
  iceConfig.opt.aggressive = PJ_FALSE;

  /* Configure STUN/srflx candidate resolution */
  pj_cstr(&iceConfig.stun.server, "titus.netlair.de");
  iceConfig.stun.port = PJ_STUN_PORT;

  /* For this demo app, configure longer STUN keep-alive time
   * so that it does't clutter the screen output.
   */
  iceConfig.stun.cfg.ka_interval = 100;

  pj_cstr(&iceConfig.turn.server, "titus.netlair.de");
  iceConfig.turn.port = PJ_STUN_PORT;


  /* TURN credential */
  iceConfig.turn.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
  //icedemo.ice_cfg.turn.auth_cred.data.static_cred.username = icedemo.opt.turn_username;
  //icedemo.ice_cfg.turn.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
  //icedemo.ice_cfg.turn.auth_cred.data.static_cred.data = icedemo.opt.turn_password;

  /* Connection type to TURN server */
  iceConfig.turn.conn_type = PJ_TURN_TP_TCP;

  /* For this demo app, configure longer keep-alive time
   * so that it does't clutter the screen output.
   */
  iceConfig.turn.alloc_param.ka_interval = 100;
}

void PjIceGlobal::run()
{
  //std::cout << "PjIceGlobal::run() start" << std::endl;
  enum { MAX_NET_EVENTS = 1 };
  pj_time_val max_timeout = {0, 0};
  pj_time_val timeout = { 0, 0};
  unsigned count = 0, net_event_count = 0;
  unsigned c;

  max_timeout.msec = 1;

  /* Poll the timer to run it and also to retrieve the earliest entry. */
  timeout.sec = timeout.msec = 0;
  c = pj_timer_heap_poll(iceConfig.stun_cfg.timer_heap,
                         &timeout);
  if (c > 0)
    count += c;

  /* timer_heap_poll should never ever returns negative value, or otherwise
   * ioqueue_poll() will block forever!
   */
  pj_assert(timeout.sec >= 0 && timeout.msec >= 0);
  if (timeout.msec >= 1000) timeout.msec = 999;

  /* compare the value with the timeout to wait from timer, and use the
   * minimum value.
  */
  if (PJ_TIME_VAL_GT(timeout, max_timeout))
    timeout = max_timeout;

  /* Poll ioqueue.
   * Repeat polling the ioqueue while we have immediate events, because
   * timer heap may process more than one events, so if we only process
   * one network events at a time (such as when IOCP backend is used),
   * the ioqueue may have trouble keeping up with the request rate.
   *
   * For example, for each send() request, one network event will be
   *   reported by ioqueue for the send() completion. If we don't poll
   *   the ioqueue often enough, the send() completion will not be
   *   reported in timely manner.
   */
  int c2;
  do
  {
    int c2 = pj_ioqueue_poll(iceConfig.stun_cfg.ioqueue,
                        &timeout);
    if (c2 < 0)
    {
      pj_status_t err = pj_get_netos_error();
      pj_thread_sleep(PJ_TIME_VAL_MSEC(timeout));
      pollCount = count;
      throw std::runtime_error("error in pj_ioqueue_poll()");
    }
    else if (c2 == 0)
    {
      break;
    }
    else
    {
      net_event_count += c2;
      timeout.sec = timeout.msec = 0;
    }
  }
  while (c2 > 0 &&
         net_event_count < MAX_NET_EVENTS);

  count += net_event_count;
  pollCount = count;
  //std::cout << "PjIceGlobal::run() end" << std::endl;
}

PjIceStreamTransportPtr PjIceGlobal::createTransport()
{
  return PjIceStreamTransportPtr(new PjIceStreamTransport(this));
}

void PjIceGlobal::logCallback(int level, const char *data, int len)
{
  std::cout << "pj " << level << "\t" << std::string(data, std::size_t(len)) << std::endl;
}
