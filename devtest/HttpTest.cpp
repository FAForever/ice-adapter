#include "HttpTest.h"

#include <cstring>
#include <iostream>
#include <sstream>

#include <microhttpd.h>
#include <pjnath/ice_strans.h>

int staticOnConnection (void *cls,
                        struct MHD_Connection *connection,
                        const char *url,
                        const char *method,
                        const char *version,
                        const char *upload_data,
                        size_t *upload_data_size,
                        void **con_cls)
{
  auto httpTest = static_cast<HttpTest*>(cls);
  return httpTest->onConnection(connection,
                                url,
                                method,
                                version,
                                upload_data,
                                upload_data_size,
                                con_cls);
}

HttpTest::HttpTest(PjIceGlobal* ice):
  mIce(ice)
{
  mDaemon = MHD_start_daemon(0,
                             8080,
                             NULL,
                             NULL,
                             &staticOnConnection,
                             this,
                             MHD_OPTION_END);
}

void HttpTest::run()
{
  //std::cout << "HttpTest::run() start" << std::endl;
  MHD_run(mDaemon);
  //std::cout << "HttpTest::run() end" << std::endl;
}

int HttpTest::onConnection(struct MHD_Connection *connection,
                           const char *url,
                           const char *method,
                           const char *version,
                           const char *upload_data,
                           size_t *upload_data_size,
                           void **con_cls)
{
  std::ostringstream os;
  os << "<html><body>";
  struct MHD_Response *response;
  int ret;

  if (strcmp(url, "/create_session") == 0)
  {
    mTransports.push_back(mIce->createTransport());
  }
  else if (strcmp(url, "/list_sessions") == 0)
  {
    os << mTransports.size() << " transports\n";
    for(const auto t: mTransports)
    {
      os << pj_ice_strans_get_running_comp_cnt(t->iceTransport) << " components\n";

      /* Get default candidate for the component */
      pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
      char ipaddr[PJ_INET6_ADDRSTRLEN];
      if (pj_ice_strans_get_def_cand(t->iceTransport,
                                     1,
                                     &cand[0])
          != PJ_SUCCESS)
      {
        throw std::runtime_error("error in pj_ice_strans_get_def_cand()");
      }

      unsigned cand_cnt = PJ_ARRAY_SIZE(cand);
      if (pj_ice_strans_enum_cands(t->iceTransport,
                                   1,
                                   &cand_cnt,
                                   cand)
          != PJ_SUCCESS)
      {
        throw std::runtime_error("error in pj_ice_strans_enum_cands()");
      }

      for(int i = 0; i < cand_cnt; ++i)
      {
        char ipaddr[PJ_INET6_ADDRSTRLEN];
        pj_sockaddr_print(&cand[0].addr,
                          ipaddr,
                          sizeof(ipaddr),
                          0);
        os << ipaddr << ":" << (int)pj_sockaddr_get_port(&cand[0].addr) << "\n";
      }
    }
  }

  os << "</body></html>";
  auto s = os.str();
  std::cout << s << std::endl;

  response = MHD_create_response_from_buffer(s.size(),
                                             (void*)s.c_str(),
                                             MHD_RESPMEM_MUST_COPY);
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  return ret;
}
