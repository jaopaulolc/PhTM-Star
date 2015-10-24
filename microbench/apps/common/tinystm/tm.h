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

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
#define TM_START(tid, ro)                  { stm_tx_attr_t _a = {{.id = tid, .read_only = ro}}; \
                                              sigjmp_buf *_e = stm_start(_a,__COUNTER__); \
                                              if (_e != NULL) sigsetjmp(*_e, 0); 
#define TM_START_TS(ts, label)             { sigjmp_buf *_e = stm_start((stm_tx_attr_t)0,__COUNTER__); \
                                              if (_e != NULL && sigsetjmp(*_e, 0)) goto label; \
	                                      stm_set_extension(0, &ts)
#define TM_LOAD(addr)                      stm_load((stm_word_t *)addr)
#define TM_UNIT_LOAD(addr, ts)             stm_unit_load((stm_word_t *)addr, ts)
#define TM_STORE(addr, value)              stm_store((stm_word_t *)addr, (stm_word_t)value)
#define TM_UNIT_STORE(addr, value, ts)     stm_unit_store((stm_word_t *)addr, (stm_word_t)value, ts)
#define TM_COMMIT                          stm_commit(); }
#define TM_MALLOC(size)                    stm_malloc(size)
#define TM_FREE(addr)                      stm_free(addr, sizeof(*addr))
#define TM_FREE2(addr, size)               stm_free(addr, size)

#define TM_INIT(nThreads)                  stm_init(); mod_mem_init(0); mod_ab_init(0, NULL)
#define TM_EXIT(nThreads)                  stm_exit()
#define TM_INIT_THREAD(tid)                stm_init_thread(3); set_affinity(tid)
#define TM_EXIT_THREAD(tid)                stm_exit_thread()

#define TM_ARGDECL_ALONE               /* Nothing */
#define TM_ARGDECL                     /* Nothing */
#define TM_ARG                         /* Nothing */
#define TM_ARG_ALONE                   /* Nothing */
#define TM_CALLABLE                    TM_SAFE

#define TM_SHARED_READ(var)            TM_LOAD(&(var))
#define TM_SHARED_READ_P(var)          TM_LOAD(&(var))

#define TM_SHARED_WRITE(var, val)      TM_STORE(&(var), val)
#define TM_SHARED_WRITE_P(var, val)    TM_STORE(&(var), val)
