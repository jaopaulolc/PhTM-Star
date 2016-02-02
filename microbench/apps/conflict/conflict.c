#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <limits.h>
#include <math.h>

#include <tm.h>

#define RO	1
#define RW	0

#if defined(__x86_64__) || defined(__i386)
	#define L1_CACHE_SIZE   (32L*1024L)
	#define L1_BLOCK_SIZE           64L // 64 bytes per block/line
	#define L1_BLOCKS_PER_SET        8L // 8-way set associative
	#define L1_WORDS_PER_BLOCK       8L
	#define L1_NUM_SETS          (L1_CACHE_SIZE/(L1_BLOCK_SIZE*L1_BLOCKS_PER_SET))
	#define L1_SET_BLOCK_OFFSET  ((L1_NUM_SETS*L1_BLOCK_SIZE)/sizeof(long))
	#define CACHE_ALIGNMENT		   (L1_NUM_SETS*L1_BLOCK_SIZE)
#else /* PowerPC */
	#define L1_CACHE_SIZE   (512L*1024L)
	#define L1_BLOCK_SIZE          128L // 128 bytes per block/line
	#define L1_BLOCKS_PER_SET        8L // 8-way set associative
	#define L1_WORDS_PER_BLOCK      16L
	#define L1_NUM_SETS          (L1_CACHE_SIZE/(L1_BLOCK_SIZE*L1_BLOCKS_PER_SET))
	#define L1_SET_BLOCK_OFFSET  ((L1_NUM_SETS*L1_BLOCK_SIZE)/sizeof(long))
	#define CACHE_ALIGNMENT			 (L1_NUM_SETS*L1_BLOCK_SIZE)
#endif /* PowerPC */

// PowerTM in theory can read 64 blocks, but we set 48 be safe.
//#define MAX_HTM_WRITES (48UL*16UL)

#ifndef __ALIGN__
#define __ALIGN__ __attribute__((aligned(CACHE_ALIGNMENT)))
#endif /* __ALIGN__ */


#define PARAM_DEFAULT_NUMTHREADS (1L)
#define PARAM_DEFAULT_CONTENTION (0L)
#define PARAM_DEFAULT_TXLENGTH  (512L)


static pthread_t *threads __ALIGN__;
static long nThreads __ALIGN__ = PARAM_DEFAULT_NUMTHREADS;
static long pWrites  __ALIGN__ = PARAM_DEFAULT_CONTENTION;
static long txLength __ALIGN__ = PARAM_DEFAULT_TXLENGTH;

static volatile long lastReader = 0;

static uint64_t *global_array __ALIGN__;
static volatile int stop __ALIGN__ = 0;
static pthread_barrier_t sync_barrier __ALIGN__ ;

void randomly_init_ushort_array(unsigned short *s, long n);
void parseArgs(int argc, char** argv);
void set_affinity(long id);

void *conflict_function(void *args){
	
	const uint64_t tid __ALIGN__= (uint64_t)args;
	const uint64_t TX_LENGTH __ALIGN__ = txLength;
	const uint64_t thread_step __ALIGN__ = (L1_CACHE_SIZE/sizeof(uint64_t))*(2*tid);
	unsigned short seed[3] __ALIGN__;
	randomly_init_ushort_array(seed,3);
	
  TM_INIT_THREAD(tid);
	pthread_barrier_wait(&sync_barrier);

	while(!stop){

		long op __ALIGN__ = nrand48(seed) % 101;
		uint64_t i __ALIGN__;
		uint64_t j __ALIGN__;
		uint64_t blockIndex __ALIGN__;
		uint64_t value __ALIGN__ = (nrand48(seed) % (UINT_MAX-1)) + 1;
		
		blockIndex = nrand48(seed) % L1_BLOCKS_PER_SET;
		i = blockIndex*L1_SET_BLOCK_OFFSET;
		
		__asm__ volatile ("":::"memory");
		
		if(op < pWrites){
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
				for(j=i; j < (i + TX_LENGTH); j++){
					if (global_array[0 + j] != 0)
						global_array[0 + j] = value;
				}
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(tid, RO)
#else /* !HW_SW_PATHS */
			TM_START(tid, RW);
#endif /* !HW_SW_PATHS */
				for(j=i; j < (i + TX_LENGTH); j++){
					if (TM_LOAD(&global_array[0 + j]) != 0)
						TM_STORE(&global_array[0 + j], value);
				}
#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
			TM_COMMIT;
#endif /* !HW_SW_PATHS */
		} else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
				for(j=i; j < (i + TX_LENGTH); j++){
					if (global_array[thread_step + j] == UINT_MAX)
						global_array[thread_step + j] = value;
				}
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(tid, RO)
#else /* !HW_SW_PATHS */
			TM_START(tid, RW);
#endif /* !HW_SW_PATHS */
				for(j=i; j < (i + TX_LENGTH); j++){
					if (TM_LOAD(&global_array[thread_step + j]) == UINT_MAX)
						TM_STORE(&global_array[thread_step + j], value);
				}
#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
			TM_COMMIT;
#endif /* !HW_SW_PATHS */
		}
	}

  TM_EXIT_THREAD(tid);

	return (void *)(tid);
}

int main(int argc, char** argv){

	parseArgs(argc, argv);
	
	struct timespec timeout = { .tv_sec = 1, .tv_nsec = 0 };
	pthread_barrier_init(&sync_barrier, NULL, nThreads+1);
	threads = (pthread_t*)malloc(sizeof(pthread_t)*nThreads);
	global_array = (uint64_t*)memalign(CACHE_ALIGNMENT, (2*nThreads)*L1_CACHE_SIZE);
	memset(global_array, 1, (2*nThreads)*L1_CACHE_SIZE);

	TM_INIT(nThreads);
	
	long i;
	for(i=0; i < nThreads; i++){
		int status;
		status = pthread_create(&threads[i],NULL,conflict_function,(void*)i);
		if(status != 0){
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	pthread_barrier_wait(&sync_barrier);
	printf("STARTING...\n");

	nanosleep(&timeout,NULL);
	stop = 1;

	printf("STOPING...\n");

	void **ret_thread = (void**)malloc(sizeof(void*)*nThreads);

	for(i=0; i < nThreads; i++){
		if(pthread_join(threads[i],&ret_thread[i])){
			perror("pthread_join");
			exit(EXIT_FAILURE);
		}
	}
	free(ret_thread);
	
	TM_EXIT(nThreads);

	free(threads);
	free(global_array);
	
	return 0;
}

void randomly_init_ushort_array(unsigned short *s, long n){
	
	srand(time(NULL));
	long i;
	for(i=0; i < n; i++){
		s[i] = (unsigned short)rand();
	}
}

void showUsage(const char* argv0){
	
	printf("\nUsage %s [options]\n",argv0);
	printf("Options:                                      (defaults)\n");
	printf("   n  <LONG>  Number of threads                 (%6ld)\n", PARAM_DEFAULT_NUMTHREADS);
	printf("   u  <LONG>  Contention level [0%%..100%%]     (%6ld)\n", PARAM_DEFAULT_CONTENTION);
	printf("   l  <LONG>  Transaction lenght                (%6ld)\n", PARAM_DEFAULT_TXLENGTH);

}

void parseArgs(int argc, char** argv){

	int opt;
	while( (opt = getopt(argc,argv,"n:u:l:")) != -1 ){
		switch(opt){
			case 'n':
				nThreads = strtol(optarg,NULL,10);
				break;
			case 'u':
				pWrites = strtol(optarg,NULL, 10);
				break;
			case 'l':
				txLength = strtol(optarg,NULL,10);
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
