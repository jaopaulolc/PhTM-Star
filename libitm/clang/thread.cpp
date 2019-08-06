#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include "libitm.h"
#include "thread.h"
#include "api/api.hpp"
#include "stm/txthread.hpp"

#if defined(BACKEND_PHASEDTM)
#include <phTM.h>
#endif

static uint32_t numberOfThreads = 0;
static pthread_mutex_t init_thread_lock = PTHREAD_MUTEX_INITIALIZER;
__thread threadDescriptor_t* __threadDescriptor = NULL;

// Provides a on-thread-exit callback used to release per-thread data.
static pthread_key_t thread_release_key;
static pthread_once_t thread_release_once = PTHREAD_ONCE_INIT;

static void thread_exit_handler(void* arg UNUSED);
static void thread_exit_init();
static void set_affinity(long id);

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
#if defined(BACKEND_NOREC)
	stm::thread_init();
#elif defined(BACKEND_PHASEDTM)
	stm::thread_init();
  phTM_thread_init();
#else
#error "unknown or no backend selected!"
#endif
	tx->stmTxDescriptor = (void*)stm::Self;
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
#if defined(BACKEND_NOREC)
    stm::thread_shutdown();
    stm::sys_shutdown();
#elif defined(BACKEND_PHASEDTM)
    stm::thread_shutdown();
    phTM_thread_exit(0,0);
    stm::sys_shutdown();
    phTM_term();
#else
#error "unknown or no backend selected!"
#endif
  }
}

static void
thread_exit_handler(void* arg UNUSED) {
	threadDescriptor_t* tx = getThreadDescriptor();
#ifdef DEBUG
	fprintf(stderr, "debug: thread %u called thread_exit_handler\n", tx->id);
#endif
	if (tx) {
#if defined(BACKEND_NOREC)
		stm::thread_shutdown();
#elif defined(BACKEND_PHASEDTM)
		stm::thread_shutdown();
    phTM_thread_exit(0,0);
#else
#error "unknown or no backend selected!"
#endif
		free(tx);
	}
	setThreadDescriptor(NULL);
}

static void
thread_exit_init() {
#if defined(BACKEND_NOREC)
  stm::sys_init(NULL);
#elif defined(BACKEND_PHASEDTM)
  stm::sys_init(NULL);
  phTM_init();
#else
#error "unknown or no backend selected!"
#endif
	if (pthread_key_create(&thread_release_key, thread_exit_handler)) {
		fprintf(stderr, "error: pthread_key_create failed at thread_exit_init!\n");
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
