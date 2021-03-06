#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include "target.h"
#include "libitm.h"
#include "thread.h"

extern "C" {
uint32_t ITM_REGPARM
GTM_beginTransaction (uint32_t codeProperties, jmpbuf_t* jmpbuf);
void ITM_REGPARM GTM_rollback();
}

uint32_t ITM_REGPARM
GTM_beginTransaction (uint32_t codeProperties, jmpbuf_t* jmpbuf) {

	threadDescriptor_t* tx = getThreadDescriptor();
	if ( unlikely (tx == NULL) ) {
		tx = new threadDescriptor_t();
		initThreadDescriptor(tx);
		setThreadDescriptor(tx);
	}
	tx->codeProperties = codeProperties;
	tx->jmpbuf = *jmpbuf;
	tx->aborted = false;

	return a_runInstrumentedCode;
}

void ITM_REGPARM
_ITM_commitTransaction () {
	
	threadDescriptor_t* tx = getThreadDescriptor();
	//tx = getThreadDescriptor();
	tx->undolog.commit();
	tx->aborted = false;
  tx->heapLocalPointers.clear();
}

bool ITM_REGPARM
_ITM_tryCommitTransaction () {
	fprintf (stderr, "error: _ITM_tryCommitTransaction not implemented yet!\n");
	exit (EXIT_FAILURE);
	return false;
}


void _ITM_NORETURN ITM_REGPARM
_ITM_abortTransaction (_ITM_abortReason abortReason UNUSED) {
	fprintf (stderr, "error: _ITM_abortTransaction not implemented yet!\n");
	exit (EXIT_FAILURE);
}

void ITM_REGPARM
GTM_rollback () {
#if defined(DEBUG)
	fprintf (stderr, "debug: GTM_rollback called!\n");
#endif
	threadDescriptor_t* tx = getThreadDescriptor();
	tx->aborted = true;
	tx->undolog.rollback();
}
