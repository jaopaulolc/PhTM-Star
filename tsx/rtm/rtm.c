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

static tsx_tx_t *__global_thread_tx;

static __thread tsx_tx_t *__thread_tx;

static __thread long __tx_status;  // _xbegin() return status
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

static void __rtm_update_abort_stats(long status,long *retries,tsx_tx_t *tx);

void TSX_START(long nthreads){

	__global_thread_tx = (tsx_tx_t*)calloc(nthreads,sizeof(tsx_tx_t));
	int i;
	for(i=0; i < nthreads; i++){
		__global_thread_tx[i].nthreads = nthreads;
	}
}

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
			__tx_status = _XABORT_CODE(__tx_status);
			// execution flow continues here on transaction abort
			__tx_retries++;
			__rtm_update_abort_stats(__tx_status,&__tx_retries,__thread_tx);
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

	if(__global_thread_tx != NULL){
		__thread_tx = &__global_thread_tx[id];
		__thread_tx->id              = id;
		__tx_status									 = 0;
		__tx_retries								 = 0;
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
		__thread_tx->totalAborts     = 0;
		__thread_tx->explicitAborts  = 0;
		__thread_tx->conflictAborts  = 0;
		__thread_tx->capacityAborts  = 0;
		__thread_tx->debugAborts     = 0;
		__thread_tx->nestedAborts    = 0;
		__thread_tx->unknownAborts   = 0;
	}
}

void TSX_FINISH(){

	#ifdef RTM_ABORT_DEBUG
		long totalAborts    = 0;
		long explicitAborts = 0;
		long conflictAborts = 0;
		long capacityAborts = 0;
		long debugAborts    = 0;
		long nestedAborts   = 0;
		long unknownAborts  = 0;
		long nthreads = __global_thread_tx[0].nthreads;
		long i;
		printf("Core total explicit conflict capacity debug nested unknown\n");
		for(i=0; i < nthreads; i++){
			tsx_tx_t *tx = __global_thread_tx;
			printf(" %ld %ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
				tx[i].id,tx[i].totalAborts,
				tx[i].explicitAborts,tx[i].conflictAborts,
				tx[i].capacityAborts,tx[i].debugAborts,
				tx[i].nestedAborts,tx[i].unknownAborts);
			
			totalAborts    += tx[i].totalAborts;
			explicitAborts += tx[i].explicitAborts;
			conflictAborts += tx[i].conflictAborts;
			capacityAborts += tx[i].capacityAborts;
			debugAborts    += tx[i].debugAborts;
			nestedAborts   += tx[i].nestedAborts;
			unknownAborts  += tx[i].unknownAborts;
		}
		printf("----------------------------------------------------------\n");
		printf(" * %ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
			totalAborts,explicitAborts,
			conflictAborts,capacityAborts,
			debugAborts,nestedAborts,unknownAborts);
		free(__global_thread_tx);
	#endif /* RTM_ABORT_DEBUG */
}

static void __rtm_update_abort_stats(long status,long *retries,tsx_tx_t *tx){
	
	tx->totalAborts++;
	switch(status){
		case _XABORT_EXPLICIT:
			tx->explicitAborts++;
			return;
		case _XABORT_CONFLICT:
			tx->conflictAborts++;
			return;
		case _XABORT_CAPACITY: /* ++ */
			tx->capacityAborts++;
			break;
		case _XABORT_DEBUG:    /* ++ */
			tx->debugAborts++;
			break;
		case _XABORT_NESTED:   /* ++ */
			tx->nestedAborts++;
			break;
		default:
			tx->unknownAborts++;
			return;
	}
	//++ transaction will not commit on future attempts
	(*retries) = RTM_MAX_RETRIES;
}
