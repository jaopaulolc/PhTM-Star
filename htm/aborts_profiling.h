#ifdef HTM_STATUS_PROFILING

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define NUM_PROF_COUNTERS 11
#else /* Haswell */
#define NUM_PROF_COUNTERS 7
#endif /* Haswell */

enum {
	ABORT_EXPLICIT_IDX=0,
	ABORT_TX_CONFLICT_IDX,
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	ABORT_SUSPENDED_CONFLICT_IDX,
	ABORT_NON_TX_CONFLICT_IDX,
	ABORT_TLB_CONFLICT_IDX,
	ABORT_FETCH_CONFLICT_IDX,
#endif /* PowerTM */
	ABORT_CAPACITY_IDX,
	ABORT_ILLEGAL_IDX,
	ABORT_NESTED_IDX,
	ABORTED_IDX,
	COMMITED_IDX,
};

static pthread_mutex_t profiling_lock __ALIGN__ = PTHREAD_MUTEX_INITIALIZER;
static long __next_tx_id = 0;
static uint64_t global_profiling_counters[NUM_PROF_COUNTERS] __ALIGN__;
static __thread uint64_t thread_profiling_counters[NUM_PROF_COUNTERS] __ALIGN__;
static uint64_t global_stm_commits __ALIGN__ = 0;
static uint64_t global_stm_aborts __ALIGN__ = 0;

static void __init_prof_counters(){

  pthread_mutex_lock(&profiling_lock);
  __next_tx_id++;
  if (__next_tx_id == 1) {
    memset(&global_profiling_counters, 0, sizeof(uint64_t)*NUM_PROF_COUNTERS);
  }
  pthread_mutex_unlock(&profiling_lock);
  memset(&thread_profiling_counters, 0, sizeof(uint64_t)*NUM_PROF_COUNTERS);
}

#ifndef PHASEDTM
static
#endif
void __report_prof_counters() {

  if (__next_tx_id != 0) { // not last tx
    return;
  }

  uint64_t starts;
  uint64_t commits  = global_profiling_counters[COMMITED_IDX];
  uint64_t aborts   = global_profiling_counters[ABORTED_IDX];
  uint64_t explicit = global_profiling_counters[ABORT_EXPLICIT_IDX];
  uint64_t conflict = global_profiling_counters[ABORT_TX_CONFLICT_IDX];
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
  uint64_t suspended_conflict = global_profiling_counters[ABORT_SUSPENDED_CONFLICT_IDX];
  uint64_t nontx_conflict     = global_profiling_counters[ABORT_NON_TX_CONFLICT_IDX];
  uint64_t tlb_conflict       = global_profiling_counters[ABORT_TLB_CONFLICT_IDX];
  uint64_t fetch_conflict     = global_profiling_counters[ABORT_FETCH_CONFLICT_IDX];
#endif /* PowerTM */
  uint64_t capacity = global_profiling_counters[ABORT_CAPACITY_IDX];
  uint64_t illegal  = global_profiling_counters[ABORT_ILLEGAL_IDX];
  uint64_t nested   = global_profiling_counters[ABORT_NESTED_IDX];

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	conflict += suspended_conflict + nontx_conflict
	         + tlb_conflict + fetch_conflict;
#endif /* PowerTM */
	starts = commits + aborts;

#define RATIO(a,b) (100.0F*((float)a/(float)b))

#ifdef PHASEDTM
  uint64_t stm_starts;
  uint64_t stm_commits = global_stm_commits;
  uint64_t stm_aborts = global_stm_aborts;
	stm_starts = stm_commits + stm_aborts;
	uint64_t tStarts  = starts + stm_starts;
	uint64_t tCommits = commits + stm_commits;
	uint64_t tAborts  = aborts + stm_aborts;
	printf("#starts    : %12ld %6.2f %6.2f\n", tStarts , RATIO(starts,tStarts)
	                                                   , RATIO(stm_starts,tStarts));
	printf("#commits   : %12ld %6.2f %6.2f %6.2f\n", tCommits, RATIO(tCommits,tStarts)
	                                                         , RATIO(commits,starts)
																													 , RATIO(stm_commits,stm_starts));
	printf("#aborts    : %12ld %6.2f %6.2f %6.2f\n", tAborts , RATIO(tAborts,tStarts)
	                                                         , RATIO(aborts,starts)
																													 , RATIO(stm_aborts,stm_starts));
#else /* !PHASEDTM */
	printf("#starts    : %12ld\n", starts);
	printf("#commits   : %12ld %6.2f\n", commits, RATIO(commits,starts));
	printf("#aborts    : %12ld %6.2f\n", aborts , RATIO(aborts,starts));
#endif /* !PHASEDTM */

	printf("#conflicts : %12ld %6.2f\n", conflict, RATIO(conflict,aborts));
	printf("#capacity  : %12ld %6.2f\n", capacity, RATIO(capacity,aborts));
	printf("#explicit  : %12ld %6.2f\n", explicit, RATIO(explicit,aborts));
	printf("#illegal   : %12ld %6.2f\n", illegal , RATIO(illegal,aborts));
	printf("#nested    : %12ld %6.2f\n", nested  , RATIO(nested,aborts));
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	printf("#suspended_conflicts : %12ld %6.2f\n", suspended_conflict, RATIO(suspended_conflict,conflict));
	printf("#nontx_conflicts     : %12ld %6.2f\n", nontx_conflict    , RATIO(nontx_conflict,conflict));
	printf("#tlb_conflicts       : %12ld %6.2f\n", tlb_conflict      , RATIO(tlb_conflict,conflict));
	printf("#fetch_conflicts     : %12ld %6.2f\n", fetch_conflict    , RATIO(fetch_conflict,conflict));
#endif /* PowerTM */

#undef RATIO
}

#ifdef PHASEDTM
void __term_prof_counters(uint64_t stm_commits, uint64_t stm_aborts){
#else
static void __term_prof_counters() {
#endif

  pthread_mutex_lock(&profiling_lock);
    __next_tx_id--;
    global_profiling_counters[COMMITED_IDX]
      += thread_profiling_counters[COMMITED_IDX];
    global_profiling_counters[ABORTED_IDX]
      += thread_profiling_counters[ABORTED_IDX];
    global_profiling_counters[ABORT_EXPLICIT_IDX]
      += thread_profiling_counters[ABORT_EXPLICIT_IDX];
    global_profiling_counters[ABORT_TX_CONFLICT_IDX]
      += thread_profiling_counters[ABORT_TX_CONFLICT_IDX];
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
    global_profiling_counters[ABORT_SUSPENDED_CONFLICT_IDX]
      += thread_profiling_counters[ABORT_SUSPENDED_CONFLICT_IDX];
    global_profiling_counters[ABORT_NON_TX_CONFLICT_IDX]
      += thread_profiling_counters[ABORT_NON_TX_CONFLICT_IDX];
    global_profiling_counters[ABORT_TLB_CONFLICT_IDX]
      += thread_profiling_counters[ABORT_TLB_CONFLICT_IDX];
    global_profiling_counters[ABORT_FETCH_CONFLICT_IDX]
      += thread_profiling_counters[ABORT_FETCH_CONFLICT_IDX];
#endif /* PowerTM */
    global_profiling_counters[ABORT_CAPACITY_IDX]
      += thread_profiling_counters[ABORT_CAPACITY_IDX];
    global_profiling_counters[ABORT_ILLEGAL_IDX]
      += thread_profiling_counters[ABORT_ILLEGAL_IDX];
    global_profiling_counters[ABORT_NESTED_IDX]
      += thread_profiling_counters[ABORT_NESTED_IDX];

#ifdef PHASEDTM
    global_stm_commits += stm_commits;
    global_stm_aborts += stm_aborts;
#endif
  pthread_mutex_unlock(&profiling_lock);

}

static void __inc_commit_counter() {

	thread_profiling_counters[COMMITED_IDX]++;
}

static void __inc_abort_counter(uint32_t abort_reason){

	thread_profiling_counters[ABORTED_IDX]++;
	if(abort_reason & ABORT_EXPLICIT){
		thread_profiling_counters[ABORT_EXPLICIT_IDX]++;
	}
	if(abort_reason & ABORT_TX_CONFLICT){
		thread_profiling_counters[ABORT_TX_CONFLICT_IDX]++;
	}
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	if(abort_reason & ABORT_SUSPENDED_CONFLICT){
		thread_profiling_counters[ABORT_SUSPENDED_CONFLICT_IDX]++;
	}
	if(abort_reason & ABORT_NON_TX_CONFLICT){
		thread_profiling_counters[ABORT_NON_TX_CONFLICT_IDX]++;
	}
	if(abort_reason & ABORT_TLB_CONFLICT){
		thread_profiling_counters[ABORT_TLB_CONFLICT_IDX]++;
	}
	if(abort_reason & ABORT_FETCH_CONFLICT){
		thread_profiling_counters[ABORT_FETCH_CONFLICT_IDX]++;
	}
#endif /* PowerTM */
	if(abort_reason & ABORT_CAPACITY){
		thread_profiling_counters[ABORT_CAPACITY_IDX]++;
	}
	if(abort_reason & ABORT_ILLEGAL){
		thread_profiling_counters[ABORT_ILLEGAL_IDX]++;
	}
	if(abort_reason & ABORT_NESTED){
		thread_profiling_counters[ABORT_NESTED_IDX]++;
	}
}

#else /* ! HTM_STATUS_PROFILING */

#define __init_prof_counters()            /* nothing */
#define __inc_commit_counter()            /* nothing */
#define __inc_abort_counter(abort_reason)	/* nothing */

#ifdef PHASEDTM
#define __term_prof_counters(stmCommits, stmAborts) /* nothing */
#else
#define __term_prof_counters()                      /* nothing */
#endif

#define __report_prof_counters()                      /* nothing */

#endif /* ! HTM_STATUS_PROFILING */
