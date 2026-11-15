/* stdio.h —entrada/salida estandar de FOC. */
#ifndef _FOC_STDIO_H
#define _FOC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef unsigned long int FOCStdioFile;
typedef FOCStdioFile FILE;

#define stdin  ((FILE *)1)
#define stdout ((FILE *)2)
#define stderr ((FILE *)3)

int fgetc(FILE *stream);
int fputc(int character, FILE *stream);
int fputs(const char *str, FILE *stream);
int puts(const char *str);
int printf(const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsprintf(char *str, const char *format, va_list args);
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int fflush(FILE *stream);
int fclose(FILE *stream);
int fseek(FILE *stream, long offset, int origin);
long ftell(FILE *stream);
void rewind(FILE *stream);

#endif /* _FOC_STDIO_H */
