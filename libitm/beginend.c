//#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <libitm.h>
#include <htm.h>
#include <stm.h>

lock_t htm_glock = LOCK_INITIALIZER;
static __thread bool htm_glock_is_mine;

__thread jmp_buf __jmpbuf;

int
ITM_FASTCALL
_ITM_initializeProcess(void){
	htm_startup();
	stm_startup();
	return 0;
}

void
ITM_FASTCALL
_ITM_finalizeProcess(void){
	htm_shutdown();
	stm_shutdown();
}

int
ITM_FASTCALL
_ITM_initializeThread(void){
	stm::thread_init();
	tx = (stm::TxThread*)stm::Self;
	return 0;
}

void
ITM_FASTCALL
_ITM_finalizeThread(void){
}

uint32_t
ITM_FASTCALL
_ITM_beginTransaction(uint32_t prop, ...){
	
	if(prop & pr_multiwayCode){
		printf("The compiler has generated multiway code for this transaction!\n");
	}
/*	
	bool htm_started = htm_begin();
	if(htm_started){
		// HW transaction started successfully
		if(isLocked(&htm_glock))
			htm_abort();
		return a_runUninstrumentedCode;
	}
	// HW transaction failed too many times
	// take the lock and run in serial mode
	htm_glock_is_mine = true;
	lock(&htm_glock);
	return a_runUninstrumentedCode;
*/

	stm_begin(&__jmpbuf);
	
	return a_runInstrumentedCode;
	
}

void
ITM_FASTCALL
_ITM_commitTransaction(void){
	
/*
	if(htm_glock_is_mine){
		unlock(&htm_glock);
		htm_glock_is_mine = false;
	} htm_commit();
*/

	stm_commit();
}


bool
ITM_FASTCALL
_ITM_tryCommitTransaction(void){
	fprintf(stderr,"_ITM_tryCommitTransaction not implemented yet!\n");
	return false;
}



void
ITM_FASTCALL
ITM_NORETURN
_ITM_abortTransaction(_ITM_abortReason reason){
	stm_abort();
}
