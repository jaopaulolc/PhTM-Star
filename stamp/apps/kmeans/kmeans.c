#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "cluster.h"
#include "normal.h"
#include "common.h"
#include "thread.h"

#define MAIN_FUNCTION_FILE 1
#include "tm.h"
#include "util.h"

#define MAX_LINE_LENGTH 1000000 /* max input is 400000 one digit input + spaces */


/* =============================================================================
 * usage
 * =============================================================================
 */
static void
usage (char* argv0)
{
    char* help =
        "Usage: %s [switches] -i filename\n"
        "       -i filename:     file containing data to be clustered\n"
        "       -b               input file is in binary format\n"
        "       -m max_clusters: maximum number of clusters allowed\n"
        "       -n min_clusters: minimum number of clusters allowed\n"
        "       -z             : don't zscore transform data\n"
        "       -T threshold   : threshold value\n"
        "       -t nproc       : number of threads\n";
    fprintf(stderr, help, argv0);
    exit(-1);
}


/* =============================================================================
 * main
 * =============================================================================
 */
MAIN(argc, argv)
{
    int     max_nclusters = 15;
    int     min_nclusters = 15;
    char*   filename = "../../data/kmeans/inputs/random-n65536-d32-c16.txt";
    float*  buf;
    float** attributes;
    float** cluster_centres = NULL;
    int     i;
    int     j;
    int     best_nclusters;
    int*    cluster_assign;
    int     numAttributes;
    int     numObjects;
    int     use_zscore_transform = 1;
    char*   line;
    int     isBinaryFile = 0;
    int     nloops;
    /* int     len; */
    int     nthreads;
    float   threshold = 0.00001;
    int     opt;
		
#if defined(__x86_64__) || defined(__i386)
		unsigned int counterBefore,counterAfter;
#endif /* Intel RAPL */

    GOTO_REAL();

    line = (char*)SEQ_MALLOC(MAX_LINE_LENGTH); /* reserve memory line */

    nthreads = 1;
    while ((opt = getopt(argc,(char**)argv,"t:i:m:n:T:bzL")) != EOF) {
        switch (opt) {
            case 'i': filename = optarg;
                      break;
            case 'b': isBinaryFile = 1;
                      break;
            case 'T': threshold = atof(optarg);
                      break;
            case 'm': max_nclusters = atoi(optarg);
                      break;
            case 'n': min_nclusters = atoi(optarg);
                      break;
            case 'z': use_zscore_transform = 0;
                      break;
            case 'L': max_nclusters = min_nclusters = 40;
                      break;
            case 't': nthreads = atoi(optarg);
                      break;
            case '?': usage((char*)argv[0]);
                      break;
            default: usage((char*)argv[0]);
                      break;
        }
    }

    if (filename == 0) {
        usage((char*)argv[0]);
    }

    if (max_nclusters < min_nclusters) {
        fprintf(stderr, "Error: max_clusters must be >= min_clusters\n");
        usage((char*)argv[0]);
    }

    SIM_GET_NUM_CPU(nthreads);

    numAttributes = 0;
    numObjects = 0;

    /*
     * From the input file, get the numAttributes and numObjects
     */
    if (isBinaryFile) {
        int infile;
        if ((infile = open(filename, O_RDONLY, "0600")) == -1) {
            fprintf(stderr, "Error: no such file (%s)\n", filename);
            exit(1);
        }
        read(infile, &numObjects, sizeof(int));
        read(infile, &numAttributes, sizeof(int));

        /* Allocate space for attributes[] and read attributes of all objects */
        buf = (float*)SEQ_MALLOC(numObjects * numAttributes * sizeof(float));
        assert(buf);
        attributes = (float**)SEQ_MALLOC(numObjects * sizeof(float*));
        assert(attributes);
        attributes[0] = (float*)SEQ_MALLOC(numObjects * numAttributes * sizeof(float));
        assert(attributes[0]);
        for (i = 1; i < numObjects; i++) {
            attributes[i] = attributes[i-1] + numAttributes;
        }
        read(infile, buf, (numObjects * numAttributes * sizeof(float)));
        close(infile);
    } else {
        FILE *infile;
        if ((infile = fopen(filename, "r")) == NULL) {
            fprintf(stderr, "Error: no such file (%s)\n", filename);
            exit(1);
        }
        while (fgets(line, MAX_LINE_LENGTH, infile) != NULL) {
            if (strtok(line, " \t\n") != 0) {
                numObjects++;
            }
        }
        rewind(infile);
        while (fgets(line, MAX_LINE_LENGTH, infile) != NULL) {
            if (strtok(line, " \t\n") != 0) {
                /* Ignore the id (first attribute): numAttributes = 1; */
                while (strtok(NULL, " ,\t\n") != NULL) {
                    numAttributes++;
                }
                break;
            }
        }

        /* Allocate space for attributes[] and read attributes of all objects */
        buf = (float*)SEQ_MALLOC(numObjects * numAttributes * sizeof(float));
        assert(buf);
        attributes = (float**)SEQ_MALLOC(numObjects * sizeof(float*));
        assert(attributes);
        attributes[0] = (float*)SEQ_MALLOC(numObjects * numAttributes * sizeof(float));
        assert(attributes[0]);
        for (i = 1; i < numObjects; i++) {
            attributes[i] = attributes[i-1] + numAttributes;
        }
        rewind(infile);
        i = 0;
        while (fgets(line, MAX_LINE_LENGTH, infile) != NULL) {
            if (strtok(line, " \t\n") == NULL) {
                continue;
            }
            for (j = 0; j < numAttributes; j++) {
                buf[i] = atof(strtok(NULL, " ,\t\n"));
                i++;
            }
        }
        fclose(infile);
    }
    SEQ_FREE(line);

    TM_STARTUP(nthreads);
    thread_startup(nthreads);

    /*
     * The core of the clustering
     */
    cluster_assign = (int*)SEQ_MALLOC(numObjects * sizeof(int));
    assert(cluster_assign);
		
#if defined(__x86_64__) || defined(__i386)
		counterBefore = msrGetCounter();
#endif /* Intel RAPL */

    nloops = 1;
    /* len = max_nclusters - min_nclusters + 1; */

    for (i = 0; i < nloops; i++) {
        /*
         * Since zscore transform may perform in cluster() which modifies the
         * contents of attributes[][], we need to re-store the originals
         */
        memcpy(attributes[0], buf, (numObjects * numAttributes * sizeof(float)));

        cluster_centres = NULL;
        cluster_exec(nthreads,
                     numObjects,
                     numAttributes,
                     attributes,           /* [numObjects][numAttributes] */
                     use_zscore_transform, /* 0 or 1 */
                     min_nclusters,        /* pre-define range from min to max */
                     max_nclusters,
                     threshold,
                     &best_nclusters,      /* return: number between min and max */
                     &cluster_centres,     /* return: [best_nclusters][numAttributes] */
                     cluster_assign);      /* return: [numObjects] cluster id for each object */

    }

#if defined(__x86_64__) || defined(__i386)
		counterAfter = msrGetCounter();
#endif /* Intel RAPL */

#ifdef GNUPLOT_OUTPUT
    {
        FILE** fptr;
        char outFileName[1024];
        fptr = (FILE**)SEQ_MALLOC(best_nclusters * sizeof(FILE*));
        for (i = 0; i < best_nclusters; i++) {
            sprintf(outFileName, "group.%d", i);
            fptr[i] = fopen(outFileName, "w");
        }
        for (i = 0; i < numObjects; i++) {
            fprintf(fptr[cluster_assign[i]],
                    "%6.4f %6.4f\n",
                    attributes[i][0],
                    attributes[i][1]);
        }
        for (i = 0; i < best_nclusters; i++) {
            fclose(fptr[i]);
        }
        SEQ_FREE(fptr);
    }
#endif /* GNUPLOT_OUTPUT */

#ifdef OUTPUT_TO_FILE
    {
        /* Output: the coordinates of the cluster centres */
        FILE* cluster_centre_file;
        FILE* clustering_file;
        char outFileName[1024];

        sprintf(outFileName, "%s.cluster_centres", filename);
        cluster_centre_file = fopen(outFileName, "w");
        for (i = 0; i < best_nclusters; i++) {
            fprintf(cluster_centre_file, "%d ", i);
            for (j = 0; j < numAttributes; j++) {
                fprintf(cluster_centre_file, "%f ", cluster_centres[i][j]);
            }
            fprintf(cluster_centre_file, "\n");
        }
        fclose(cluster_centre_file);

        /* Output: the closest cluster centre to each of the data points */
        sprintf(outFileName, "%s.cluster_assign", filename);
        clustering_file = fopen(outFileName, "w");
        for (i = 0; i < numObjects; i++) {
            fprintf(clustering_file, "%d %d\n", i, cluster_assign[i]);
        }
        fclose(clustering_file);
    }
#endif /* OUTPUT_TO_FILE */

#ifdef OUTPUT_TO_STDOUT
    {
        /* Output: the coordinates of the cluster centres */
        for (i = 0; i < best_nclusters; i++) {
            printf("%d ", i);
            for (j = 0; j < numAttributes; j++) {
                printf("%f ", cluster_centres[i][j]);
            }
            printf("\n");
        }
    }
#endif /* OUTPUT_TO_STDOUT */

    printf("Time = %lg\n", global_time);

    SEQ_FREE(cluster_assign);
    SEQ_FREE(attributes[0]);
    SEQ_FREE(attributes);
    SEQ_FREE(cluster_centres[0]);
    SEQ_FREE(cluster_centres);
    SEQ_FREE(buf);
		
#if defined(__x86_64__) || defined(__i386)
		printf("\nEnergy = %lf J\n",msrDiffCounter(counterBefore,counterAfter));
#endif /* Intel RAPL */

    TM_SHUTDOWN();
    GOTO_SIM();
    thread_shutdown();

    MAIN_RETURN(0);
}


/* =============================================================================
 *
 * End of kmeans.c
 *
 * =============================================================================
 */
