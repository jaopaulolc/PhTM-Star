#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif
#include <rtm.h>
#include <lightlock.h>
#include <hle.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

static __thread long __tx_status;  // _xbegin() return status
static __thread long __tx_id;  // tx thread id
#define RTM_MAX_RETRIES 5
static __thread long __tx_retries; // current number of retries

#ifdef RTM_CM_AUXLOCK
static lock_t __aux_lock = LOCK_INITIALIZER;
static __thread long __aux_lock_owner = 0;
#endif /* RTM_CM_AUXLOCK */

#ifdef RTM_CM_TRYLOCK
static __thread struct timespec __tx_timespec; //transaction's sleep time specification
#endif /* RTM_CM_TRYLOCK */

#ifdef RTM_CM_BACKOFF
#define MIN_BACKOFF (1UL << 2)
#define MAX_BACKOFF (1UL << 31)
static __thread unsigned long __thread_backoff; /* Maximum backoff duration */
static __thread unsigned long __thread_seed; /* Random generated seed */
#endif /* RTM_CM_BACKOFF */

static lock_t __rtm_global_lock = LOCK_INITIALIZER;

#define _XABORT_LOCKED 0xff

static void set_affinity(long id);
static void __rtm_abort_update_retries(long status,long *retries);

void TX_START(){
	
	__tx_retries = 0;

	do{
		if((__tx_status = _xbegin()) == _XBEGIN_STARTED){
			if( __atomic_load_n(&__rtm_global_lock,__ATOMIC_ACQUIRE)  == 1){
				_xabort(_XABORT_LOCKED);
			}
			return;
		}
		else{
			__tx_status = __tx_status & 0x0000003F;
			// execution flow continues here on transaction abort
			__rtm_abort_update_retries(__tx_status,&__tx_retries);
		#ifdef RTM_CM_AUXLOCK
			if(__aux_lock_owner == 0 && __tx_status == _XABORT_CONFLICT){
				lock(&__aux_lock);
				__aux_lock_owner = 1;
			}
		#endif /* RTM_CM_AUXLOCK */
		#ifdef RTM_CM_BACKOFF
			__thread_seed ^= (__thread_seed << 17);
			__thread_seed ^= (__thread_seed >> 13);
			__thread_seed ^= (__thread_seed << 5);
			unsigned long j, wait = __thread_seed % __thread_backoff;
			for (j = 0; j < wait; j++) {
				/* Do nothing */
			}
			if (__thread_backoff < MAX_BACKOFF)
				__thread_backoff <<= 1;
		#endif /* RTM_CM_BACKOFF */
			if(__tx_retries >= RTM_MAX_RETRIES){
		#ifdef RTM_CM_SPINLOCK1
				lock(&__rtm_global_lock);
				return;
		#endif /* RTM_CM_SPINLOCK1 */
		#ifdef RTM_CM_SPINLOCK2
				hle_lock(&__rtm_global_lock);
				return;
		#endif /* RTM_CM_SPINLOCK2 */
		#ifdef RTM_CM_TRYLOCK
				while(trylock(&__rtm_global_lock)){
					//failure, lock not acquired!
					//yeild and sleep
					nanosleep(&__tx_timespec,NULL);
				}
				//success, lock acquired!
				//return and execute critical section.
				return;
		#endif /* RTM_CM_TRYLOCK */
			}
		}
	} while(1);
}

void TX_END(){
	
	if(__tx_retries >= RTM_MAX_RETRIES){
	#ifdef RTM_CM_SPINLOCK1
		unlock(&__rtm_global_lock);
	#endif /* RTM_CM_SPINLOCK1 */
	#ifdef RTM_CM_SPINLOCK2
		hle_unlock(&__rtm_global_lock);
	#endif /* RTM_CM_SPINLOCK2 */
	}
	else _xend();
#ifdef RTM_CM_AUXLOCK
	if(__aux_lock_owner == 1){
		unlock(&__aux_lock);
		__aux_lock_owner = 0;
	}
#endif /* RTM_CM_AUXLOCK */
	__tx_retries = 0;
}

void TX_INIT(long id){

	set_affinity(id);

	__tx_id      = id;
	__tx_status  = 0;
	__tx_retries = 0;
	#ifdef RTM_CM_TRYLOCK
		//set transaction's sleep time to 3 nanoseconds
		__tx_timespec.tv_sec = 0;
		__tx_timespec.tv_nsec = 3;
	#endif /* RTM_CM_TRYLOCK */
	#ifdef RTM_CM_BACKOFF
		unsigned short seed[3];
		seed[0] = (unsigned short)rand()%USHRT_MAX;
		seed[1] = (unsigned short)rand()%USHRT_MAX;
		seed[2] = (unsigned short)rand()%USHRT_MAX;
		__thread_seed = nrand48(seed);
		__thread_backoff = MIN_BACKOFF;
	#endif /* RTM_CM_BACKOFF */
}

static void __rtm_abort_update_retries(long status, long *retries){
	
	switch(status){
		case _XABORT_EXPLICIT:
		case _XABORT_CONFLICT:
			(*retries)++;
			break;
		case _XABORT_CAPACITY: /* ++ */
		case _XABORT_DEBUG:    /* ++ */
		case _XABORT_NESTED:   /* ++ */
		default:
		//++ transaction will not commit on future attempts
			(*retries) = RTM_MAX_RETRIES;
			break;
	}
}

static void set_affinity(long id){
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN)/2; // 2 threads per core
	if (id < 0 || id >= num_cores){
		fprintf(stderr,"error: invalid number of threads (nthreads > ncores)!\n");
		exit(EXIT_FAILURE);
	}
	
	/* swthread | hwthread | core
	 *    0     |  0 ou 1  |  0
	 *    1     |  2 ou 3  |  1
	 *    2     |  4 ou 5  |  2
	 *    3     |  6 ou 7  |  3 */
	int map[] = {1, 3, 5, 7};
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(map[id]-1, &cpuset);
	CPU_SET(map[id], &cpuset);

	pthread_t current_thread = pthread_self();
	if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset)){
		perror("pthread_setaffinity_np");
		exit(EXIT_FAILURE);
	}
}
