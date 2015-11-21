
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

static __thread long __tx_status;  // htm_begin() return status
static __thread long __tx_id;  // tx thread id
#ifndef HTM_MAX_RETRIES
#define HTM_MAX_RETRIES 16
#endif
static __thread long __tx_retries; // current number of retries for non-lock aborts
static __thread long __tx_lock_retries; // current number of retries for lock aborts

static lock_t __htm_global_lock = LOCK_INITIALIZER;
static long __nThreads;

void TX_START(){
	
	__tx_retries = 0;
	__tx_lock_retries = 0;

	do{
		int abort_lock = 0;
		__tx_status = htm_begin();
		if ( htm_has_started(__tx_status) ) {
			if( isLocked(&__htm_global_lock) ){
				htm_end();
				abort_lock = 1;
			}
			else return;
		}
		// execution flow continues here on transaction abort
		/* Avoid Lemming effect by delaying tx retry till lock is free. */
		while( isLocked(&__htm_global_lock) ) pthread_yield();

		uint32_t abort_reason = htm_abort_reason(__tx_status);
		__inc_abort_counter(__tx_id, abort_reason);

		if(abort_lock){
			__tx_lock_retries++;
		} else {
			if (htm_abort_persistent(abort_reason)){
				__tx_retries = HTM_MAX_RETRIES;
			} else {
				__tx_retries++;
				if(abort_reason & ABORT_CAPACITY) __tx_retries++;
			}
		}
	
		if(__tx_retries >= HTM_MAX_RETRIES || __tx_lock_retries >= HTM_MAX_RETRIES){
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
	__tx_lock_retries = 0;
}
