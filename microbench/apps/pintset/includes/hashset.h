#ifndef HASHSET_H
#define HASHSET_H

#include <common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hashset hashset_t;

hashset_t *hashset_new();
void hashset_delete(hashset_t *set);
int hashset_size(hashset_t *set);

int hashset_contains(hashset_t *set, val_t val, thread_data_t *td);
int hashset_add(hashset_t *set, val_t val, thread_data_t *td);
int hashset_remove(hashset_t *set, val_t val, thread_data_t *td);

#ifdef __cplusplus
}
#endif

#endif /* HASHSET_H */
