#ifndef _RTM_INCLUDE
#define _RTM_INCLUDE

#include <sched.h>
#include <pthread.h>
/* Requires TSX support in the CPU */
#include <immintrin.h>

void TX_START(); 
void TX_END(); 
void TX_INIT(long id); 
void TSX_START(long nthreads); 
void TSX_FINISH(); 

#endif /* _RTM_INCLUDE */
