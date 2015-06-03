#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif
#include <rtm.h>
#include <locks.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

static __thread long __tx_status;  // _xbegin() return status
static __thread long __tx_id;  // tx thread id
#ifndef RTM_MAX_RETRIES
#define RTM_MAX_RETRIES 5
#endif
static __thread long __tx_retries; // current number of retries

#ifdef RTM_CM_AUXLOCK
static lock_t __aux_lock = LOCK_INITIALIZER;
static __thread long __aux_lock_owner = 0;
#endif /* RTM_CM_AUXLOCK */

#ifdef RTM_CM_DIEGUES
static lock_t __aux_lock = LOCK_INITIALIZER;
static __thread long __aux_lock_owner = 0;
static __thread unsigned short __thread_seed[3];
#endif /* RTM_CM_DIEGUES */

#ifdef RTM_CM_BACKOFF
#define MIN_BACKOFF (1UL << 2)
#define MAX_BACKOFF (1UL << 31)
static __thread unsigned long __thread_backoff; /* Maximum backoff duration */
static __thread unsigned long __thread_seed; /* Random generated seed */
#endif /* RTM_CM_BACKOFF */

static lock_t __rtm_global_lock = LOCK_INITIALIZER;

#define _XABORT_LOCKED 0xff

void TX_START(){
	
#ifdef RTM_CM_DIEGUES
	__tx_retries = 4;
#else /* !RTM_CM_DIEGUES */
	__tx_retries = 0;
#endif /* !RTM_CM_DIEGUES */

	do{
		if((__tx_status = _xbegin()) == _XBEGIN_STARTED){
			if( isLocked(&__rtm_global_lock) ){
				_xabort(_XABORT_LOCKED);
			}
			return;
		}
		else{
			// execution flow continues here on transaction abort
		#ifdef RTM_CM_DIEGUES
			__tx_retries--;
			if (__tx_retries == 2 && __aux_lock_owner == 0) {
				lock(&__aux_lock);
				__aux_lock_owner = 1;
			} else if (__tx_retries <= 0) { 
				lock(&__rtm_global_lock); 
				return;
			}
			if (__tx_retries > 2) {
				if (__tx_retries > 2 && nrand48(__thread_seed) % 100 < FALLBACK_PROB) __tx_retries = 3;
				else __tx_retries = 4;
			}
		#else /* !RTM_CM_DIEGUES */
			__tx_status = __tx_status & 0x3E;
			switch(__tx_status){
				default: // unknown reason (spurious?)
				case _XABORT_EXPLICIT:
				case _XABORT_CONFLICT:
					__tx_retries++;
					break;
				case _XABORT_CAPACITY: /* ++ */
				case _XABORT_DEBUG:    /* ++ */
				case _XABORT_NESTED:   /* ++ */
				//++ transaction will not commit on future attempts
					__tx_retries = RTM_MAX_RETRIES;
					break;
			}
		#ifdef RTM_CM_AUXLOCK
			if(__aux_lock_owner == 0 && __tx_status == _XABORT_CONFLICT){
				lock(&__aux_lock);
				__aux_lock_owner = 1;
			}
		#endif /* RTM_CM_AUXLOCK */
		#ifdef RTM_CM_BACKOFF
			__thread_seed ^= (__thread_seed << 17);
			__thread_seed ^= (__thread_seed >> 13);
			__thread_seed ^= (__thread_seed << 5);
			unsigned long j, wait = __thread_seed % __thread_backoff;
			for (j = 0; j < wait; j++) { /* Do nothing */ }
			if (__thread_backoff < MAX_BACKOFF)
				__thread_backoff <<= 1;
		#endif /* RTM_CM_BACKOFF */
			/* Avoid Lemming effect by delaying tx retry till lock is free. */
			while( isLocked(&__rtm_global_lock) ) pthread_yield();
			if(__tx_retries >= RTM_MAX_RETRIES){
				lock(&__rtm_global_lock);
				return;
			}
		#endif /* !RTM_CM_DIEGUES */
		}
	} while(1);
}

void TX_END(){
	
#ifdef RTM_CM_DIEGUES
	if (__tx_retries > 0) {
		_xend();
		if (__tx_retries <= 2) {
			unlock(&__aux_lock);
			__aux_lock_owner = 0;
		}
	} else {
		unlock(&__rtm_global_lock);
		unlock(&__aux_lock);
		__aux_lock_owner = 0;
	}
#else /* !RTM_CM_DIEGUES */
	if(__tx_retries >= RTM_MAX_RETRIES){
		unlock(&__rtm_global_lock);
	}
	else{
		_xend();
	}
	#if RTM_CM_AUXLOCK
		if(__aux_lock_owner == 1){
			unlock(&__aux_lock);
			__aux_lock_owner = 0;
		}
	#endif /* RTM_CM_AUXLOCK */
#endif /* !RTM_CM_DIEGUES */
}


void TSX_STARTUP(long numThreads){
}

void TSX_SHUTDOWN(){
}

void TX_INIT(long id){

	__tx_id      = id;
	__tx_status  = 0;
	__tx_retries = 0;
	#ifdef RTM_CM_BACKOFF
		unsigned short seed[3];
		seed[0] = (unsigned short)rand()%USHRT_MAX;
		seed[1] = (unsigned short)rand()%USHRT_MAX;
		seed[2] = (unsigned short)rand()%USHRT_MAX;
		__thread_seed = nrand48(seed);
		__thread_backoff = MIN_BACKOFF;
	#endif /* RTM_CM_BACKOFF */
	#ifdef RTM_CM_DIEGUES
		__thread_seed[0] = (unsigned short)rand()%USHRT_MAX;
		__thread_seed[1] = (unsigned short)rand()%USHRT_MAX;
		__thread_seed[2] = (unsigned short)rand()%USHRT_MAX;
	#endif /* RTM_CM_DIEGUES */
}
