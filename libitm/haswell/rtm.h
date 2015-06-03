#ifndef _RTM_H
#define _RTM_H

#include <immintrin.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <locks.h>

extern lock_t htm_glock;

static __thread long __tx_id;  // tx thread id
#ifndef RTM_MAX_RETRIES
#define RTM_MAX_RETRIES 5
#endif
static __thread long __tx_retries; // current number of retries

bool htm_begin(){
	
	__tx_retries = 0;
	
	uint32_t htm_status;

	while(1){
		if((htm_status = _xbegin()) == _XBEGIN_STARTED){
			return true;
		}
		else{
			// Fallback Path
			// This code only decides if transaction should be retried in HW. Progress
			// guarantee should be implemented by the caller when false is returned.
			htm_status = htm_status & 0x3E;
			switch(htm_status){
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
			/* Avoid Lemming effect by delaying tx retry till lock is free. */
			while( isLocked(&htm_glock) ) pthread_yield();
			if(__tx_retries >= RTM_MAX_RETRIES) break;
		}
	}
	return false;
}

void htm_commit(){
	_xend();
}


void htm_abort(){
	_xabort(_XABORT_EXPLICIT);
}

void htm_startup(){
}

void htm_shutdown(){
}

void htm_thread_init(long id){

	__tx_id      = id;
	__tx_retries = 0;
}

void htm_thread_destroy(){
}

#endif /* _RTM_H */
