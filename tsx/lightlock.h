#ifndef _LIGHT_LOCK_INCLUDE
#define _LIGHT_LOCK_INCLUDE

#include <immintrin.h>

#define light_lock_t volatile long
#define LIGHT_LOCK_INITIALIZER 0
#define light_lock_init(a) ((*a) = 0)

#define light_lock(l) \
	while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE)) \
			_mm_pause()

#define light_unlock(l) \
	__atomic_store_n(l, 0, __ATOMIC_RELEASE)

#endif /* _LIGHT_LOCK_INCLUDE */
