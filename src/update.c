#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "cache/cache.h"

#include "vsa.h"
#include "vtim.h"
#include "vpridir.h"
#include "vcc_disco_if.h"
#include "disco.h"

struct update_rwlock {
  unsigned magic;
#define UPDATE_RWLOCK_MAGIC 0x1eeffeef
  pthread_rwlock_t lock;
  vatomic_uint32_t w;
};

void update_rwlock_new(struct update_rwlock **rwp)
{
  struct update_rwlock *lock;

  AN(rwp);
  ALLOC_OBJ(lock, UPDATE_RWLOCK_MAGIC);
  AN(lock);
  AZ(pthread_rwlock_init(&lock->lock, NULL));
  VATOMIC_INIT(lock->w);
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
  AZ(VATOMIC_GET32(lock->w));
  AZ(pthread_rwlock_unlock(&lock->lock));
  AZ(pthread_rwlock_destroy(&lock->lock));
  VATOMIC_FINI(lock->w);
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
    AN(VATOMIC_CAS32(lock->w, 1, 0));
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

/* Block and acquire a write lock if nobody else has one. Otherwise, try to acquire a read lock.
 * wrlocked will be set to 1 if not NULL and the write lock was successful
 * wrlocked will be set to 0 if not NULL and the write lock was already held by someone else.
 * Returns: 0 upon either a successful read or write lock.
 *          EBUSY write lock was held by someone else and read lock is busy.
 *
 * Function only blocks if a write lock is possible but read locks are pending.
 */
int update_rwlock_tryanylock(struct update_rwlock *lock, int *wrlocked)
{
  int ret;
  CHECK_OBJ_NOTNULL(lock, UPDATE_RWLOCK_MAGIC);
  if (VATOMIC_CAS32(lock->w, 0, 1)) {
    if ((ret = pthread_rwlock_wrlock(&lock->lock)) == 0 && wrlocked)
      *wrlocked = 1;
    else if (ret != 0)
      AN(VATOMIC_CAS32(lock->w, 1, 0));
    return ret;
  } else if(wrlocked)
    *wrlocked = 0;
  return pthread_rwlock_tryrdlock(&lock->lock);
}

