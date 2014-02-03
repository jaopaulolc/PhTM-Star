#define _GNU_SOURCE
#include <rtm.h>
#include <lightlock.h>

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
light_lock_t __aux_lock = LIGHT_LOCK_INITIALIZER;
__thread long __aux_lock_owner = 0;
#endif /* RTM_CM_AUXLOCK */


#ifdef RTM_CM_BACKOFF
__thread unsigned short __thread_seed[3];
#endif /* RTM_CM_BACKOFF */

light_lock_t __rtm_global_lock = LIGHT_LOCK_INITIALIZER;

#define _XABORT_LOCKED 0xff
#define RTM_MAX_RETRIES 10

void _RTM_FORCE_INLINE _RTM_ABORT_STATS();

void _RTM_FORCE_INLINE TSX_START(long nthreads){

	__global_thread_tx = (tsx_tx_t*)calloc(nthreads,sizeof(tsx_tx_t));
	__global_thread_tx[0].nthreads = nthreads;
}

void _RTM_FORCE_INLINE TX_START(){

	do{
		if((__tx_status = _xbegin()) == _XBEGIN_STARTED){
			if(__rtm_global_lock == 1){
				_xabort(_XABORT_LOCKED);
			}
			return;
		}
		else{
			// execution flow continues here on transaction abort
			_RTM_ABORT_STATS();
		#ifdef RTM_CM_AUXLOCK
			if(__aux_lock_owner == 0 && __tx_status & _XABORT_CONFLICT){
				light_lock(&__aux_lock);
				__aux_lock_owner = 1;
			}
		#endif /* RTM_CM_AUXLOCK */
			if((__tx_status & _XABORT_RETRY)){
				__tx_retries++;
				if(__tx_retries >= RTM_MAX_RETRIES){
					light_lock(&__rtm_global_lock);
					return;
				}
		#ifdef RTM_CM_BACKOFF
				else{
					long a = nrand48(__thread_seed);
					long b = __thread_tx.totalAborts >> 2;
					usleep( (useconds_t)(a%b) );
				}
		#endif /* RTM_CM_BACKOFF */
			}
			else {
				__tx_retries = RTM_MAX_RETRIES;
				light_lock(&__rtm_global_lock);
				return;
			}
		}
	} while(1);
}

void _RTM_FORCE_INLINE TX_END(){
	
	if(__tx_retries >= RTM_MAX_RETRIES){
		light_unlock(&__rtm_global_lock);
		__tx_retries = 0;
	}
	else _xend();
#ifdef RTM_CM_AUXLOCK
	if(__aux_lock_owner == 1){
		light_unlock(&__aux_lock);
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
		#ifdef RTM_CM_BACKOFF
			__thread_seed[0] = (unsigned short)rand()%USHRT_MAX;
			__thread_seed[1] = (unsigned short)rand()%USHRT_MAX;
			__thread_seed[2] = (unsigned short)rand()%USHRT_MAX;
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

