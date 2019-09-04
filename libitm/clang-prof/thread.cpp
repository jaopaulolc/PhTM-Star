#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <assert.h>
#include "libitm.h"
#include "thread.h"
#include "DebugInfo.h"

#include <set>
#include <algorithm>
#include <functional>

static uint32_t numberOfThreads = 0;
static pthread_mutex_t init_thread_lock = PTHREAD_MUTEX_INITIALIZER;
__thread threadDescriptor_t* __threadDescriptor = NULL;

// Provides a on-thread-exit callback used to release per-thread data.
static pthread_key_t thread_release_key;
static pthread_once_t thread_release_once = PTHREAD_ONCE_INIT;

static void thread_exit_handler(void* arg UNUSED);
static void thread_exit_init();
static void set_affinity(long id);

static void _ITM_printBarrierStats();

void*
threadDescriptor_t::operator new (size_t size) {
	void* tx;
	assert(sizeof(threadDescriptor_t) == size);
	tx = malloc(sizeof(threadDescriptor_t));
	memset(tx, 0, sizeof(threadDescriptor_t));
	return tx;
}

void
initThreadDescriptor(threadDescriptor_t* tx) {

	pthread_mutex_lock (&init_thread_lock);
	tx->id = numberOfThreads++;
	pthread_mutex_unlock (&init_thread_lock);
#ifdef DEBUG
	puts ("debug: _ITM_beginTransaction initialized.");
#endif
	if (pthread_once(&thread_release_once, thread_exit_init)) {
		fprintf(stderr, "error: pthread_once failed at getThreadDescriptor!\n");
		exit(EXIT_FAILURE);
	}
	if (pthread_setspecific(thread_release_key, tx)) {
		fprintf(stderr, "error: pthread_setspecific failed at getThreadDescriptor!\n");
	}
	set_affinity(tx->id);
}

void
threadDescriptor_t::operator delete (void* tx) {
	if ( likely(tx != NULL) ) {
		free(tx);
	}
}

__attribute__((destructor))
void pre_exit_main_thread(void) {
  threadDescriptor_t* tx = getThreadDescriptor();
  if (tx) {
    _ITM_printBarrierStats();
    free(tx);
  }
  setThreadDescriptor(NULL);
}

static void
thread_exit_handler(void* arg UNUSED) {
	threadDescriptor_t* tx = getThreadDescriptor();
#ifdef DEBUG
	fprintf(stderr, "debug: thread %u called thread_exit_handler\n", tx->id);
#endif
	if (tx) {
    long next_tid;
    pthread_mutex_lock (&init_thread_lock);
    next_tid = --numberOfThreads;
    if (next_tid == 0) {
      pre_exit_main_thread();
      pthread_mutex_unlock (&init_thread_lock);
      return;
    }
    pthread_mutex_unlock (&init_thread_lock);
		free(tx);
	}
	setThreadDescriptor(NULL);
}

static void
thread_exit_init() {
	if (pthread_key_create(&thread_release_key, thread_exit_handler)) {
		fprintf(stderr, "error: pthread_key_create failed at thread_exit_init!\n");
	}
}

static
void showTop10PCs(std::unordered_map<uint64_t, uint64_t>& M);

static void _ITM_printBarrierStats() {
	threadDescriptor_t* tx = getThreadDescriptor();
  initDebugInfo();
	if (tx) {
    uint64_t reads = tx->numberOfReads;
    uint64_t writes = tx->numberOfWrites;
    uint64_t total = reads + writes;

    uint64_t stackReads = tx->numberOfStackReads;
    uint64_t stackWrites = tx->numberOfStackWrites;
    uint64_t totalStack = stackReads + stackWrites;

    uint64_t heapReads = tx->numberOfHeapReads;
    uint64_t heapWrites = tx->numberOfHeapWrites;
    uint64_t totalHeap = heapReads + heapWrites;

    uint64_t requiredReads = tx->numberOfRequiredReads;
    uint64_t requiredWrites = tx->numberOfRequiredWrites;
    uint64_t totalRequired = requiredReads + requiredWrites;

    printf("\n\nTotal acesses: %lu\n", total);
    printf("\tReads: %lu (%6.2lf)\n",
        reads, 100.0*((double)reads / (double)total));
    printf("\tWrites: %lu (%6.2lf)\n",
        writes, 100.0*((double)writes / (double)total));

    printf("\nStack accesses: %lu (%6.2lf)\n",
        totalStack, 100.0*((double)totalStack / (double)total));
    printf("\tStack Reads: %lu (%6.2lf)\n",
        stackReads, 100.0*((double)stackReads / (double)totalStack));
    printf("\tStack Writes: %lu (%6.2lf)\n",
        stackWrites, 100.0*((double)stackWrites / (double)totalStack));

    printf("\nHeap accesses: %lu (%6.2lf)\n",
        totalHeap, 100.0*((double)totalHeap / (double)total));
    printf("\tHeap Reads: %lu (%6.2lf)\n",
        heapReads, 100.0*((double)heapReads / (double)totalHeap));
    printf("\tHeap Writes: %lu (%6.2lf)\n",
        heapWrites, 100.0*((double)heapWrites / (double)totalHeap));

    printf("\nRequired accesses: %lu (%6.2lf)\n",
        totalRequired, 100.0*((double)totalRequired / (double)total));
    printf("\tRequired Reads: %lu (%6.2lf)\n",
        requiredReads, 100.0*((double)requiredReads / (double)totalRequired));
    printf("\tRequired Writes: %lu (%6.2lf)\n",
        requiredWrites, 100.0*((double)requiredWrites / (double)totalRequired));

    printf("\n============== STACK READS ===============\n");
    showTop10PCs(tx->mapStackReadsPCtoCounter);
    printf("\n============ REQ STACK READS =============\n");
    showTop10PCs(tx->mapReqStackReadsPCtoCounter);
    printf("\n============== STACK WRITES ==============\n");
    showTop10PCs(tx->mapStackWritesPCtoCounter);
    printf("\n=========== REQ STACK WRITES =============\n");
    showTop10PCs(tx->mapReqStackWritesPCtoCounter);
    printf("\n=========== GLOBAL HEAP READS ============\n");
    showTop10PCs(tx->mapGlobalHeapReadsPCtoCounter);
    printf("\n========= REQ GLOBAL HEAP READS ==========\n");
    showTop10PCs(tx->mapReqGlobalHeapReadsPCtoCounter);
    printf("\n========== GLOBAL HEAP WRITES ============\n");
    showTop10PCs(tx->mapGlobalHeapWritesPCtoCounter);
    printf("\n========= REQ GLOBAL HEAP WRITES =========\n");
    showTop10PCs(tx->mapReqGlobalHeapWritesPCtoCounter);
    printf("\n========== LOCAL HEAP READS ==============\n");
    showTop10PCs(tx->mapLocalHeapReadsPCtoCounter);
    printf("\n========= REQ LOCAL HEAP READS ===========\n");
    showTop10PCs(tx->mapReqLocalHeapReadsPCtoCounter);
    printf("\n========== LOCAL HEAP WRITES =============\n");
    showTop10PCs(tx->mapLocalHeapWritesPCtoCounter);
    printf("\n========= REQ LOCAL HEAP WRITES ==========\n");
    showTop10PCs(tx->mapReqLocalHeapWritesPCtoCounter);
    printf("\n==========================================\n");
	}
  termDebugInfo();
}

static
void showTop10PCs(std::unordered_map<uint64_t, uint64_t>& M) {

  typedef std::pair<uint64_t, uint64_t> PCandCountPair;
  typedef std::function<bool(PCandCountPair, PCandCountPair)> Comparator;

  Comparator compFunc =
    [](PCandCountPair p1, PCandCountPair p2)
			{
				return p1.second > p2.second;
			};
  std::set<PCandCountPair, Comparator> S( M.begin(), M.end(), compFunc);
  unsigned i = 0;
  for (const PCandCountPair& p : S) {
    if (i >= 10) break;
    printf("PC: %lx Count:  %lu\n", p.first, p.second);
    DebugInfo DI(p.first);
    getDebugInfo(&DI);
    if (DI.found) {
      printf("\tSource file: %s\n", DI.filename);
      printf("\tFunction: %s\n", DI.functionname);
      printf("\tSource line: %d\n", DI.line);
    }
    i++;
  }
}

static void set_affinity(long id) {
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (id < 0 || id >= num_cores){
		fprintf(stderr,"error: invalid number of threads (nthreads > ncores) at set_affinity!\n");
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
