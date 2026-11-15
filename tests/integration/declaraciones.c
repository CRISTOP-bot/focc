/* Programa de prueba: declaraciones con varios declaradores. */
int g1 = 10, g2 = 20, g3 = 12;

int main(void) {
    int i = 4, j = 3;
    int acc = 0;
    for (int k = 0; k < 5; k++) {
        acc = acc + g1;
    }
    for (int p = 0, q = 1; p < 3; p++, q = q * 2) {
        acc = acc + q;
    }
    int *pi = &i, *pj = &j;
    acc = acc + *pi + *pj + j;
    return acc + i + g2 + g3 - 10;
}
