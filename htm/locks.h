#ifndef _LOCKS_INCLUDE
#define _LOCKS_INCLUDE


#if defined(__x86_64__) || defined (__i386)

#ifdef SIMPLE_LOCK

#include <immintrin.h>
#include <pthread.h>

#define lock_t volatile long
#define LOCK_INITIALIZER 0

#define isLocked(l) (__atomic_load_n(l, __ATOMIC_SEQ_CST) == 1)
#define lock(l) \
	while (__atomic_exchange_n(l,1, __ATOMIC_SEQ_CST)) pthread_yield()

#define unlock(l) \
	__atomic_store_n(l, 0, __ATOMIC_RELEASE)

#elif HLE_LOCK

#include <hle.h>

#define lock_t	hle_lock_t
#define LOCK_INITIALIZER	HLE_LOCK_INITIALIZER

#define isLocked(l) (__atomic_load_n(l, __ATOMIC_SEQ_CST) == 1)
#define lock(l)		hle_lock(l)
#define unlock(l)	hle_unlock(l)

#else

#error "no lock type specified!"

#endif

#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)

#include <pthread.h>

#define lock_t volatile long
#define LOCK_INITIALIZER 0

#define isLocked(l) (__atomic_load_n(l,__ATOMIC_SEQ_CST) == 1)
#define lock(l) \
	while (__atomic_exchange_n(l,1, __ATOMIC_SEQ_CST)) pthread_yield()

#define unlock(l) \
	__atomic_store_n(l, 0, __ATOMIC_SEQ_CST)

#else /* ! ( x86_64 || PowerPC ) */

#error "unsupported architecture!"

#endif /* usupported  architecture */


#endif /* _LOCKS_INCLUDE */
