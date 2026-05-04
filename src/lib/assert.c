#include "assert.h"
#include "stdlib.h"

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    (void)assertion; (void)file; (void)line; (void)function;
    abort();
}
