#ifndef _RTM_INCLUDE
#define _RTM_INCLUDE

#include <sched.h>
#include <pthread.h>
/* Requires TSX support in the CPU */
#include <immintrin.h>

typedef struct _tsx_tx_t{
	long id; 								// thread id
	long totalAborts; 			// total number of aborts
	long explicitAborts;		// aborts due to _xabort() call
	long conflictAborts;		// aborts due to data conflict
	long capacityAborts;		// aborts due to L1 overflow
	long debugAborts;				// aborts due to debug trap
	long nestedAborts;			// aborts due to inner nested transaction abort
	long unknownAborts; 		// aborts whose cause is unknown
	long status;						// _xbegin() return status
	long retries;						// number of retries
}tsx_tx_t;

extern __thread tsx_tx_t __thread_tx;

#define tsx_getThreadId()				__thread_tx.id
#define tsx_getTotalAborts() 		__thread_tx.totalAborts
#define tsx_getExplicitAborts() __thread_tx.explicitAborts
#define tsx_getConflictAborts()	__thread_tx.conflictAborts
#define tsx_getCapacityAborts() __thread_tx.capacityAborts
#define tsx_getDebugAborts()		__thread_tx.debugAborts
#define tsx_getNestedAborts()		__thread_tx.nestedAborts
#define tsx_getUnknownAborts()	__thread_tx.unknownAborts

#define _RTM_FORCE_INLINE __attribute__((__always_inline__)) inline

void TX_START(); 
void TX_END(); 
void TX_INIT(long id); 
void TX_FINISH(); 

#endif /* _RTM_INCLUDE */
