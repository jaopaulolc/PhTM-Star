#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "region.h"
#include "list.h"
#include "mesh.h"
#include "heap.h"
#include "thread.h"
#include "timer.h"
#include "tm.h"

#ifdef STM
#ifdef COMMIT_RATE_PROFILING
unsigned int **coreSTM_commits;
unsigned int **coreSTM_aborts;
#endif /* COMMIT_RATE_PROFILING */
#ifdef RW_SET_PROFILING
unsigned int ***coreSTM_r_set_size;
unsigned int ***coreSTM_w_set_size;
unsigned int **coreSTM_counter;
#endif /* RW_SET_PROFILING */
#endif /* STM */

#define PARAM_DEFAULT_INPUTPREFIX ("../../data/yada/inputs/ttimeu1000000.2")
#define PARAM_DEFAULT_NUMTHREAD   (1L)
#define PARAM_DEFAULT_ANGLE       (15.0)


char*    global_inputPrefix     = PARAM_DEFAULT_INPUTPREFIX;
long     global_numThread       = PARAM_DEFAULT_NUMTHREAD;
double   global_angleConstraint = PARAM_DEFAULT_ANGLE;
mesh_t*  global_meshPtr;
heap_t*  global_workHeapPtr;
long     global_totalNumAdded = 0;
long     global_numProcess    = 0;


/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                              (defaults)\n");
    printf("    a <FLT>   Min [a]ngle constraint  (%lf)\n", PARAM_DEFAULT_ANGLE);
    printf("    i <STR>   [i]nput name prefix     (%s)\n",  PARAM_DEFAULT_INPUTPREFIX);
    printf("    t <UINT>  Number of [t]hreads     (%li)\n", PARAM_DEFAULT_NUMTHREAD);
    exit(1);
}


/* =============================================================================
 * parseArgs
 * =============================================================================
 */
static void
parseArgs (long argc, char* const argv[])
{
    long i;
    long opt;

    opterr = 0;

    while ((opt = getopt(argc, argv, "a:i:t:")) != -1) {
        switch (opt) {
            case 'a':
                global_angleConstraint = atof(optarg);
                break;
            case 'i':
                global_inputPrefix = optarg;
                break;
            case 't':
                global_numThread = atol(optarg);
                break;
            case '?':
            default:
                opterr++;
                break;
        }
    }

    for (i = optind; i < argc; i++) {
        fprintf(stderr, "Non-option argument: %s\n", argv[i]);
        opterr++;
    }

    if (opterr) {
        displayUsage(argv[0]);
    }
}


/* =============================================================================
 * initializeWork
 * =============================================================================
 */
static long
initializeWork (heap_t* workHeapPtr, mesh_t* meshPtr)
{
    random_t* randomPtr = random_alloc();
    random_seed(randomPtr, 0);
    mesh_shuffleBad(meshPtr, randomPtr);
    random_free(randomPtr);

    long numBad = 0;

    while (1) {
        element_t* elementPtr = mesh_getBad(meshPtr);
        if (!elementPtr) {
            break;
        }
        numBad++;
        bool_t status = heap_insert(workHeapPtr, (void*)elementPtr);
        assert(status);
        element_setIsReferenced(elementPtr, TRUE);
    }

    return numBad;
}


/* =============================================================================
 * process
 * =============================================================================
 */
static void
process (void *arg)
{
    TM_THREAD_ENTER();

    heap_t* workHeapPtr = global_workHeapPtr;
    mesh_t* meshPtr = global_meshPtr;
    region_t* regionPtr;
    long totalNumAdded = 0;
    long numProcess = 0;

    regionPtr = PREGION_ALLOC();
    assert(regionPtr);

    while (1) {

        element_t* elementPtr;
	#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
        elementPtr = (element_t*)heap_remove(workHeapPtr);
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(RW)
	#else /* !HW_SW_PATHS */
      TM_BEGIN();
	#endif /* !HW_SW_PATHS */
        elementPtr = (element_t*)TMHEAP_REMOVE(workHeapPtr);
	#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
	#else /* !HW_SW_PATHS */
      TM_END();
	#endif /* !HW_SW_PATHS */
        if (elementPtr == NULL) {
            break;
        }

        bool_t isGarbage;
	#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
        isGarbage = element_isGarbage(elementPtr);
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(RW)
	#else /* !HW_SW_PATHS */
      TM_BEGIN();
	#endif /* !HW_SW_PATHS */
        isGarbage = TMELEMENT_ISGARBAGE(elementPtr);
	#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
	#else /* !HW_SW_PATHS */
      TM_END();
	#endif /* !HW_SW_PATHS */
        if (isGarbage) {
            /*
             * Handle delayed deallocation
             */
            PELEMENT_FREE(elementPtr);
            continue;
        }
				

        long numAdded;
	#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
        PREGION_CLEARBAD(regionPtr);
        numAdded = region_refine(regionPtr, elementPtr, meshPtr);
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(RW)
	#else /* !HW_SW_PATHS */
      TM_BEGIN();
	#endif /* !HW_SW_PATHS */
        PREGION_CLEARBAD(regionPtr);
        numAdded = TMREGION_REFINE(regionPtr, elementPtr, meshPtr);
	#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
	#else /* !HW_SW_PATHS */
      TM_END();
	#endif /* !HW_SW_PATHS */

	#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
        element_setIsReferenced(elementPtr, FALSE);
        isGarbage = element_isGarbage(elementPtr);
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(RW)
	#else /* !HW_SW_PATHS */
      TM_BEGIN();
	#endif /* !HW_SW_PATHS */
        TMELEMENT_SETISREFERENCED(elementPtr, FALSE);
        isGarbage = TMELEMENT_ISGARBAGE(elementPtr);
	#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
	#else /* !HW_SW_PATHS */
      TM_END();
	#endif /* !HW_SW_PATHS */
        if (isGarbage) {
            /*
             * Handle delayed deallocation
             */
            PELEMENT_FREE(elementPtr);
        }

        totalNumAdded += numAdded;

	#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
        region_transferBad(regionPtr, workHeapPtr);
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(RW)
	#else /* !HW_SW_PATHS */
      TM_BEGIN();
	#endif /* !HW_SW_PATHS */
        TMREGION_TRANSFERBAD(regionPtr, workHeapPtr);
	#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
	#else /* !HW_SW_PATHS */
      TM_END();
	#endif /* !HW_SW_PATHS */

        numProcess++;

    }

#ifdef HW_SW_PATHS
IF_HTM_MODE
	START_HTM_MODE
    global_totalNumAdded =
					global_totalNumAdded + totalNumAdded;
    global_numProcess =
					global_numProcess + numProcess;
	COMMIT_HTM_MODE
ELSE_STM_MODE
	START_STM_MODE(RW)
#else /* !HW_SW_PATHS */
      TM_BEGIN();
#endif /* !HW_SW_PATHS */
    TM_SHARED_WRITE(global_totalNumAdded,
                    TM_SHARED_READ(global_totalNumAdded) + totalNumAdded);
    TM_SHARED_WRITE(global_numProcess,
                    TM_SHARED_READ(global_numProcess) + numProcess);
#ifdef HW_SW_PATHS
	COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
  TM_END();
#endif /* !HW_SW_PATHS */

    PREGION_FREE(regionPtr);

    TM_THREAD_EXIT();
}


/* =============================================================================
 * main
 * =============================================================================
 */
MAIN(argc, argv)
{

#if defined(__x86_64__) || defined(__i386)
		unsigned int counterBefore,counterAfter;
#endif /* Intel RAPL */

    GOTO_REAL();

    /*
     * Initialization
     */

    parseArgs(argc, (char** const)argv);
    SIM_GET_NUM_CPU(global_numThread);
    TM_STARTUP(global_numThread);
    P_MEMORY_STARTUP(global_numThread);
    thread_startup(global_numThread);
		
    global_meshPtr = mesh_alloc();
    assert(global_meshPtr);
    printf("Angle constraint = %lf\n", global_angleConstraint);
    printf("Reading input... ");
    long initNumElement = mesh_read(global_meshPtr, global_inputPrefix);
    puts("done.");
    global_workHeapPtr = heap_alloc(1, &element_heapCompare);
    assert(global_workHeapPtr);
    long initNumBadElement = initializeWork(global_workHeapPtr, global_meshPtr);

    printf("Initial number of mesh elements = %li\n", initNumElement);
    printf("Initial number of bad elements  = %li\n", initNumBadElement);
    printf("Starting triangulation...");
    fflush(stdout);

    /*
     * Run benchmark
     */

    TIMER_T start;
#if defined(__x86_64__) || defined(__i386)
		counterBefore = msrGetCounter();
#endif /* Intel RAPL */
    TIMER_READ(start);
    GOTO_SIM();
#ifdef OTM
#pragma omp parallel
    {
        process();
    }
#else
    thread_start(process, NULL);
#endif
    GOTO_REAL();
    TIMER_T stop;
    TIMER_READ(stop);
#if defined(__x86_64__) || defined(__i386)
		counterAfter = msrGetCounter();
#endif /* Intel RAPL */

    puts(" done.");
    printf("Time                            = %0.3lf\n",
           TIMER_DIFF_SECONDS(start, stop));
    fflush(stdout);

    /*
     * Check solution
     */

    long finalNumElement = initNumElement + global_totalNumAdded;
    printf("Final mesh size                 = %li\n", finalNumElement);
    printf("Number of elements processed    = %li\n", global_numProcess);
    fflush(stdout);

#if 1
    bool_t isSuccess = mesh_check(global_meshPtr, finalNumElement);
#else
    bool_t isSuccess = TRUE;
#endif
    printf("Final mesh is %s\n", (isSuccess ? "valid." : "INVALID!"));
    fflush(stdout);
    assert(isSuccess);

    /*
     * TODO: deallocate mesh and work heap
     */

#if defined(__x86_64__) || defined(__i386)
		printf("\nEnergy = %lf J\n",msrDiffCounter(counterBefore,counterAfter));
#endif /* Intel RAPL */

    TM_SHUTDOWN();
    P_MEMORY_SHUTDOWN();

    GOTO_SIM();

    thread_shutdown();
		
    MAIN_RETURN(0);
}


/* =============================================================================
 *
 * End of ruppert.c
 *
 * =============================================================================
 */
