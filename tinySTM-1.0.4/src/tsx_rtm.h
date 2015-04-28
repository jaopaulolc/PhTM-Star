#ifndef _TSX_RTM_H
#define _TSX_RTM_H

#include <pthread.h>
#include <immintrin.h>


#define lock_t volatile long
#define LOCK_INITIALIZER 0

#define isLocked(l) (__atomic_load_n(l,__ATOMIC_ACQUIRE) == 1)
#define lock(l) \
	while (__atomic_exchange_n(l,1,__ATOMIC_RELEASE)) pthread_yield()

#define unlock(l) \
	__atomic_store_n(l, 0, __ATOMIC_RELEASE)


extern lock_t __global_lock;
extern __thread int num_retries;

#define RTM_MAX_RETRIES 5

#define RTM_BEGIN()                               \
	num_retries = 0;                                \
	while(1){                                       \
		if( _xbegin() == _XBEGIN_STARTED ){           \
			if(isLocked(&__global_lock)) _xabort(0xFF); \
			break;                                      \
		}                                             \
		while(isLocked(&__global_lock));              \
		if(++num_retries >= RTM_MAX_RETRIES){         \
			lock(&__global_lock);                       \
			break;                                      \
		}                                             \
	}

#define RTM_END()                     \
	if(num_retries >= RTM_MAX_RETRIES){ \
		unlock(&__global_lock);           \
	} else _xend();

#endif /* _TSX_RTM_H */
