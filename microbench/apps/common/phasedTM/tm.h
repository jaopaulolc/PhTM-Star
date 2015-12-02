/*
 * File:
 *   tm.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Empty file (to avoid source modifications red-black tree).
 *
 * Copyright (c) 2007-2012.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program has a dual license and can also be distributed
 * under the terms of the MIT license.
 */

#include <stm.h>
#include <mod_mem.h>
#include <mod_ab.h>
#include <mod_stats.h>

#include <phTM.h>

#define RO	1
#define RW	0

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
#define STM_START(tid, ro, restarted)       { stm_tx_attr_t _a = {{.id = tid, .read_only = ro}}; \
                                              sigjmp_buf *_e = stm_start(_a,0);           \
																							int status = 0;                             \
                                              if (_e != NULL) status = sigsetjmp(*_e, 0); \
																							(*restarted) = status != 0;                 \
																						}


#define STM_COMMIT                          stm_commit()

#define TM_LOAD(addr)                      stm_load((stm_word_t *)addr)
#define TM_STORE(addr, value)              stm_store((stm_word_t *)addr, (stm_word_t)value)

#define TM_MALLOC(size)                    stm_malloc(size)
#define TM_FREE(addr)                      stm_free(addr, sizeof(*addr))
#define TM_FREE2(addr, size)               stm_free(addr, size)

static unsigned int **coreSTM_commits;
static unsigned int **coreSTM_aborts;

#define TM_INIT(nThreads)                  stm_init(); mod_mem_init(0); mod_ab_init(0, NULL);   \
                                           phTM_init(nThreads); \
										coreSTM_commits = (unsigned int **)malloc(sizeof(unsigned int *)*nThreads); \
								    coreSTM_aborts  = (unsigned int **)malloc(sizeof(unsigned int *)*nThreads)

#define TM_EXIT(nThreads)   phTM_term(nThreads, NUMBER_OF_TRANSACTIONS, coreSTM_commits, coreSTM_aborts); \
													  stm_exit()              

#define TM_INIT_THREAD(tid)                set_affinity(tid);                       \
																					 stm_init_thread(NUMBER_OF_TRANSACTIONS); \
																					 phTM_thread_init(tid)
#define TM_EXIT_THREAD(tid)                \
										if(stm_get_stats("nb_commits",&coreSTM_commits[tid]) == 0){                 \
											fprintf(stderr,"error: get nb_commits failed!\n");                        \
										}                                                                           \
										if(stm_get_stats("nb_aborts",&coreSTM_aborts[tid]) == 0){                   \
											fprintf(stderr,"error: get nb_aborts failed!\n");                         \
										}                                                                           \
																					stm_exit_thread()

#define IF_HTM_MODE							while(1){ \
																	modeIndicator_t indicator = atomicReadModeIndicator(); \
																	if (indicator.mode == HW){
#define START_HTM_MODE 							bool modeChanged = HTM_Start_Tx(); \
																		if (!modeChanged) {
#define COMMIT_HTM_MODE								HTM_Commit_Tx(); \
																			break; \
																		}
#define ELSE_STM_MODE							} else {
#define START_STM_MODE(tid, ro)			bool restarted = false; \
																		STM_START(tid, ro, &restarted); \
																		bool modeChanged = STM_PreStart_Tx(restarted); \
																		if (!modeChanged){
#define COMMIT_STM_MODE								STM_COMMIT; \
																			STM_PostCommit_Tx(); \
																			break; \
																		} \
																		STM_COMMIT; \
																	} \
																}

#define TM_ARGDECL_ALONE               /* Nothing */
#define TM_ARGDECL                     /* Nothing */
#define TM_ARG                         /* Nothing */
#define TM_ARG_ALONE                   /* Nothing */
#define TM_CALLABLE                    TM_SAFE

#define TM_SHARED_READ(var)            TM_LOAD(&(var))
#define TM_SHARED_READ_P(var)          TM_LOAD(&(var))

#define TM_SHARED_WRITE(var, val)      TM_STORE(&(var), val)
#define TM_SHARED_WRITE_P(var, val)    TM_STORE(&(var), val)
