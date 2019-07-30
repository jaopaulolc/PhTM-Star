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
phTM_init();

void
phTM_term();

void
phTM_thread_init();

void
phTM_thread_exit(uint64_t stmCommits,uint64_t stmAborts);

#if defined(__cplusplus)
} /* extern "C" { */
#endif

#endif /* _PHTM_H */
