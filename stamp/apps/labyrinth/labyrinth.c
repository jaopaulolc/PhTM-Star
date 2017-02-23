#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "list.h"
#include "maze.h"
#include "router.h"
#include "thread.h"
#include "timer.h"
#include "types.h"

#define MAIN_FUNCTION_FILE 1
#include "tm.h"

enum param_types {
    PARAM_BENDCOST = (unsigned char)'b',
    PARAM_THREAD   = (unsigned char)'t',
    PARAM_XCOST    = (unsigned char)'x',
    PARAM_YCOST    = (unsigned char)'y',
    PARAM_ZCOST    = (unsigned char)'z'
};

enum param_defaults {
    PARAM_DEFAULT_BENDCOST = 1,
    PARAM_DEFAULT_THREAD   = 1,
    PARAM_DEFAULT_XCOST    = 1,
    PARAM_DEFAULT_YCOST    = 1,
    PARAM_DEFAULT_ZCOST    = 2
};

bool_t global_doPrint = FALSE;
char* global_inputFile = "../../data/labyrinth/inputs/random-x512-y512-z7-n512.txt";
long global_params[256]; /* 256 = ascii limit */


/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                            (defaults)\n");
    printf("    b <INT>    [b]end cost          (%i)\n", PARAM_DEFAULT_BENDCOST);
    printf("    i <FILE>   [i]nput file name    (%s)\n", global_inputFile);
    printf("    p          [p]rint routed maze  (false)\n");
    printf("    t <UINT>   Number of [t]hreads  (%i)\n", PARAM_DEFAULT_THREAD);
    printf("    x <UINT>   [x] movement cost    (%i)\n", PARAM_DEFAULT_XCOST);
    printf("    y <UINT>   [y] movement cost    (%i)\n", PARAM_DEFAULT_YCOST);
    printf("    z <UINT>   [z] movement cost    (%i)\n", PARAM_DEFAULT_ZCOST);
    exit(1);
}


/* =============================================================================
 * setDefaultParams
 * =============================================================================
 */
static void
setDefaultParams ()
{
    global_params[PARAM_BENDCOST] = PARAM_DEFAULT_BENDCOST;
    global_params[PARAM_THREAD]   = PARAM_DEFAULT_THREAD;
    global_params[PARAM_XCOST]    = PARAM_DEFAULT_XCOST;
    global_params[PARAM_YCOST]    = PARAM_DEFAULT_YCOST;
    global_params[PARAM_ZCOST]    = PARAM_DEFAULT_ZCOST;
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

    setDefaultParams();

    while ((opt = getopt(argc, argv, "b:i:pt:x:y:z:")) != -1) {
        switch (opt) {
            case 'b':
            case 't':
            case 'x':
            case 'y':
            case 'z':
                global_params[(unsigned char)opt] = atol(optarg);
                break;
            case 'i':
                global_inputFile = optarg;
                break;
            case 'p':
                global_doPrint = TRUE;
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
 * main
 * =============================================================================
 */
MAIN(argc, argv)
{
    GOTO_REAL();

    /*
     * Initialization
     */
#if defined(__x86_64__) || defined(__i386)
		unsigned int counterBefore,counterAfter;
#endif /* Intel RAPL */

    parseArgs(argc, (char** const)argv);
    long numThread = global_params[PARAM_THREAD];
    SIM_GET_NUM_CPU(numThread);
    TM_STARTUP(numThread);
    P_MEMORY_STARTUP(numThread);
    thread_startup(numThread);
		
		maze_t* mazePtr = maze_alloc();
    assert(mazePtr);
    long numPathToRoute = maze_read(mazePtr, global_inputFile);
    router_t* routerPtr = router_alloc(global_params[PARAM_XCOST],
                                       global_params[PARAM_YCOST],
                                       global_params[PARAM_ZCOST],
                                       global_params[PARAM_BENDCOST]);
    assert(routerPtr);
    list_t* pathVectorListPtr = list_alloc(NULL);
    assert(pathVectorListPtr);

    /*
     * Run transactions
     */
    router_solve_arg_t routerArg = {routerPtr, mazePtr, pathVectorListPtr};
    TIMER_T startTime;
#if defined(__x86_64__) || defined(__i386)
		counterBefore = msrGetCounter();
#endif /* Intel RAPL */
    TIMER_READ(startTime);
    GOTO_SIM();
#ifdef OTM
#pragma omp parallel
    {
        router_solve((void *)&routerArg);
    }
#else
    thread_start(router_solve, (void*)&routerArg);
#endif
    GOTO_REAL();
    TIMER_T stopTime;
    TIMER_READ(stopTime);
#if defined(__x86_64__) || defined(__i386)
		counterAfter = msrGetCounter();
#endif /* Intel RAPL */

    long numPathRouted = 0;
    list_iter_t it;
    list_iter_reset(&it, pathVectorListPtr);
    while (list_iter_hasNext(&it, pathVectorListPtr)) {
        vector_t* pathVectorPtr = (vector_t*)list_iter_next(&it, pathVectorListPtr);
        numPathRouted += vector_getSize(pathVectorPtr);
    }
    printf("Paths routed    = %li\n", numPathRouted);
    printf("Time            = %f\n", TIMER_DIFF_SECONDS(startTime, stopTime));

    /*
     * Check solution and clean up
     */
    assert(numPathRouted <= numPathToRoute);
    bool_t status = maze_checkPaths(mazePtr, pathVectorListPtr, global_doPrint);
    assert(status == TRUE);
    puts("Verification passed.");
    maze_free(mazePtr);
    router_free(routerPtr);

    list_iter_reset(&it, pathVectorListPtr);
    while (list_iter_hasNext(&it, pathVectorListPtr)) {
        vector_t* pathVectorPtr = (vector_t*)list_iter_next(&it, pathVectorListPtr);
        vector_t *pointVectorPtr;
        while ((pointVectorPtr = (vector_t *)vector_popBack (pathVectorPtr)) != NULL) {
            PVECTOR_FREE(pointVectorPtr);
        }
        PVECTOR_FREE(pathVectorPtr);
    }
    list_free(pathVectorListPtr);
		
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
 * End of labyrinth.c
 *
 * =============================================================================
 */
