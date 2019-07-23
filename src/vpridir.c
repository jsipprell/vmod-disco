#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "cache/cache.h"

#include "vbm.h"
#include "vsb.h"

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
                vdi_healthy_f *healthy, vdi_resolve_f *resolve, vdi_list_f *list, void *priv)
{
  struct vpridir *vp;

  AN(vpd);
  ALLOC_OBJ(vp, VPRIDIR_MAGIC);
  AN(vp);

  *vpd = vp;
  VTAILQ_INIT(&vp->vdirs);
  AZ(pthread_rwlock_init(&vp->mtx, NULL));

  ALLOC_OBJ(vp->methods, VDI_METHODS_MAGIC);
  vp->methods->type = "disco";
  vp->methods->healthy = healthy;
  vp->methods->resolve = resolve;
  vp->methods->list = list;
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

int vpridir_add_backend(struct vpridir *vp, VCL_BACKEND be, unsigned short pri, double weight)
{
  unsigned i;
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
      vdir_new(&nv->vd, vp->dir->vcl_name);
      VTAILQ_INSERT_BEFORE(v, nv, list);
      v = nv;
      goto foundpri;
    }
  }
  ALLOC_OBJ(v, VPRI_MAGIC);
  AN(v);
  v->pri = pri;
  vdir_new(&v->vd, vp->dir->vcl_name);
  VTAILQ_INSERT_TAIL(&vp->vdirs, v, list);
foundpri:
  CHECK_OBJ_NOTNULL(v, VPRI_MAGIC);
  assert(v->pri == pri);

  i = vdir_add_backend(v->vd, be, weight);
  if (i < 0) {
    vdir_delete(&v->vd);
    AZ(v->vd);
    VTAILQ_REMOVE(&vp->vdirs, v, list);
    FREE_OBJ(v);
  }
  vpridir_unlock(vp);
  return (i);
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
    be = vdir_pick_be(ctx, v->vd, vp->dir, w);
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
    be = vdir_pick_ben(ctx, v->vd, vp->dir, i);
    if (be)
      goto benpicked;
  }
benpicked:
  vpridir_unlock(vp);
  return (be);
}

void vpridir_list(VRT_CTX, struct vpridir *vp, struct vsb *vsb, int pflag, int jflag)
{
  vpridir_t *v;
  unsigned count = 0;

  vpridir_rdlock(vp);
  VTAILQ_FOREACH(v, &vp->vdirs, list) {
    CHECK_OBJ_NOTNULL(v, VPRI_MAGIC);
    vdir_list(ctx, v->vd, vp->dir, vsb, pflag, jflag, v->pri);
    count++;
  }
  vpridir_unlock(vp);

  if (!count) {
    if (jflag && pflag) {
      VSB_cat(vsb, "\n");
      VSB_indent(vsb, -2);
      VSB_cat(vsb, "}\n");
      VSB_indent(vsb, -2);
      VSB_cat(vsb, "},\n");
    }

    if (!pflag) {
      if (jflag)
        VSB_cat(vsb, "[0, 0, \"sick\"]");
      else
        VSB_cat(vsb, "0/0\tsick");
    }
  }
}

unsigned vpridir_any_healthy(VRT_CTX, struct vpridir *vp, VCL_TIME *changed)
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

