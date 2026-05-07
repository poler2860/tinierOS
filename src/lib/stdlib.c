#include "stdlib.h"

void abort(void) {
    while(1);
}

/* Stubs - should not be called in static allocation mode */
void* malloc(size_t size) { (void)size; return (void*)0; }
void free(void* ptr) { (void)ptr; }
void* aligned_alloc(size_t alignment, size_t size) {
    (void)alignment; (void)size; return (void*)0;
}
