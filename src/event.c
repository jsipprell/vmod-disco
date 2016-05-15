#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>

#include "vcl.h"
#include "vrt.h"
#include "cache/cache.h"
#include "cache/cache_director.h"

#include "vtim.h"
#include "vdir.h"
#include "vcc_disco_if.h"
#include "disco.h"

static pthread_mutex_t global_mtx = PTHREAD_MUTEX_INITIALIZER;
static int global_load_count = 0;
static struct vmod_disco *default_mod = NULL;
static struct vmod_disco *warmed_mod = NULL;

static void free_func(void *p)
{
  struct vmod_disco *vd;

  CAST_OBJ_NOTNULL(vd, p, VMOD_DISCO_MAGIC);
  AZ(pthread_mutex_lock(&global_mtx));
  AZ(pthread_rwlock_destroy(&vd->mtx));
  if (default_mod == vd)
    default_mod = NULL;
  if (warmed_mod == vd)
    warmed_mod = NULL;
  else if(warmed_mod && !default_mod)
    default_mod = warmed_mod;
  FREE_OBJ(vd);
  AZ(pthread_mutex_unlock(&global_mtx));
}

void current_vmod(struct vmod_priv *priv)
{
  struct vmod_disco *vd = priv->priv;
  CHECK_OBJ_ORNULL(vd, VMOD_DISCO_MAGIC);
  if (!vd) {
    AZ(pthread_mutex_lock(&global_mtx));
    vd = default_mod ? default_mod : warmed_mod;
    CHECK_OBJ_NOTNULL(vd, VMOD_DISCO_MAGIC);
    priv->priv = default_mod;
    AZ(pthread_mutex_unlock(&global_mtx));
  }
}

int __match_proto__(vmod_event_f)
vmod_event(VRT_CTX, struct vmod_priv *priv, enum vcl_event_e ev)
{
  struct vmod_disco *vd;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

  switch(ev) {
  case VCL_EVENT_LOAD:
    AZ(pthread_mutex_lock(&global_mtx));
    if (global_load_count == 0) {
      ALLOC_OBJ(vd, VMOD_DISCO_MAGIC);
      AN(vd);
      priv->len = sizeof(*vd);
      priv->free = free_func;
      VTAILQ_INIT(&vd->dirs);
      AZ(pthread_rwlock_init(&vd->mtx, NULL));
      default_mod = vd;
    } else {
      vd = priv->priv;
      CHECK_OBJ_ORNULL(vd, VMOD_DISCO_MAGIC);
      if (!vd) {
        struct vmod_disco *dvd = default_mod ? default_mod : warmed_mod;

        if (dvd) {
          AZ(pthread_rwlock_wrlock(&dvd->mtx));
        }
        ALLOC_OBJ(vd, VMOD_DISCO_MAGIC);
        AN(vd);
        priv->len = sizeof(*vd);
        priv->free = free_func;
        VTAILQ_INIT(&vd->dirs);
        AZ(pthread_rwlock_init(&vd->mtx, NULL));
        default_mod = vd;
        if (dvd) {
          AZ(pthread_rwlock_unlock(&dvd->mtx));
        }
      }
    }
    CHECK_OBJ_NOTNULL(vd, VMOD_DISCO_MAGIC);
    priv->priv = vd;
    global_load_count++;
    AZ(pthread_mutex_unlock(&global_mtx));
    break;
  case VCL_EVENT_DISCARD:
    AZ(pthread_mutex_lock(&global_mtx));
    AN(global_load_count);
    global_load_count--;
    AZ(pthread_mutex_unlock(&global_mtx));
    break;
  case VCL_EVENT_WARM:
    AZ(pthread_mutex_lock(&global_mtx));
    vd = priv->priv;
    CHECK_OBJ_NOTNULL(vd, VMOD_DISCO_MAGIC);
    AZ(vd->wrk);
    AZ(pthread_rwlock_wrlock(&vd->mtx));
    if (default_mod == NULL && warmed_mod != NULL)
      default_mod = warmed_mod;
    warmed_mod = vd;
    vmod_disco_bgthread_start(&vd->wrk, vd, 1.0);
    AZ(pthread_rwlock_unlock(&vd->mtx));
    AZ(pthread_mutex_unlock(&global_mtx));
    break;
  case VCL_EVENT_COLD:
    AZ(pthread_mutex_lock(&global_mtx));
    CAST_OBJ_NOTNULL(vd, priv->priv, VMOD_DISCO_MAGIC);
    CHECK_OBJ_NOTNULL(vd->wrk, VMOD_DISCO_BGTHREAD_MAGIC);
    vmod_disco_bgthread_delete(&vd->wrk);
    AZ(vd->wrk);
    if (vd == default_mod && warmed_mod != NULL)
      default_mod = warmed_mod;
    AZ(pthread_mutex_unlock(&global_mtx));
    break;
  case VCL_EVENT_USE:
    AZ(pthread_mutex_lock(&global_mtx));
    CAST_OBJ_NOTNULL(vd, priv->priv, VMOD_DISCO_MAGIC);
    if (vd != default_mod)
      default_mod = vd;
    AZ(pthread_mutex_unlock(&global_mtx));
  default:
    break;
  }
  return 0;
}
