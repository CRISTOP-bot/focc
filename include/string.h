/* string.h -cadenas estandar de FOC. */
#ifndef _FOC_STRING_H
#define _FOC_STRING_H

#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t size);
void *memmove(void *dest, const void *src, size_t size);
void *memset(void *dest, int value, size_t size);
int memcmp(const void *a, const void *b, size_t size);
size_t strlen(const char *str);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t size);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t count);
char *strcat(char *dest, const char *src);
const char *strstr(const char *haystack, const char *needle);
char *strchr(const char *str, int character);

#endif /* _FOC_STRING_H */
