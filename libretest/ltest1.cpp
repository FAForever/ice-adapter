#include "ltest1.h"

#include <iostream>

#include <re.h>
#include <string.h>


ltest1::ltest1()
{

}

/* called when all sip transactions are completed */
static void exit_handler(void *arg)
{
}

/* called upon incoming calls */
static void connect_handler(const struct sip_msg *msg, void *arg)
{
}

static int auth_handler(char **user, char **pass, const char *realm, void *arg)
{
}

static int offer_handler(struct mbuf **mbp, const struct sip_msg *msg,
       void *arg)
{
}

/* called when an SDP answer is received */
static int answer_handler(const struct sip_msg *msg, void *arg)
{
}

/* called when SIP progress (like 180 Ringing) responses are received */
static void progress_handler(const struct sip_msg *msg, void *arg)
{
}

/* called when the session is established */
static void establish_handler(const struct sip_msg *msg, void *arg)
{
}

/* called when the session fails to connect or is terminated from peer */
static void close_handler(int err, const struct sip_msg *msg, void *arg)
{
}

/* called when register responses are received */
static void register_handler(int err, const struct sip_msg *msg, void *arg)
{
}

/* called upon reception of  SIGINT, SIGALRM or SIGTERM */
static void signal_handler(int sig)
{
}

int main(int argc, char *argv[])
{
  struct sip *sip;

  struct sa nsv[1];
  struct dnsc *dnsc = NULL;
  struct sa laddr;
  uint32_t nsc;
  int err; /* errno return values */

  /* enable coredumps to aid debugging */
  (void)sys_coredump_set(true);

  /* initialize libre state */
  err = libre_init();
  if (err) {
    re_fprintf(stderr, "re init failed: %s\n", strerror(err));
    return 1;
  }

  nsc = ARRAY_SIZE(nsv);

  /* fetch list of DNS server IP addresses */
  err = dns_srv_get(NULL, 0, nsv, &nsc);
  if (err) {
    re_fprintf(stderr, "unable to get dns servers: %s\n",
         strerror(err));
    return 1;
  }

  for(int i = 0; i < nsc; ++i)
  {
    re_fprintf(stdout, "DNS Server %i: %J\n", i, &nsv[i]);
  }

  /* create DNS client */
  err = dnsc_alloc(&dnsc, NULL, nsv, nsc);
  if (err) {
    re_fprintf(stderr, "unable to create dns client: %s\n",
         strerror(err));
    return 1;
  }

  /* create SIP stack instance */
  err = sip_alloc(&sip, dnsc, 32, 32, 32,
      "ua demo v" VERSION " (" ARCH "/" OS ")",
      exit_handler, NULL);
  if (err) {
    re_fprintf(stderr, "sip error: %s\n", strerror(err));
    return 1;
  }

  /* fetch local IP address */
  err = net_default_source_addr_get(AF_INET, &laddr);
  if (err) {
    re_fprintf(stderr, "local address error: %s\n", strerror(err));
    return 1;
  }

  /* listen on random port */
  sa_set_port(&laddr, 0);

  /* add supported SIP transports */
  err |= sip_transp_add(sip, SIP_TRANSP_UDP, &laddr);
  err |= sip_transp_add(sip, SIP_TRANSP_TCP, &laddr);
  if (err) {
    re_fprintf(stderr, "transport error: %s\n", strerror(err));
    return 1;
  }

  struct sipsess_sock *sess_sock;

  /* create SIP session socket */
  err = sipsess_listen(&sess_sock, sip, 32, connect_handler, NULL);
  if (err) {
    re_fprintf(stderr, "session listen error: %s\n",
         strerror(err));
    return 1;
  }

  struct sdp_session *sdp;
  /* create SDP session */
  err = sdp_session_alloc(&sdp, &laddr);
  if (err) {
    re_fprintf(stderr, "sdp session error: %s\n", strerror(err));
    return 1;
  }

  struct sdp_media *sdp_media;
  /* add audio sdp media, using dummy port: 4242 */
  err = sdp_media_add(&sdp_media, sdp, "audio", 4242, "RTP/AVP");
  if (err) {
    re_fprintf(stderr, "sdp media error: %s\n", strerror(err));
    return 1;
  }

  /* add G.711 sdp media format */
  err = sdp_format_add(NULL, sdp_media, false, "0", "PCMU", 8000, 1,
           NULL, NULL, NULL, false, NULL);
  if (err) {
    re_fprintf(stderr, "sdp format error: %s\n", strerror(err));
    return 1;
  }

  /* invite provided URI */
  if (argc > 1) {

    struct mbuf *mb;

    /* create SDP offer */
    err = sdp_encode(&mb, sdp, true);
    if (err) {
      re_fprintf(stderr, "sdp encode error: %s\n",
           strerror(err));
      return 1;
    }

    struct sipsess *sess;
    const char *name = "demo";
    const char *uri  = "sip:demo@creytiv.com";
    err = sipsess_connect(&sess, sess_sock, argv[1], name,
              uri, name,
              NULL, 0, "application/sdp", mb,
              auth_handler, NULL, false,
              offer_handler, answer_handler,
              progress_handler, establish_handler,
              NULL, NULL, close_handler, NULL, NULL);
    mem_deref(mb); /* free SDP buffer */
    if (err) {
      re_fprintf(stderr, "session connect error: %s\n",
           strerror(err));
      return 1;
    }

    re_printf("inviting <%s>...\n", argv[1]);
  }
  else {
   struct sipreg *reg;
   const char *registrar  = "sip:creytiv.com";
   const char *uri  = "sip:demo@creytiv.com";
   const char *name = "demo";
    err = sipreg_register(&reg, sip, registrar, uri, uri, 60, name,
              NULL, 0, 0, auth_handler, NULL, false,
              register_handler, NULL, NULL, NULL);
    if (err) {
      re_fprintf(stderr, "register error: %s\n",
           strerror(err));
      return 1;
    }

    re_printf("registering <%s>...\n", uri);
  }

  /* main loop */
  err = re_main(signal_handler);

 out:
  /* clean up/free all state */
  mem_deref(sdp); /* will alse free sdp_media */
  mem_deref(sess_sock);
  mem_deref(sip);
  mem_deref(dnsc);

  /* free librar state */
  libre_close();

  /* check for memory leaks */
  tmr_debug();
  mem_debug();

  return err;
}
