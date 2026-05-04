#include "stdio.h"

FILE* stderr = NULL;

int fprintf(FILE* stream, const char* format, ...) {
    (void)stream; (void)format;
    return 0;
}
