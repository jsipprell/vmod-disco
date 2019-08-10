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
vatomic_declare_get(32)
vatomic_declare_get(64)
vatomic_declare_add(32)
vatomic_declare_add(64)
#undef vatomic_declare_get
#undef vatomic_declare


#else /* HAVE_ATOMICPTR */
typedef struct { volatile uint32_t _atomic; } __attribute__((aligned(8))) vatomic_uint32_t;
typedef struct { volatile uint64_t _atomic; } __attribute__((aligned(8))) vatomic_uint64_t;
typedef struct { const void* volatile _atomic; } __attribute__((aligned(SIZEOF_VOIDP))) vatomic_ptr_t;

#define VATOMIC_INIT(op) ((op)._atomic = 0)
#define VATOMIC_FINI(op) ((void)(op))
#define VATOMIC_SET32(ap,v) ((ap)._atomic = (v))
#define VATOMIC_SET64(ap,v) ((ap)._atomic = (v))
#define VATOMIC_GET32(ap) ((ap)._atomic)
#define VATOMIC_GET64(op) ((op)._atomic)
#define VATOMIC_INC64(ap) (vatomic_add64(&(ap), 1)+1)
#define VATOMIC_DEC64(op) (vatomic_add64(&(op), -1)-1)
#define VATOMIC_INC32(op) (vatomic_add32(&(op), 1)+1)
#define VATOMIC_DEC32(op) (vatomic_add32(&(op), -1)-1)

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
#endif /* HAVE_ATOMICPTR */
