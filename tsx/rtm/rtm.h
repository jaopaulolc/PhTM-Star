#ifndef _RTM_INCLUDE
#define _RTM_INCLUDE

#include <sched.h>
#include <pthread.h>
/* Requires TSX support in the CPU */
#include <immintrin.h>

typedef struct _tsx_tx_t{
	long id; 								// thread id
	long totalAborts; 			// total number of aborts
#ifdef RTM_ABORT_DEBUG
	long explicitAborts;		// aborts due to _xabort() call
	long conflictAborts;		// aborts due to data conflict
	long capacityAborts;		// aborts due to L1 overflow
	long debugAborts;				// aborts due to debug trap
	long nestedAborts;			// aborts due to inner nested transaction abort
	long unknownAborts; 		// aborts whose cause is unknown
#endif  /* RTM_ABORT_DEBUG */
}tsx_tx_t;

#define _RTM_FORCE_INLINE __attribute__((__always_inline__)) inline

void _RTM_FORCE_INLINE TX_START(); 
void _RTM_FORCE_INLINE TX_END(); 
void _RTM_FORCE_INLINE TX_INIT(long id); 
void _RTM_FORCE_INLINE TSX_START(long nthreads); 
void _RTM_FORCE_INLINE TSX_FINISH(); 

#endif /* _RTM_INCLUDE */
