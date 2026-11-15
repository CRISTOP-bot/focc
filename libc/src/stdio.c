/* Implementacion minima de salida formateada para la libc de FOC. */
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

static int emit_char(FILE *stream, int ch, int *written) {
    if (*written == INT_MAX || fputc(ch, stream) == EOF) {
        return -1;
    }
    (*written)++;
    return 0;
}

static int emit_repeat(FILE *stream, char ch, int count, int *written) {
    while (count-- > 0) {
        if (emit_char(stream, ch, written) != 0) {
            return -1;
        }
    }
    return 0;
}

static int emit_string(FILE *stream, const char *text, int width, bool left, int *written) {
    if (!text) {
        text = "(null)";
    }
    size_t length = 0;
    while (text[length]) {
        if (length == INT_MAX) {
            return -1;
        }
        length++;
    }
    int padding = width > (int)length ? width - (int)length : 0;
    if (!left && emit_repeat(stream, ' ', padding, written) != 0) {
        return -1;
    }
    while (*text) {
        if (emit_char(stream, (unsigned char)*text++, written) != 0) {
            return -1;
        }
    }
    return left ? emit_repeat(stream, ' ', padding, written) : 0;
}

static int emit_unsigned(FILE *stream, unsigned long long value, int base, bool uppercase,
                         bool negative, int width, bool zero_pad, bool left, bool show_sign,
                         bool space_sign, int *written) {
    char buffer[sizeof(value) * CHAR_BIT];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int length = 0;
    do {
        buffer[length++] = digits[value % (unsigned)base];
        value /= (unsigned)base;
    } while (value > 0);

    char sign = negative ? '-' : (show_sign ? '+' : (space_sign ? ' ' : '\0'));
    int sign_length = sign != '\0' ? 1 : 0;
    int padding = width > length + sign_length ? width - length - sign_length : 0;
    if (!left && !zero_pad && emit_repeat(stream, ' ', padding, written) != 0) {
        return -1;
    }
    if (sign && emit_char(stream, sign, written) != 0) {
        return -1;
    }
    if (!left && zero_pad && emit_repeat(stream, '0', padding, written) != 0) {
        return -1;
    }
    while (length > 0) {
        if (emit_char(stream, buffer[--length], written) != 0) {
            return -1;
        }
    }
    return left ? emit_repeat(stream, ' ', padding, written) : 0;
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int written = 0;
    const char *p = format;

    while (*p) {
        if (*p != '%') {
            if (emit_char(stdout, (unsigned char)*p++, &written) != 0) {
                goto output_error;
            }
            continue;
        }

        p++;
        if (!*p) {
            if (emit_char(stdout, '%', &written) != 0) {
                goto output_error;
            }
            break;
        }

        bool left = false;
        bool zero_pad = false;
        bool show_sign = false;
        bool space_sign = false;
        bool flags_done = false;
        while (!flags_done) {
            switch (*p) {
            case '-':
                left = true;
                p++;
                break;
            case '0':
                zero_pad = true;
                p++;
                break;
            case '+':
                show_sign = true;
                p++;
                break;
            case ' ':
                space_sign = true;
                p++;
                break;
            default:
                flags_done = true;
                break;
            }
        }

        int width = 0;
        if (*p == '*') {
            int argument_width = va_arg(args, int);
            p++;
            if (argument_width < 0) {
                left = true;
                if (argument_width == INT_MIN) {
                    goto output_error;
                }
                width = -argument_width;
            } else {
                width = argument_width;
            }
        } else {
            while (*p >= '0' && *p <= '9') {
                int digit = *p - '0';
                if (width > (INT_MAX - digit) / 10) {
                    goto output_error;
                }
                width = width * 10 + digit;
                p++;
            }
        }

        enum { LEN_DEFAULT, LEN_HH, LEN_H, LEN_L, LEN_LL } length = LEN_DEFAULT;
        if (*p == 'h') {
            p++;
            if (*p == 'h') {
                length = LEN_HH;
                p++;
            } else {
                length = LEN_H;
            }
        } else if (*p == 'l') {
            p++;
            if (*p == 'l') {
                length = LEN_LL;
                p++;
            } else {
                length = LEN_L;
            }
        }

        if (!*p) {
            if (emit_char(stdout, '%', &written) != 0) {
                goto output_error;
            }
            break;
        }

        switch (*p) {
        case 'd':
        case 'i': {
            long long value;
            if (length == LEN_LL) {
                value = va_arg(args, long long);
            } else if (length == LEN_L) {
                value = va_arg(args, long);
            } else if (length == LEN_H) {
                value = (short)va_arg(args, int);
            } else if (length == LEN_HH) {
                value = (signed char)va_arg(args, int);
            } else {
                value = va_arg(args, int);
            }
            bool negative = value < 0;
            unsigned long long magnitude =
                negative ? (unsigned long long)(-(value + 1)) + 1 : (unsigned long long)value;
            if (emit_unsigned(stdout, magnitude, 10, false, negative, width, zero_pad, left,
                              show_sign, space_sign, &written) != 0) {
                goto output_error;
            }
            break;
        }
        case 'u':
        case 'x':
        case 'X': {
            unsigned long long value;
            if (length == LEN_LL) {
                value = va_arg(args, unsigned long long);
            } else if (length == LEN_L) {
                value = va_arg(args, unsigned long);
            } else if (length == LEN_HH) {
#if UCHAR_MAX <= INT_MAX
                value = (unsigned char)va_arg(args, int);
#else
                value = (unsigned char)va_arg(args, unsigned int);
#endif
            } else if (length == LEN_H) {
#if USHRT_MAX <= INT_MAX
                value = (unsigned short)va_arg(args, int);
#else
                value = (unsigned short)va_arg(args, unsigned int);
#endif
            } else {
                value = va_arg(args, unsigned int);
            }
            int base = *p == 'x' || *p == 'X' ? 16 : 10;
            if (emit_unsigned(stdout, value, base, *p == 'X', false, width, zero_pad, left, false,
                              false, &written) != 0) {
                goto output_error;
            }
            break;
        }
        case 'c': {
            int padding = width > 1 ? width - 1 : 0;
            if (!left && emit_repeat(stdout, ' ', padding, &written) != 0) {
                goto output_error;
            }
            if (emit_char(stdout, va_arg(args, int), &written) != 0) {
                goto output_error;
            }
            if (left && emit_repeat(stdout, ' ', padding, &written) != 0) {
                goto output_error;
            }
            break;
        }
        case 's':
            if (emit_string(stdout, va_arg(args, const char *), width, left, &written) != 0) {
                goto output_error;
            }
            break;
        case '%':
            if (emit_char(stdout, '%', &written) != 0) {
                goto output_error;
            }
            break;
        default:
            if (emit_char(stdout, (unsigned char)*p, &written) != 0) {
                goto output_error;
            }
            break;
        }
        p++;
    }

    va_end(args);
    return written;

output_error:
    va_end(args);
    return EOF;
}

int puts(const char *s) {
    int written = 0;
    if (!s || emit_string(stdout, s, 0, false, &written) != 0 ||
        emit_char(stdout, '\n', &written) != 0) {
        return EOF;
    }
    return 0;
}

int fputs(const char *s, FILE *stream) {
    int written = 0;
    if (!s || !stream || emit_string(stream, s, 0, false, &written) != 0) {
        return EOF;
    }
    return 0;
}
