/* Marco minimo de pruebas unitarias de FOC. */
#ifndef FOC_TEST_H
#define FOC_TEST_H

#include <stdio.h>
#include <string.h>

extern int foc_test_failures;
extern int foc_test_checks;
extern const char *foc_test_current;

void foc_test_begin(const char *name);
void foc_test_fail(const char *file, int line, const char *format, ...);
int foc_test_summary(void);

#define CHECK(condition)                                                             \
    do {                                                                             \
        foc_test_checks++;                                                           \
        if (!(condition)) {                                                          \
            foc_test_fail(__FILE__, __LINE__, "se esperaba: %s", #condition);        \
        }                                                                            \
    } while (0)

#define CHECK_INT(actual, expected)                                                       \
    do {                                                                                  \
        long long actual_value = (long long)(actual);                                     \
        long long expected_value = (long long)(expected);                                 \
        foc_test_checks++;                                                                \
        if (actual_value != expected_value) {                                              \
            foc_test_fail(__FILE__, __LINE__, "%s: se esperaba %lld pero se obtuvo %lld",  \
                          #actual, expected_value, actual_value);                          \
        }                                                                                 \
    } while (0)

#define CHECK_STR(actual, expected)                                                        \
    do {                                                                                  \
        const char *actual_value = (actual);                                              \
        const char *expected_value = (expected);                                          \
        foc_test_checks++;                                                                \
        if (!actual_value || !expected_value ||                                            \
            strcmp(actual_value, expected_value) != 0) {                                   \
            foc_test_fail(__FILE__, __LINE__, "%s: se esperaba \"%s\" pero se obtuvo \"%s\"", \
                          #actual, expected_value ? expected_value : "(null)",             \
                          actual_value ? actual_value : "(null)");                          \
        }                                                                                 \
    } while (0)

void test_lexer(void);
void test_parser(void);
void test_types(void);
void test_preproc(void);
void test_ir(void);
void test_backend(void);
void test_backend(void);

#endif /* FOC_TEST_H */
