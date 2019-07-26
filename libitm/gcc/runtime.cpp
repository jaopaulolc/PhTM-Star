#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include "target.h"
#include "libitm.h"
#include "thread.h"
#include "api/api.hpp"
#include "stm/txthread.hpp"

#if defined(BACKEND_PHASEDTM)
#include <phTM.h>
#endif

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
  uint32_t codePathToRun = a_runInstrumentedCode;
	tx->aborted = false;
#if defined(BACKEND_NOREC)
	// RSTM handles nesting by flattening transactions
	stm::begin((stm::TxThread*)tx->stmTxDescriptor, &tx->jmpbuf, 0);
	CFENCE;
#elif defined(BACKEND_PHASEDTM)
  while(1) {
    uint64_t mode = getMode();
    if (mode == HW || mode == GLOCK) {
      bool modeChanged = HTM_Start_Tx();
      if (!modeChanged) {
        codePathToRun = a_runUninstrumentedCode;
        break;
      }
    } else { // mode == SW
      bool restarted = codeProperties != 0;
      bool modeChanged = STM_PreStart_Tx(restarted);
      if (!modeChanged) {
        stm::begin((stm::TxThread*)tx->stmTxDescriptor,
            &tx->jmpbuf, codeProperties);
        CFENCE;
        codePathToRun = a_runInstrumentedCode;
        break;
      }
    }
  }
#else
#error "unknown or no backend selected!"
#endif

	return codePathToRun;
}

void ITM_REGPARM
_ITM_commitTransaction () {

	threadDescriptor_t* tx = getThreadDescriptor();
#if defined(BACKEND_NOREC)
	stm::commit((stm::TxThread*)tx->stmTxDescriptor);
	CFENCE;
	tx = getThreadDescriptor();
	tx->undolog.commit();
#elif defined(BACKEND_PHASEDTM)
	uint64_t mode = getMode();
  if (mode == SW) {
    stm::commit((stm::TxThread*)tx->stmTxDescriptor);
    CFENCE;
    STM_PostCommit_Tx();
    tx = getThreadDescriptor();
    tx->undolog.commit();
  } else {
    // runmode == HW || GLOCK
    HTM_Commit_Tx();
  }
#else
#error "unknown or no backend selected!"
#endif
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
#if defined(BACKEND_NOREC)
	stm::restart();
#elif defined(BACKEND_PHASEDTM)
  uint64_t mode = getMode();
  if (mode == SW) {
    stm::restart();
  } else {
    htm_abort();
  }
#else
#error "unknown or no backend selected!"
#endif
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
