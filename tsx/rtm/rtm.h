#ifndef _RTM_INCLUDE
#define _RTM_INCLUDE

#include <pthread.h>
/* Requires TSX support in the CPU */
#include <immintrin.h>

void TX_START(); 
void TX_END(); 
void TX_INIT(long id); 
void TSX_STARTUP(long numThreads); 
void TSX_SHUTDOWN(); 

#endif /* _RTM_INCLUDE */
