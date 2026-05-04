#include "stdlib.h"
#include "bios.h"

void abort(void) {
    /* For now, just halt the CPU */
    cpu_core_halt();
    while(1);
}

/* Stubs for now, need a proper heap allocator later */
void* malloc(size_t size) { (void)size; return NULL; }
void free(void* ptr) { (void)ptr; }
void* aligned_alloc(size_t alignment, size_t size) { 
    (void)alignment; (void)size; return NULL; 
}
