
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


#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
#include <sys/time.h>
#define MAX_TRANS 1000000
static uint64_t start_time __ALIGN__ = 0;
static uint64_t trans_index __ALIGN__ = 1;
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
static uint64_t hw_lock_transitions __ALIGN__ = 0;
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
static uint64_t trans_timestamp[MAX_TRANS] __ALIGN__;

static uint64_t getTime(){
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)(t.tv_sec*1.0e9) + (uint64_t)(t.tv_nsec);
}

#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */


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
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
			trans_timestamp[trans_index++] = getTime() - start_time;
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
			hw_lock_transitions++;
			return;
		}
	} while(1);
}

void TX_END(){
	
	if(__tx_retries >= HTM_MAX_RETRIES){
		unlock(&__htm_global_lock);
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
		trans_timestamp[trans_index++] = getTime() - start_time;
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
	}
	else{
		htm_end();
		__inc_commit_counter(__tx_id);
	}
}


void HTM_STARTUP(long numThreads){

	__nThreads = numThreads;
	__init_prof_counters(__nThreads);
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
	start_time = getTime();
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
}

void HTM_SHUTDOWN(){

	__term_prof_counters(__nThreads);

	printf("hw->lock: %lu\n", hw_lock_transitions);
#ifdef PHASE_PROFILING
	
	FILE *f = fopen("transitions.timestamp", "w");
	if(f == NULL){
		perror("fopen");
	}

	trans_timestamp[0] = 0;

	uint64_t i, ttime = 0;
	for (i=1; i < trans_index; i++){
		uint64_t dx = trans_timestamp[i] - trans_timestamp[i-1];
		fprintf(f, "%lu %d\n", dx, i % 2 == 1);
		ttime += dx;
	}
	if(ttime < (uint64_t)1.0e9){
		uint64_t dx = ((uint64_t)1.0e9) - trans_timestamp[i-1];
		fprintf(f, "%lu %d\n", dx, i % 2 == 1);
	}
	fprintf(f, "\n\n");
	fclose(f);
/* PHASE_PROFILING */
#elif defined(TIME_MODE_PROFILING)
	trans_timestamp[0] = 0;

	uint64_t i, hw_time = 0, lock_time = 0, ttime = 0;
	for (i=1; i < trans_index; i++){
		uint64_t dx = trans_timestamp[i] - trans_timestamp[i-1];
		if( i % 2 == 1 ){ hw_time += dx; }
		else { lock_time += dx; }
		ttime += dx;
	}

	if(ttime < (uint64_t)1.0e9){
		uint64_t dx = ((uint64_t)1.0e9) - trans_timestamp[i-1];
		if( i % 2 == 1 ){ hw_time += dx; }
		else { lock_time += dx; }
		ttime += dx;
	}

	printf("hw, lock: %6.2lf %6.2lf\n", 100.0*((double)hw_time/(double)ttime), 100.0*((double)lock_time/(double)ttime));
	
#endif /* TIME_MODE_PROFILING */
}

void TX_INIT(long id){

	__tx_id      = id;
	__tx_status  = 0;
	__tx_retries = 0;
}
