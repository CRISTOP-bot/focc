---
name: foc-testing
description: >
  Cómo probar FOC: suite unitaria en C, pruebas de integración que compilan
  y ejecutan programas reales, y qué hacer cuando una prueba falla. Usar al
  añadir o depurar pruebas.
---

# Pruebas de FOC

## Estructura

- `tests/unit/`: pruebas del lexer, parser, tipos, preprocesador e IR.
  Marco mínimo propio (`tests/unit/test.h`), sin dependencias externas.
- `tests/integration/`: programas de C reales que FOC compila y ejecuta;
  el código de salida es el resultado esperado.
- `tests/run-integration.sh`: recorre los programas, compila con `foc` y
  comprueba el resultado.

## Ejecutar

```sh
make check                 # unitarias + integración
./build/foc_tests          # solo unitarias
```

## Al añadir una prueba

1. **Un caso, un fallo.** Si el test falla, el mensaje debe decir qué se
   esperaba y qué se obtuvo.
2. **Prueba de regresión**: todo error corregido necesita su prueba.
3. Los tests de integración deben ser deterministas y no depender de la red
   ni del sistema de ficheros del anfitrión.

## Depurar un fallo

1. Reproducir con el comando mínimo (`foc fichero.c -o salida`).
2. Localizar la etapa: `--dump-tokens` (preprocesador), `--dump-ir` (semántica
   y lowering), `-S` (backend).
3. Si el programa generado falla, inspecciona el `.s`: suele ser el orden de
   la IR, la secuencia de llamada o la ABI.
4. No declares nada resuelto sin ejecutar la suite.
