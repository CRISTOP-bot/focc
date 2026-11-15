/* Punteros, arrays yCadenas. */
int longitud(const char *texto) {
    int n = 0;
    while (texto[n] != 0) {
        n = n + 1;
    }
    return n;
}

int suma_array(int *datos, int n) {
    int total = 0;
    int i;
    for (i = 0; i < n; i = i + 1) {
        total = total + datos[i];
    }
    return total;
}

int main(void) {
    int numeros[4];
    int *p = numeros;
    numeros[0] = 10;
    numeros[1] = 20;
    numeros[2] = 30;
    numeros[3] = 40;

    *p = 11;             /* numeros[0] = 11 */
    p = p + 1;
    *p = *p + 5;         /* numeros[1] = 25 */

    int total = suma_array(numeros, 4);   /* 11 + 25 + 30 + 40 = 106 */
    int largo = longitud("Hola");         /* 4 */
    return total + largo;                 /* 110 */
}
