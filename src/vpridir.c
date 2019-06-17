#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "cache/cache.h"

#include "vbm.h"

#include "vpridir.h"
#include "vdir.h"

typedef struct vpri_director {
  unsigned magic;
#define VPRI_MAGIC 0x82ee190c
  unsigned short pri;
  struct vdir *vd;

  VTAILQ_ENTRY(vpri_director) list;
} vpridir_t;


void vpridir_new(VRT_CTX, struct vpridir **vpd, const char *vcl_name,
                vdi_healthy_f *healthy, vdi_resolve_f *resolve, void *priv)
{
  struct vpridir *vp;

  AN(vpd);
  ALLOC_OBJ(vp, VPRIDIR_MAGIC);
  AN(vp);

  *vpd = vp;
  VTAILQ_INIT(&vp->vdirs);
  AZ(pthread_rwlock_init(&vp->mtx, NULL));

  ALLOC_OBJ(vp->methods, VDI_METHODS_MAGIC);
  vp->methods->healthy = healthy;
  vp->methods->resolve = resolve;
  vp->dir = VRT_AddDirector(ctx, vp->methods, priv, "%s", vcl_name);
  AN(vp->dir);
}

void vpridir_delete(struct vpridir **vpd)
{
  struct vpridir *vp;
  vpridir_t *v;

  AN(vpd);
  CAST_OBJ_NOTNULL(vp, *vpd, VPRIDIR_MAGIC);
  CHECK_OBJ_NOTNULL(vp->dir, DIRECTOR_MAGIC);

  AZ(pthread_rwlock_wrlock(&vp->mtx));
  *vpd = NULL;
  for ( ;!VTAILQ_EMPTY(&vp->vdirs); ) {
    v = VTAILQ_FIRST(&vp->vdirs);
    CHECK_OBJ_NOTNULL(v, VPRI_MAGIC);
    CHECK_OBJ_NOTNULL(v->vd, VDIR_MAGIC);
    VTAILQ_REMOVE(&vp->vdirs, v, list);
    vdir_delete(&v->vd);
    AZ(v->vd);
    FREE_OBJ(v);
  }

  CHECK_OBJ_NOTNULL(vp->dir, DIRECTOR_MAGIC);
  VRT_DelDirector(&vp->dir);
  AZ(pthread_rwlock_unlock(&vp->mtx));
  AZ(pthread_rwlock_destroy(&vp->mtx));
  FREE_OBJ(vp->methods);
  FREE_OBJ(vp);
}

void vpridir_rdlock(struct vpridir *vp)
{
  CHECK_OBJ_NOTNULL(vp, VPRIDIR_MAGIC);
  AZ(pthread_rwlock_rdlock(&vp->mtx));
}

void vpridir_wrlock(struct vpridir *vp)
{
  CHECK_OBJ_NOTNULL(vp, VPRIDIR_MAGIC);
  AZ(pthread_rwlock_wrlock(&vp->mtx));
}

void vpridir_unlock(struct vpridir *vp)
{
  CHECK_OBJ_NOTNULL(vp, VPRIDIR_MAGIC);
  AZ(pthread_rwlock_unlock(&vp->mtx));
}

unsigned vpridir_add_backend(VRT_CTX, struct vpridir *vp, VCL_BACKEND be, unsigned short pri, double weight)
{
  unsigned u;
  vpridir_t *v;

  CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
  vpridir_wrlock(vp);

  VTAILQ_FOREACH(v, &vp->vdirs, list) {
    CHECK_OBJ_NOTNULL(v, VPRI_MAGIC);
    if (v->pri == pri)
      goto foundpri;
    else if (pri < v->pri) {
      vpridir_t *nv;

      ALLOC_OBJ(nv, VPRI_MAGIC);
      AN(nv);
      nv->pri = pri;
      vdir_new(ctx, &nv->vd, vp->dir->vcl_name, vp->dir->vcl_name, NULL, NULL, NULL);
      VTAILQ_INSERT_BEFORE(v, nv, list);
      v = nv;
      goto foundpri;
    }
  }
  ALLOC_OBJ(v, VPRI_MAGIC);
  AN(v);
  v->pri = pri;
  vdir_new(ctx, &v->vd, vp->dir->vcl_name, vp->dir->vcl_name, NULL, NULL, NULL);
  VTAILQ_INSERT_TAIL(&vp->vdirs, v, list);
foundpri:
  CHECK_OBJ_NOTNULL(v, VPRI_MAGIC);
  assert(v->pri == pri);

  u = vdir_add_backend(v->vd, be, weight);
  vpridir_unlock(vp);
  return (u);
}

unsigned vpridir_remove_backend(struct vpridir *vp, VCL_BACKEND be)
{
  unsigned n, u = 0;
  vpridir_t *v,*t;

  CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
  vpridir_wrlock(vp);

  VTAILQ_FOREACH_SAFE(v, &vp->vdirs, list, t) {
    CHECK_OBJ_NOTNULL(v, VPRI_MAGIC);
    CHECK_OBJ_NOTNULL(v->vd, VDIR_MAGIC);
    n = vdir_remove_backend(v->vd, be);
    u += n;
    if (!n) {
      vdir_delete(&v->vd);
      AZ(v->vd);
      VTAILQ_REMOVE(&vp->vdirs, v, list);
      FREE_OBJ(v);
    }
  }
  vpridir_unlock(vp);

  return (u);
}

VCL_BACKEND vpridir_pick_be(VRT_CTX, struct vpridir *vp, double w)
{
  VCL_BACKEND be = NULL;
  vpridir_t *v;

  vpridir_rdlock(vp);
  VTAILQ_FOREACH(v, &vp->vdirs, list) {
    CHECK_OBJ_NOTNULL(v, VPRI_MAGIC);
    be = vdir_pick_be(ctx, v->vd, w);
    if (be)
      goto bepicked;
  }
bepicked:
  vpridir_unlock(vp);
  return (be);
}

VCL_BACKEND vpridir_pick_ben(VRT_CTX, struct vpridir *vp, unsigned i)
{
  VCL_BACKEND be = NULL;
  vpridir_t *v;

  vpridir_rdlock(vp);
  VTAILQ_FOREACH(v, &vp->vdirs, list) {
    CHECK_OBJ_NOTNULL(v, VPRI_MAGIC);
    be = vdir_pick_ben(ctx, v->vd, i);
    if (be)
      goto benpicked;
  }
benpicked:
  vpridir_unlock(vp);
  return (be);
}

unsigned vpridir_any_healthy(VRT_CTX, struct vpridir *vp, double *changed)
{
  unsigned u = 0;
  vpridir_t *v;

  vpridir_rdlock(vp);
  VTAILQ_FOREACH(v, &vp->vdirs, list) { 
    CHECK_OBJ_NOTNULL(v, VPRI_MAGIC);
    u = vdir_any_healthy(ctx, v->vd, changed);
    if (u)
      goto anyhealthy;
  }
anyhealthy:
  vpridir_unlock(vp);
  return (u);
}

