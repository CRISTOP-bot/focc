/* Programa de prueba: sizeof mide el array (128), no el puntero (8). */
int main(void) {
    char buffer[128];
    char *puntero = buffer;
    if (sizeof(buffer) != 128) {
        return 1;
    }
    if (sizeof(puntero) != 8) {
        return 2;
    }
    return 128;
}
