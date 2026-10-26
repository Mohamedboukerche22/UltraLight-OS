#ifndef UL_HEAP_H
#define UL_HEAP_H
#include "types.h"
void  heap_init(void);
void *kmalloc(size_t n);
void *kcalloc(size_t n, size_t sz);
void *krealloc(void *p, size_t n);
void  kfree(void *p);
typedef struct { size_t used, free, blocks, heap_size; } heap_stats_t;
void  heap_stats(heap_stats_t *out);
#endif
