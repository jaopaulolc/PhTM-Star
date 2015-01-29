#ifndef TM_H
#define TM_H 1

#define MAIN(argc, argv)            int main (int argc, char** argv)
#define MAIN_RETURN(val)            return val

#define GOTO_SIM()                  /* nothing */
#define GOTO_REAL()                 /* nothing */
#define IS_IN_SIM()                 (0)

#define SIM_GET_NUM_CPU(var)        /* nothing */

#define P_MEMORY_STARTUP(numThread) /* nothing */
#define P_MEMORY_SHUTDOWN()         /* nothing */

#include <rtm.h>
#include <stm.h>
#include <mod_mem.h>
#include <mod_stats.h>

#include <msr.h>
#include <pmu.h>

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

#if STM_VERSION_NB <= 103
# define TM_START(ro, tx_count)     do { \
                                      sigjmp_buf *_e; \
                                      stm_tx_attr_t _a = {0, ro}; \
                                      _e = stm_start(&_a, tx_count); \
                                      sigsetjmp(*_e, 0); \
                                    } while (0)
#else /* STM_VERSION_NB > 103 */
# define TM_START(ro, tx_count)			do { \
                                      sigjmp_buf *_e; \
                                      stm_tx_attr_t _a = {{.id = 0, .read_only = ro, .visible_reads = 0}}; \
                                      _e = stm_start(_a, tx_count); \
                                      sigsetjmp(*_e, 0); \
                                    } while (0)
#endif /* STM_VERSION_NB > 103 */

#ifdef PROFILING
#define TM_STARTUP(numThread)       msrInitialize(); \
																		pmuStartup(NUMBER_OF_TRANSACTIONS); \
																		coreSTM_commits = (unsigned int **)malloc(sizeof(unsigned int *)*numThread); \
																		coreSTM_aborts  = (unsigned int **)malloc(sizeof(unsigned int *)*numThread); \
																		TSX_STARTUP(numThread); \
																		stm_init(); \
                                    mod_mem_init(0);
#define TM_SHUTDOWN()               TSX_SHUTDOWN(); \
                                    stm_exit(); \
																		pmuShutdown(); \
																		msrTerminate()
#else /* NO PROFILING */
#define TM_STARTUP(numThread)       msrInitialize(); \
																		TSX_STARTUP(numThread); \
																		stm_init(); \
                                    mod_mem_init(0);
#define TM_SHUTDOWN()               TSX_SHUTDOWN(); \
                                    stm_exit(); \
																		msrTerminate()
#endif /* NO PROFILING */

#define TM_THREAD_ENTER()           long __threadId__ = thread_getId(); \
																		stm_init_thread(NUMBER_OF_TRANSACTIONS); \
																		TX_INIT(__threadId__); \
																		set_affinity(__threadId__)

#ifdef PROFILING
extern unsigned int **coreSTM_commits;
extern unsigned int **coreSTM_aborts;
#define TM_THREAD_EXIT()						if(stm_get_stats("nb_commits",&coreSTM_commits[__threadId__]) == 0){ \
																			fprintf(stderr,"error: get nb_commits failed!\n"); \
																		} \
																		if(stm_get_stats("nb_aborts",&coreSTM_aborts[__threadId__]) == 0){ \
																			fprintf(stderr,"error: get nb_aborts failed!\n"); \
																		} \
																		stm_exit_thread()
int __TX_COUNT__;

#define STM_BEGIN()									__TX_COUNT__ = __COUNTER__; \
																		pmuStartCounting(__threadId__, __TX_COUNT__); \
																		TM_START(0, __TX_COUNT__)
#define STM_BEGIN_RO()              __TX_COUNT__ = __COUNTER__; \
																		pmuStartCounting(__threadId__, __TX_COUNT__); \
																		TM_START(1, __TX_COUNT__)
#define STM_END()                   stm_commit(); \
																		pmuStopCounting(__threadId__)

#define RTM_BEGIN()									pmuStartCounting(__threadId__, __COUNTER__); \
																		TX_START()
#define RTM_BEGIN_RO()							pmuStartCounting(__threadId__, __COUNTER__); \
																		TX_START()
#define RTM_END()                   TX_END(); \
																		pmuStopCounting(__threadId__)
#else /* NO PROFILING */
#define TM_THREAD_EXIT()						stm_exit_thread()

#define STM_BEGIN()									TM_START(0,__COUNTER__)
#define STM_BEGIN_RO()              TM_START(1,__COUNTER__)
#define STM_END()                   stm_commit()

#define RTM_BEGIN()									TX_START()
#define RTM_BEGIN_RO()							TX_START()
#define RTM_END()                   TX_END()
#endif /* NO PROFILING */

#define TM_RESTART()                stm_abort(0)
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

#endif /* TM_H */

