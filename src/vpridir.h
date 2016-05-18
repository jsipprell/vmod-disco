struct vpri_director;

struct vpridir {
  unsigned magic;
#define VPRIDIR_MAGIC     0x99f4b276
  pthread_rwlock_t mtx;
  struct director *dir;

  VTAILQ_HEAD(,vpri_director) vdirs;
};

void vpridir_new(struct vpridir **vdp, const char *name, const char *vcl_name,
    vdi_healthy_f *healthy, vdi_resolve_f *resolve, void *priv);
void vpridir_delete(struct vpridir **vdp);
void vpridir_rdlock(struct vpridir *vd);
void vpridir_wrlock(struct vpridir *vd);
void vpridir_unlock(struct vpridir *vd);

unsigned vpridir_add_backend(struct vpridir *, VCL_BACKEND be, unsigned short pri, double weight);
unsigned vpridir_remove_backend(struct vpridir *, VCL_BACKEND be);
unsigned vpridir_any_healthy(struct vpridir *, const struct busyobj *,
    double *changed);
VCL_BACKEND vpridir_pick_be(struct vpridir *, double w, const struct busyobj *);
