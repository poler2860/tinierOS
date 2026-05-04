#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef void* FILE;
extern FILE* stderr;

int fprintf(FILE* stream, const char* format, ...);

#endif
