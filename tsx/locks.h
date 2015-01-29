#ifndef _LOCKS_INCLUDE
#define _LOCKS_INCLUDE

#ifdef SIMPLE_LOCK

#include <immintrin.h>
#include <pthread.h>

#define lock_t volatile long
#define LOCK_INITIALIZER 0

#define isLocked(l) (__atomic_load_n(l,__ATOMIC_ACQUIRE) == 1)
#define lock(l) \
	while (__atomic_exchange_n(l,1,__ATOMIC_RELEASE)) pthread_yield()

#define unlock(l) \
	__atomic_store_n(l, 0, __ATOMIC_RELEASE)

#elif HLE_LOCK

#include <hle.h>

#define lock_t	hle_lock_t
#define LOCK_INITIALIZER	HLE_LOCK_INITIALIZER

#define isLocked(l) (__atomic_load_n(l,__ATOMIC_ACQUIRE) == 1)
#define lock(l)		hle_lock(l)
#define unlock(l)	hle_unlock(l)

#else

#error "no lock type specified!"

#endif


#endif /* _LOCKS_INCLUDE */
