#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "cache/cache.h"

#include "vtim.h"
#include "vsb.h"
#include "vpridir.h"
#include "vcc_disco_if.h"
#include "disco.h"


#ifdef ADNS_LOG
static void disco_thread_dnslog(adns_state, void *priv, const char *fmt, va_list ap);

#define ADNS_INIT(bg) \
  do { \
    AZ((bg)->dns); \
    AZ(adns_init_logfn(&(bg)->dns, adns_if_nosigpipe|adns_if_permit_ipv4|adns_if_permit_ipv6, NULL, \
                     disco_thread_dnslog, (bg))); \
  } while(0)
#else /* ! ADNS_LOG */
#define ADNS_INIT(bg) \
  do { \
    AZ((bg)->dns); \
    AZ(adns_init(&(bg)->dns, adns_if_noerrprint|adns_if_noserverwarn|adns_if_nosigpipe| \
                             adns_if_permit_ipv4|adns_if_permit_ipv6, NULL)); \
  } while(0)
#endif /* ADNS_LOG */
#define ADNS_FREE(bg) \
  do { \
    AN((bg)->dns); \
    adns_finish((bg)->dns); \
    (bg)->dns = NULL; \
  } while(0)

extern struct VSC_lck *lck_disco;

static void expand_srv(disco_t *d, unsigned sz)
{
  CHECK_OBJ_NOTNULL(d, VMOD_DISCO_DIRECTOR_MAGIC);

  d->srv = realloc(d->srv, sz * sizeof(*d->srv));
  AN(d->srv);
  d->l_srv = sz;
}


static inline int cmpsrv_sa(adns_sockaddr *s1, adns_sockaddr *s2)
{
  if (s1->sa.sa_family == s2->sa.sa_family) {
    switch(s1->sa.sa_family) {
    case AF_INET6:
      return memcmp(&s1->inet6.sin6_addr, &s2->inet6.sin6_addr, sizeof(s1->inet6.sin6_addr));
    case AF_INET:
      return memcmp(&s1->inet.sin_addr, &s2->inet.sin_addr, sizeof(s1->inet.sin_addr));
    default:
      WRONG("unsupported protocol family");
    }
  }
  return -1;
}

static int cmpsrv(dns_srv_t *s1, adns_rr_srvha *s2)
{
  if (s2->ha.naddrs > 0)
    return (s1->priority == s2->priority &&
           s1->weight == s2->weight &&
           s1->port == s2->port) ?
           cmpsrv_sa(&s1->addr.addr, &s2->ha.addrs->addr) : 1;

  return -1;
}

static void dump_director(disco_t *d)
{
  unsigned u;
  dns_srv_t *s;
  char buf[256];
  int buflen = sizeof(buf);
  int p;

  CHECK_OBJ_NOTNULL(d, VMOD_DISCO_DIRECTOR_MAGIC);

  for (u = 0; u < d->n_srv; u++) {
    s = &d->srv[u];
    if (s->port) {
      p = s->port;
      AZ(adns_addr2text(&s->addr.addr.sa, 0, buf, &buflen, &p));

      VSL(SLT_Debug, 0, "disco: DNS-SD %s SRV#%u: %hu %hu %hu %s:%d", d->name,
              u+1, s->priority, s->weight, s->port, buf, p);
    }
  }
}

#ifdef ADNS_LOG
static void disco_thread_dnslog(adns_state dns, void *priv, const char *fmt, va_list ap)
{
  int l;
  struct vmod_disco_bgthread *bg;

  CAST_OBJ_NOTNULL(bg, priv, VMOD_DISCO_BGTHREAD_MAGIC);

  (void)dns;
  VSB_vprintf(bg->vsb, fmt, ap);
  l = VSB_len(bg->vsb);
  if (l  > 0 && bg->vsb->s_buf[l-1] == '\n') {
    char  *data;

    AZ(VSB_finish(bg->vsb));
    data = VSB_data(bg->vsb);
    AN(data);
    data[l-1] = '\0';
    if (*data) {
      VSL(SLT_Debug, 0, data);
    }
    VSB_clear(bg->vsb);
  } else {
    assert(l < 1024);
  }
}
#endif

static void disco_thread_dnsresp(void *priv, disco_t *d, adns_answer *ans)
{
  unsigned u, w;
  struct vmod_disco *mod;
  dns_srv_t *s;
  char *cp;
  const char *hp;

  CAST_OBJ_NOTNULL(mod, priv, VMOD_DISCO_MAGIC);
  CHECK_OBJ_NOTNULL(d, VMOD_DISCO_DIRECTOR_MAGIC);

  if (ans->nrrs == 0) {
    VSL(SLT_Debug, 0, "disco: %s: %s", d->name, adns_strerror(ans->status));
    if ((ans->status == adns_s_nxdomain || ans->status == adns_s_nodata) && ans->type == adns_r_srv && d->n_srv > 0) {
      for (u = 0; u < d->n_srv; u++) {
        if (d->srv[u].port > 0) {
          memset(&d->srv[u], 0, sizeof(d->srv[u]));
          d->changes++;
        }
      }
    }
    free(ans);
    return;
  }

  assert(ans->type == adns_r_srv);

  while (d->l_srv <= ans->nrrs + d->n_srv) {
    expand_srv(d, d->l_srv + 16);
  }

  for (u = 0; u < d->n_srv; u++) {
    if (d->srv[u].port > 0) {
      for (w = 0; w < ans->nrrs; w++) {
        if (ans->rrs.srvha[w].ha.naddrs > 0 && cmpsrv(&d->srv[u], &ans->rrs.srvha[w]) == 0) {
          ans->rrs.srvha[w].ha.naddrs = 0;
          w = ans->nrrs+1;
          break;
        }
      }
      if (w <= ans->nrrs) {
        assert(u < d->n_srv);
        memset(&d->srv[u], 0, sizeof(d->srv[u]));
        d->changes++;
      }
    }
  }

  for (u = 0; u < ans->nrrs; u++) {
    if (ans->rrs.srvha[u].ha.naddrs > 0) {
      s = &d->srv[d->n_srv++];
      d->changes++;
      assert(d->n_srv < d->l_srv);
      memset(s, 0, sizeof(*s));
      cp = &s->name[0];
      for (hp = ans->rrs.srvha[u].ha.host; (hp && *hp) && *hp != '.'; hp++) {
        *cp++ = *hp;
        if(cp - &s->name[0] >= sizeof(s->name)-1)
          break;
      }
      *cp = '\0';
      s->priority = ans->rrs.srvha[u].priority;
      s->weight = ans->rrs.srvha[u].weight;
      s->port = ans->rrs.srvha[u].port ? ans->rrs.srvha[u].port : 80;
      memcpy(&s->addr, ans->rrs.srvha[u].ha.addrs, sizeof(s->addr));
      switch (s->addr.addr.sa.sa_family) {
      case AF_INET6:
        s->addr.addr.inet6.sin6_port = htons(s->port);
        break;
      case AF_INET:
        s->addr.addr.inet.sin_port = htons(s->port);
        break;
      default:
        WRONG("unsupported address family");
      }
    }
  }

  free(ans);
  if (d->n_srv > 0)
    dump_director(d);
}

static double disco_thread_run(struct worker *wrk,
                             struct vmod_disco_bgthread *bg,
                             double now)
{
  char *name;
  size_t l;
  unsigned u, npending = 0;
  double interval;
  struct vmod_disco *mod;
  disco_t *d;

  CHECK_OBJ_NOTNULL(wrk, WORKER_MAGIC);
  CHECK_OBJ_NOTNULL(bg, VMOD_DISCO_BGTHREAD_MAGIC);
  CAST_OBJ_NOTNULL(mod, bg->priv, VMOD_DISCO_MAGIC);
  CHECK_OBJ_NOTNULL(bg->ws, WS_MAGIC);

  VSL(SLT_Debug, 0, "disco_thread_run");
  Lck_AssertHeld(&bg->mtx);
  (void)wrk;
  interval = bg->interval;
  update_rwlock_rdlock(mod->mtx);
  VTAILQ_FOREACH(d, &mod->dirs, list) {
    CHECK_OBJ_NOTNULL(d, VMOD_DISCO_DIRECTOR_MAGIC);
    if (d->query) {
      adns_answer *ans = NULL;
      void *ctx = NULL;

      AN(bg->dns);
      npending++;
      switch(adns_check(bg->dns, &d->query, &ans, &ctx)) {
      case ESRCH:
      case EAGAIN:
        if (d->query && d->nxt > 0 && now >= d->nxt) {
          adns_cancel(d->query);
          d->query = NULL;
          VSL(SLT_Error, 0, "no response to query for '%s', giving up (next query is pending)", d->name);
          goto nextquery;
        } else if(interval > 1e-2)
          interval = 1e-2;
        break;
      case 0:
        d->query = NULL;
        disco_thread_dnsresp(ctx, d, ans);
        d->nxt = now + d->freq + d->fuzz;
        d->fuzz = 0;
        npending--;
        if (now + interval > d->nxt)
          interval = d->nxt - now;
        break;
      default:
        WRONG("unexpected response from adns_check");
      }
      continue;
    }
    if (d->nxt > now) {
      if (now + interval > d->nxt)
        interval = d->nxt - now;
      continue;
    }
nextquery:
    d->nxt = now + d->freq + d->fuzz;
    u = WS_Reserve(bg->ws, 0);
    l = strlen(d->name);
    assert(u > l+2);
    name = strncpy(bg->ws->f, d->name, u-1);
    *(name + l) = '.';
    *(name + l + 1) = '\0';
    if (!bg->dns) ADNS_INIT(bg);
    AN(bg->dns);
    AZ(adns_submit(bg->dns, name, adns_r_srv, d->dnsflags, mod, &d->query));
    npending++;
    if (interval > 5e-3)
      interval = 5e-3;

    VSL(SLT_Debug, 0, "disco: DNS-SD %s: Q SRV %s (now=%f)", d->name, name, now);
    WS_Release(bg->ws, 0);
  }
  update_rwlock_unlock(mod->mtx, NULL);

  if (!npending && bg->dns)
    ADNS_FREE(bg);
  return now + interval;
}

static void * v_matchproto_(bgthread_t)
disco_thread(struct worker *wrk, void *priv)
{
  struct vmod_disco_bgthread *bg;
  unsigned gen, shutdown;
  double d;

  CAST_OBJ_NOTNULL(bg, priv, VMOD_DISCO_BGTHREAD_MAGIC);
  (void)wrk;

  gen = bg->gen;
  shutdown = 0;
  Lck_Lock(&bg->mtx);
  VSL(SLT_Debug, 0, "disco: bgthread startup");
  while (!shutdown) {
// #ifdef HAVE_CLOCK_GETTIME
//    d = disco_thread_run(wrk, bg, VTIM_mono());
//#else
    d = disco_thread_run(wrk, bg, VTIM_real());
//#endif
    Lck_AssertHeld(&bg->mtx);
    if (!bg->shutdown && bg->dns) {
      Lck_Unlock(&bg->mtx);
      adns_processany(bg->dns);
      Lck_Lock(&bg->mtx);
    }
    if (d > 0 && d <= 1e9) {
      d = (1e9)+1;
    }
    if (gen == bg->gen && !bg->shutdown)
    {
      (void)Lck_CondWait(&bg->cond, &bg->mtx, d);
    }
    Lck_AssertHeld(&bg->mtx);
    gen = bg->gen;
    shutdown = bg->shutdown;
    if (!shutdown && bg->dns)
      adns_processany(bg->dns);
  }
  Lck_AssertHeld(&bg->mtx);
  VSL(SLT_Debug, 0, "disco: bgthread shutdown");
  bg->gen = 0;
  if (bg->dns) ADNS_FREE(bg);
  AZ(pthread_cond_broadcast(&bg->cond));
  Lck_Unlock(&bg->mtx);
  pthread_exit(0);
  return NULL;
}

void vmod_disco_bgthread_start(struct vmod_disco_bgthread **wrkp, void *priv, unsigned interval)
{
  struct vmod_disco_bgthread *wrk;
  unsigned char *s;

  ALLOC_OBJ(wrk, VMOD_DISCO_BGTHREAD_MAGIC);
  AN(wrk);

  wrk->ws = (struct ws*)PRNDUP(&wrk->__scratch[0]);
  s  = (unsigned char*)wrk->ws + PRNDUP(sizeof(struct ws));
  WS_Init(wrk->ws, "mii", s, sizeof(wrk->__scratch) - (s - &wrk->__scratch[0]));

  AN(lck_disco);
  Lck_New(&wrk->mtx, lck_disco);
// #ifdef HAVE_CLOCK_GETTIME
//  AZ(pthread_condattr_init(&wrk->conda));
//  AZ(pthread_condattr_setclock(&wrk->conda, CLOCK_MONOTONIC));
//  AZ(pthread_cond_init(&wrk->cond, &wrk->conda));
//#else
  AZ(pthread_cond_init(&wrk->cond, NULL));
//#endif
  wrk->gen = 1;
  wrk->interval = interval;
  wrk->priv = priv;
  wrk->vsb = VSB_new_auto();
  WRK_BgThread(&wrk->thr, "disco", disco_thread, wrk);
  if (wrkp)
    *wrkp = wrk;
}

void vmod_disco_bgthread_kick(struct vmod_disco_bgthread *wrk, unsigned shutdown)
{
  CHECK_OBJ_NOTNULL(wrk, VMOD_DISCO_BGTHREAD_MAGIC);

  Lck_Lock(&wrk->mtx);
  if(!wrk->gen) {
    AN(wrk->shutdown);
    Lck_Unlock(&wrk->mtx);
    return;
  }
  wrk->gen++;
  if (wrk->gen == 0)
    wrk->gen++;
  if (shutdown)
    wrk->shutdown++;

  AZ(pthread_cond_signal(&wrk->cond));
  Lck_Unlock(&wrk->mtx);
}

void vmod_disco_bgthread_delete(struct vmod_disco_bgthread **wrkp)
{
  struct vmod_disco_bgthread *bg;

  bg = *wrkp;
  *wrkp = NULL;

  CHECK_OBJ_NOTNULL(bg, VMOD_DISCO_BGTHREAD_MAGIC);
  vmod_disco_bgthread_kick(bg, 1);
  AZ(pthread_join(bg->thr, NULL));
  AZ(bg->gen);
  AZ(pthread_cond_destroy(&bg->cond));
#ifdef HAVE_CLOCK_GETTIME
  AZ(pthread_condattr_destroy(&bg->conda));
#endif
  VSB_destroy(&bg->vsb);
  Lck_Delete(&bg->mtx);
  FREE_OBJ(bg);
}

