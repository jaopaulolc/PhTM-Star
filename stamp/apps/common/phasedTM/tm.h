#ifndef _TM_H
#define _TM_H

#include <phTM.h>

#if defined(__x86_64__) || defined(__i386)
#include <msr.h>
#include <pmu.h>
#else
#define msrInitialize()         		/* nothing */
#define msrTerminate()          		/* nothing */
#endif

#define RO	1
#define RW	0

#define TinySTM 1
#define NOrec   2

#define MAIN(argc, argv)            int main (int argc, char** argv)
#define MAIN_RETURN(val)            return val

#define GOTO_SIM()                  /* nothing */
#define GOTO_REAL()                 /* nothing */
#define IS_IN_SIM()                 (0)

#define SIM_GET_NUM_CPU(var)        /* nothing */

#define P_MEMORY_STARTUP(numThread) /* nothing */
#define P_MEMORY_SHUTDOWN()         /* nothing */

#if STM == TinySTM

#include <stm.h>
#include <mod_mem.h>
#include <mod_ab.h>
#include <mod_stats.h>

#define TM_ARG                      /* nothing */
#define TM_ARG_ALONE                /* nothing */
#define TM_ARGDECL                  /* nothing */
#define TM_ARGDECL_ALONE            /* nothing */
#define TM_SAFE                     /* nothing */
#define TM_PURE                     /* nothing */

#define SEQ_MALLOC(size)            malloc(size)
#define SEQ_FREE(ptr)               free(ptr)
#define P_MALLOC(size)              malloc(size)
#define P_FREE(ptr)                 free(ptr)
#define TM_MALLOC(size)             stm_malloc(size)
/* Note that we only lock the first word and not the complete object */
#define TM_FREE(ptr)                stm_free(ptr, sizeof(stm_word_t))

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
#if STM_VERSION_NB <= 103
# define STM_START(ro, tx_count, restarted)     \
																		do { \
                                      sigjmp_buf *_e; \
                                      stm_tx_attr_t _a = {0, ro}; \
                                      _e = stm_start(&_a, tx_count); \
                                      int status = sigsetjmp(*_e, 0); \
																			(*restarted) = status != 0; \
                                    } while (0)
#else /* STM_VERSION_NB > 103 */
# define STM_START(ro, tx_count, restarted)			\
																		do { \
                                      sigjmp_buf *_e; \
                                      stm_tx_attr_t _a = {{.id = 0, .read_only = ro, .visible_reads = 0}}; \
                                      _e = stm_start(_a, tx_count); \
                                      int status = sigsetjmp(*_e, 0); \
																			(*restarted) = status != 0; \
                                    } while (0)
#endif /* STM_VERSION_NB > 103 */

#define STM_COMMIT                  stm_commit()

#define IF_HTM_MODE							while(1){ \
																	uint64_t mode = getMode(); \
																	if (mode == HW){
#define START_HTM_MODE 							bool modeChanged = HTM_Start_Tx(); \
																		if (!modeChanged) {
#define COMMIT_HTM_MODE								HTM_Commit_Tx(); \
																			break; \
																		}
#define ELSE_STM_MODE							} else {
#define START_STM_MODE(ro)					bool restarted = false;                        \
																		STM_START(ro, __COUNTER__, &restarted);        \
																		bool modeChanged = STM_PreStart_Tx(restarted); \
																		if (!modeChanged){
#define COMMIT_STM_MODE								STM_COMMIT;                                  \
																			STM_PostCommit_Tx();                         \
																			break;                                       \
																		}                                              \
																		STM_COMMIT;                                    \
																	}                                                \
																}

#define TM_RESTART()            { \
																	uint64_t mode = getMode(); \
																	if(mode == SW) stm_abort(0); \
																	else htm_abort(); \
																}
#define TM_EARLY_RELEASE(var)       /* nothing */

#include <wrappers.h>

/* We could also map macros to the stm_(load|store)_long functions if needed */

typedef union { stm_word_t w; float f;} floatconv_t;

#define TM_SHARED_READ(var)           stm_load((volatile stm_word_t *)(void *)&(var))
#define TM_SHARED_READ_P(var)         stm_load_ptr((volatile void **)(void *)&(var))
#define TM_SHARED_READ_F(var)         stm_load_float((volatile float *)(void *)&(var))
//#define TM_SHARED_READ_P(var)         stm_load((volatile stm_word_t *)(void *)&(var))
//#define TM_SHARED_READ_F(var)         ({floatconv_t c; c.w = stm_load((volatile stm_word_t *)&(var)); c.f;})

#define TM_SHARED_WRITE(var, val)     stm_store((volatile stm_word_t *)(void *)&(var), (stm_word_t)val)
#define TM_SHARED_WRITE_P(var, val)   stm_store_ptr((volatile void **)(void *)&(var), val)
#define TM_SHARED_WRITE_F(var, val)   stm_store_float((volatile float *)(void *)&(var), val)
//#define TM_SHARED_WRITE_P(var, val)   stm_store((volatile stm_word_t *)(void *)&(var), (stm_word_t)val)
//#define TM_SHARED_WRITE_F(var, val)   ({floatconv_t c; c.f = val; stm_store((volatile stm_word_t *)&(var), c.w);})

/* TODO: test with mod_log */
#define TM_LOCAL_WRITE(var, val)      var = val
#define TM_LOCAL_WRITE_P(var, val)    var = val
#define TM_LOCAL_WRITE_F(var, val)    var = val

#define TM_IFUNC_DECL
#define TM_IFUNC_CALL1(r, f, a1)      r = f(a1)
#define TM_IFUNC_CALL2(r, f, a1, a2)  r = f((a1), (a2))

static unsigned int **coreSTM_commits;
static unsigned int **coreSTM_aborts;

#define TM_STARTUP(nThreads)	msrInitialize(); \
															stm_init();      \
															mod_mem_init(0); \
                        			phTM_init(nThreads); \
												coreSTM_commits = (unsigned int **)malloc(sizeof(unsigned int *)*nThreads); \
								    		coreSTM_aborts  = (unsigned int **)malloc(sizeof(unsigned int *)*nThreads)

#define TM_SHUTDOWN()       phTM_term(thread_getNumThread(), NUMBER_OF_TRANSACTIONS, coreSTM_commits, coreSTM_aborts); \
														stm_exit(); \
														msrTerminate()

#define TM_THREAD_ENTER()           long __tid__ = thread_getId(); \
																		set_affinity(__tid__);                       \
																		stm_init_thread(NUMBER_OF_TRANSACTIONS); \
																		phTM_thread_init(__tid__)

#define TM_THREAD_EXIT()         \
										{ \
											long tid = thread_getId(); \
											if(stm_get_stats("nb_commits",&coreSTM_commits[tid]) == 0){                 \
												fprintf(stderr,"error: get nb_commits failed!\n");                        \
											}                                                                           \
											if(stm_get_stats("nb_aborts",&coreSTM_aborts[tid]) == 0){                   \
												fprintf(stderr,"error: get nb_aborts failed!\n");                         \
											} \
										} \
																		stm_exit_thread()

#elif STM == NOrec

#include <stdio.h>
#include <string.h>
#include <api/api.hpp>
#include <stm/txthread.hpp>
#include <thread.h>

#undef TM_ARG
#undef TM_ARG_ALONE 

#define TM_SAFE                       /* nothing */
#define TM_PURE                       /* nothing */
#define TM_CALLABLE                   /* nothing */

#define TM_ARG                        /* nothing */
#define TM_ARG_ALONE                  /* nothing */
#define TM_ARGDECL                    /* nothing */
#define TM_ARGDECL_ALONE              /* nothing */

#define P_MALLOC(size)                malloc(size)
#define P_FREE(ptr)                   free(ptr)
#define SEQ_MALLOC(size)              malloc(size)
#define SEQ_FREE(ptr)                 free(ptr)
#define TM_MALLOC(size)               TM_ALLOC(size)
/* TM_FREE(ptr) is already defined in the file interface. */

#define TM_STARTUP(numThread)					msrInitialize();        \
																			stm::sys_init(NULL); \
																			phTM_init(numThread)

#define TM_SHUTDOWN()                 phTM_term(thread_getNumThread(), NUMBER_OF_TRANSACTIONS, NULL, NULL); \
																			stm::sys_shutdown(); \
											                msrTerminate()

#define TM_THREAD_ENTER()             long __tid__ = thread_getId(); \
																			set_affinity(__tid__);   \
																			stm::thread_init(); \
																			phTM_thread_init(__tid__)

#define TM_THREAD_EXIT()              stm::thread_shutdown(); \
																			phTM_thread_exit()

#define STM_START(abort_flags)            				 \
    stm::TxThread* tx = (stm::TxThread*)stm::Self; \
    stm::begin(tx, &_jmpbuf, abort_flags);         \
    CFENCE; 

#define STM_COMMIT  stm::commit(tx)

#define IF_HTM_MODE							while(1){ \
																	uint64_t mode = getMode(); \
																	if (mode == HW){
#define START_HTM_MODE 							bool modeChanged = HTM_Start_Tx(); \
																		if (!modeChanged) {
#define COMMIT_HTM_MODE								HTM_Commit_Tx(); \
																			break; \
																		}
#define ELSE_STM_MODE							} else {
#define START_STM_MODE(ro)			    jmp_buf _jmpbuf; \
																		uint32_t abort_flags = setjmp(_jmpbuf); \
																		bool restarted = abort_flags != 0; \
																		bool modeChanged = STM_PreStart_Tx(restarted); \
																		if (!modeChanged){ \
																			STM_START(abort_flags);
#define COMMIT_STM_MODE								STM_COMMIT; \
																			STM_PostCommit_Tx(); \
																			break; \
																		} \
																	} \
																}

#define TM_RESTART()            { \
																	uint64_t mode = getMode(); \
																	if(mode == SW) stm::restart(); \
																	else htm_abort(); \
																}
#define TM_EARLY_RELEASE(var)         /* nothing */

#define TM_LOAD(addr)                 stm::stm_read(addr, (stm::TxThread*)stm::Self)
#define TM_STORE(addr, val)           stm::stm_write(addr, val, (stm::TxThread*)stm::Self)

#define TM_SHARED_READ(var)           TM_LOAD(&var)
#define TM_SHARED_READ_P(var)         TM_LOAD(&var)
#define TM_SHARED_READ_F(var)         TM_LOAD(&var)

#define TM_SHARED_WRITE(var, val)     TM_STORE(&var, val)
#define TM_SHARED_WRITE_P(var, val)   TM_STORE(&var, val)
#define TM_SHARED_WRITE_F(var, val)   TM_STORE(&var, val)

#define TM_LOCAL_WRITE(var, val)     	({var = val; var;})
#define TM_LOCAL_WRITE_P(var, val)    ({var = val; var;})
#define TM_LOCAL_WRITE_F(var, val)    ({var = val; var;})
#define TM_LOCAL_WRITE_D(var, val)    ({var = val; var;})

#define TM_IFUNC_DECL                 /* nothing */
#define TM_IFUNC_CALL1(r, f, a1)      r = f(a1)
#define TM_IFUNC_CALL2(r, f, a1, a2)  r = f((a1), (a2))

#else
#error "no STM selected!"
#endif

#endif /* _TM_H */
