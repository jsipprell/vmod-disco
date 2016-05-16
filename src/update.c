#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>
#include <errno.h>

#include "vcl.h"
#include "vrt.h"
#include "cache/cache.h"
#include "cache/cache_director.h"

#include "vsa.h"
#include "vtim.h"
#include "vdir.h"
#include "vcc_disco_if.h"
#include "disco.h"

struct update_rwlock {
  unsigned magic;
#define UPDATE_RWLOCK_MAGIC 0x1eeffeef
  pthread_rwlock_t lock;

  pthread_mutex_t mtx;
  short           w;
};

void update_rwlock_new(struct update_rwlock **rwp)
{
  struct update_rwlock *lock;

  AN(rwp);
  ALLOC_OBJ(lock, UPDATE_RWLOCK_MAGIC);
  AN(lock);
  AZ(pthread_rwlock_init(&lock->lock, NULL));
  AZ(pthread_mutex_init(&lock->mtx, NULL));
  *rwp = lock;
}

void update_rwlock_delete(struct update_rwlock **rwp)
{
  struct update_rwlock *lock;

  AN(*rwp);
  lock = *rwp;
  *rwp = NULL;
  CHECK_OBJ_NOTNULL(lock, UPDATE_RWLOCK_MAGIC);
  AZ(pthread_rwlock_wrlock(&lock->lock));
  AZ(lock->w);
  AZ(pthread_mutex_destroy(&lock->mtx));
  AZ(pthread_rwlock_unlock(&lock->lock));
  AZ(pthread_rwlock_destroy(&lock->lock));
  FREE_OBJ(lock);
}

void update_rwlock_rdlock(struct update_rwlock *lock)
{
  CHECK_OBJ_NOTNULL(lock, UPDATE_RWLOCK_MAGIC);
  AZ(pthread_rwlock_rdlock(&lock->lock));
}

void update_rwlock_unlock(struct update_rwlock *lock, const struct update_rwlock *wrlock)
{
  CHECK_OBJ_NOTNULL(lock, UPDATE_RWLOCK_MAGIC);
  if(wrlock) {
    CHECK_OBJ_NOTNULL(wrlock, UPDATE_RWLOCK_MAGIC);
    assert(wrlock == lock);
    AZ(pthread_mutex_lock(&lock->mtx));
    AN(lock->w);
    lock->w--;
    AZ(lock->w);
    AZ(pthread_mutex_unlock(&lock->mtx));
  }
  AZ(pthread_rwlock_unlock(&lock->lock));
}

void update_rwlock_wrlock(struct update_rwlock *lock)
{
  CHECK_OBJ_NOTNULL(lock, UPDATE_RWLOCK_MAGIC);
  AZ(pthread_rwlock_wrlock(&lock->lock));
}

int update_rwlock_tryrdlock(struct update_rwlock *lock)
{
  CHECK_OBJ_NOTNULL(lock, UPDATE_RWLOCK_MAGIC);
  return (pthread_rwlock_tryrdlock(&lock->lock));
}

int update_rwlock_tryanylock(struct update_rwlock *lock, int *wrlocked)
{
  CHECK_OBJ_NOTNULL(lock, UPDATE_RWLOCK_MAGIC);
  AZ(pthread_mutex_lock(&lock->mtx));
  if (lock->w == 0) {
    lock->w++;
    if (wrlocked)
      *wrlocked = lock->w;
    AZ(pthread_mutex_unlock(&lock->mtx));
    AZ(pthread_rwlock_wrlock(&lock->lock));
    return 0;
  } else if(wrlocked)
    *wrlocked = 0;
  AZ(pthread_mutex_unlock(&lock->mtx));
  return pthread_rwlock_tryrdlock(&lock->lock);
}

