# 0001 — Lenguaje de implementación: C

Estado: aceptada.

## Contexto

FOC debe ser portable, ligero, rápido de compilar y capaz de construir
herramientas para sistemas embebidos y Freestanding. El propio compilador
debe poder compilarse con un compilador C mínimo, sin runtime complicated y
sin dependencias externas.

## Decisión

Implementar el compilador en **C11 portable**, sin dependencias más allá de la
biblioteca estándar de C.

## Razones

- Es el lenguaje canónico para compiladores y para el objetivo prioritario.
- Permite que FOC se autohospede (self-hosting) y forme parte de una
  toolchain mínima para sistemas Freestanding.
- No requiere runtime de lenguaje: FOC puede enlazarse con una libc mínima o
  ser Freestanding.
- El consumo de memoria y el tamaño del binario quedan bajo control directo.
- El compilador anfitrión (cualquier C11) es solo una herramienta de
  construcción, nunca un frontend oculto del código de usuario.

## Restricciones

- Solo C11: sin VLA en el propio FOC, sin `goto` fuera de la limpieza, sin
  extensiones del compilador anfitrión.
- Sin GNU extensions: nada de `__attribute__`, `typeof`, statement
  expressions ni designadores inexistentes.
- Las únicas bibliotecas permitidas son C estándar y una capa de portabilidad
  propia (`foc/src/core`) para procesos, ficheros y rutas.
- Las invocaciones al ensamblador y al enlazador del sistema se aíslan en
  `foc/src/driver/toolchain.c` para poder sustituirlas.

## Consecuencias

- El código del compilador es portable entre-hosted y Freestanding con poco
  esfuerzo.
- Se pierde el garbage collector: la gestión de memoria es explícita con una
  arena (`foc/src/core/util.c`), lo que además reduce el consumo y el tiempo de
  compilación.
- El estilo se guiding por `docs/coding-standards.md`.
