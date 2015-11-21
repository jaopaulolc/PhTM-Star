
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define NUM_PROF_COUNTERS 11
#else /* Haswell */
#define NUM_PROF_COUNTERS 7
#endif /* Haswell */

static enum {
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

static uint64_t **profCounters;

static void __init_prof_counters(long nThreads){

	long i;
	profCounters = (uint64_t**)malloc(nThreads*sizeof(uint64_t*));
	for (i=0; i < nThreads; i++){
		profCounters[i] = (uint64_t*)calloc(NUM_PROF_COUNTERS, sizeof(uint64_t));
	}
}

static void __term_prof_counters(long nThreads){
	
	uint64_t starts = 0;
	uint64_t commits = 0;
	uint64_t aborts = 0;
	uint64_t explicit = 0;
	uint64_t conflict = 0;
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	uint64_t suspended_conflict = 0;
	uint64_t nontx_conflict = 0;
	uint64_t tlb_conflict = 0;
	uint64_t fetch_conflict = 0;
#endif /* PowerTM */
	uint64_t capacity = 0;
	uint64_t illegal = 0;
	uint64_t nested = 0;

	long i;
	for (i=0; i < nThreads; i++){
		commits  += profCounters[i][COMMITED_IDX];
		aborts   += profCounters[i][ABORTED_IDX];
		explicit += profCounters[i][ABORT_EXPLICIT_IDX];
		conflict += profCounters[i][ABORT_TX_CONFLICT_IDX];
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
		suspended_conflict += profCounters[i][ABORT_SUSPENDED_CONFLICT_IDX];
		nontx_conflict     += profCounters[i][ABORT_NON_TX_CONFLICT_IDX];
		tlb_conflict       += profCounters[i][ABORT_TLB_CONFLICT_IDX];
		fetch_conflict     += profCounters[i][ABORT_FETCH_CONFLICT_IDX];
#endif /* PowerTM */
		capacity += profCounters[i][ABORT_CAPACITY_IDX];
		illegal  += profCounters[i][ABORT_ILLEGAL_IDX];
		nested   += profCounters[i][ABORT_NESTED_IDX];

		free(profCounters[i]);
	}
	free(profCounters);
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	conflict += suspended_conflict + nontx_conflict 
	         + tlb_conflict + fetch_conflict;
#endif /* PowerTM */
	starts = commits + aborts;
	printf("#starts    : %12ld\n", starts);
	printf("#commits   : %12ld %6.2f\n", commits, 100.0*((float)commits/(float)starts));
	printf("#aborts    : %12ld %6.2f\n", aborts, 100.0*((float)aborts/(float)starts));
	printf("#explicit  : %12ld %6.2f\n", explicit, 100.0*((float)explicit/(float)aborts));
	printf("#illegal   : %12ld %6.2f\n", illegal, 100.0*((float)illegal/(float)aborts));
	printf("#nested    : %12ld %6.2f\n", nested, 100.0*((float)nested/(float)aborts));
	printf("#capacity  : %12ld %6.2f\n", capacity, 100.0*((float)capacity/(float)aborts));
	printf("#conflicts : %12ld %6.2f\n", conflict, 100.0*((float)conflict/(float)aborts));
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	printf("#suspended_conflicts : %12ld %6.2f\n", suspended_conflict, 100.0*((float)suspended_conflict/(float)conflict));
	printf("#nontx_conflicts     : %12ld %6.2f\n", nontx_conflict, 100.0*((float)nontx_conflict/(float)conflict));
	printf("#tlb_conflicts       : %12ld %6.2f\n", tlb_conflict, 100.0*((float)tlb_conflict/(float)conflict));
	printf("#fetch_conflicts     : %12ld %6.2f\n", fetch_conflict, 100.0*((float)fetch_conflict/(float)conflict));
#endif /* PowerTM */
}

static void __inc_commit_counter(long tid){

	profCounters[tid][COMMITED_IDX]++;
}

static void __inc_abort_counter(long tid, uint32_t abort_reason){

	profCounters[tid][ABORTED_IDX]++;
	if(abort_reason & ABORT_EXPLICIT){
		profCounters[tid][ABORT_EXPLICIT_IDX]++;
	}
	if(abort_reason & ABORT_TX_CONFLICT){
		profCounters[tid][ABORT_TX_CONFLICT_IDX]++;
	}
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	if(abort_reason & ABORT_SUSPENDED_CONFLICT){
		profCounters[tid][ABORT_SUSPENDED_CONFLICT_IDX]++;
	}
	if(abort_reason & ABORT_NON_TX_CONFLICT){
		profCounters[tid][ABORT_NON_TX_CONFLICT_IDX]++;
	}
	if(abort_reason & ABORT_TLB_CONFLICT){
		profCounters[tid][ABORT_TLB_CONFLICT_IDX]++;
	}
	if(abort_reason & ABORT_FETCH_CONFLICT){
		profCounters[tid][ABORT_FETCH_CONFLICT_IDX]++;
	}
#endif /* PowerTM */
	if(abort_reason & ABORT_CAPACITY){
		profCounters[tid][ABORT_CAPACITY_IDX]++;
	}
	if(abort_reason & ABORT_ILLEGAL){
		profCounters[tid][ABORT_ILLEGAL_IDX]++;
	}
	if(abort_reason & ABORT_NESTED){
		profCounters[tid][ABORT_NESTED_IDX]++;
	}
}
