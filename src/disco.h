#ifndef VMOD_DISCO_MAGIC
#include <adns.h>

typedef struct update_rwlock* update_rwlock_t;

struct vmod_disco_bgthread {
  unsigned magic;
#define VMOD_DISCO_BGTHREAD_MAGIC 0x83e012af
  pthread_t thr;
  struct lock mtx;
  unsigned gen;
  pthread_cond_t cond;
#ifdef HAVE_CLOCK_GETTIME
  pthread_condattr_t conda;
#endif
  unsigned shutdown;
  double interval;
  void *priv;
  adns_state dns;
  struct ws *ws;
  struct vsb *vsb;
  unsigned char __scratch[2048];
};

typedef struct dns_rr_srv {
  char name[32];
  uint16_t priority, weight, port;
  adns_rr_addr addr;
} dns_srv_t;

struct vmod_disco {
  unsigned magic;
#define VMOD_DISCO_MAGIC 0x6ff301a9

  struct vmod_disco_bgthread *wrk;
  update_rwlock_t mtx;

  VTAILQ_HEAD(,vmod_disco_director) dirs;
};

struct vmod_disco_director {
  unsigned magic;
#define VMOD_DISCO_DIRECTOR_MAGIC 0x0561ffea

  struct vpridir *vd;
  char *name;
  const struct vrt_backend_probe *probe;

  double nxt, freq, fuzz;
  adns_query query;
  adns_queryflags dnsflags;

  dns_srv_t *srv;
  unsigned n_srv, l_srv;
  unsigned changes;

  const struct suckaddr **addrs;
  VCL_BACKEND *backends;
  unsigned n_backends, l_backends;

  void *priv;
  VTAILQ_ENTRY(vmod_disco_director) list;
};

typedef struct vmod_disco_director disco_t;

struct vmod_disco_selector {
  unsigned magic;
#define VMOD_DISCO_SELECTOR_MAGIC 0xaa1c87b2
  disco_t *d;
  struct vmod_disco *mod;
  struct ws *ws;
  unsigned char __scratch[2048];
};


void vmod_disco_bgthread_start(struct vmod_disco_bgthread **wrkp, void *priv, unsigned interval);
void vmod_disco_bgthread_kick(struct vmod_disco_bgthread *wrk, unsigned shutdown);
void vmod_disco_bgthread_delete(struct vmod_disco_bgthread **wrkp);

void current_vmod(struct vmod_priv*);

void update_rwlock_new(update_rwlock_t*);
void update_rwlock_delete(update_rwlock_t*);
void update_rwlock_rdlock(update_rwlock_t);
void update_rwlock_wrlock(update_rwlock_t);
void update_rwlock_unlock(update_rwlock_t, const struct update_rwlock*);
int update_rwlock_tryrdlock(update_rwlock_t);
int update_rwlock_tryanylock(update_rwlock_t, int*);

#endif /* VMOD_DISCO_MAGIC */
