#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <htm.h>
#include <aborts_profiling.h>
#include <types.h>

const modeIndicator_t NULL_INDICATOR = { .value = 0 } ;

#define HTM_MAX_RETRIES 16
static __thread long __tx_tid;
static __thread long htm_retries;
static __thread bool deferredTx = false;

modeIndicator_t modeIndicator	= { .mode = HW,
																	.deferredCount = 0,
										  						.undeferredCount = 0
																};

#include <utils.h>

#ifdef PHASE_PROFILING
#include <sys/time.h>
#define MAX_TRANS 1000000
static uint64_t start_time __ALIGN__ = 0;
static uint64_t trans_index __ALIGN__ = 1;
static uint64_t capacity_abort_transitions __ALIGN__ = 0;
static uint64_t hw_sw_transitions __ALIGN__ = 0;
static uint64_t sw_hw_transitions __ALIGN__ = 0;
static uint64_t trans_timestamp[MAX_TRANS] __ALIGN__;

static uint64_t getTime(){
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)(t.tv_sec*1.0e9) + (uint64_t)(t.tv_nsec);
}

#endif /* PHASE_PROFILING */

static
void
changeMode(mode_t newMode, uint32_t htm_abort_reason) {
	
	bool success;
	modeIndicator_t indicator;
	modeIndicator_t expected;
	modeIndicator_t new;

	if (newMode == SW) {
		do {
			indicator = atomicReadModeIndicator();
			expected = setMode(indicator, HW);
			new = incDeferredCount(expected);
			success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
		} while (!success && (indicator.mode == HW));
		if(success) deferredTx = true;
		do {
			indicator = atomicReadModeIndicator();
			expected = setMode(indicator, HW);
			new = setMode(indicator, SW);
			success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
		} while (!success && (indicator.mode != SW));
#ifdef PHASE_PROFILING
		if(success){
			trans_timestamp[trans_index++] = getTime() - start_time;
			hw_sw_transitions++;
			if(htm_abort_reason & ABORT_CAPACITY) capacity_abort_transitions++; 
		}
#endif /* PHASE_PROFILING */
	} else {
		do {
			indicator = atomicReadModeIndicator();
			expected = setMode(NULL_INDICATOR, SW);
			new = setMode(NULL_INDICATOR, HW);
			success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
		} while (!success && (indicator.mode != HW));
#ifdef PHASE_PROFILING
		if(success){
			trans_timestamp[trans_index++] = getTime() - start_time;
			sw_hw_transitions++;
		}
#endif /* PHASE_PROFILING */
	}
}

inline
bool
HTM_Start_Tx() {
	
	htm_retries = 0;

	while (true) {
		uint32_t status = htm_begin();
		if (htm_has_started(status)) {
			if (modeIndicator.mode == HW) {
				return false;
			} else {
				htm_end();
				return true;
			}
		}
		
		uint32_t abort_reason = htm_abort_reason(status);
		__inc_abort_counter(__tx_tid, abort_reason);

		modeIndicator_t indicator = atomicReadModeIndicator();
		if (indicator.mode != HW) {
			return true;
		}
		
		htm_retries++;
		if (htm_retries >= HTM_MAX_RETRIES || abort_reason & ABORT_CAPACITY) {
			changeMode(SW, abort_reason);
			return true;
		}
	}
}

inline
void
HTM_Commit_Tx() {
	
	htm_end();
	__inc_commit_counter(__tx_tid);
}


bool
STM_PreStart_Tx(bool restarted) {
	
	bool success;
	modeIndicator_t indicator;
	modeIndicator_t expected;
	modeIndicator_t new;
	
	if (!deferredTx) {
		do {
			indicator = atomicReadModeIndicator();
			if (indicator.deferredCount == 0 || indicator.mode == HW) {
				if(restarted) atomicDecUndeferredCount();
				changeMode(HW, 0);
				return true;
			}
			if(restarted) break;
			expected = setMode(indicator, SW);
			new = incUndeferredCount(expected);
			success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
		} while (!success && (indicator.mode == SW));
		if(indicator.mode == HW){
			if(restarted) atomicDecUndeferredCount();
			return true;
		}
	}
	return false;
}

void
STM_PostCommit_Tx() {
	
	bool success;
	modeIndicator_t expected;
	modeIndicator_t new;
	
	do {
		expected = atomicReadModeIndicator();
		if (deferredTx) {
			new = decDeferredCount(expected);
		} else{
			new = decUndeferredCount(expected);
		}
		success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
	} while (!success);
	if (deferredTx){
		deferredTx = false;
		if (new.deferredCount == 0) {
			changeMode(HW, 0);
		}
	}
}

void
phTM_init(long nThreads){
	__init_prof_counters(nThreads);
}

void
phTM_thread_init(long tid){
	__tx_tid = tid;
#ifdef PHASE_PROFILING
	start_time = getTime();
#endif /* PHASE_PROFILING */
}

void
phTM_term(long nThreads, long nTxs, unsigned int **stmCommits, unsigned int **stmAborts){
	__term_prof_counters(nThreads, nTxs, stmCommits, stmAborts);

#ifdef PHASE_PROFILING
	
	FILE *f = fopen("transitions.timestamp", "w");
	if(f == NULL){
		perror("fopen");
	}

	printf("hw->sw: %lu\n", hw_sw_transitions);
	printf("sw->hw: %lu\n", sw_hw_transitions);
	printf("capacity_abort_transations: %lu\n", capacity_abort_transitions);
	
	trans_timestamp[0] = 0;

	uint64_t i;
	for (i=1; i < trans_index; i++){
		uint64_t dx = trans_timestamp[i] - trans_timestamp[i-1];
		fprintf(f, "%lu %d\n", dx, i % 2 == 1);
	}
	fprintf(f, "\n\n");
	fclose(f);
#endif /* PHASE_PROFILING */
}
