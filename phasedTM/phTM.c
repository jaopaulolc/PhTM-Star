#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

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
#endif /* DESIGN == OPTIMIZED */

#include <utils.h>

#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
#include <sys/time.h>
#define MAX_TRANS 2000000
static uint64_t start_time __ALIGN__ = 0;
static uint64_t end_time __ALIGN__ = 0;
static uint64_t trans_index __ALIGN__ = 1;
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
static uint64_t capacity_abort_transitions __ALIGN__ = 0;
static uint64_t numtransitions __ALIGN__ = 0;
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
static uint64_t trans_timestamp[MAX_TRANS] __ALIGN__;
static uint64_t hw_sw_wait_time  __ALIGN__ = 0;
static uint64_t sw_hw_wait_time  __ALIGN__ = 0;

static uint64_t getTime(){
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)(t.tv_sec*1.0e9) + (uint64_t)(t.tv_nsec);
}
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */

static
int
changeMode(uint64_t newMode) {
	
	bool success;
	modeIndicator_t indicator;
	modeIndicator_t expected;
	modeIndicator_t new;

#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
	uint64_t t = 0;
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */

	switch(newMode) {
		case SW:
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
			t = getTime();
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
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
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
				uint64_t now = getTime();
				hw_sw_wait_time += now - t;
				trans_timestamp[trans_index++] = now - start_time;
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
				numtransitions++;
			}
			break;
		case HW:
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
			t = getTime();
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
			do {
				indicator = atomicReadModeIndicator();
				expected = setMode(NULL_INDICATOR, SW);
				new = setMode(NULL_INDICATOR, HW);
				success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
			} while (!success && (indicator.mode != HW));
			if(success){
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
				uint64_t now = getTime();
				sw_hw_wait_time += now - t;
				trans_timestamp[trans_index++] = now - start_time;
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
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
}
#endif /* DESIGN == OPTIMIZED */

inline
bool
HTM_Start_Tx() {
	
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
				htm_end();
#if DESIGN == PROTOTYPE
				return true;
#else  /* DESIGN == OPTIMIZED */
				if ( isModeSW() ){
					return true;
				}
#endif /* DESIGN == OPTIMIZED */
			}
		}

#if DESIGN == OPTIMIZED
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
		} else isCapacityAbortPersistent = 0;
		if ( (abort_reason & ABORT_TX_CONFLICT) &&
				 (previous_abort_reason == abort_reason)) {
			isConflictAbortPersistent = 1;
		} else isConflictAbortPersistent = 0;
		if (htm_retries >= HTM_MAX_RETRIES || isCapacityAbortPersistent){
			if (isCapacityAbortPersistent || isConflictAbortPersistent){
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
}

void
phTM_thread_init(long tid){
	__tx_tid = tid;
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
	start_time = getTime();
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
}

void
phTM_thread_exit(void){
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
	end_time = getTime() - start_time;
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
}

void
phTM_term(long nThreads, long nTxs, unsigned int **stmCommits, unsigned int **stmAborts){
	__term_prof_counters(nThreads, nTxs, stmCommits, stmAborts);

	printf("ntrans: %lu %6.2lf\n", numtransitions, 100.0*((double)capacity_abort_transitions/(double)numtransitions));

#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
	
#ifdef PHASE_PROFILING
	FILE *f = fopen("transitions.timestamp", "w");
	if(f == NULL){
		perror("fopen");
	}
#endif /* PHASE_PROFILING*/
	
	trans_timestamp[0] = 0;

	uint64_t i, ttime = 0;
#ifdef TIME_MODE_PROFILING
	uint64_t hw_time = 0, sw_time = 0;
#endif /* TIME_MODE_PROFILING */
	for (i=1; i < trans_index; i++){
		uint64_t dx = trans_timestamp[i] - trans_timestamp[i-1];
#ifdef PHASE_PROFILING
		fprintf(f, "%lu %d\n", dx, i % 2 == 1);
#else /* TIME_MODE_PROFILING */
		if( i % 2 == 1 ){ hw_time += dx; }
		else { sw_time += dx; }
#endif /* TIME_MODE_PROFILING */
		ttime += dx;
	}
	if(ttime < end_time){
		uint64_t dx = end_time - trans_timestamp[i-1];
#ifdef PHASE_PROFILING
		fprintf(f, "%lu %d\n", dx, i % 2 == 1);
#else /* TIME_MODE_PROFILING */
		if( i % 2 == 1 ){ hw_time += dx; }
		else { sw_time += dx; }
#endif /* TIME_MODE_PROFILING */
		ttime += dx;
	}

#ifdef PHASE_PROFILING
	fprintf(f, "\n\n");
	fclose(f);
#endif /* PHASE_PROFILING */

#ifdef TIME_MODE_PROFILING
	printf("hw, sw: %6.2lf %6.2lf\n", 100.0*((double)hw_time/(double)ttime), 100.0*((double)sw_time/(double)ttime));
	printf("hw_sw_wtime: %lu (%6.2lf)\n", hw_sw_wait_time,100.0*((double)hw_sw_wait_time/ttime));
	printf("sw_hw_wtime: %lu (%6.2lf)\n", sw_hw_wait_time,100.0*((double)sw_hw_wait_time/ttime));
#endif /* PHASE_PROFILING */
	
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
}
