#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include "libitm.h"
#include "thread.h"
#include "api/api.hpp"
#include "stm/txthread.hpp"

#if defined(BACKEND_PHASEDTM)
#include <phTM.h>
#endif

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
  uint32_t codePathToRun = a_runInstrumentedCode;
#if defined(BACKEND_NOREC)
	// RSTM handles nesting by flattening transactions
	stm::begin((stm::TxThread*)tx->stmTxDescriptor, tx->jmpbuf, codeProperties);
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
            tx->jmpbuf, codeProperties);
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
