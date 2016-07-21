#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include <htm.h>
#include <aborts_profiling.h>
#include <types.h>

#define HTM_MAX_RETRIES 9

const modeIndicator_t NULL_INDICATOR = { .value = 0 };

static __thread bool deferredTx __ALIGN__ = false;
volatile modeIndicator_t modeIndicator	__ALIGN__ = {
																	.mode = HW,
																	.deferredCount = 0,
										  						.undeferredCount = 0
																};
volatile char padding1[__CACHE_LINE_SIZE__ - sizeof(modeIndicator_t)] __ALIGN__;

static __thread long __tx_tid __ALIGN__;
static __thread long htm_retries __ALIGN__;
#if DESIGN == OPTIMIZED
static __thread bool htm_global_lock_is_mine __ALIGN__ = false;
static __thread int isCapacityAbortPersistent __ALIGN__;
static __thread int isConflictAbortPersistent __ALIGN__;
#define MAX_STM_RUNS 5
static __thread long max_stm_runs __ALIGN__ = 1;
static __thread long num_stm_runs __ALIGN__;
#endif /* DESIGN == OPTIMIZED */

#include <utils.h>
#include <phase_profiling.h>

static
int
changeMode(uint64_t newMode) {
	
	bool success;
	modeIndicator_t indicator;
	modeIndicator_t expected;
	modeIndicator_t new;

	switch(newMode) {
		case SW:
			setProfilingReferenceTime();
			do {
				indicator = atomicReadModeIndicator();
				expected = setMode(indicator, HW);
				new = incDeferredCount(expected);
				success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
			} while (!success && (indicator.mode != SW));
			if (success) deferredTx = true;
			do {
				indicator = atomicReadModeIndicator();
				expected = setMode(indicator, HW);
				new = setMode(indicator, SW);
				success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
			} while (!success && (indicator.mode != SW));
			if(success){
				updateTransitionProfilingData(SW);
			}
			break;
		case HW:
			setProfilingReferenceTime();
			do {
				indicator = atomicReadModeIndicator();
				expected = setMode(NULL_INDICATOR, SW);
				new = setMode(NULL_INDICATOR, HW);
				success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
			} while (!success && (indicator.mode != HW));
			if(success){
				updateTransitionProfilingData(HW);
			}
			break;
#if DESIGN == OPTIMIZED
		case GLOCK:
			do {
				indicator = atomicReadModeIndicator();
				if ( indicator.mode == SW ) return 0;
				if ( indicator.mode == GLOCK) return -1;
				expected = setMode(NULL_INDICATOR, HW);
				new = setMode(NULL_INDICATOR, GLOCK);
				success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
			} while (!success);
			updateTransitionProfilingData(GLOCK);
			break;
#endif /* DESIGN == OPTIMIZED */
		default:
			fprintf(stderr,"error: unknown mode %lu\n", newMode);
			exit(EXIT_FAILURE);
	}
	return 1;
}

#if DESIGN == OPTIMIZED
static inline
void
unlockMode(){
	bool success;
	do {
		modeIndicator_t expected = setMode(NULL_INDICATOR, GLOCK);
		modeIndicator_t new = setMode(NULL_INDICATOR, HW);
		success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
	} while (!success);
	updateTransitionProfilingData(HW);
}
#endif /* DESIGN == OPTIMIZED */

inline
bool
HTM_Start_Tx() {

	phase_profiling_start();
	
	htm_retries = 0;
	uint32_t abort_reason = 0;
#if DESIGN == OPTIMIZED
	uint32_t previous_abort_reason;
	isCapacityAbortPersistent = 0;
	isConflictAbortPersistent = 0;
#endif /* DESIGN == OPTIMIZED */

	while (true) {
		uint32_t status = htm_begin();
		if (htm_has_started(status)) {
			if (modeIndicator.value == 0) {
				return false;
			} else {
				htm_abort();
			}
		}
		
#if DESIGN == OPTIMIZED
		if ( isModeSW() ){
			return true;
		}

		if ( isModeGLOCK() ){
			// locked acquired
			// wait till lock release and restart
			while( isModeGLOCK() ) pthread_yield();
			continue;
		}
#endif /* DESIGN == OPTIMIZED */

#if DESIGN == OPTIMIZED
		previous_abort_reason = abort_reason; 
#endif /* DESIGN == OPTIMIZED */
		abort_reason = htm_abort_reason(status);
		__inc_abort_counter(__tx_tid, abort_reason);
		
		modeIndicator_t indicator = atomicReadModeIndicator();
		if (indicator.value != 0) {
			return true;
		}

#if DESIGN == PROTOTYPE
		htm_retries++;
		if ( htm_retries >= HTM_MAX_RETRIES ) {
			changeMode(SW);
			return true;
		}
#else  /* DESIGN == OPTIMIZED */
		htm_retries++;
		if ( (abort_reason & ABORT_CAPACITY) &&
				 (previous_abort_reason == abort_reason)) {
			isCapacityAbortPersistent = 1;
		}
		if ( (abort_reason & ABORT_TX_CONFLICT) &&
				 (previous_abort_reason == abort_reason)) {
			isConflictAbortPersistent = 1;
		} else isConflictAbortPersistent = 0;
		if (htm_retries >= HTM_MAX_RETRIES || isCapacityAbortPersistent){
			if (isCapacityAbortPersistent || isConflictAbortPersistent){
				if (max_stm_runs < MAX_STM_RUNS) max_stm_runs = 2*max_stm_runs;
				num_stm_runs = 0;
				changeMode(SW);
				return true;
			} else {
				int status = changeMode(GLOCK);
				if(status == 0){
					// Mode already changed to SW
					return true;
				} else {
					// Success! We are in LOCK mode
					if ( status == 1 ){
						htm_retries = 0;
						// I own the lock, so return and
						// execute in mutual exclusion
						htm_global_lock_is_mine = true;
						return false;
					} else {
						// I don't own the lock, so wait
						// till lock is release and restart
						while( isModeGLOCK() ) pthread_yield();
						continue;
					}
				}
			}
		}
#endif /* DESIGN == OPTIMIZED */
	}
}

inline
void
HTM_Commit_Tx() {

#if	DESIGN == PROTOTYPE
	htm_end();
	__inc_commit_counter(__tx_tid);
#else  /* DESIGN == OPTIMIZED */
	if (htm_global_lock_is_mine){
		unlockMode();
		htm_global_lock_is_mine = false;
	} else {
		htm_end();
		__inc_commit_counter(__tx_tid);
	}
#endif /* DESIGN == OPTIMIZED */

}


bool
STM_PreStart_Tx(bool restarted) {
	
	modeIndicator_t indicator;
	modeIndicator_t expected;
	modeIndicator_t new;
	bool success;
	
	if (!deferredTx) {
		do {
			indicator = atomicReadModeIndicator();
				if (indicator.deferredCount == 0 || indicator.mode == HW) {
					if(restarted) atomicDecUndeferredCount();
					changeMode(HW);
					return true;
				}
			if(restarted) break;
			expected = setMode(indicator, SW);
			new = incUndeferredCount(expected);
			success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
		} while (!success);
	}
	return false;
}

void
STM_PostCommit_Tx() {
	
	bool success;
	modeIndicator_t expected;
	modeIndicator_t new;

#if DESIGN == OPTIMIZED
	if (deferredTx) {
		num_stm_runs++;
		if (num_stm_runs < max_stm_runs) {
			return;
		}
	}
#endif /* DESIGN == OPTIMIZED */
	
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
			changeMode(HW);
		}
	}
}

void
phTM_init(long nThreads){
	printf("DESIGN: %s\n", (DESIGN == PROTOTYPE) ? "PROTOTYPE" : "OPTIMIZED");
	__init_prof_counters(nThreads);
	phase_profiling_init();
}

void
phTM_thread_init(long tid){
	__tx_tid = tid;
}

void
phTM_thread_exit(void){
	phase_profiling_stop();
}

void
phTM_term(long nThreads, long nTxs, unsigned int **stmCommits, unsigned int **stmAborts){
	__term_prof_counters(nThreads, nTxs, stmCommits, stmAborts);
	phase_profiling_report();
}
