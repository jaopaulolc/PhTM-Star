#ifndef _RTM_INCLUDE
#define _RTM_INCLUDE

#include <sched.h>
#include <pthread.h>
/* Requires TSX support in the CPU */
#include <immintrin.h>

void TX_START(); 
void TX_END(); 
void TX_INIT(long id); 
void TSX_STARTUP(long numThreads); 
void TSX_SHUTDOWN(); 


/* STATS COUNTER'S IDS */
enum{
	_RTM_TX_STARTED = 0, // number of rtm transactions started
	_RTM_TX_COMMITED ,   // number of rtm transactions commited
	_LOCK_TX_STARTED ,   // number of lock/HLE critical regions started
};

#ifndef MAX_NUM_THREADS
	#define MAX_NUM_THREADS 4
#endif

#ifndef MAX_NUM_COUNTERS
	#define MAX_NUM_COUNTERS 3
#endif

#endif /* _RTM_INCLUDE */
