
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include <htm.h>
#include <aborts_profiling.h>
#include <locks.h>

static __thread long __tx_status __ALIGN__;  // htm_begin() return status
static __thread long __tx_id __ALIGN__;  // tx thread id
#ifndef HTM_MAX_RETRIES
#define HTM_MAX_RETRIES 16
#endif
static __thread long __tx_retries __ALIGN__; // current number of retries for non-lock aborts

static lock_t __htm_global_lock __ALIGN__ = LOCK_INITIALIZER;
static long __nThreads __ALIGN__;

void TX_START(){
	
	__tx_retries = 0;

	do{
		__tx_status = htm_begin();
		if ( htm_has_started(__tx_status) ) {
			if( isLocked(&__htm_global_lock) ){
				htm_abort();
			}
			else return;
		}
		// execution flow continues here on transaction abort
		/* Avoid Lemming effect by delaying tx retry till lock is free. */
		while( isLocked(&__htm_global_lock) ) pthread_yield();

		uint32_t abort_reason __ALIGN__ = htm_abort_reason(__tx_status);
		__inc_abort_counter(__tx_id, abort_reason);

		if (htm_abort_persistent(abort_reason)){
			__tx_retries = HTM_MAX_RETRIES;
		} else {
			__tx_retries++;
		}
	
		if(__tx_retries >= HTM_MAX_RETRIES){
			__tx_retries = HTM_MAX_RETRIES;
			lock(&__htm_global_lock);
			return;
		}
	} while(1);
}

void TX_END(){
	
	if(__tx_retries >= HTM_MAX_RETRIES){
		unlock(&__htm_global_lock);
	}
	else{
		htm_end();
		__inc_commit_counter(__tx_id);
	}
}


void HTM_STARTUP(long numThreads){

	__nThreads = numThreads;
	__init_prof_counters(__nThreads);
}

void HTM_SHUTDOWN(){

	__term_prof_counters(__nThreads);
}

void TX_INIT(long id){

	__tx_id      = id;
	__tx_status  = 0;
	__tx_retries = 0;
}
