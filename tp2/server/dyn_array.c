#include "dyn_array.h"

#include <errno.h>
#include <malloc.h>

struct array_t *new_array (size_t capacity) {
  struct array_t *new = malloc(sizeof(*new));
  if (!new) {
    errno = ENOMEM;
    return NULL;
  }

  new->capacity = capacity;
  new->size = 0;

  new->data = malloc(capacity*sizeof(void*));
  if(!new->data) {
    free(new);
    new = NULL;
  }
  return new;
}

void for_each(struct array_t *array, void (*callback)(void*)) {
  for (int i = 0; i < array->size; ++i) {
    callback(array->data[i]);
  }
}

int push_back(struct array_t *array, void *element) {
  if (array->size > (array->capacity - 2)) {
    size_t newsize = array->capacity << 1;
    void *tmp = realloc(array->data, newsize*sizeof(void*));
    if (!tmp) {
      return -1;
    }
    array->capacity = newsize;
    array->data = tmp;
  }
  array->data[array->size] = element;
  array->size++;
  return 0; 
}

void delete_array (struct array_t **array) {
  if(array) {
    struct array_t *ptr; 
    if ((ptr = *array)) {
      free(ptr->data);
      free(ptr);
    }
    *array = NULL;
  }
}

void delete_array_callback(struct array_t **array, void (*callback)(void*)) {
  if (array) {
    struct array_t *ptr;
    if ((ptr = *array)) {
      if (ptr->data) {

        for(int i = 0; i < ptr->size; i++) {
		  if(ptr->data[i] != NULL) {
            callback(ptr->data[i]);
		  }
        }
        free(ptr->data);
      }
      free(ptr);
    }
    *array = NULL;
  }
}

