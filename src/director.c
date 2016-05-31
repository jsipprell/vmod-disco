#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>
#include <errno.h>

#include "vcl.h"
#include "vrt.h"
#include "cache/cache.h"
#include "cache/cache_director.h"

#include "vsa.h"
#include "vtim.h"
#include "vpridir.h"
#include "vcc_disco_if.h"
#include "disco.h"

static unsigned __match_proto__(vdi_healthy_f)
vd_healthy(const struct director *d, const struct busyobj *bo, double *changed)
{
  disco_t *dd;
  CHECK_OBJ_NOTNULL(d, DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(dd, d->priv, VMOD_DISCO_DIRECTOR_MAGIC);
  return (vpridir_any_healthy(dd->vd, bo, changed));
}

static const struct director * __match_proto__(vdi_resolve_f)
vd_resolve(const struct director *d, struct worker *wrk, struct busyobj *bo)
{
  disco_t *dd;

  CHECK_OBJ_NOTNULL(d, DIRECTOR_MAGIC);
  CHECK_OBJ_NOTNULL(wrk, WORKER_MAGIC);
  CHECK_OBJ_NOTNULL(bo, BUSYOBJ_MAGIC);

  CAST_OBJ_NOTNULL(dd, d->priv, VMOD_DISCO_DIRECTOR_MAGIC);
  return vpridir_pick_be(dd->vd, scalbn(random(), -31), bo);
}

static void expand_discovered_backends(disco_t *d, unsigned sz)
{
  d->backends = realloc(d->backends, sizeof(*(d->backends)) * sz);
  AN(d->backends);
  d->addrs = realloc(d->addrs, sizeof(*(d->addrs)) * sz);
  AN(d->addrs);
  d->l_backends = sz;
}

static void update_vdir_add_backend(struct vpridir *vdir, unsigned pri, unsigned weight, const struct director *be)
{
  CHECK_OBJ_NOTNULL(vdir, VPRIDIR_MAGIC);

  assert(weight <= 0xffff);
  vpridir_add_backend(vdir, be, pri, 1.0 + scalbn(weight, -16));
}

static void compact_backends(disco_t *d)
{
  int i;
  unsigned u = 0;

  for (i = (int)d->n_backends-1; i >= 0; i--) {
    assert(i < d->n_srv);
    if (!d->addrs[i]) {
      AZ(d->srv[i].port);
      AZ(d->backends[i]);
      if (u > 0) {
        assert(u < d->n_backends);
        memmove(&d->addrs[i], &d->addrs[u], (d->n_backends - u) * sizeof(d->addrs[i]));
        memmove(&d->backends[i], &d->backends[u], (d->n_backends - u) * sizeof(d->backends[i]));
        memmove(&d->srv[i], &d->srv[u], (d->n_srv - u) * sizeof(d->srv[i]));
        u = i;
      }
      d->n_backends--;
      d->n_srv--;
    } else u = i;
  }

  assert(d->n_srv == d->n_backends);
}

static const char *mkvclname(struct ws *ws, struct vmod_disco *vd,
                             const char *prefix, const char *suffix,
                             unsigned short port)
{
  int i;
  unsigned u, l;
  disco_t *d;
  char *cp, *name;

  CHECK_OBJ_NOTNULL(ws, WS_MAGIC);
  CHECK_OBJ_NOTNULL(vd, VMOD_DISCO_MAGIC);

  l = strlen(prefix) + strlen(suffix);
  u = WS_Reserve(ws, 0);
  assert(u > l+8);
  name = ws->f;
  strcpy(name, prefix);
  cp = name + strlen(prefix);
  *cp++ = '_';
  strcpy(cp, suffix);
  cp = name + l + 1;
  AZ(*cp);

  VTAILQ_FOREACH(d, &vd->dirs, list) {
    CHECK_OBJ_NOTNULL(d, VMOD_DISCO_DIRECTOR_MAGIC);
    for(i = 0; i < d->n_backends; i++) {
      if (i >= d->n_srv || !d->addrs[i] || !d->srv[i].port)
        continue;
      CHECK_OBJ_NOTNULL(d->backends[i], DIRECTOR_MAGIC);
      if (strcasecmp(d->backends[i]->vcl_name, name))
        continue;
      sprintf(cp, "_%hu", port);
      goto madevclname;
    }
  }

madevclname:
  WS_Release(ws, strlen(name)+1);
  return name;
}

static void update_backends(VRT_CTX, struct vmod_disco *vd, disco_t *d, short recreate)
{
  unsigned u,i,c = 0;
  struct suckaddr *ip;
  struct vrt_backend be;
  char *snap;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(d, VMOD_DISCO_DIRECTOR_MAGIC);

  snap = WS_Snapshot(ctx->ws);

  for (i = 0; i < d->n_srv; i++) {
    while(i >= d->l_backends)
      expand_discovered_backends(d, d->l_backends+16);
    if (d->srv[i].port == 0 || recreate) {
      if (i < d->n_backends && d->addrs[i]) {
        CHECK_OBJ_NOTNULL(d->backends[i], DIRECTOR_MAGIC);
        vpridir_remove_backend(d->vd, d->backends[i]);
        VRT_delete_backend(ctx, &d->backends[i]);
        AZ(d->backends[i]);
        free((void*)d->addrs[i]);
        d->addrs[i] = NULL;
      } else {
        while(i >= d->n_backends) {
          assert(d->n_backends < d->l_backends);
          d->addrs[d->n_backends] = NULL;
          d->backends[d->n_backends] = NULL;
          d->n_backends++;
        }
      }
      c++;
      if (d->srv[i].port == 0)
        continue;
    }
    WS_Reset(ctx->ws, snap);
    u = WS_Reserve(ctx->ws, 0);
    assert(u > vsa_suckaddr_len*2);
    ip = VSA_Build(ctx->ws->f, &d->srv[i].addr.addr.sa, d->srv[i].addr.len);
    AN(ip);
    assert(VSA_Sane(ip));
    WS_Release(ctx->ws, PRNDUP(vsa_suckaddr_len));
    if (i < d->n_backends && d->addrs[i]) {
      if (VSA_Compare(ip, d->addrs[i])) {
        CHECK_OBJ_NOTNULL(d->backends[i], DIRECTOR_MAGIC);
        vpridir_remove_backend(d->vd, d->backends[i]);
        VRT_delete_backend(ctx, &d->backends[i]);
        AZ(d->backends[i]);
        free((void*)d->addrs[i]);
        d->addrs[i] = NULL;
      }
    } else {
      while (i >= d->n_backends) {
        assert(d->n_backends < d->l_backends);
        d->addrs[d->n_backends] = NULL;
        d->backends[d->n_backends] = NULL;
        d->n_backends++;
        c++;
      }
    }
    if (!d->addrs[i]) {
      assert(VSA_Sane(ip));
      INIT_OBJ(&be, VRT_BACKEND_MAGIC);
      AN(d->srv[i].name);
      be.vcl_name = mkvclname(ctx->ws, vd, d->vd->dir->vcl_name, d->srv[i].name, VSA_Port(ip));
      AN(be.vcl_name);
      be.probe = d->probe;
      be.hosthdr = d->name;
      switch (VSA_Get_Proto(ip)) {
      case PF_INET6:
        AN(d->addrs[i] = be.ipv6_suckaddr = VSA_Clone(ip));
        break;
      case PF_INET:
        AN(d->addrs[i] = be.ipv4_suckaddr = VSA_Clone(ip));
        break;
      default:
        WRONG("Protocol family not supported (was neither PF_INET6 nor PF_INET)");
      }
      d->backends[i] = VRT_new_backend(ctx, &be);
      AN(d->backends[i]);
      update_vdir_add_backend(d->vd, d->srv[i].priority, d->srv[i].weight, d->backends[i]);
    }
  }
  assert(d->n_backends >= d->n_srv);
  for (i = d->n_srv; i < d->n_backends; i++) {
    if(d->addrs[i]) {
      AN(d->backends[i]);
      vpridir_remove_backend(d->vd, d->backends[i]);
      VRT_delete_backend(ctx, &d->backends[i]);
      AZ(d->backends[i]);
      free((void*)d->addrs[i]);
      d->addrs[i] = NULL;
      c++;
    }
  }
  d->n_backends = d->n_srv;

  WS_Reset(ctx->ws, snap);

  if(c)
    compact_backends(d);
}

VCL_VOID __match_proto__(td_disco_dance)
vmod_dance(VRT_CTX, struct vmod_priv *priv)
{
  struct vmod_disco *vd;
  disco_t *d;
  int err, wrlock = 0;
  unsigned changes;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  AN(priv);
  current_vmod(priv);
  CAST_OBJ_NOTNULL(vd, priv->priv, VMOD_DISCO_MAGIC);

  if (ctx->method == VCL_MET_INIT || ctx->method == VCL_MET_FINI) {
    if (ctx->vsl == NULL && ctx->msg != NULL) {
      VSB_printf(ctx->msg, "disco.dancer doesn't dance in vcl_init or vcl_fini\n");
      VRT_handling(ctx, VCL_RET_FAIL);
      return;
    }
    WRONG("inappropriate attempt at disco dancing");
  }

  err = update_rwlock_tryrdlock(vd->mtx);
  if (err == EBUSY) {
    /* silently ignore, disco is being updated by someone else */
    return;
  }
  AZ(err);

  VTAILQ_FOREACH(d, &vd->dirs, list) {
    if ((changes = d->changes) > 0) {
      if (!wrlock) {
        update_rwlock_unlock(vd->mtx, NULL);
        err = update_rwlock_tryanylock(vd->mtx, &wrlock);
        if (err == EBUSY) {
          AZ(wrlock);
          return;
        } else if (wrlock) changes = d->changes;
        AZ(err);
      }
      if (wrlock && changes) {
        VSL(SLT_Debug, 0, "%u changes to %s director", changes, d->name);
        d->changes = 0;
        update_backends(ctx,vd,d,0);
      }
    }
  }
  update_rwlock_unlock(vd->mtx, (wrlock ? vd->mtx : NULL));
}


VCL_VOID __match_proto__(td_disco_random__init)
vmod_random__init(VRT_CTX, struct vmod_disco_random **p, const char *vcl_name,
                       struct vmod_priv *priv, const char *name, double interval)
{
  struct vmod_disco *vd;
  unsigned u;
  unsigned char *s;
  char *b, *e;
  disco_t *d;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  AN(priv);

  current_vmod(priv);
  CAST_OBJ_NOTNULL(vd, priv->priv, VMOD_DISCO_MAGIC);

  ALLOC_OBJ(*p, VMOD_DISCO_ROUND_ROBIN_MAGIC);
  AN(*p);
  (*p)->mod = vd;
  (*p)->ws = (struct ws*)PRNDUP(&(*p)->__scratch[0]);
  s = ((unsigned char*)(*p)->ws) + PRNDUP(sizeof(struct ws));
  WS_Init((*p)->ws, "mii", s, sizeof((*p)->__scratch) - (s - &(*p)->__scratch[0]));

  update_rwlock_wrlock(vd->mtx);
  ALLOC_OBJ(d, VMOD_DISCO_DIRECTOR_MAGIC);
  AN(d);
  u = WS_Reserve((*p)->ws, strlen(name)+1);
  AN(u);
  b = (*p)->ws->f;
  e = b + (u-1);
  strcpy(b, name);
  assert(e > b);
  while(--e > b) {
    if (*e == '.') {
      *e = '\0';
    } else if(*e) {
      e++;
      *e++ = '\0';
      break;
    }
  }
  WS_Release((*p)->ws, e-b);
  d->name = b;
  d->dnsflags = adns_qf_want_allaf|adns_qf_quoteok_query|adns_qf_cname_loose;
  d->freq = interval;
  d->fuzz = (interval / 2) + ((interval / 4) - (interval / 2) * scalbn(random(), -31));
  vpridir_new(&d->vd, d->name, vcl_name, vd_healthy, vd_resolve, d);
  VTAILQ_INSERT_TAIL(&vd->dirs, d, list);
  (*p)->d = d;

  update_rwlock_unlock(vd->mtx, NULL);
  if (vd->wrk)
    vmod_disco_bgthread_kick(vd->wrk, 0);
}

VCL_VOID __match_proto__(td_disco_random__fini)
vmod_random__fini(struct vmod_disco_random **rrp)
{
  int i;
  struct vmod_disco *vd;
  struct vmod_disco_random *rr;
  struct lock *vdlock = NULL;

  AN(rrp);
  rr = *rrp;
  *rrp = NULL;

  CHECK_OBJ_NOTNULL(rr, VMOD_DISCO_ROUND_ROBIN_MAGIC);
  CHECK_OBJ_NOTNULL(rr->d, VMOD_DISCO_DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(vd, rr->mod, VMOD_DISCO_MAGIC);
  CHECK_OBJ_ORNULL(vd->wrk, VMOD_DISCO_BGTHREAD_MAGIC);
  if (vd->wrk) {
    vdlock = &vd->wrk->mtx;
    Lck_Lock(vdlock);
  }

  update_rwlock_wrlock(vd->mtx);
  VTAILQ_REMOVE(&vd->dirs, rr->d, list);

  if (rr->d->l_srv > 0)
    free(rr->d->srv);
  rr->d->l_srv = rr->d->n_srv = 0;
  rr->d->srv = NULL;

  for (i = (int)rr->d->n_backends - 1; i >= 0; i--) {
    if (rr->d->backends[i]) {
      vpridir_remove_backend(rr->d->vd, rr->d->backends[i]);
      rr->d->backends[i] = NULL;
    }
    if (rr->d->addrs[i]) {
      free((void*)rr->d->addrs[i]);
      rr->d->addrs[i] = NULL;
    }
  }
  rr->d->n_backends = rr->d->l_backends = 0;
  free(rr->d->backends);
  rr->d->backends = NULL;
  free(rr->d->addrs);
  rr->d->addrs = NULL;

  vpridir_delete(&rr->d->vd);
  FREE_OBJ(rr->d);
  rr->d = NULL;
  rr->mod = NULL;
  FREE_OBJ(rr);
  update_rwlock_unlock(vd->mtx, NULL);

  if (vdlock)
    Lck_Unlock(vdlock);
}

VCL_BACKEND __match_proto__(td_disco_random_backend)
vmod_random_backend(VRT_CTX, struct vmod_disco_random *rr)
{
  CHECK_OBJ_NOTNULL(rr, VMOD_DISCO_ROUND_ROBIN_MAGIC);
  CHECK_OBJ_NOTNULL(rr->d, VMOD_DISCO_DIRECTOR_MAGIC);
  CHECK_OBJ_NOTNULL(rr->d->vd, VPRIDIR_MAGIC);

  (void)ctx;
  return rr->d->vd->dir;
}

VCL_VOID __match_proto__(td_disco_random_use_tcp)
vmod_random_use_tcp(VRT_CTX, struct vmod_disco_random *rr)
{
  struct vmod_disco *vd;

  CHECK_OBJ_NOTNULL(rr, VMOD_DISCO_ROUND_ROBIN_MAGIC);
  CHECK_OBJ_NOTNULL(rr->d, VMOD_DISCO_DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(vd, rr->mod, VMOD_DISCO_MAGIC);

  (void)ctx;
  update_rwlock_wrlock(vd->mtx);
  rr->d->dnsflags |= adns_qf_usevc;
  update_rwlock_unlock(vd->mtx, NULL);
}

VCL_VOID __match_proto__(td_disco_random_set_probe)
vmod_random_set_probe(VRT_CTX, struct vmod_disco_random *rr, const struct vrt_backend_probe *probe)
{
  struct vmod_disco *vd;
  disco_t *d;

  CHECK_OBJ_NOTNULL(rr, VMOD_DISCO_ROUND_ROBIN_MAGIC);
  CHECK_OBJ_NOTNULL(rr->d, VMOD_DISCO_DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(vd, rr->mod, VMOD_DISCO_MAGIC);

  (void)ctx;
  update_rwlock_wrlock(vd->mtx);
  rr->d->probe = probe;
  if (ctx->ws) {
    VTAILQ_FOREACH(d, &vd->dirs, list) {
      d->changes = 0;
      update_backends(ctx,vd,d,1);
    }
  }
  update_rwlock_unlock(vd->mtx, NULL);
}
