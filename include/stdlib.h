/* stdlib.h —utilidades estandar de FOC. */
#ifndef _FOC_STDLIB_H
#define _FOC_STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
void abort(void);
void exit(int status);
long strtol(const char *str, char **end, int base);
unsigned long strtoul(const char *str, char **end, int base);
double strtod(const char *str, char **end);
int abs(int value);
long labs(long value);
int atoi(const char *str);
long atol(const char *str);

#endif /* _FOC_STDLIB_H */
