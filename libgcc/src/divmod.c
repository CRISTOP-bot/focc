/* Ayudantes de la runtime del compilador (libgcc).
 *
 * Operaciones de 64 bits que algunos compiladores no generan de forma
 * nativa en todas las arquitecturas.
 */
#include <stddef.h>

unsigned long long __udivmoddi4(unsigned long long a, unsigned long long b,
                                unsigned long long *remainder) {
    if (b == 0) {
        if (remainder) {
            *remainder = 0;
        }
        return 0;
    }
    unsigned long long quotient = 0;
    unsigned long long rest = 0;
    for (int bit = 63; bit >= 0; bit--) {
        rest = (rest << 1) | ((a >> bit) & 1ULL);
        if (rest >= b) {
            rest -= b;
            quotient |= 1ULL << bit;
        }
    }
    if (remainder) {
        *remainder = rest;
    }
    return quotient;
}

unsigned long long __udivdi3(unsigned long long a, unsigned long long b) {
    return __udivmoddi4(a, b, NULL);
}

unsigned long long __umoddi3(unsigned long long a, unsigned long long b) {
    unsigned long long remainder = 0;
    __udivmoddi4(a, b, &remainder);
    return remainder;
}

long long __ashrdi3(long long a, int shift) {
    if (shift <= 0) {
        return a;
    }
    if (shift >= 64) {
        return a < 0 ? -1 : 0;
    }
    return a >> shift;
}

unsigned long long __ashlti3(unsigned long long a, int shift) {
    if (shift <= 0) {
        return a;
    }
    if (shift >= 64) {
        return 0;
    }
    return a << shift;
}

int __ffsdi2(int value) {
    if (value == 0) {
        return 0;
    }
    int index = 1;
    while ((value & 1) == 0) {
        value >>= 1;
        index++;
    }
    return index;
}
