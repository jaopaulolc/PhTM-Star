#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>

#include <tm.h>

#define NINCREMENTS 100000000

#define PARAM_DEFAULT_NUMTHREADS (1L)

static pthread_t *threads;
static long nThreads = PARAM_DEFAULT_NUMTHREADS;

static long volatile gCounter = 0;

void parseArgs(int argc, char** argv);
void set_affinity(long id);

void *
work(void *arg){

	const long tid = (long)arg;
	const long nIncrements = NINCREMENTS/nThreads;

	TM_INIT_THREAD(tid);
	
	long i;
	
	IF_HTM_MODE
		START_HTM_MODE
				for (i=0; i < nIncrements; i++){
					gCounter = gCounter + 1;
				}
		COMMIT_HTM_MODE
	ELSE_STM_MODE
		START_STM_MODE
				for (i=0; i < nIncrements; i++){
					TM_STORE(&gCounter, TM_LOAD(&gCounter) + 1);
				}
		COMMIT_STM_MODE

	TM_EXIT_THREAD(tid);

	return NULL;
}


int main(int argc, char** argv){

	parseArgs(argc, argv);
	
	threads = (pthread_t*)malloc(sizeof(pthread_t)*nThreads);
	
	TM_INIT(nThreads);
	
	fprintf(stderr, "STARTING...");
	
	long i;
	for(i=0; i < nThreads; i++){
		int status;
		status = pthread_create(&threads[i],NULL,work,(void*)i);
		if(status != 0){
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	fprintf(stderr, "done.\n");

	void **ret_thread = (void**)malloc(sizeof(void*)*nThreads);

	for(i=0; i < nThreads; i++){
		if(pthread_join(threads[i],&ret_thread[i])){
			perror("pthread_join");
			exit(EXIT_FAILURE);
		}
	}
	free(ret_thread);
	
	fprintf(stderr, "STOPING...done.\n");
	
	TM_EXIT(nThreads);
	
	printf("globalCounter = %ld (check = %s)\n", gCounter
										,(gCounter==NINCREMENTS) ? "PASSED!"
									                           : "NOT PASSED!");

	free(threads);
	return 0;
}

void showUsage(const char* argv0){
	
	printf("\nUsage %s [options]\n",argv0);
	printf("Options:                                      (defaults)\n");
	printf("   n <LONG>   Number of threads                 (%ld)\n", PARAM_DEFAULT_NUMTHREADS);

}

void parseArgs(int argc, char** argv){

	int opt;
	while( (opt = getopt(argc,argv,"n:")) != -1 ){
		switch(opt){
			case 'n':
				nThreads = strtol(optarg,NULL,10);
				break;
			default:
				showUsage(argv[0]);
				exit(EXIT_FAILURE);
		}	
	}
}

void set_affinity(long id){
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (id < 0 || id >= num_cores){
		fprintf(stderr,"error: invalid number of threads (nthreads > ncores)!\n");
		exit(EXIT_FAILURE);
	}
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	int hw_tid = (id%4)*8 + id/4;
  // 4 cores, 8 threads per core
	/* core | hw_thread
	 *  0   |    0..7
	 *  1   |   8..15
	 *  2   |  16..23
	 *  3   |  24..31 */
	CPU_SET(hw_tid, &cpuset);
#else /* Haswell */
	int hw_tid = id;
  // 4 cores, 2 threads per core
	/* core | hw_thread
	 *  0   |   0,4
	 *  1   |   1,5
	 *  2   |   2,6
	 *  3   |   3,7 */
	CPU_SET(hw_tid, &cpuset);
#endif /* Haswell*/

	pthread_t current_thread = pthread_self();
	if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset)){
		perror("pthread_setaffinity_np");
		exit(EXIT_FAILURE);
	}
	
	while( hw_tid != sched_getcpu() );
}
