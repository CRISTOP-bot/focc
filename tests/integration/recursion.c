/* Recursividad, macros y el preprocesador. */
#define CUADRADO(x) ((x) * (x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

int mcd(int a, int b) {
    if (b == 0) {
        return a;
    }
    return mcd(b, a % b);
}

int fibonacci(int n) {
    if (n < 2) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main(void) {
    int c = CUADRADO(5);            /* 25 */
    int m = MAX(3, 9);               /* 9 */
    int g = mcd(48, 18);             /* 6 */
    int f = fibonacci(10);           /* 55 */
    return c + m + g + f;            /* 95 */
}
