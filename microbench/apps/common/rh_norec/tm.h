#ifndef _TM_H
#define _TM_H
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

#define RO	1
#define RW	0

#include <stdio.h>
#include <string.h>
#include <api/api.hpp>
#include <stm/txthread.hpp>

extern __thread uint64_t __txId__;

#undef TM_ARG
#undef TM_ARG_ALONE 

#define TM_SAFE                       /* nothing */
#define TM_PURE                       /* nothing */
#define TM_CALLABLE                   /* nothing */

#define TM_ARG                        /* nothing */
#define TM_ARG_ALONE                  /* nothing */
#define TM_ARGDECL                    /* nothing */
#define TM_ARGDECL_ALONE              /* nothing */

#define TM_INIT(nThreads)	            stm::sys_init(NULL)

#define TM_EXIT(nThreads)             stm::sys_shutdown()

#define TM_INIT_THREAD(tid)           set_affinity(tid);    \
																			stm::thread_init()

#define TM_EXIT_THREAD(tid)           stm::thread_shutdown()

#define TM_MALLOC(size)               TM_ALLOC(size)
/* TM_FREE(ptr) is already defined in the file interface. */
#define TM_FREE2(ptr,size)            TM_FREE(ptr)

#define STM_START(tid, ro, abort_flags)            \
	{                                                \
		__txId__ = __COUNTER__;                        \
    stm::TxThread* tx = (stm::TxThread*)stm::Self; \
    stm::begin(tx, &_jmpbuf, abort_flags);         \
    CFENCE;                                        \
	{

#define STM_COMMIT      \
  }                     \
		stm::commit(tx);    \
	}

#define IF_HTM_MODE							do { \
																	if ( RH_NOrec::TxBeginHTx() ) {
#define START_HTM_MODE            	CFENCE;
#define COMMIT_HTM_MODE							RH_NOrec::TxCommitHTx();
#define ELSE_STM_MODE							} else {
#define START_STM_MODE(tid, ro)			jmp_buf _jmpbuf; \
																		uint32_t abort_flags = setjmp(_jmpbuf); \
																		STM_START(tid, ro, abort_flags);
#define COMMIT_STM_MODE							STM_COMMIT; \
																	} \
																} while(0);

#define TM_RESTART()                  stm::restart()

#define TM_LOAD(addr)                 stm::stm_read((long int*)addr, (stm::TxThread*)stm::Self)
#define TM_STORE(addr, val)           stm::stm_write((long int*)addr,(long int)val, (stm::TxThread*)stm::Self)

#define TM_SHARED_READ(var)            TM_LOAD(&(var))
#define TM_SHARED_READ_P(var)          TM_LOAD(&(var))

#define TM_SHARED_WRITE(var, val)      TM_STORE(&(var), val)
#define TM_SHARED_WRITE_P(var, val)    TM_STORE(&(var), val)

#endif /* _TM_H */
