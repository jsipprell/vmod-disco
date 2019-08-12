#ifndef HAVE_ATOMICPTR

typedef struct { pthread_mutex_t _m; uint32_t _atomic; } vatomic_uint32_t;
typedef struct { pthread_mutex_t _m; uint64_t _atomic; } vatomic_uint64_t;
typedef struct { pthread_mutex_t _m; const void *_atomic; } vatomic_ptr_t;

#define VATOMIC_INIT(op) do { AZ(pthread_mutex_init(&((op)._m),NULL)); (op)._atomic = 0; } while (0)
#define VATOMIC_FINI(op) AZ(pthread_mutex_destroy(&((op)._m)))
#define _vatomic_set(op,v) do { AZ(pthread_mutex_lock(&(op)._m)); (op)._atomic = (v); AZ(pthread_mutex_unlock(&(op)._m)); } while(0)
#define VATOMIC_GET32(op) _vatomic_get32(&(op))
#define VATOMIC_GET64(op) _vatomic_get64(&(op))
#define VATOMIC_SET32 _vatomic_set
#define VATOMIC_SET64 _vatomic_set
#define VATOMIC_INC32(op) (_vatomic_add32(&(op), 1)+1)
#define VATOMIC_DEC32(op) (_vatomic_add32(&(op), -1)-1)
#define VATOMIC_INC64(op) (_vatomic_add64(&(op), 1)+1)
#define VATOMIC_DEC64(op) (_vatomic_add64(&(op), -1)-1)
#define VATOMIC_CAS32(op,cmp,set) (_vatomic_cas32(&(op), (cmp), (set)))
#define vatomic_declare(name, size, ...) _vatomic_##name##size(vatomic_uint##size##_t *v, ##__VA_ARGS__)
#define vatomic_declare_get(size) static inline uint##size##_t vatomic_declare(get, size) { \
  register uint##size##_t value; \
  AZ(pthread_mutex_lock(&v->_m)); \
  value = v->_atomic; \
  AZ(pthread_mutex_unlock(&v->_m)); \
  return (value); \
}
#define vatomic_declare_add(size) static inline int##size##_t vatomic_declare(add, size, int##size##_t value) { \
  register int##size##_t old_value; \
  AZ(pthread_mutex_lock(&v->_m)); \
  old_value = v->_atomic; \
  v->_atomic += value; \
  AZ(pthread_mutex_unlock(&v->_m)); \
  return (old_value); \
}
#define vatomic_declare_cas(size) static inline unsigned char _vatomic_cas##size(vatomic_uint##size##_t *v, uint##size##_t cmp, uint##size##_t set) { \
  register unsigned char ret; \
  AZ(pthread_mutex_lock(&v->_m)); \
  if ((ret = (v->_atomic == cmp))) v->_atomic = set; \
  AZ(pthread_mutex_unlock(&v->_m)); \
  return (ret); \
}

vatomic_declare_get(32)
vatomic_declare_get(64)
vatomic_declare_add(32)
vatomic_declare_add(64)
vatomic_declare_cas(32)
#undef vatomic_declare_cas
#undef vatomic_declare_get
#undef vatomic_declare


#else /* HAVE_ATOMICPTR */
typedef struct { volatile uint32_t _atomic; } __attribute__((aligned(8))) vatomic_uint32_t;
typedef struct { volatile uint64_t _atomic; } __attribute__((aligned(8))) vatomic_uint64_t;
typedef struct { const void* volatile _atomic; } __attribute__((aligned(SIZEOF_VOIDP))) vatomic_ptr_t;

#define VATOMIC_INIT(op) ((op)._atomic = 0)
#define VATOMIC_FINI(op) ((void)(op))
#define VATOMIC_SET32(op,v) ((op)._atomic = (v))
#define VATOMIC_GET32(op) ((op)._atomic)
#define VATOMIC_INC64(op) (vatomic_add64(&(op), 1)+1)
#define VATOMIC_DEC64(op) (vatomic_add64(&(op), -1)-1)
#define VATOMIC_INC32(op) (vatomic_add32(&(op), 1)+1)
#define VATOMIC_DEC32(op) (vatomic_add32(&(op), -1)-1)
#define VATOMIC_CAS32(op,cmp,set) (vatomic_cas32(&(op), (cmp), (set)))

#ifdef ARCH_X86_64
#define VATOMIC_SET64(op,v) ((op)._atomic = (v))
#define VATOMIC_GET64(op) ((op)._atomic)

static inline uint64_t vatomic_add64(vatomic_uint64_t *v, volatile int64_t i)
{
  asm volatile("lock xaddq %0, %1;"
               :"+r" (i)
               :"m" (v->_atomic)
               :"memory");
  return (uint64_t)i;
}

#elif defined(ARCH_X86)
#define VATOMIC_SET64(op,v) (vatomic_set64(&(op), (v)))
#define VATOMIC_GET64(op) (vatomic_get64(&(op)))

static inline uint64_t vatomic_add64(vatomic_uint64_t *v, volatile int64_t i)
{
  volatile uint32_t i32lo = i, i32hi = (i >> 32);

  asm volatile("movl %%ebx, %%eax;"
               "movl %%ecx, %%edx;"
               "lock cmpxchg8b %3;"
               "1: movl %%eax, %%ebx;"
               "movl %%edx, %%ecx;"
               "addl %1, %%ebx;"
               "adcl %2, %%ecx;"
               "lock cmpxchg8b %3;"
               "jne 1b"
               : "=&A" (i)
               : "m" (i32lo), "m" (i32hi), "m" (v->_atomic)
               :"ebx","ecx","memory");
  return (uint64_t)i;
}

static inline uint64_t vatomic_get64(vatomic_uint64_t *v)
{
  volatile uint64_t i;

  asm volatile("movl %%ebx, %%eax;"
               "movl %%ecx, %%edx;"
               "lock cmpxchg8b %1;"
               : "=&A" (i)
               : "m" (v->_atomic)
               : "ebx","ecx","memory");

  return i;
}

static inline void vatomic_set64(vatomic_uint64_t *v, volatile uint64_t i)
{
  volatile uint32_t i32lo = i, i32hi = (i >> 32);

  asm volatile("movl %%ebx, %%eax;"
               "movl %%ecx, %%edx;"
               "lock cmpxchg8b %2;"
               "1: movl %0, %%ebx;"
               "movl %1, %%ecx;"
               "lock cmpxchg8b %2;"
               "jne 1b"
               :
               : "m" (i32lo), "m" (i32hi), "m" (v->_atomic)
               : "eax","edx","ebx","ecx","memory");
}
#else
# error "Unsupported architecture"
#endif
static inline uint32_t vatomic_add32(vatomic_uint32_t *v, volatile int32_t i)
{
  asm volatile("lock xaddl %0, %1;"
               :"+r" (i)
               :"m" (v->_atomic)
               :"memory");
  return (uint32_t)i;
}

static inline unsigned char vatomic_cas32(vatomic_uint32_t *v, volatile uint32_t cmp, volatile uint32_t set)
{
  volatile unsigned char ret;

  asm volatile("lock cmpxchgl %3, %1;"
               "sete %0"
               :"=q" (ret), "+m" (v->_atomic)
               :"a" (cmp), "r" (set)
               :"memory");

  return ret;
}

#endif /* HAVE_ATOMICPTR */
