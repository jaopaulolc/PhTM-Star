#ifndef _HLE_INCLUDE
#define _HLE_INCLUDE

#include <pthread.h>
/* Requires TSX support in the CPU */
#include <immintrin.h>

#define hle_lock_t volatile long
#define HLE_LOCK_INITIALIZER 0
#define hle_lock_init(a) ((*a) = 0)

#define hle_lock(l) \
	while (__atomic_exchange_n(l, 1, __ATOMIC_SEQ_CST | __ATOMIC_HLE_ACQUIRE) != 0){ \
		do{ \
			pthread_yield(); \
		}while(__atomic_load_n(l, __ATOMIC_CONSUME) == 1);\
	}

#define hle_unlock(l) \
	__atomic_store_n(l, 0, __ATOMIC_SEQ_CST | __ATOMIC_HLE_RELEASE)

#endif /* _HLE_INCLUDE */
