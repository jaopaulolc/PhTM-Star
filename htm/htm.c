
#include <htm.h>
#include <locks.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

static __thread long __tx_status;  // _xbegin() return status
static __thread long __tx_id;  // tx thread id
#ifndef HTM_MAX_RETRIES
#define HTM_MAX_RETRIES 5
#endif
static __thread long __tx_retries; // current number of retries

#ifdef HTM_CM_AUXLOCK
static lock_t __aux_lock = LOCK_INITIALIZER;
static __thread long __aux_lock_owner = 0;
#endif /* HTM_CM_AUXLOCK */

#ifdef HTM_CM_DIEGUES
static lock_t __aux_lock = LOCK_INITIALIZER;
static __thread long __aux_lock_owner = 0;
static __thread unsigned short __thread_seed[3];
#endif /* HTM_CM_DIEGUES */

#ifdef HTM_CM_BACKOFF
#define MIN_BACKOFF (1UL << 2)
#define MAX_BACKOFF (1UL << 31)
static __thread unsigned long __thread_backoff; /* Maximum backoff duration */
static __thread unsigned long __thread_seed; /* Random generated seed */
#endif /* HTM_CM_BACKOFF */

static lock_t __htm_global_lock = LOCK_INITIALIZER;

void TX_START(){
	
#ifdef HTM_CM_DIEGUES
	__tx_retries = 4;
#else /* !HTM_CM_DIEGUES */
	__tx_retries = 0;
#endif /* !HTM_CM_DIEGUES */

	do{
		__tx_status = htm_begin();
		if ( htm_has_started(__tx_status) ) {
			if( isLocked(&__htm_global_lock) ){
				htm_abort();
			}
			return;
		}
		else{
			// execution flow continues here on transaction abort
		#ifdef HTM_CM_DIEGUES
			__tx_retries--;
			if (__tx_retries == 2 && __aux_lock_owner == 0) {
				lock(&__aux_lock);
				__aux_lock_owner = 1;
			} else if (__tx_retries <= 0) { 
				lock(&__htm_global_lock); 
				return;
			}
			if (__tx_retries > 2) {
				if (__tx_retries > 2 && nrand48(__thread_seed) % 100 < FALLBACK_PROB) __tx_retries = 3;
				else __tx_retries = 4;
			}
		#else /* !HTM_CM_DIEGUES */
			uint32_t abort_reason = htm_abort_reason(__tx_status);
			switch(abort_reason){
				default: // unknown reason (spurious?)
				case ABORT_EXPLICIT:
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
				case ABORT_SUSPENDED_CONFLICT:
				case ABORT_NON_TX_CONFLICT:
				case ABORT_TLB_CONFLICT:
				case ABORT_FETCH_CONFLICT:
				case ABORT_IMPL_SPECIFIC:
#endif /* PowerTM */
				case ABORT_TX_CONFLICT:
					__tx_retries++;
					break;
				case ABORT_CAPACITY: /* ++ */
				case ABORT_ILLEGAL:  /* ++ */
				case ABORT_NESTED:   /* ++ */
				//++ transaction will not commit on future attempts
					__tx_retries = HTM_MAX_RETRIES;
					break;
			}
		#ifdef HTM_CM_AUXLOCK
			if(__aux_lock_owner == 0 && htm_abort_reason(__tx_status) == ABORT_CONFLICT){
				lock(&__aux_lock);
				__aux_lock_owner = 1;
			}
		#endif /* HTM_CM_AUXLOCK */
		#ifdef HTM_CM_BACKOFF
			__thread_seed ^= (__thread_seed << 17);
			__thread_seed ^= (__thread_seed >> 13);
			__thread_seed ^= (__thread_seed << 5);
			unsigned long j, wait = __thread_seed % __thread_backoff;
			for (j = 0; j < wait; j++) { /* Do nothing */ }
			if (__thread_backoff < MAX_BACKOFF)
				__thread_backoff <<= 1;
		#endif /* HTM_CM_BACKOFF */
			/* Avoid Lemming effect by delaying tx retry till lock is free. */
			while( isLocked(&__htm_global_lock) ) pthread_yield();
			if(__tx_retries >= HTM_MAX_RETRIES){
				lock(&__htm_global_lock);
				return;
			}
		#endif /* !HTM_CM_DIEGUES */
		}
	} while(1);
}

void TX_END(){
	
#ifdef HTM_CM_DIEGUES
	if (__tx_retries > 0) {
		htm_end();
		if (__tx_retries <= 2) {
			unlock(&__aux_lock);
			__aux_lock_owner = 0;
		}
	} else {
		unlock(&__htm_global_lock);
		unlock(&__aux_lock);
		__aux_lock_owner = 0;
	}
#else /* !HTM_CM_DIEGUES */
	if(__tx_retries >= HTM_MAX_RETRIES){
		unlock(&__htm_global_lock);
	}
	else{
		htm_end();
	}
	#if HTM_CM_AUXLOCK
		if(__aux_lock_owner == 1){
			unlock(&__aux_lock);
			__aux_lock_owner = 0;
		}
	#endif /* HTM_CM_AUXLOCK */
#endif /* !HTM_CM_DIEGUES */
}


void HTM_STARTUP(long numThreads){
}

void HTM_SHUTDOWN(){
}

void TX_INIT(long id){

	__tx_id      = id;
	__tx_status  = 0;
	__tx_retries = 0;
	#ifdef HTM_CM_BACKOFF
		unsigned short seed[3];
		seed[0] = (unsigned short)rand()%USHRT_MAX;
		seed[1] = (unsigned short)rand()%USHRT_MAX;
		seed[2] = (unsigned short)rand()%USHRT_MAX;
		__thread_seed = nrand48(seed);
		__thread_backoff = MIN_BACKOFF;
	#endif /* HTM_CM_BACKOFF */
	#ifdef HTM_CM_DIEGUES
		__thread_seed[0] = (unsigned short)rand()%USHRT_MAX;
		__thread_seed[1] = (unsigned short)rand()%USHRT_MAX;
		__thread_seed[2] = (unsigned short)rand()%USHRT_MAX;
	#endif /* HTM_CM_DIEGUES */
}
