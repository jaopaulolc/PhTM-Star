#define _GNU_SOURCE
#include <rtm.h>
#include <lightlock.h>
#include <hle.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

tsx_tx_t *__global_thread_tx;

__thread tsx_tx_t *__thread_tx;

__thread long __tx_status;  // _xbegin() return status
__thread long __tx_retries; // number of retries

#ifdef RTM_CM_AUXLOCK
lock_t __aux_lock = LOCK_INITIALIZER;
__thread long __aux_lock_owner = 0;
#endif /* RTM_CM_AUXLOCK */

#ifdef RTM_CM_TRYLOCK
__thread struct timespec __tx_timespec; //transaction's sleep time specification
#endif /* RTM_CM_TRYLOCK */

#ifdef RTM_CM_BACKOFF
#define MIN_BACKOFF (1UL << 2)
#define MAX_BACKOFF (1UL << 31)
__thread unsigned long __thread_backoff; /* Maximum backoff duration */
__thread unsigned long __thread_seed; /* Random generated seed */
#endif /* RTM_CM_BACKOFF */

lock_t __rtm_global_lock = LOCK_INITIALIZER;

#define _XABORT_LOCKED 0xff
#define RTM_MAX_RETRIES 5

void _RTM_FORCE_INLINE _RTM_ABORT_STATS();

void _RTM_FORCE_INLINE TSX_START(long nthreads){

	__global_thread_tx = (tsx_tx_t*)calloc(nthreads,sizeof(tsx_tx_t));
	__global_thread_tx[0].nthreads = nthreads;
}

void _RTM_FORCE_INLINE TX_START(){

	do{
		if((__tx_status = _xbegin()) == _XBEGIN_STARTED){
			if( __atomic_load_n(&__rtm_global_lock,__ATOMIC_ACQUIRE)  == 1){
				_xabort(_XABORT_LOCKED);
			}
			return;
		}
		else{
			// execution flow continues here on transaction abort
			_RTM_ABORT_STATS();
		#ifdef RTM_CM_AUXLOCK
			if(__aux_lock_owner == 0 && __tx_status & _XABORT_CONFLICT){
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
			__tx_retries++;
			if(__tx_retries >= RTM_MAX_RETRIES){
			#ifdef RTM_CM_SPINLOCK
				hle_lock(&__rtm_global_lock);
				return;
			#endif /* RTM_CM_SPINLOCK */
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

void _RTM_FORCE_INLINE TX_END(){
	
	if(__tx_retries >= RTM_MAX_RETRIES){
		hle_unlock(&__rtm_global_lock);
		__tx_retries = 0;
	}
	else _xend();
#ifdef RTM_CM_AUXLOCK
	if(__aux_lock_owner == 1){
		unlock(&__aux_lock);
		__aux_lock_owner = 0;
	}
#endif /* RTM_CM_AUXLOCK */
}

void _RTM_FORCE_INLINE TX_INIT(long id){

	if(__global_thread_tx != NULL){
		__thread_tx = &__global_thread_tx[id];
		__thread_tx->id              = id;
		__thread_tx->nthreads        = __global_thread_tx[0].nthreads;
		__tx_status									 = 0;
		__tx_retries								 = 0;
		#ifdef RTM_CM_TRYLOCK
			//set transaction's sleep time to 20 nanoseconds
			//(10.2 cycles - 3.4 GHz)
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

void _RTM_FORCE_INLINE TSX_FINISH(){

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
			printf(" %ld %ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
				__global_thread_tx[i].id,
				__global_thread_tx[i].totalAborts,
				__global_thread_tx[i].explicitAborts,
				__global_thread_tx[i].conflictAborts,
				__global_thread_tx[i].capacityAborts,
				__global_thread_tx[i].debugAborts,
				__global_thread_tx[i].nestedAborts,
				__global_thread_tx[i].unknownAborts);
			
			totalAborts    += __global_thread_tx[i].totalAborts;
			explicitAborts += __global_thread_tx[i].explicitAborts;
			conflictAborts += __global_thread_tx[i].conflictAborts;
			capacityAborts += __global_thread_tx[i].capacityAborts;
			debugAborts    += __global_thread_tx[i].debugAborts;
			nestedAborts   += __global_thread_tx[i].nestedAborts;
			unknownAborts  += __global_thread_tx[i].unknownAborts;
		}
		printf("----------------------------------------------------------\n");
		printf(" * %ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
			totalAborts,explicitAborts,
			conflictAborts,capacityAborts,
			debugAborts,nestedAborts,unknownAborts);
		free(__global_thread_tx);
	#endif /* RTM_ABORT_DEBUG */
}

void _RTM_FORCE_INLINE _RTM_ABORT_STATS(){
	__thread_tx->totalAborts++;
	if(__tx_status & _XABORT_EXPLICIT){
		__thread_tx->explicitAborts++;
	}
	else if(__tx_status & _XABORT_CONFLICT){
		__thread_tx->conflictAborts++;
	}
	else if(__tx_status & _XABORT_CAPACITY){
		__thread_tx->capacityAborts++;
		//transaction will not commit on future attempts
		__tx_retries = RTM_MAX_RETRIES;
	}
	else if(__tx_status & _XABORT_DEBUG){
		__thread_tx->debugAborts++;
		//transaction will not commit on future attempts
		__tx_retries = RTM_MAX_RETRIES;
	}
	else if(__tx_status & _XABORT_NESTED){
		__thread_tx->nestedAborts++;
		//transaction will not commit on future attempts
		__tx_retries = RTM_MAX_RETRIES;
	}
	else {
		__thread_tx->unknownAborts++;
	}
}

