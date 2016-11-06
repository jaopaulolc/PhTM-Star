#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct slistset slistset_t;

slistset_t *slistset_new();
void slistset_delete(slistset_t *set);
int slistset_size(slistset_t *set);

int slistset_contains(slistset_t *set, val_t val, thread_data_t *td);
int slistset_add(slistset_t *set, val_t val, thread_data_t *td);
int slistset_remove(slistset_t *set, val_t val, thread_data_t *td);

#ifdef __cplusplus
}
#endif

#endif /* SKIPLIST_H */
