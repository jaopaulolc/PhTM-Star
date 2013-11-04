#define _GNU_SOURCE
#include <rtm.h>
#include <lightlock.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

__thread tsx_tx_t __thread_tx;

__thread long __tx_status;  // _xbegin() return status
__thread long __tx_retries; // number of retries

#ifdef RTM_BACKOFF_ENABLED
__thread unsigned short __thread_seed[3];
#endif /* RTM_BACKOFF_ENABLED */

light_lock_t __rtm_global_lock = LIGHT_LOCK_INITIALIZER;

#define _XABORT_LOCKED 0xff
#define RTM_MAX_RETRIES 100

void _RTM_FORCE_INLINE _RTM_ABORT_STATS();

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
			if((__tx_status & _XABORT_RETRY)){
				__tx_retries++;
				if(__tx_retries >= RTM_MAX_RETRIES){
					light_lock(&__rtm_global_lock);
					return;
				}
		#ifdef RTM_BACKOFF_ENABLED
				else{
					long a = nrand48(__thread_seed);
					long b = 2 << __thread_tx.totalAborts;
					usleep( (useconds_t)(a%b) );
				}
		#endif /* RTM_BACKOFF_ENABLED */
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
}

void TX_INIT(long id){
	__thread_tx.id              = id;
	__tx_status									= 0;
	__tx_retries								= 0;
	#ifdef RTM_BACKOFF_ENABLED
	__thread_seed[0] = (unsigned short)rand()%USHRT_MAX;
	__thread_seed[1] = (unsigned short)rand()%USHRT_MAX;
	__thread_seed[2] = (unsigned short)rand()%USHRT_MAX;
	#endif /* RTM_BACKOFF_ENABLED */
	#ifdef RTM_ABORT_DEBUG
	__thread_tx.totalAborts     = 0;
	__thread_tx.explicitAborts  = 0;
	__thread_tx.conflictAborts  = 0;
	__thread_tx.capacityAborts  = 0;
	__thread_tx.debugAborts     = 0;
	__thread_tx.nestedAborts    = 0;
	__thread_tx.unknownAborts   = 0;
	#endif /* RTM_ABORT_DEBUG */
}

void TX_FINISH(){

	#ifdef RTM_ABORT_DEBUG
		fprintf(stderr,"\nthread_tx[%ld]\n",__thread_tx.id);
		long total = __thread_tx.totalAborts;
		fprintf(stderr," Total aborts    = %ld\n",total);
		fprintf(stderr," Explicit aborts = %ld (%.2lf%%)\n",__thread_tx.explicitAborts,1.0e2*__thread_tx.explicitAborts/total);
		fprintf(stderr," Conflict aborts = %ld (%.2lf%%)\n",__thread_tx.conflictAborts,1.0e2*__thread_tx.conflictAborts/total);
		fprintf(stderr," Capacity aborts = %ld (%.2lf%%)\n",__thread_tx.capacityAborts,1.0e2*__thread_tx.capacityAborts/total);
		fprintf(stderr," Debug aborts    = %ld (%.2lf%%)\n",__thread_tx.debugAborts,1.0e2*__thread_tx.debugAborts/total);
		fprintf(stderr," Nested aborts   = %ld (%.2lf%%)\n",__thread_tx.nestedAborts,1.0e2*__thread_tx.nestedAborts/total);
		fprintf(stderr," Unknown aborts  = %ld (%.2lf%%)\n",__thread_tx.unknownAborts,1.0e2*__thread_tx.unknownAborts/total);
	#endif /* RTM_ABORT_DEBUG */
}

void _RTM_FORCE_INLINE _RTM_ABORT_STATS(){
	#ifdef RTM_ABORT_DEBUG
		__thread_tx.totalAborts++;
		if(__tx_status & _XABORT_EXPLICIT){
			__thread_tx.explicitAborts++;
		}
		else if(__tx_status & _XABORT_CONFLICT){
			__thread_tx.conflictAborts++;
		}
		else if(__tx_status & _XABORT_CAPACITY){
			__thread_tx.capacityAborts++;
		}
		else if(__tx_status & _XABORT_DEBUG){
			__thread_tx.debugAborts++;
		}
		else if(__tx_status & _XABORT_NESTED){
			__thread_tx.nestedAborts++;
		}
		else {
			__thread_tx.unknownAborts++;
		}
	#endif /* _RTM_ABORT_DEBUG */
}

