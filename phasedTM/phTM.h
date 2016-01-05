#ifndef _PHTM_H
#define _PHTM_H

#include <stdbool.h>

#include <htm.h>
#include <types.h>

#if defined(__cplusplus)
extern "C" {
#endif

modeIndicator_t
atomicReadModeIndicator();

void
changeMode(mode_t newMode);

bool
HTM_Start_Tx();

void
HTM_Commit_Tx();

bool
STM_PreStart_Tx(bool restarted);

void
STM_PostCommit_Tx();

void
phTM_init(long nThreads);

void
phTM_thread_init(long tid);

void
phTM_term(long nThreads, long nTxs, unsigned int **stmCommits, unsigned int **stmAborts);

#if defined(__cplusplus)
} /* extern "C" { */
#endif

#endif /* _PHTM_H */
