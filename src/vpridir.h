struct vpri_director;

struct vpridir {
  unsigned magic;
#define VPRIDIR_MAGIC     0x99f4b276
  pthread_rwlock_t mtx;
  VCL_BACKEND dir;
  struct vdi_methods *methods;
  VTAILQ_HEAD(,vpri_director) vdirs;
};

void vpridir_new(VRT_CTX, struct vpridir **vdp, const char *vcl_name,
    vdi_healthy_f *healthy, vdi_resolve_f *resolve, vdi_list_f *list, void *priv);
void vpridir_delete(struct vpridir **vdp);
void vpridir_rdlock(struct vpridir *vd);
void vpridir_wrlock(struct vpridir *vd);
void vpridir_unlock(struct vpridir *vd);

int vpridir_add_backend(struct vpridir *, VCL_BACKEND be, unsigned short pri, double weight);
unsigned vpridir_remove_backend(struct vpridir *, VCL_BACKEND be);
unsigned vpridir_any_healthy(VRT_CTX, struct vpridir *, VCL_TIME *changed);
VCL_BACKEND vpridir_pick_be(VRT_CTX, struct vpridir *, double w);
VCL_BACKEND vpridir_pick_ben(VRT_CTX, struct vpridir *, unsigned i);
void vpridir_list(VRT_CTX, struct vpridir*, struct vsb*, int, int);
