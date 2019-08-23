#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include <htm.h>
#include <aborts_profiling.h>
#include <types.h>

#define UNUSED __attribute__((unused))

#define HTM_MAX_RETRIES 9

const modeIndicator_t NULL_INDICATOR = { .value = 0 };

static __thread bool deferredTx __ALIGN__ = false;
static __thread bool decUndefCounter __ALIGN__ = false;
volatile modeIndicator_t modeIndicator	__ALIGN__ = {
																	.mode = HW,
																	.deferredCount = 0,
                                  .undeferredCount = 0
																};
volatile char padding1[__CACHE_LINE_SIZE__ - sizeof(modeIndicator_t)] __ALIGN__;

__thread uint64_t htm_retries __ALIGN__;
__thread uint32_t abort_reason __ALIGN__ = 0;
#if DESIGN == OPTIMIZED
__thread uint32_t previous_abort_reason __ALIGN__;
__thread bool htm_global_lock_is_mine __ALIGN__ = false;
__thread bool isCapacityAbortPersistent __ALIGN__;
__thread uint32_t abort_rate __ALIGN__ = 0;
__thread uint64_t num_htm_runs __ALIGN__ = 0;
#define MAX_STM_RUNS 1000
#define MAX_GLOCK_RUNS 100
__thread uint64_t max_stm_runs __ALIGN__ = 100;
__thread uint64_t num_stm_runs __ALIGN__;
__thread uint64_t t0 __ALIGN__ = 0;
__thread uint64_t sum_cycles __ALIGN__ = 0;
#define TX_CYCLES_THRESHOLD (30000) // HTM-friendly apps in STAMP have tx with 20k cycles or less

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
static inline uint64_t getCycles()
{
		uint32_t upper, lower,tmp;
		__asm__ volatile(
			"0:                  \n"
			"\tmftbu   %0        \n"
			"\tmftb    %1        \n"
			"\tmftbu   %2        \n"
			"\tcmpw    %2,%0     \n"
			"\tbne     0b        \n"
   	 : "=r"(upper),"=r"(lower),"=r"(tmp)
	  );
		return  (((uint64_t)upper) << 32) | lower;
}
#elif defined(__x86_64__)
static inline uint64_t getCycles()
{
    uint32_t tmp[2];
    __asm__ ("rdtsc" : "=a" (tmp[1]), "=d" (tmp[0]) : "c" (0x10) );
    return (((uint64_t)tmp[0]) << 32) | tmp[1];
}
#else
#error "unsupported architecture!"
#endif

#endif /* DESIGN == OPTIMIZED */

#include <utils.h>
#include <phase_profiling.h>

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
#if DESIGN == OPTIMIZED
				t0 = getCycles();
#endif /* DESIGN == OPTIMIZED */
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

bool
HTM_Start_Tx() {

	phase_profiling_start();
	
	htm_retries = 0;
	abort_reason = 0;
#if DESIGN == OPTIMIZED
	isCapacityAbortPersistent = 0;
	t0 = getCycles();
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

		
		if ( isModeSW() ){
			return true;
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
		__inc_abort_counter(abort_reason);
		
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
		isCapacityAbortPersistent = (abort_reason & ABORT_CAPACITY)
		                 && (previous_abort_reason == abort_reason);

		if ( (abort_reason & ABORT_TX_CONFLICT) 
				&& (previous_abort_reason == abort_reason) )
			abort_rate = (abort_rate * 75 + 25*100) / 100;

		num_htm_runs++;
		uint64_t t1 = getCycles();
		uint64_t tx_cycles = t1 - t0;
		t0 = t1;
		sum_cycles += tx_cycles;
		uint64_t mean_cycles = 0;
		if (htm_retries >= 2) {
			mean_cycles = sum_cycles / num_htm_runs;
		}

		if ( (isCapacityAbortPersistent
					&& (mean_cycles > TX_CYCLES_THRESHOLD))
				||
				 (isCapacityAbortPersistent
					&& (abort_rate >= 80)) ) {
			num_stm_runs = 0;
			changeMode(SW);
			return true;
		} else if (htm_retries >= HTM_MAX_RETRIES) {
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
					t0 = getCycles();
					return false;
				} else {
					// I don't own the lock, so wait
					// till lock is release and restart
					while( isModeGLOCK() ) pthread_yield();
					continue;
				}
			}
		}
#endif /* DESIGN == OPTIMIZED */
	}
}

void
HTM_Commit_Tx() {

#if	DESIGN == PROTOTYPE
	htm_end();
	__inc_commit_counter();
#else  /* DESIGN == OPTIMIZED */
	if (htm_global_lock_is_mine){
		unlockMode();
		htm_global_lock_is_mine = false;
		uint64_t t1 = getCycles();
		uint64_t tx_cycles = t1 - t0;
		t0 = t1;
		sum_cycles += tx_cycles;
	} else {
		htm_end();
		__inc_commit_counter();
  }
  abort_rate = (abort_rate * 75) / 100;
#endif /* DESIGN == OPTIMIZED */

}


bool
STM_PreStart_Tx(bool restarted UNUSED) {
	
	modeIndicator_t indicator;
	modeIndicator_t expected;
	modeIndicator_t new;
	bool success;
	
	if (!deferredTx) {
		do {
			indicator = atomicReadModeIndicator();
				if (indicator.deferredCount == 0 || indicator.mode == HW) {
          if (decUndefCounter) atomicDecUndeferredCount();
          decUndefCounter = false;
					changeMode(HW);
					return true;
				}
      if(decUndefCounter) break;
			expected = setMode(indicator, SW);
			new = incUndeferredCount(expected);
			success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
		} while (!success);
    decUndefCounter = true;
	}
#if DESIGN == OPTIMIZED
	else {
		t0 = getCycles();
	}
#endif /* DESIGN == OPTIMIZED */
	return false;
}

void
STM_PostCommit_Tx() {
	
	bool success;
	modeIndicator_t expected;
	modeIndicator_t new;

#if DESIGN == OPTIMIZED
	if (deferredTx) {
		uint64_t t1 = getCycles();
		uint64_t tx_cycles = t1 - t0;
		t0 = t1;
		sum_cycles += tx_cycles;
		num_stm_runs++;
		if (num_stm_runs < max_stm_runs) {
			return;
		}
		uint64_t mean_cycles = sum_cycles / max_stm_runs;
		num_stm_runs = 0;
		sum_cycles = 0;
	
		if (mean_cycles > TX_CYCLES_THRESHOLD){
			if (max_stm_runs < MAX_STM_RUNS) max_stm_runs = 2*max_stm_runs;
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
phTM_init(){
	printf("DESIGN: %s\n", (DESIGN == PROTOTYPE) ? "PROTOTYPE" : "OPTIMIZED");
	phase_profiling_init();
}

void
phTM_thread_init(){
#if DESIGN == OPTIMIZED
  abort_rate = 0.0;
#endif
	__init_prof_counters();
}

void
phTM_thread_exit(uint64_t stmCommits, uint64_t stmAborts){
	phase_profiling_stop();
#if DESIGN == OPTIMIZED
	if (deferredTx) {
		deferredTx = false;
		atomicDecDeferredCount();
	}
#endif /* DESIGN == OPTIMIZED */
	__term_prof_counters(stmCommits, stmAborts);
}

void
phTM_term() {
	__report_prof_counters();
	phase_profiling_report();
}
