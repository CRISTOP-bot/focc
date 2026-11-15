/* Ejecutor de las pruebas unitarias de FOC. */
#include "test.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int foc_test_failures = 0;
int foc_test_checks = 0;
const char *foc_test_current = "";

void foc_test_begin(const char *name) {
    foc_test_current = name;
    printf("== %s\n", name);
}

void foc_test_fail(const char *file, int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "  FALLO %s:%d [%s] ", file, line, foc_test_current);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    foc_test_failures++;
}

int foc_test_summary(void) {
    printf("\n%d comprobaciones, %d fallos\n", foc_test_checks, foc_test_failures);
    return foc_test_failures == 0 ? 0 : 1;
}

int main(void) {
    test_lexer();
    test_parser();
    test_types();
    test_preproc();
    test_ir();
    test_backend();
    return foc_test_summary();
}
