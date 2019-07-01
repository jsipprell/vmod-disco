#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cache/cache.h"

#include "vtim.h"
#include "vpridir.h"
#include "vcc_disco_if.h"
#include "disco.h"

static int global_load_count = 0;

struct VSC_lck *lck_disco = NULL;
struct vsc_seg *lck_disco_sg = NULL;

static void free_func(void *p)
{
  struct vmod_disco *vd;

  CAST_OBJ_NOTNULL(vd, p, VMOD_DISCO_MAGIC);
  update_rwlock_delete(&vd->mtx);
  AZ(vd->mtx);
  FREE_OBJ(vd);
}

static int event_load(VRT_CTX, struct vmod_priv *priv) {
  struct vmod_disco *vd;
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

  AZ(priv->priv);
  ALLOC_OBJ(vd, VMOD_DISCO_MAGIC);
  AN(vd);
  priv->priv = vd;
  priv->len = sizeof(*vd);
  priv->free = free_func;
  VTAILQ_INIT(&vd->dirs);
  update_rwlock_new(&vd->mtx);
  //update_rwlock_unlock(vd->mtx, NULL);
  AN(vd->mtx);

  if (global_load_count == 0) {
    AZ(lck_disco);
    AZ(lck_disco_sg);
    lck_disco = Lck_CreateClass(&lck_disco_sg, "disco");
  }
  global_load_count++;
  return (0);
}


static int event_discard(VRT_CTX, struct vmod_priv *priv) {
  struct vmod_disco *vd;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CAST_OBJ_NOTNULL(vd, priv->priv, VMOD_DISCO_MAGIC);
  AN(global_load_count);
  if (--global_load_count == 0) {
    AN(lck_disco);
    AN(lck_disco_sg);
    Lck_DestroyClass(&lck_disco_sg);
    lck_disco = NULL;
    AZ(lck_disco_sg);
  }
  return 0;
}

static int event_warm(VRT_CTX, struct vmod_priv *priv) {
  struct vmod_disco *vd;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CAST_OBJ_NOTNULL(vd, priv->priv, VMOD_DISCO_MAGIC);
  AN(global_load_count);
  AN(lck_disco);
  AZ(vd->wrk);
  update_rwlock_wrlock(vd->mtx);
  vmod_disco_bgthread_start(&vd->wrk, vd, 10);
  update_rwlock_unlock(vd->mtx, NULL);
  return 0;
}

static int event_cold(VRT_CTX, struct vmod_priv *priv) {
  struct vmod_disco *vd;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CAST_OBJ_NOTNULL(vd, priv->priv, VMOD_DISCO_MAGIC);
  AN(global_load_count);
  AN(lck_disco);
  AN(vd->wrk);
  vmod_disco_bgthread_delete(&vd->wrk);
  AZ(vd->wrk);
  return 0;
}

int v_matchproto_(vmod_event_f)
vmod_disco_event(VRT_CTX, struct vmod_priv *priv, enum vcl_event_e ev)
{
  switch(ev) {
  case VCL_EVENT_LOAD: return (event_load(ctx, priv));
  case VCL_EVENT_WARM: return (event_warm(ctx, priv));
  case VCL_EVENT_COLD: return (event_cold(ctx, priv));
  case VCL_EVENT_DISCARD: return (event_discard(ctx, priv));
  default: return (0);
  }
}


