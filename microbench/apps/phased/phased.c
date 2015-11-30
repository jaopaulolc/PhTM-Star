#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
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

#ifndef __ALIGN__
#define __ALIGN__ __attribute__((aligned(CACHE_ALIGNMENT)))
#endif /* __ALIGN__ */


#define PARAM_DEFAULT_NUMTHREADS (1L)


static pthread_t *threads __ALIGN__;
static long nThreads __ALIGN__ = PARAM_DEFAULT_NUMTHREADS;

static volatile long lastReader = 0;

static uint64_t *global_array __ALIGN__;
static volatile int stop __ALIGN__ = 0;
static pthread_barrier_t sync_barrier __ALIGN__ ;

void randomly_init_ushort_array(unsigned short *s, long n);
void parseArgs(int argc, char** argv);
void set_affinity(long id);
double getTimeSeconds();

static __thread unsigned short seed[3] __ALIGN__;

inline static
void conflict_function(const long tid, const uint64_t pWrites, long repeat){
	
	uint64_t const blocksPerThread __ALIGN__ = L1_BLOCKS_PER_SET/nThreads;
	
	//pthread_barrier_wait(&sync_barrier);

	while(--repeat){

		uint64_t op __ALIGN__ = nrand48(seed) % 100;
		uint64_t i, j, blockIndex __ALIGN__;
		uint64_t value __ALIGN__ = nrand48(seed) % (L1_CACHE_SIZE*L1_CACHE_SIZE);
		
		if(op < pWrites){
			uint64_t idx __ALIGN__ = __atomic_load_n(&lastReader, __ATOMIC_SEQ_CST);
			blockIndex = idx*blocksPerThread;
			i = blockIndex*L1_SET_BLOCK_OFFSET;

			__asm__ volatile ("":::"memory");

#ifdef phasedTM
		IF_HTM_MODE
			START_HTM_MODE
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET/L1_WORDS_PER_BLOCK); j++){
					global_array[j] = value;
				}
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET/L1_WORDS_PER_BLOCK); j+=4*L1_WORDS_PER_BLOCK){
					TM_STORE(&global_array[j], value);
				}
			COMMIT_STM_MODE
#else /* !phasedTM */
			TM_START(tid, RW);
#if STM
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET/L1_WORDS_PER_BLOCK); j+=4*L1_WORDS_PER_BLOCK){
#else
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET/L1_WORDS_PER_BLOCK); j++){
#endif
					TM_STORE(&global_array[j], value);
				}
			TM_COMMIT;
#endif /* !phasedTM */
		} else {
			blockIndex = tid*blocksPerThread;
			i = blockIndex*L1_SET_BLOCK_OFFSET;
			__atomic_store_n(&lastReader, tid , __ATOMIC_SEQ_CST);

			__asm__ volatile ("":::"memory");

#ifdef phasedTM
		IF_HTM_MODE
			START_HTM_MODE
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET/L1_WORDS_PER_BLOCK); j++){
					global_array[j];
				}
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET/L1_WORDS_PER_BLOCK); j+=4*L1_WORDS_PER_BLOCK){
					TM_LOAD(&global_array[j]);
				}
			COMMIT_STM_MODE
#else /* !phasedTM */
			TM_START(tid, RW);
#if STM
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET/L1_WORDS_PER_BLOCK); j+=4*L1_WORDS_PER_BLOCK){
#else
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET/L1_WORDS_PER_BLOCK); j++){
#endif
					TM_LOAD(&global_array[j]);
				}
			TM_COMMIT;
#endif /* !phasedTM */
		}
	}
}

void *work(void *arg){

	const long tid = (long)arg;
	randomly_init_ushort_array(seed,3);

  TM_INIT_THREAD(tid);
	
	const float step = (5*M_PI)/180.0; // step = 5º
	float i;
	
	for (i=0; i < 2.0*M_PI; i+=step){
		const uint64_t pWrites = (uint64_t)(100.0*(sin(i)*0.5 + 0.5));
		conflict_function(tid, pWrites, (long)(1.0e6/nThreads));
	}

  TM_EXIT_THREAD(tid);
	return NULL;
}


int main(int argc, char** argv){

	parseArgs(argc, argv);
	
	pthread_barrier_init(&sync_barrier, NULL, nThreads);
	threads = (pthread_t*)malloc(sizeof(pthread_t)*nThreads);
	global_array = (uint64_t*)memalign(CACHE_ALIGNMENT, L1_CACHE_SIZE);

	TM_INIT(nThreads);
	
	printf("STARTING...");

	long i;
	for(i=0; i < nThreads; i++){
		int status;
		status = pthread_create(&threads[i],NULL,work,(void*)i);
		if(status != 0){
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	printf("done.\nSTOPING...");

	void **ret_thread = (void**)malloc(sizeof(void*)*nThreads);

	for(i=0; i < nThreads; i++){
		if(pthread_join(threads[i],&ret_thread[i])){
			perror("pthread_join");
			exit(EXIT_FAILURE);
		}
	}
	free(ret_thread);
	
	printf("done.\n");
	
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
	printf("   n <LONG>   Number of threads                 (%ld)\n", PARAM_DEFAULT_NUMTHREADS);

}

void parseArgs(int argc, char** argv){

	int opt;
	while( (opt = getopt(argc,argv,"n:u:")) != -1 ){
		switch(opt){
			case 'n':
				nThreads = strtol(optarg,NULL,10);
				break;
			case 'u':
				//pWrites = strtol(optarg,NULL,10);
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
