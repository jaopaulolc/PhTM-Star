#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include "libitm.h"
#include "thread.h"
#include "api/api.hpp"
#include "stm/txthread.hpp"

extern "C" {
//void __begin_tm_slow_path(void);
//void __end_tm_slow_path(void);

//void __begin_tm_fast_path(void);
//void __end_tm_fast_path(void);
}

uint32_t ITM_REGPARM
_ITM_beginTransaction (jmp_buf* jmpbuf, int codeProperties) {

	threadDescriptor_t* tx = getThreadDescriptor();
	if ( unlikely (tx == NULL) ) {
		tx = new threadDescriptor_t();
		initThreadDescriptor(tx);
		setThreadDescriptor(tx);
	}
	tx->codeProperties = codeProperties;
	tx->jmpbuf = jmpbuf;
	tx->aborted = false;
	// RSTM handles nesting by flattening transactions
	stm::begin((stm::TxThread*)tx->stmTxDescriptor, tx->jmpbuf, 0);
	CFENCE;

	return a_runInstrumentedCode;
}

#if 0
void __begin_tm_slow_path(void) {
  //fprintf(stderr, "debug: __begin_tm_slow_path called!\n");
}

void __end_tm_slow_path(void) {
  //fprintf(stderr, "debug: __end_tm_slow_path called!\n");
}

void __begin_tm_fast_path(void) {
  //fprintf(stderr, "debug: __begin_tm_fast_path called!\n");
}

void __end_tm_fast_path(void) {
  //fprintf(stderr, "debug: __end_tm_fast_path called!\n");
}
#endif

void ITM_REGPARM
_ITM_commitTransaction () {

	threadDescriptor_t* tx = getThreadDescriptor();
	stm::commit((stm::TxThread*)tx->stmTxDescriptor);
	CFENCE;
	tx = getThreadDescriptor();
	tx->undolog.commit();
	tx->aborted = false;
}

bool ITM_REGPARM
_ITM_tryCommitTransaction () {
	fprintf (stderr, "error: _ITM_tryCommitTransaction not implemented yet!\n");
	exit (EXIT_FAILURE);
	return false;
}


void _ITM_NORETURN ITM_REGPARM
_ITM_abortTransaction (_ITM_abortReason abortReason UNUSED) {
	stm::restart();
}
