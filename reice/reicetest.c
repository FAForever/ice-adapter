/**
 * @file ice.c ICE Testcode
 *
 * Copyright (C) 2010 Creytiv.com
 */
#include <string.h>
#include <stdint.h>
#include <re.h>
//#include "test.h"


#define DEBUG_MODULE "test_ice"
#define DEBUG_LEVEL 5
#include <re_dbg.h>


/*
 * Protocol testcode for 2 ICE agents. We setup 2 ICE agents A and B,
 * with only a basic host candidate. Gathering is done using a small
 * STUN Server running on localhost, SDP is exchanged with Offer/Answer.
 * Finally the connectivity checks are run and the result is verified.
 *
 *
 *                    .-------------.
 *                    | STUN Server |
 *                    '-------------'
 *              STUN /               \ STUN
 *                  /                 \
 *                 /                   \
 *  .-------------.                     .-------------.
 *  | ICE-Agent A |---------------------| ICE-Agent B |
 *  '-------------'     Connectivity    '-------------'
 *                         checks
 *
 */


struct attrs {
  struct attr {
    char name[16];
    char value[128];
  } attrv[16];
  unsigned attrc;
};

struct agent {
  struct ice *ice;
  struct icem *icem;
  struct udp_sock *us;
  struct sa laddr;
  struct attrs attr_s;
  struct attrs attr_m;
  struct ice_test *it;  /* parent */
  struct stunserver *stun;
  struct turnserver *turn;
  enum ice_mode mode;
  char name[16];
  uint8_t compid;
  bool offerer;
  bool use_turn;
  size_t n_cand;

  /* results: */
  bool gathering_ok;
  bool conncheck_ok;
};

struct ice_test {
  struct agent *a;
  struct agent *b;
  struct tmr tmr;
  int err;
};


static void icetest_check_gatherings(struct ice_test *it);
static void icetest_check_connchecks(struct ice_test *it);


/*
 * Test tools
 */


static void complete_test(struct ice_test *it, int err)
{
  it->err = err;

#if 0
  re_printf("\n\x1b[32m%H\x1b[;m\n", ice_debug, it->a->ice);
  re_printf("\n\x1b[36m%H\x1b[;m\n", ice_debug, it->b->ice);
#endif

  re_cancel();
}


static bool find_debug_string(struct ice *ice, const char *str)
{
  char buf[1024];

  if (re_snprintf(buf, sizeof(buf), "%H", ice_debug, ice) < 0)
    return false;

  return 0 == re_regex(buf, strlen(buf), str);
}


static int attr_add(struct attrs *attrs, const char *name,
        const char *value, ...)
{
  struct attr *attr = &attrs->attrv[attrs->attrc];
  va_list ap;
  int r, err = 0;

  if (!attrs || !name)
    return EINVAL;

  TEST_ASSERT(attrs->attrc <= ARRAY_SIZE(attrs->attrv));

  TEST_ASSERT(strlen(name) < sizeof(attr->name));
  str_ncpy(attr->name, name, sizeof(attr->name));

  if (value) {
    va_start(ap, value);
    r = re_vsnprintf(attr->value, sizeof(attr->value), value, ap);
    va_end(ap);
    TEST_ASSERT(r > 0);
  }

  attrs->attrc++;

 out:
  return err;
}


static const char *attr_find(const struct attrs *attrs, const char *name)
{
  unsigned i;

  if (!attrs || !name)
    return NULL;

  for (i=0; i<attrs->attrc; i++) {
    const struct attr *attr = &attrs->attrv[i];

    if (0 == str_casecmp(attr->name, name))
      return attr->value;
  }

  return NULL;
}


/*
 * ICE Agent
 */


static void agent_destructor(void *arg)
{
  struct agent *agent = arg;

  mem_deref(agent->icem);
  mem_deref(agent->ice);
  mem_deref(agent->us);
  mem_deref(agent->stun);
  mem_deref(agent->turn);
}


static struct agent *agent_other(struct agent *agent)
{
  if (agent->it->a == agent)
    return agent->it->b;
  else
    return agent->it->a;
}


static int agent_encode_sdp(struct agent *ag)
{
  struct le *le;
  int err = 0;

  for (le = icem_lcandl(ag->icem)->head; le; le = le->next) {

    struct cand *cand = le->data;

    err = attr_add(&ag->attr_m, "candidate", "%H",
             ice_cand_encode, cand);
    if (err)
      break;
  }

  err |= attr_add(&ag->attr_m, "ice-ufrag", ice_ufrag(ag->ice));
  err |= attr_add(&ag->attr_m, "ice-pwd", ice_pwd(ag->ice));

  if (ag->mode == ICE_MODE_LITE) {
    err |= attr_add(&ag->attr_s, ice_attr_lite, NULL);
  }

  return err;
}


static int agent_verify_outgoing_sdp(const struct agent *agent)
{
  const char *cand, *ufrag, *pwd;
  char buf[1024];
  int err = 0;

  if (re_snprintf(buf, sizeof(buf),
      "7f000001 %u UDP 2113929465 127.0.0.1 %u typ host",
      agent->compid, sa_port(&agent->laddr)) < 0) {
    return ENOMEM;
  }
  cand = attr_find(&agent->attr_m, "candidate");
  TEST_STRCMP(buf, str_len(buf), cand, str_len(cand));

  ufrag = attr_find(&agent->attr_m, "ice-ufrag");
  pwd   = attr_find(&agent->attr_m, "ice-pwd");
  TEST_STRCMP(ice_ufrag(agent->ice), str_len(ice_ufrag(agent->ice)),
        ufrag, str_len(ufrag));
  TEST_STRCMP(ice_pwd(agent->ice), str_len(ice_pwd(agent->ice)),
        pwd, str_len(pwd));

  if (agent->mode == ICE_MODE_FULL) {
    TEST_ASSERT(NULL == attr_find(&agent->attr_s, "ice-lite"));
  }
  else {
    TEST_ASSERT(NULL != attr_find(&agent->attr_s, "ice-lite"));
  }

 out:
  return err;
}


static int agent_decode_sdp(struct agent *agent, struct agent *other)
{
  unsigned i;
  int err = 0;

  for (i=0; i<other->attr_s.attrc; i++) {
    struct attr *attr = &other->attr_s.attrv[i];
    err = ice_sdp_decode(agent->ice, attr->name, attr->value);
    if (err)
      return err;
  }

  for (i=0; i<other->attr_m.attrc; i++) {
    struct attr *attr = &other->attr_m.attrv[i];
    err = icem_sdp_decode(agent->icem, attr->name, attr->value);
    if (err)
      return err;
  }

  return err;
}


static int send_sdp(struct agent *agent)
{
  int err;

  /* verify ICE states */
  TEST_ASSERT(!icem_mismatch(agent->icem));

  /* after gathering is complete we expect:
   *   1 local candidate
   *   0 remote candidates
   *   checklist and validlist is empty
   */
  TEST_EQUALS(agent->n_cand, list_count(icem_lcandl(agent->icem)));
  TEST_EQUALS(0, list_count(icem_rcandl(agent->icem)));
  TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
  TEST_EQUALS(0, list_count(icem_validl(agent->icem)));

  if (agent->use_turn) {
    /* verify that default candidate is the relayed address */
    TEST_SACMP(&agent->turn->relay,
         icem_cand_default(agent->icem, agent->compid),
         SA_ALL);
  }
  else {
    /* verify that default candidate is our local address */
    TEST_SACMP(&agent->laddr,
         icem_cand_default(agent->icem, agent->compid),
         SA_ALL);
  }

  /* we should not have selected candidate-pairs yet */
  TEST_ASSERT(!icem_selected_laddr(agent->icem, agent->compid));

  err = agent_encode_sdp(agent);
  if (err)
    return err;

  err = agent_verify_outgoing_sdp(agent);
  if (err)
    return err;

 out:
  return err;
}


static void agent_gather_handler(int err, uint16_t scode, const char *reason,
         void *arg)
{
  struct agent *agent = arg;

  if (err)
    goto out;
  if (scode) {
    DEBUG_WARNING("gathering failed: %u %s\n", scode, reason);
    complete_test(agent->it, EPROTO);
    return;
  }

  agent->gathering_ok = true;

  err = send_sdp(agent);
  if (err)
    goto out;

  icetest_check_gatherings(agent->it);

  return;

 out:
  complete_test(agent->it, err);
}


static void agent_connchk_handler(int err, bool update, void *arg)
{
  struct agent *agent = arg;
  struct agent *other = agent_other(agent);
  const struct sa *laddr, *raddr;

  if (err) {
    if (err != ENOMEM) {
      DEBUG_WARNING("%s: connectivity checks failed: %m\n",
              agent->name, err);
    }

    complete_test(agent->it, err);
    return;
  }

  if (agent->offerer ^ update) {
    DEBUG_WARNING("error in update flag\n");
    complete_test(agent->it, EPROTO);
    return;
  }

  agent->conncheck_ok = true;

  /* verify ICE states */
  TEST_ASSERT(!icem_mismatch(agent->icem));

  /* after connectivity checks are complete we expect:
   *   1 local candidate
   *   1 remote candidates
   */
  TEST_EQUALS(agent->n_cand, list_count(icem_lcandl(agent->icem)));
  TEST_EQUALS(other->n_cand, list_count(icem_rcandl(agent->icem)));
  TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
  TEST_EQUALS(agent->n_cand * other->n_cand,
        list_count(icem_validl(agent->icem)));

  laddr = icem_selected_laddr(agent->icem, agent->compid);
  raddr = &agent_other(agent)->laddr;

  if (!sa_cmp(&agent->laddr, laddr, SA_ALL)) {
    DEBUG_WARNING("unexpected selected address: %J\n", laddr);
    complete_test(agent->it, EPROTO);
    return;
  }

  if (!icem_verify_support(agent->icem, agent->compid, raddr)) {
    complete_test(agent->it, EPROTO);
    return;
  }

#if 0
  (void)re_printf("Agent %s -- Selected address: local=%J  remote=%J\n",
      agent->name, laddr, raddr);
#endif

  icetest_check_connchecks(agent->it);

 out:
  if (err)
    complete_test(agent->it, err);
}


static int agent_alloc(struct agent **agentp, struct ice_test *it,
           enum ice_mode mode, bool use_turn,
           const char *name, uint8_t compid, bool offerer)
{
  struct agent *agent;
  int err;

  agent = mem_zalloc(sizeof(*agent), agent_destructor);
  if (!agent)
    return ENOMEM;

  agent->use_turn = use_turn;
  agent->it = it;
  str_ncpy(agent->name, name, sizeof(agent->name));
  agent->compid = compid;
  agent->offerer = offerer;
  agent->mode = mode;

  if (mode == ICE_MODE_FULL) {

    if (agent->use_turn) {
      err = turnserver_alloc(&agent->turn);
      if (err)
        goto out;
    }
    else {
      err = stunserver_alloc(&agent->stun);
      if (err)
        goto out;
    }
  }
  else {
    TEST_ASSERT(use_turn == false);
  }

  err = sa_set_str(&agent->laddr, "127.0.0.1", 0);
  if (err)
    goto out;

  err = udp_listen(&agent->us, &agent->laddr, 0, 0);
  if (err)
    goto out;

  err = udp_local_get(agent->us, &agent->laddr);
  if (err)
    goto out;

  err = ice_alloc(&agent->ice, mode, offerer);
  if (err)
    goto out;

  /* verify Mode and Role using debug strings (temp) */
  if (mode == ICE_MODE_FULL) {
    TEST_ASSERT(find_debug_string(agent->ice, "local_mode=Full"));
  }
  else {
    TEST_ASSERT(find_debug_string(agent->ice, "local_mode=Lite"));
  }

  if (offerer) {
    TEST_ASSERT(find_debug_string(agent->ice,
                "local_role=Controlling"));
  }
  else {
    TEST_ASSERT(find_debug_string(agent->ice,
                "local_role=Controlled"));
  }

#if 0
  ice_conf(agent->ice)->debug = true;
#endif

  err = icem_alloc(&agent->icem, agent->ice, IPPROTO_UDP, 0,
       agent_gather_handler, agent_connchk_handler, agent);
  if (err)
    goto out;

  icem_set_name(agent->icem, name);

  err = icem_comp_add(agent->icem, compid, agent->us);
  if (err)
    goto out;

  err = icem_cand_add(agent->icem, compid, 0, "eth0", &agent->laddr);
  if (err)
    goto out;

  ++agent->n_cand;

  /* Start gathering now -- full mode only
   *
   * A lite implementation doesn't gather candidates;
   * it includes only host candidates for any media stream.
   */
  if (mode == ICE_MODE_FULL) {

    if (agent->use_turn) {

      err = icem_gather_relay(agent->icem,
            &agent->turn->laddr,
            "username", "password");
      ++agent->n_cand;
    }
    else {
      err = icem_gather_srflx(agent->icem,
            &agent->stun->laddr);
    }

    if (err)
      goto out;
  }
  else { /* Lite mode */

    err = icem_lite_set_default_candidates(agent->icem);
    if (err)
      goto out;

    err = send_sdp(agent);
    if (err)
      goto out;
  }

 out:
  if (err)
    mem_deref(agent);
  else
    *agentp = agent;

  return err;
}


/*
 * ICE Test
 */


static int verify_after_sdp_exchange(struct agent *agent)
{
  struct agent *other = agent_other(agent);
  int err = 0;

  /* verify remote mode (after SDP exchange) */
  if (other->mode == ICE_MODE_FULL) {
    TEST_ASSERT(find_debug_string(agent->ice, "remote_mode=Full"));
  }
  else {
    TEST_ASSERT(find_debug_string(agent->ice, "remote_mode=Lite"));
  }

  /* verify ICE states */
  TEST_ASSERT(!icem_mismatch(agent->icem));

  /* after SDP was exchanged, we expect:
   *   1 local candidate
   *   1 remote candidates
   *   checklist and validlist is empty
   */
  TEST_EQUALS(agent->n_cand, list_count(icem_lcandl(agent->icem)));
  TEST_EQUALS(other->n_cand, list_count(icem_rcandl(agent->icem)));
  TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
  TEST_EQUALS(0, list_count(icem_validl(agent->icem)));

  if (agent->use_turn) {
    /* verify that default candidate is the relayed address */
    TEST_SACMP(&agent->turn->relay,
         icem_cand_default(agent->icem, agent->compid),
         SA_ALL);
  }
  else {
    /* verify that default candidate is our local address */
    TEST_SACMP(&agent->laddr,
         icem_cand_default(agent->icem, agent->compid),
         SA_ALL);
  }

  /* we should not have selected candidate-pairs yet */
  TEST_ASSERT(!icem_selected_laddr(agent->icem, agent->compid));

 out:
  if (err) {
    DEBUG_WARNING("agent %s failed\n", agent->name);
  }
  return err;
}


static int agent_start(struct agent *agent)
{
  struct agent *other = agent_other(agent);
  int err = 0;

  /* verify that check-list is empty before we start */
  TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
  TEST_EQUALS(0, list_count(icem_validl(agent->icem)));

  if (agent->mode == ICE_MODE_FULL) {

    err = ice_conncheck_start(agent->ice);
    if (err)
      return err;

    TEST_EQUALS(agent->n_cand * other->n_cand,
          list_count(icem_checkl(agent->icem)));
  }
  else {
    /* Formation of check lists is performed
       only by full implementations. */
    TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
  }

  TEST_EQUALS(0, list_count(icem_validl(agent->icem)));

 out:
  return err;
}


static int agent_verify_completed(struct agent *agent)
{
  struct agent *other = agent_other(agent);
  uint32_t validc;
  int err = 0;

  if (agent->mode == ICE_MODE_FULL) {
    TEST_ASSERT(agent->gathering_ok);
    TEST_ASSERT(agent->conncheck_ok);
  }

  TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
  validc = list_count(icem_validl(agent->icem));
  if (agent->mode == ICE_MODE_FULL) {
    TEST_EQUALS(agent->n_cand * other->n_cand, validc);
  }
  else {
    TEST_ASSERT(validc==0 || validc==1);
  }

  /* verify state of STUN/TURN server */
  if (agent->mode == ICE_MODE_FULL) {
    if (agent->use_turn) {
      TEST_ASSERT(agent->turn->n_allocate >= 1);
      TEST_ASSERT(agent->turn->n_chanbind >= 1);
    }
    else {
      TEST_ASSERT(agent->stun->nrecv >= 1);
    }
  }

 out:
  return err;
}


static void icetest_check_gatherings(struct ice_test *it)
{
  int err;

  if (it->a->mode == ICE_MODE_FULL && !it->a->gathering_ok)
    return;
  if (it->b->mode == ICE_MODE_FULL && !it->b->gathering_ok)
    return;

  /* both gatherings are complete
   * exchange SDP and start conncheck
   */

  err = agent_decode_sdp(it->a, it->b);
  if (err)
    goto out;
  err = agent_decode_sdp(it->b, it->a);
  if (err)
    goto out;

  err = verify_after_sdp_exchange(it->a);
  if (err)
    goto error;
  err = verify_after_sdp_exchange(it->b);
  if (err)
    goto error;

  err  = agent_start(it->a);
  if (err)
    goto out;
  err = agent_start(it->b);
  if (err)
    goto out;

  return;

 out:
 error:
  complete_test(it, err);
}


static void tmr_handler(void *arg)
{
  struct ice_test *it = arg;

#if 0
  re_printf("\n\x1b[32m%H\x1b[;m\n", ice_debug, it->a->ice);
  re_printf("\n\x1b[36m%H\x1b[;m\n", ice_debug, it->b->ice);
#endif

  complete_test(it, 0);
}


static void icetest_check_connchecks(struct ice_test *it)
{
  if (it->a->mode == ICE_MODE_FULL && !it->a->conncheck_ok)
    return;
  if (it->b->mode == ICE_MODE_FULL && !it->b->conncheck_ok)
    return;

  /* start an async timer to let the socket traffic complete */
  tmr_start(&it->tmr, 1, tmr_handler, it);
}


static void icetest_destructor(void *arg)
{
  struct ice_test *it = arg;

  tmr_cancel(&it->tmr);
  mem_deref(it->b);
  mem_deref(it->a);
}


static int icetest_alloc(struct ice_test **itp,
       enum ice_mode mode_a, bool turn_a,
       enum ice_mode mode_b, bool turn_b)
{
  struct ice_test *it;
  int err;

  it = mem_zalloc(sizeof(*it), icetest_destructor);
  if (!it)
    return ENOMEM;

  err = agent_alloc(&it->a, it, mode_a, turn_a, "A", 7, true);
  if (err)
    goto out;

  err = agent_alloc(&it->b, it, mode_b, turn_b, "B", 7, false);
  if (err)
    goto out;

 out:
  if (err)
    mem_deref(it);
  else
    *itp = it;

  return err;
}


static int _test_ice_loop(enum ice_mode mode_a, bool turn_a,
       enum ice_mode mode_b, bool turn_b)
{
  struct ice_test *it = NULL;
  int err;

  err = icetest_alloc(&it, mode_a, turn_a, mode_b, turn_b);
  if (err)
    goto out;

  err = re_main_timeout(300);
  if (err)
    goto out;

  /* read back global errorcode */
  if (it->err) {
    err = it->err;
    goto out;
  }

  /* now verify all results after test was finished */
  err  = agent_verify_completed(it->a);
  err |= agent_verify_completed(it->b);
  if (err)
    goto out;

 out:
  mem_deref(it);

  return err;
}


/* also verify that these symbols are exported */
static int test_ice_basic_candidate(void)
{
  static const enum ice_cand_type typev[4] = {
    ICE_CAND_TYPE_HOST,
    ICE_CAND_TYPE_SRFLX,
    ICE_CAND_TYPE_PRFLX,
    ICE_CAND_TYPE_RELAY
  };
  unsigned i;
  int err = 0;

  for (i=0; i<ARRAY_SIZE(typev); i++) {

    const char *name;
    enum ice_cand_type type;

    name = ice_cand_type2name(typev[i]);
    TEST_ASSERT(str_isset(name));

    type = ice_cand_name2type(name);
    TEST_EQUALS(typev[i], type);
  }

 out:
  return err;
}


static int test_ice_cand_prio(void)
{
  int err = 0;

  TEST_EQUALS(0x7e0000ff, ice_cand_calc_prio(ICE_CAND_TYPE_HOST,  0, 1));
  TEST_EQUALS(0x640004ff, ice_cand_calc_prio(ICE_CAND_TYPE_SRFLX, 4, 1));
  TEST_EQUALS(0x6e0000fe, ice_cand_calc_prio(ICE_CAND_TYPE_PRFLX, 0, 2));
  TEST_EQUALS(0x000004fe, ice_cand_calc_prio(ICE_CAND_TYPE_RELAY, 4, 2));
 out:
  return err;
}


static const char *testv[] = {
"1 1 UDP 2130706431 10.0.1.1 5000 typ host",
"1 2 UDP 2130706431 10.0.1.1 5001 typ host",
"2 1 UDP 1694498815 192.0.2.3 5000 typ srflx raddr 10.0.1.1 rport 8998",
"2 2 UDP 1694498815 192.0.2.3 5001 typ srflx raddr 10.0.1.1 rport 8998",

"1 1 TCP 2128609279 10.0.1.1 9 typ host tcptype active",
"2 1 TCP 2124414975 10.0.1.1 8998 typ host tcptype passive",
"3 1 TCP 2120220671 10.0.1.1 8999 typ host tcptype so",
"4 1 TCP 1688207359 192.0.2.3 9 typ srflx raddr"
  " 10.0.1.1 rport 9 tcptype active",
"5 1 TCP 1684013055 192.0.2.3 45664 typ srflx raddr"
  " 10.0.1.1 rport 8998 tcptype passive",
"6 1 TCP 1692401663 192.0.2.3 45687 typ srflx raddr"
  " 10.0.1.1 rport 8999 tcptype so",

#ifdef HAVE_INET6
"H76f0ae12 1 UDP 2130706431 fda8:de2d:e95f:4811::1 6054 typ host",

"3113280040 1 UDP 2122255103 2001:aaaa:5ef5:79fb:1847:2c0d:a230:23ab 53329"
  " typ host",
#endif

};


static int test_ice_cand_attribute(void)
{
  unsigned i;
  int err = 0;

  for (i=0; i<ARRAY_SIZE(testv); i++) {

    struct ice_cand_attr cand;
    char buf[256];
    int n;

    err = ice_cand_attr_decode(&cand, testv[i]);
    TEST_ERR(err);

    /* sanity-check of decoded attribute */
    TEST_ASSERT(str_isset(cand.foundation));
    TEST_ASSERT(1 <= cand.compid && cand.compid <= 2);
    TEST_ASSERT(cand.proto == IPPROTO_UDP ||
          cand.proto == IPPROTO_TCP);
    TEST_ASSERT(cand.prio > 0);
    TEST_ASSERT(sa_isset(&cand.addr, SA_ALL));

    n = re_snprintf(buf, sizeof(buf), "%H",
        ice_cand_attr_encode, &cand);
    if (n < 0)
      return ENOMEM;

    TEST_STRCMP(testv[i], strlen(testv[i]), buf, (unsigned)n);
  }

 out:
  return err;
}


int test_ice_cand(void)
{
  int err = 0;

  err = test_ice_basic_candidate();
  if (err)
    return err;

  err = test_ice_cand_prio();
  if (err)
    return err;

  err = test_ice_cand_attribute();
  if (err)
    return err;

  return err;
}


int test_ice_loop(void)
{
  return _test_ice_loop(ICE_MODE_FULL, false, ICE_MODE_FULL, false);
}


int test_ice_loop_turn(void)
{
  return _test_ice_loop(ICE_MODE_FULL, true,  ICE_MODE_FULL, true);
}


int test_ice_lite(void)
{
  int err = 0;

  err |= _test_ice_loop(ICE_MODE_FULL, true, ICE_MODE_LITE, false);

  return err;
}
