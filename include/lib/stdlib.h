#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

void abort(void);

/* malloc/free will need a kernel allocator implementation */
void* malloc(size_t size);
void free(void* ptr);
void* aligned_alloc(size_t alignment, size_t size);

#endif
