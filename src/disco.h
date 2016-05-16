#ifndef VMOD_DISCO_MAGIC
#include <adns.h>

struct vmod_disco_bgthread {
  unsigned magic;
#define VMOD_DISCO_BGTHREAD_MAGIC 0x83e012af
  pthread_t thr;
  struct lock mtx;
  unsigned gen;
  pthread_cond_t cond;
  unsigned shutdown;
  double interval;
  void *priv;
  adns_state dns;
  struct ws *ws;
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
  pthread_rwlock_t mtx;
  VTAILQ_HEAD(,vmod_disco_director) dirs;
};

struct vmod_disco_director {
  unsigned magic;
#define VMOD_DISCO_DIRECTOR_MAGIC 0x0561ffea

  struct vdir *vd;
  char *name;
  const struct vrt_backend_probe *probe;

  double nxt, freq;
  adns_query query;

  dns_srv_t *srv;
  unsigned n_srv, l_srv;
  unsigned changes;

  const struct suckaddr **addrs;
  struct director **backends;
  unsigned n_backends, l_backends;

  VTAILQ_ENTRY(vmod_disco_director) list;
};

typedef struct vmod_disco_director disco_t;

struct vmod_disco_random {
  unsigned magic;
#define VMOD_DISCO_ROUND_ROBIN_MAGIC 0x4eef931a
  disco_t *d;

  struct vmod_disco *mod;
  struct ws *ws;
  unsigned char __scratch[2048];
};

void vmod_disco_bgthread_start(struct vmod_disco_bgthread **wrkp, void *priv, unsigned interval);
void vmod_disco_bgthread_kick(struct vmod_disco_bgthread *wrk, unsigned shutdown);
void vmod_disco_bgthread_delete(struct vmod_disco_bgthread **wrkp);

void current_vmod(struct vmod_priv*);

#endif /* VMOD_DISCO_MAGIC */
