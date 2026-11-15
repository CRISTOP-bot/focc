/* Programa de prueba: aritmética y control de flujo. */
int suma(int a, int b) { return a + b; }

int factorial(int n) {
    int resultado = 1;
    int i;
    for (i = 1; i <= n; i = i + 1) {
        resultado = resultado * i;
    }
    return resultado;
}

int main(void) {
    int total = suma(3, 4);
    if (total == 7) {
        total = total + 1;
    } else {
        total = 0;
    }
    while (total > 8) {
        total = total - 1;
    }
    switch (total) {
    case 8:
        total = 100;
        break;
    default:
        total = -1;
        break;
    }
    return total + factorial(5);
}
