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

void
changeMode(mode_t newMode) {
	
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
	} else {
		do {
			indicator = atomicReadModeIndicator();
			expected = setMode(NULL_INDICATOR, SW);
			new = setMode(NULL_INDICATOR, HW);
			success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
		} while (!success && (indicator.mode != HW));
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
			changeMode(SW);
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
				changeMode(HW);
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
			changeMode(HW);
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
}

void
phTM_term(long nThreads){
	__term_prof_counters(nThreads);
}
