#define _GNU_SOURCE
#include <rtm.h>
#include <lightlock.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

__thread tsx_tx_t __thread_tx;

light_lock_t __rtm_global_lock = LIGHT_LOCK_INITIALIZER;

#define _XABORT_LOCKED 0xff
#define RTM_MAX_RETRIES 100

void _RTM_FORCE_INLINE _RTM_ABORT_STATS();

void _RTM_FORCE_INLINE TX_START(){

	do{
		if((__thread_tx.status = _xbegin()) == _XBEGIN_STARTED){
			if(__rtm_global_lock == 1){
				_xabort(_XABORT_LOCKED);
			}
			return;
		}
		else{
			// execution flow continues here on transaction abort
			_RTM_ABORT_STATS();
			if((__thread_tx.status & _XABORT_RETRY)){
				__thread_tx.retries++;
				if(__thread_tx.retries >= RTM_MAX_RETRIES){
					light_lock(&__rtm_global_lock);
					return;
				}
			}
			else {
				__thread_tx.retries = RTM_MAX_RETRIES;
				light_lock(&__rtm_global_lock);
				return;
			}
		}
	} while(1);
}

void _RTM_FORCE_INLINE TX_END(){
	
	if(__thread_tx.retries >= RTM_MAX_RETRIES){
		light_unlock(&__rtm_global_lock);
		__thread_tx.retries = 0;
	}
	else _xend();
}

void TX_INIT(long id){
	__thread_tx.id              = id;
	#ifdef RTM_ABORT_DEBUG
	__thread_tx.totalAborts     = 0;
	__thread_tx.explicitAborts  = 0;
	__thread_tx.conflictAborts  = 0;
	__thread_tx.capacityAborts  = 0;
	__thread_tx.debugAborts     = 0;
	__thread_tx.nestedAborts    = 0;
	__thread_tx.unknownAborts   = 0;
	__thread_tx.retries         = 0;
	#endif /* _RTM_ABORT_DEBUG */
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
	#endif /* _RTM_ABORT_DEBUG */
}

void _RTM_FORCE_INLINE _RTM_ABORT_STATS(){
	#ifdef RTM_ABORT_DEBUG
		__thread_tx.totalAborts++;
		if(__thread_tx.status & _XABORT_EXPLICIT){
			__thread_tx.explicitAborts++;
		}
		else if(__thread_tx.status & _XABORT_CONFLICT){
			__thread_tx.conflictAborts++;
		}
		else if(__thread_tx.status & _XABORT_CAPACITY){
			__thread_tx.capacityAborts++;
		}
		else if(__thread_tx.status & _XABORT_DEBUG){
			__thread_tx.debugAborts++;
		}
		else if(__thread_tx.status & _XABORT_NESTED){
			__thread_tx.nestedAborts++;
		}
		else {
			__thread_tx.unknownAborts++;
		}
	#endif /* _RTM_ABORT_DEBUG */
}

