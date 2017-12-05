#ifndef RBFOREST_H
#define RBFOREST_H 1

#include <common.h>

typedef struct forest forest_t;

forest_t* forest_new(size_t nb_trees);
void forest_delete(forest_t* forest);
int forest_size(forest_t* forest);
size_t forest_nb_trees(forest_t* forest);

int forest_contains(forest_t* forest, val_t val, size_t k, thread_data_t* td);
int forest_add(forest_t* forest, val_t val, size_t k, thread_data_t* td);
int forest_remove(forest_t* forest, val_t val, size_t k, thread_data_t* td);

#endif /* RBFOREST_H */
