#ifndef COMMON_H
#define COMMON_H

#include <limits.h>

#include <tm.h>

#ifdef __cplusplus
extern "C" {
#endif

# define VAL_MIN                        INT_MIN
# define VAL_MAX                        INT_MAX
typedef intptr_t val_t;

typedef struct thread_data {
	long threadId;
	struct llistset *small_llistset;
	struct llistset *large_llistset;
  struct barrier *barrier;
  unsigned long nb_add;
  unsigned long nb_remove;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned short seed[3];
  int diff;
  int small_range;
  int large_range;
  int update;
  int alternate;
  char padding[64];
} thread_data_t;

#define RO                              1
#define RW                              0

/* Annotations used in this benchmark */
# define TM_SAFE
# define TM_PURE

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */
