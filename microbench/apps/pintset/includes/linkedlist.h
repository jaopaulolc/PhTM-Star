#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct llistset llistset_t;

llistset_t *llistset_new();
void llistset_delete(llistset_t *set);
int llistset_size(llistset_t *set);

int llistset_contains(llistset_t *set, val_t val, thread_data_t *td);
int llistset_add(llistset_t *set, val_t val, thread_data_t *td);
int llistset_remove(llistset_t *set, val_t val, thread_data_t *td);

#ifdef __cplusplus
}
#endif

#endif /* LINKEDLIST_H */
