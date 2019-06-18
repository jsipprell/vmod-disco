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

static pthread_mutex_t global_mtx = PTHREAD_MUTEX_INITIALIZER;
static int global_load_count = 0;
static struct vmod_disco *default_mod = NULL;
static struct vmod_disco *warmed_mod = NULL;

struct VSC_lck *lck_disco = NULL;

static void free_func(void *p)
{
  struct vmod_disco *vd;

  CAST_OBJ_NOTNULL(vd, p, VMOD_DISCO_MAGIC);
  AZ(pthread_mutex_lock(&global_mtx));
  update_rwlock_delete(&vd->mtx);
  AZ(vd->mtx);
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

int v_matchproto_(vmod_event_f)
vmod_disco_event(VRT_CTX, struct vmod_priv *priv, enum vcl_event_e ev)
{
  struct vmod_disco *vd;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

  switch(ev) {
  case VCL_EVENT_LOAD:
    if (lck_disco == NULL) {
      lck_disco = Lck_CreateClass(NULL, "mii");
    }

    VSL(SLT_Debug, 0, "%s: VCL_EVENT_LOAD", VCL_Name(ctx->vcl));

    AZ(pthread_mutex_lock(&global_mtx));
    if (global_load_count == 0) {
      ALLOC_OBJ(vd, VMOD_DISCO_MAGIC);
      AN(vd);
      priv->len = sizeof(*vd);
      priv->free = free_func;
      VTAILQ_INIT(&vd->dirs);
      update_rwlock_new(&vd->mtx);
      AN(vd->mtx);
      default_mod = vd;
    } else {
      vd = priv->priv;
      CHECK_OBJ_ORNULL(vd, VMOD_DISCO_MAGIC);
      if (!vd) {
        struct vmod_disco *dvd = default_mod ? default_mod : warmed_mod;

        if (dvd) {
          update_rwlock_wrlock(dvd->mtx);
        }
        ALLOC_OBJ(vd, VMOD_DISCO_MAGIC);
        AN(vd);
        priv->len = sizeof(*vd);
        priv->free = free_func;
        VTAILQ_INIT(&vd->dirs);
        update_rwlock_new(&vd->mtx);
        AN(vd->mtx);
        default_mod = vd;
        if (dvd) {
          update_rwlock_unlock(dvd->mtx, NULL);
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
    update_rwlock_wrlock(vd->mtx);
    if (default_mod == NULL && warmed_mod != NULL)
      default_mod = warmed_mod;
    warmed_mod = vd;
    vmod_disco_bgthread_start(&vd->wrk, vd, 10);
    update_rwlock_unlock(vd->mtx, NULL);
    AZ(pthread_mutex_unlock(&global_mtx));
    break;
  case VCL_EVENT_COLD:
    AZ(pthread_mutex_lock(&global_mtx));
    CAST_OBJ_NOTNULL(vd, priv->priv, VMOD_DISCO_MAGIC);
    CHECK_OBJ_NOTNULL(vd->wrk, VMOD_DISCO_BGTHREAD_MAGIC);
    AN(vd->wrk);
    vmod_disco_bgthread_delete(&vd->wrk);
    AZ(vd->wrk);
    if (vd == default_mod && warmed_mod != NULL)
      default_mod = warmed_mod;
    AZ(pthread_mutex_unlock(&global_mtx));
    break;
/*
  case VCL_EVENT_USE:
    AZ(pthread_mutex_lock(&global_mtx));
    CAST_OBJ_NOTNULL(vd, priv->priv, VMOD_DISCO_MAGIC);
    if (vd != default_mod)
      default_mod = vd;
    AZ(pthread_mutex_unlock(&global_mtx));
*/
  default:
    break;
  }
  return 0;
}
