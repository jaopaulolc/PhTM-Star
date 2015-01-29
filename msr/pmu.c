#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <msr.h>
#include <pmu.h>

static int __ncores           = 0;
static int __ncustomCounters  = 0;
static int __nfixedCounters   = 0;
static int __nmeasurements    = 0;
static CoreData *coreData     = NULL;

// MUST BE kept synchronized with enum in pmu.h
const CoreEvent coreEventTable[] = {
	{"HLE_RETIRED.START"          , 0xC8, 0x01},
	{"HLE_RETIRED.COMMIT"         , 0xC8, 0x02},
	{"HLE_RETIRED.ABORTED"        , 0xC8, 0x04},
	{"HLE_RETIRED.ABORTED_MISC1"  , 0xC8, 0x08}, // Number of aborts due to various memory events (e.g. read/write capacity and conflicts)
	{"HLE_RETIRED.ABORTED_MISC2"  , 0xC8, 0x10}, // Number of times an HLE execution aborted due to uncommon conditions
	{"HLE_RETIRED.ABORTED_MISC3"  , 0xC8, 0x20}, // Number of times an HLE execution aborted due to HLE-unfriendly instructions
	{"HLE_RETIRED.ABORTED_MISC4"  , 0xC8, 0x40}, // Number of times an HLE execution aborted due to incompatible memory type
	{"HLE_RETIRED.ABORTED_MISC5"  , 0xC8, 0x80}, // Number of aborts due to none of the previous 4 categories (e.g. interrupts)
	{"RTM_RETIRED.START"          , 0xC9, 0x01},
	{"RTM_RETIRED.COMMIT"         , 0xC9, 0x02},
	{"RTM_RETIRED.ABORTED"        , 0xC9, 0x04},
	{"RTM_RETIRED.ABORTED_MISC1"  , 0xC9, 0x08}, // Number of aborts due to various memory events (e.g. read/write capacity and conflicts)
	{"RTM_RETIRED.ABORTED_MISC2"  , 0xC9, 0x10}, // Number of times an RTM execution aborted due to uncommon conditions
	{"RTM_RETIRED.ABORTED_MISC3"  , 0xC9, 0x20}, // Number of times an RTM execution aborted due to HLE-unfriendly instructions
	{"RTM_RETIRED.ABORTED_MISC4"  , 0xC9, 0x40}, // Number of times an RTM execution aborted due to incompatible memory type
	{"RTM_RETIRED.ABORTED_MISC5"  , 0xC9, 0x80}, // Number of aborts due to none of the previous 4 categories (e.g. interrupts)
	{"TX_MEM.ABORT_CONFLICT"      , 0x54, 0x01},
	{"TX_MEM.ABORT_CAPACITY_WRITE", 0x54, 0x02},
};

static void getNumberOfPerfCounters(int *custom, int *fixed);

void pmuStartup(int numberOfMeasurements){
	
	__ncores = sysconf(_SC_NPROCESSORS_ONLN);
	getNumberOfPerfCounters(&__ncustomCounters,&__nfixedCounters);

	__nmeasurements = numberOfMeasurements;
	
	coreData = (CoreData*)malloc(sizeof(CoreData) * __ncores);

	int i;
	for(i=0; i < __ncores; i++){
		
		int j;
		
		coreData[i].msrFD = __msrOpen(i);
		// disable counters till pmuStartCounting() is called
		__msrWrite(coreData[i].msrFD,IA32_PERF_GLOBAL_CTRL, 0);
		
		coreData[i].controlReg  = (EventSelectRegister*)calloc(__ncustomCounters, sizeof(EventSelectRegister));

		coreData[i].measurements = (uint64_t**)malloc(__nmeasurements * sizeof(uint64_t*));
		for(j=0; j < __nmeasurements; j++){
			coreData[i].measurements[j] = (uint64_t*)calloc(__ncustomCounters + __nfixedCounters, sizeof(uint64_t));
		}

		// Custom/Programmable Counters
		for(j=0; j < __ncustomCounters; j++){
			coreData[i].controlReg[j].fields.usr          = 1;
			coreData[i].controlReg[j].fields.os           = 0;
			coreData[i].controlReg[j].fields.any_thread   = 1;
			coreData[i].controlReg[j].fields.in_tx        = 0;
			coreData[i].controlReg[j].fields.in_txcp      = 0;
			coreData[i].controlReg[j].fields.enable       = 1;
		}

		// Fixed Counters
		coreData[i].fixedControlReg  = 0;
		// PMI | AnyThread | 00 - disabled, 01 - OS, 10 - USR, 11 - OS and USR
		uint64_t value = (0ULL << 3) + (0ULL << 2) + (1ULL << 1) + (0ULL << 0);
		for(j=0; j < __nfixedCounters; j++){
			coreData[i].fixedControlReg += value << (4*j);
		}
		__msrWrite(coreData[i].msrFD,IA32_FIXED_CTR_CTRL, coreData[i].fixedControlReg);
	}
}

void pmuShutdown(){

	if(__ncores == 0 || __ncustomCounters == 0){
		fprintf(stderr,"error: pmuStartup() was not called!\n");
		exit(EXIT_FAILURE);
	}

	int i;
	for(i=0; i < __ncores; i++){
		
		int j;
		// disable all counters
		__msrWrite(coreData[i].msrFD,IA32_PERF_GLOBAL_CTRL, 0);
		
		for(j=0; j < __ncustomCounters; j++){
			__msrWrite(coreData[i].msrFD, IA32_PERFEVTSEL0 + j, 0);
		}
		__msrWrite(coreData[i].msrFD,IA32_FIXED_CTR_CTRL, 0);
		free(coreData[i].controlReg);
		
		for(j=0; j < __nmeasurements; j++){
			free(coreData[i].measurements[j]);
		}
		free(coreData[i].measurements);
		__msrClose(coreData[i].msrFD);
	}
	free(coreData);
}

void pmuStartCounting(int coreId, int measurement_i){
	
	if(coreData == NULL){
		fprintf(stderr,"error:pmuStartCounting: pmuStartup() was not called!\n");
		exit(EXIT_FAILURE);
	}

	if(measurement_i == __nmeasurements){
		fprintf(stderr,"error:pmuStartCounting: maximum number of measurements reached!\n");
		exit(EXIT_FAILURE);
	}
	coreData[coreId].measurement_i = measurement_i;
	int i;
	for(i=0; i < __ncustomCounters; i++){
		__msrWrite(coreData[coreId].msrFD, IA32_PMC0 + i, 0);
	}

	for(i=0; i < __nfixedCounters; i++){
		__msrWrite(coreData[coreId].msrFD, IA32_FIXED_CTR0 + i, 0);
	}
	// start counting, enable all 4 programmable and 3 fixed counters
	uint64_t value = (1ULL << 0) + (1ULL << 1) + (1ULL << 2) + (1ULL << 3) + (1ULL << 32) + (1ULL << 33) + (1ULL << 34);
	__msrWrite(coreData[coreId].msrFD, IA32_PERF_GLOBAL_CTRL, value);
}

void pmuStopCounting(int coreId){
	
	if(coreData == NULL){
		fprintf(stderr,"error:pmuStopCounting: pmuStartup() was not called!\n");
		exit(EXIT_FAILURE);
	}
	
	// stop counting, disable all counters
	__msrWrite(coreData[coreId].msrFD, IA32_PERF_GLOBAL_CTRL, 0);
	int i, measurement_i = coreData[coreId].measurement_i;
	
	for(i=0; i < __ncustomCounters; i++){
		coreData[coreId].measurements[measurement_i][i] += __msrRead(coreData[coreId].msrFD, IA32_PMC0 + i);
	}
	int j = __ncustomCounters;
	for(i=0; i < __nfixedCounters; i++,j++){
		coreData[coreId].measurements[measurement_i][j] += __msrRead(coreData[coreId].msrFD, IA32_FIXED_CTR0 + i);
	}
}

int pmuNumberOfCustomCounters(){
	return __ncustomCounters;
}

int pmuNumberOfFixedCounters(){
	return __nfixedCounters;
}

int pmuNumberOfMeasurements(){
	return __nmeasurements;
}

int pmuNumberOfOnlineCores(){
	return __ncores;
}

int pmuAddCustomCounter(int counterSlotIdx, int coreEventId){

	if(coreData == NULL){
		fprintf(stderr,"error:pmuAddCounter: pmuStartup() was not called!\n");
		exit(EXIT_FAILURE);
	}

	if(counterSlotIdx >  __ncustomCounters || counterSlotIdx < 0){
		fprintf(stderr,"error: no counter slot available (max. counters = %d)\n",__ncustomCounters);
		return -1;
	}
	
	uint64_t event_select = coreEventTable[coreEventId].event_select;
	uint64_t umask = coreEventTable[coreEventId].umask;

	int i;
	for(i=0; i < __ncores; i++){
		coreData[i].controlReg[counterSlotIdx].fields.event_select = event_select;
		coreData[i].controlReg[counterSlotIdx].fields.umask = umask;
		__msrWrite(coreData[i].msrFD, IA32_PERFEVTSEL0 + counterSlotIdx, coreData[i].controlReg[counterSlotIdx].value);
	}

	return 0;
}

uint64_t **pmuGetMeasurements(int coreId){
	if( coreId < __ncores)
		return coreData[coreId].measurements;
	else return NULL;
}
 
static void getNumberOfPerfCounters(int *custom, int *fixed){
	
	uint32_t eax;
	uint32_t edx;

	asm ("\txor %%ecx,%%ecx\n"
			 "\tmov $0x0A,%%eax\n"
			 "\tcpuid\n"
	     "\tmov %%eax,%0\n"
	     "\tmov %%edx,%1\n"
			 : "=r" (eax), "=r" (edx) // Output operands
			 : // Input operands
			 : "eax", "ebx", "ecx", "edx"); // Restore operands
	
	//int pmuVersionNumber = (eax >> 0) & 0xEF;
	//int pmuCustomCounterWidth = (eax >> 16) & 0xFF;
	//int pmuFixedCounterWidth = (edx >> 5) & 0xFF;
	(*custom) = (eax >> 8) & 0xFF;
	(*fixed)  = (edx >> 0) & 0x0F;
}
