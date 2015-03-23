#ifndef _PMU_INCLUDE
#define _PMU_INCLUDE

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 
#endif /* _GNU_SOURCE*/
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * PMU macros
 */
#define IA32_PMC0								(0x0C1)
#define IA32_PERFEVTSEL0				(0x186)
#define IA32_PERF_GLOBAL_CTRL		(0x38F)
#define IA32_FIXED_CTR_CTRL			(0x38D)
// 0 - INST_RETIRED.ANY
// 1 - CPU_CLK_UNHALTED.THREAD
// 2 - CPU_CLK_UNHALTED.REF
#define IA32_FIXED_CTR0					(0x309)

/**
 * PMU data structures
 */
typedef struct _EventSelectRegister {
	union {
		struct {
			uint64_t event_select : 8;
			uint64_t umask : 8;
			uint64_t usr : 1;
			uint64_t os : 1;
			uint64_t edge : 1;
			uint64_t pin_control : 1;
			uint64_t apic_int : 1;
			uint64_t any_thread : 1;
			uint64_t enable : 1;
			uint64_t invert : 1;
			uint64_t cmask : 8;
			uint64_t in_tx : 1;
			uint64_t in_txcp : 1;
			uint64_t reserved : 30;
		} fields;
		uint64_t value;
	};
} EventSelectRegister;

typedef struct _CoreEvent {
	const char *eventString;
	uint64_t event_select;
	uint64_t umask;
} CoreEvent;

typedef struct _CoreData {
	EventSelectRegister *controlReg;
	uint64_t fixedControlReg;
	uint64_t **measurements;
	int measurement_i;
	int msrFD;
} CoreData;

void pmuStartup(int numberOfMeasurements);
void pmuShutdown();

/**
 * Enable coreId's counters
 */
void pmuStartCounting(int coreId, int measurement_i);

/**
 * Disable coreId's counters
 */
void pmuStopCounting(int coreId);

/**
 * @return: number of programmable counters available
 */
int pmuNumberOfCustomCounters();

/**
 * @return: number of fixed counters available
 */
int pmuNumberOfFixedCounters();

/**
 * @return: number of measurements
 */
int pmuNumberOfMeasurements();

/**
 * @return: number of cores available
 */
int pmuNumberOfOnlineCores();

/**
 * @args:
 * 	+ coreId: core id :: integer between 0 and _SC_NPROCESSORS_ONLN - 1 
 * @return: coreId's measurement table if coreId is valid, NULL otherwise :: measurements[nmeasurements][ntotalCounters]
 */
uint64_t **pmuGetMeasurements(int coreId);

/**
 * @args:
 * 	+ counterSlotIdx: counter slot index
 * 	+ coreEventId: core event id (see PMU enum below)
 * @return: 0 if available counter register, -1 otherwise
 */
int pmuAddCustomCounter(int counterSlotIdx, int coreEventId);

/**
 * PMU enum :: core event ids -- MUST BE kept synchronized with coreEventTable in pmu.c
 */
enum{
	HLE_TX_STARTED = 0     ,
	HLE_TX_COMMITED        ,
	HLE_TX_ABORTED         ,
	HLE_TX_ABORT_MEMORY    ,
	HLE_TX_ABORT_UNCOMMON  ,
	HLE_TX_ABORT_UNFRIENDLY,
	HLE_TX_ABORT_INCOMP_MEM,
	HLE_TX_ABORT_OTHER     ,
	RTM_TX_STARTED         ,
	RTM_TX_COMMITED        ,
	RTM_TX_ABORTED         ,
	RTM_TX_ABORT_MEMORY    ,
	RTM_TX_ABORT_UNCOMMON  ,
	RTM_TX_ABORT_UNFRIENDLY,
	RTM_TX_ABORT_INCOMP_MEM,
	RTM_TX_ABORT_OTHER     ,
	TX_ABORT_CONFLICT  ,
	TX_ABORT_CAPACITY  ,
};

#if defined(__cplusplus)
} /* extern "C" { */
#endif
#endif /* _PMU_INCLUDE */
