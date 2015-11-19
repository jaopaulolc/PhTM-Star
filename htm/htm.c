
#include <htm.h>
#include <locks.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <string.h>

static __thread long __tx_status;  // _xbegin() return status
static __thread long __tx_id;  // tx thread id
#ifndef HTM_MAX_RETRIES
#define HTM_MAX_RETRIES 16
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

#define MAX_THREADS 8
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define NUM_PROF_COUNTERS 11
#else /* Haswell */
#define NUM_PROF_COUNTERS 7
#endif /* Haswell */
static enum abort_tag {
	ABORT_EXPLICIT_IDX=0,
	ABORT_TX_CONFLICT_IDX,
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	ABORT_SUSPENDED_CONFLICT_IDX,
	ABORT_NON_TX_CONFLICT_IDX,
	ABORT_TLB_CONFLICT_IDX,
	ABORT_FETCH_CONFLICT_IDX,
#endif /* PowerTM */
	ABORT_CAPACITY_IDX,
	ABORT_ILLEGAL_IDX,
	ABORT_NESTED_IDX,
	ABORTED_IDX,
	COMMITED_IDX,

} aborts;
static uint64_t profCounters[MAX_THREADS][NUM_PROF_COUNTERS] __attribute__((aligned(0x1000)));

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
			profCounters[__tx_id][ABORTED_IDX]++;
			uint32_t abort_reason = htm_abort_reason(__tx_status);
			if(abort_reason & ABORT_EXPLICIT){
				profCounters[__tx_id][ABORT_EXPLICIT_IDX]++;
			}
			if(abort_reason & ABORT_TX_CONFLICT){
				profCounters[__tx_id][ABORT_TX_CONFLICT_IDX]++;
			}
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
			if(abort_reason & ABORT_SUSPENDED_CONFLICT){
				profCounters[__tx_id][ABORT_SUSPENDED_CONFLICT_IDX]++;
			}
			if(abort_reason & ABORT_NON_TX_CONFLICT){
				profCounters[__tx_id][ABORT_NON_TX_CONFLICT_IDX]++;
			}
			if(abort_reason & ABORT_TLB_CONFLICT){
				profCounters[__tx_id][ABORT_TLB_CONFLICT_IDX]++;
			}
			if(abort_reason & ABORT_FETCH_CONFLICT){
				profCounters[__tx_id][ABORT_FETCH_CONFLICT_IDX]++;
			}
#endif /* PowerTM */
			if(abort_reason & ABORT_CAPACITY){
				profCounters[__tx_id][ABORT_CAPACITY_IDX]++;
			}
			if(abort_reason & ABORT_ILLEGAL){
				profCounters[__tx_id][ABORT_ILLEGAL_IDX]++;
			}
			if(abort_reason & ABORT_NESTED){
				profCounters[__tx_id][ABORT_NESTED_IDX]++;
			}

			if (htm_abort_persistent(abort_reason)){
				__tx_retries = HTM_MAX_RETRIES;
			} else {
				__tx_retries++;
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
		profCounters[__tx_id][COMMITED_IDX]++;
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
		profCounters[__tx_id][COMMITED_IDX]++;
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
	memset(&profCounters,0, MAX_THREADS*NUM_PROF_COUNTERS*sizeof(uint64_t));
}

void HTM_SHUTDOWN(){
	
	uint64_t starts = 0;
	uint64_t commits = 0;
	uint64_t aborts = 0;
	uint64_t explicit = 0;
	uint64_t conflict = 0;
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	uint64_t suspended_conflict = 0;
	uint64_t nontx_conflict = 0;
	uint64_t tlb_conflict = 0;
	uint64_t fetch_conflict = 0;
#endif /* PowerTM */
	uint64_t capacity = 0;
	uint64_t illegal = 0;
	uint64_t nested = 0;

	int i;
	for (i=0; i < MAX_THREADS; i++){
		commits  += profCounters[i][COMMITED_IDX];
		aborts   += profCounters[i][ABORTED_IDX];
		explicit += profCounters[i][ABORT_EXPLICIT_IDX];
		conflict += profCounters[i][ABORT_TX_CONFLICT_IDX];
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
		suspended_conflict += profCounters[i][ABORT_SUSPENDED_CONFLICT_IDX];
		nontx_conflict     += profCounters[i][ABORT_NON_TX_CONFLICT_IDX];
		tlb_conflict       += profCounters[i][ABORT_TLB_CONFLICT_IDX];
		fetch_conflict     += profCounters[i][ABORT_FETCH_CONFLICT_IDX];
#endif /* PowerTM */
		capacity += profCounters[i][ABORT_CAPACITY_IDX];
		illegal  += profCounters[i][ABORT_ILLEGAL_IDX];
		nested   += profCounters[i][ABORT_NESTED_IDX];
	}
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	conflict += suspended_conflict + nontx_conflict 
	         + tlb_conflict + fetch_conflict;
#endif /* PowerTM */
	starts = commits + aborts;
	printf("#starts    : %12ld\n", starts);
	printf("#commits   : %12ld %6.2f\n", commits, 100.0*((float)commits/(float)starts));
	printf("#aborts    : %12ld %6.2f\n", aborts, 100.0*((float)aborts/(float)starts));
	printf("#explicit  : %12ld %6.2f\n", explicit, 100.0*((float)explicit/(float)aborts));
	printf("#illegal   : %12ld %6.2f\n", illegal, 100.0*((float)illegal/(float)aborts));
	printf("#nested    : %12ld %6.2f\n", nested, 100.0*((float)nested/(float)aborts));
	printf("#capacity  : %12ld %6.2f\n", capacity, 100.0*((float)capacity/(float)aborts));
	printf("#conflicts : %12ld %6.2f\n", conflict, 100.0*((float)conflict/(float)aborts));
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	printf("#suspended_conflicts : %12ld %6.2f\n", suspended_conflict, 100.0*((float)suspended_conflict/(float)conflict));
	printf("#nontx_conflicts     : %12ld %6.2f\n", nontx_conflict, 100.0*((float)nontx_conflict/(float)conflict));
	printf("#tlb_conflicts       : %12ld %6.2f\n", tlb_conflict, 100.0*((float)tlb_conflict/(float)conflict));
	printf("#fetch_conflicts     : %12ld %6.2f\n", fetch_conflict, 100.0*((float)fetch_conflict/(float)conflict));
#endif /* PowerTM */
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
