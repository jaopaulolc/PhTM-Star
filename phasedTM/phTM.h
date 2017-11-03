#ifndef _PHTM_H
#define _PHTM_H

#include <stdbool.h>
#include <stdint.h>

#include <htm.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum { HW = 0, SW = 1, GLOCK = 2 };

uint64_t
getMode();

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
phTM_term(long nThreads, long nTxs, uint64_t **stmCommits, uint64_t **stmAborts);

void
phTM_thread_init(long tid);

void
phTM_thread_exit(void);

#if defined(__cplusplus)
} /* extern "C" { */
#endif

#endif /* _PHTM_H */
