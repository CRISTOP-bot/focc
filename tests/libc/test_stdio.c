#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static char output[128];
static size_t output_len;
static FILE *capture_stream;
static int fail_output;

int fputc(int ch, FILE *stream) {
    if (fail_output || stream != capture_stream || output_len == sizeof(output)) {
        return EOF;
    }
    output[output_len++] = (char)(unsigned char)ch;
    return (unsigned char)ch;
}

static void reset_output(FILE *stream) {
    output_len = 0;
    capture_stream = stream;
    fail_output = 0;
}

static int output_is(const char *expected) {
    size_t expected_len = strlen(expected);
    return output_len == expected_len && memcmp(output, expected, expected_len) == 0;
}

int main(void) {
    reset_output(stdout);
    if (printf("%d %u %x", -42, 7u, 42u) != 8 || !output_is("-42 7 2a")) {
        return 1;
    }

    reset_output(stdout);
    if (printf("%05d %-4s", 12, "x") != 10 || !output_is("00012 x   ")) {
        return 2;
    }

    reset_output(stdout);
    if (printf("%lld", (long long)LLONG_MIN) != 20 ||
        !output_is("-9223372036854775808")) {
        return 3;
    }

    FILE *stream = tmpfile();
    if (!stream) {
        return 4;
    }
    reset_output(stream);
    if (fputs("stream", stream) < 0 || !output_is("stream")) {
        fclose(stream);
        return 5;
    }
    fclose(stream);

    reset_output(stdout);
    fail_output = 1;
    if (printf("write failure") != EOF) {
        return 6;
    }

    reset_output(stdout);
    fail_output = 1;
    if (fputs("write failure", stdout) != EOF) {
        return 7;
    }

    reset_output(stdout);
    if (puts("line") < 0 || !output_is("line\n")) {
        return 8;
    }
    return 0;
}
