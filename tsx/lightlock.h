#ifndef _LIGHT_LOCK_INCLUDE
#define _LIGHT_LOCK_INCLUDE

#include <immintrin.h>

#define lock_t volatile long
#define LOCK_INITIALIZER 0
#define lock_init(a) ((*a) = 0)

#define lock(l) \
	while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE)) \
			_mm_pause()

#define trylock(l) \
	(__atomic_exchange_n(l,1,__ATOMIC_ACQUIRE) == 0)

#define unlock(l) \
	__atomic_store_n(l, 0, __ATOMIC_RELEASE)

#endif /* _LIGHT_LOCK_INCLUDE */
