/* Structs, uniones y enumeraciones. */
struct Punto {
    int x;
    int y;
};

union Valor {
    int entero;
    char bytes[4];
};

enum Color { ROJO, VERDE = 5, AZUL };

struct Punto sumar(struct Punto a, struct Punto b) {
    struct Punto r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    return r;
}

int main(void) {
    struct Punto p;
    p.x = 3;
    p.y = 4;

    struct Punto q = sumar(p, p);   /* (6, 8) */

    union Valor v;
    v.entero = 0;

    enum Color c = AZUL;            /* 6 */

    return q.x + q.y + c;           /* 6 + 8 + 6 = 20 */
}
