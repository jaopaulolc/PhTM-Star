#ifndef RBTREE_H
#define RBTREE_H 1

#include <common.h>

typedef struct rbtree rbtree_t;

rbtree_t *rbtreeset_new();
void rbtreeset_delete(rbtree_t *set);
int rbtreeset_size(rbtree_t *set);

int rbtreeset_contains(rbtree_t *set, val_t val, thread_data_t *td);
int rbtreeset_add(rbtree_t *set, val_t val, thread_data_t *td);
int rbtreeset_remove(rbtree_t *set, val_t val, thread_data_t *td);

#endif /* RBTREE_H */
