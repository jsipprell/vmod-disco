#ifdef HAVE_ATOMICPTR

typedef struct { volatile uint32_t _atomic; } __attribute__((aligned(8))) vatomic_uint32_t;
typedef struct { volatile uint64_t _atomic; } __attribute__((aligned(8))) vatomic_uint64_t;
typedef struct { const void* volatile _atomic; } __attribute__((aligned(SIZEOF_VOIDP))) vatomic_ptr_t;

#define VATOMIC_SET(vd,ap,v) ((ap)._atomic = (v))
#define VATOMIC_GET(vd,ap) ((ap)._atomic)
#define VATOMIC_INC64(vd,ap) (vatomic_add64(&(ap), 1)+1)
#define VATOMIC_DEC64(vd,op) (vatomic_add64(&(op), -1)-1)
#define VATOMIC_INC32(vd,op) (vatomic_add32(&(op), 1)+1)
#define VATOMIC_DEC32(vd,op) (vatomic_add32(&(op), -1)-1)

#ifdef ARCH_X86_64
static inline int64_t vatomic_add64(vatomic_uint64_t *v, int64_t i)
{
  asm volatile("lock; xaddq %%rax, %2;"
               :"=a" (i)
               :"a" (i), "m" (v->_atomic)
               :"memory");
  return i;
}
#elif defined(ARCH_X86)
static inline int64_t vatomic_add64(vatomic_uint64_t *v, int64_t i)
{
  volatile uint32_t i32lo = i, i32hi = (i >> 32);

  asm volatile("movl %%ebx, %%eax;"
               "movl %%ecx, %%edx;"
               "lock; cmpxchg8b %3;"
               "1: movl %%eax, %%ebx;"
               "movl %%edx, %%ecx;"
               "addl %1, %%ebx;"
               "adcl %2, %%ecx;"
               "lock; cmpxchg8b %3;"
               "jne 1b"
               : "=&A" (i)
               : "m" (i32lo), "m" (i32hi), "m" (v->_atomic)
               :"ebx","ecx","memory");
  return i;
}
#else
# error "Unsupported architecture"
#endif
static inline int32_t vatomic_add32(vatomic_uint32_t *v, int32_t i)
{
  asm volatile("lock; xaddl %%eax, %2;"
               :"=a" (i)
               :"a" (i), "m" (v->_atomic)
               :"memory");
  return i;
}

#else /* !HAVE_ATOMICPTR */

typedef uint32_t vatomic_uint32_t;
typedef uint64_t vatomic_uint64_t;
typedef const void* vatomic_ptr_t;

#define VATOMIC_SET(vd,ap,v) do { vdir_wrlock((vd)); (ap) = (v); vdir_unlock((vd)); } while(0)
#define VATOMIC_GET vatomic_get
static inline const void* vatomic_get(struct vdir *vd, vatomic_ptr_t ap) {
  const void *v;

  vdir_rdlock(vd);
  v = ap;
  vdir_unlock(vd);
  return v;
}

#endif
