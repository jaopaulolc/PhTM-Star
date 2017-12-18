#ifndef COMMON_H
#define COMMON_H

#include <limits.h>
#include <stdint.h>

#include <tm.h>

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define CACHE_LINE_SIZE 128
#else /* Haswell */
#define CACHE_LINE_SIZE  64
#endif /* Haswell*/

#ifndef __ALIGN__

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define __CACHE_ALIGNMENT__ 0x10000
#endif

#if defined(__x86_64__) || defined(__i386)
#define __CACHE_ALIGNMENT__ 0x1000
#endif

#define __ALIGN__ __attribute__ ((aligned (__CACHE_ALIGNMENT__)))

#endif

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#ifdef __cplusplus
extern "C" {
#endif

# define VAL_MIN                        INT_MIN
# define VAL_MAX                        INT_MAX
typedef intptr_t val_t;

typedef struct forest forest_t;

typedef struct thread_data {
	long threadId;
  struct barrier *barrier;
  unsigned short seed[3];
	forest_t* forest;
  int initial;
  int update;
	int duration;
  int range;
  unsigned long nb_add;
  unsigned long nb_remove;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned long diff;
	uint64_t sampleCount;
	uint64_t maxSamples;
	double *throughputSamples;
  char padding[CACHE_LINE_SIZE];
} __ALIGN__ thread_data_t;

#define RO                              1
#define RW                              0

/* Annotations used in this benchmark */
# define TM_SAFE
# define TM_PURE

void rand_init(unsigned short *seed);
int rand_range(int n, unsigned short *seed);
int rand_range_non_zero(int n, unsigned short *seed);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */
