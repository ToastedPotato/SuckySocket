#ifndef _DYN_ARRAY_H
#define _DYN_ARRAY_H

#include <stddef.h>

struct array_t *new_array (size_t capacity);
void for_each(struct array_t *array, void (*callback)(void*));
int push_back(struct array_t *array, void *element);
void delete_array_callback(struct array_t **array, void (*callback)(void*));
void delete_array (struct array_t **array);

#endif
