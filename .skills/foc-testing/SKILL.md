---
name: foc-testing
description: >
  Como probar FOC: suite unitaria en C, pruebas de integracion que compilan
  y ejecutan programas reales, y que hacer cuando una prueba falla. Usar al
  anadir o depurar pruebas.
---

# Pruebas de FOC

## Estructura

- `tests/unit/`: pruebas del lexer, parser, tipos, preprocesador e IR.
  Marco minimo propio (`tests/unit/test.h`), sin dependencias externas.
- `tests/integration/`: programas de C reales que FOC compila y ejecuta;
  el codigo de salida es el resultado esperado.
- `tests/run-integration.sh`: recorre los programas, compila con `foc` y
  comprueba el resultado.

## Ejecutar

```sh
make check                 # unitarias + integracion
./build/foc_tests          # solo unitarias
```

## Al anadir una prueba

1. **Un caso, un fallo.** Si el test falla, el mensaje debe decir que se
   esperaba y que se obtuvo.
2. **Prueba de regresion**: todo error corregido necesita su prueba.
3. Los tests de integracion deben ser deterministas y no depender de la red
   ni del sistema de ficheros del anfitrion.

## Depurar un fallo

1. Reproducir con el comando minimo (`foc fichero.c -o salida`).
2. Localizar la etapa: `--dump-tokens` (preprocesador), `--dump-ir` (semantica
   y lowering), `-S` (backend).
3. Si el programa generado falla, inspecciona el `.s`: suele ser el orden de
   la IR, la secuencia de llamada o la ABI.
4. No declares nada resuelto sin ejecutar la suite.
