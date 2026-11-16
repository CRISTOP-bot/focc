# 0001 - Lenguaje de implementacion: C

Estado: aceptada.

## Contexto

FOC debe ser portable, ligero, rapido de compilar y capaz de construir
herramientas para sistemas embebidos y Freestanding. El propio compilador
debe poder compilarse con un compilador C minimo, sin runtime complicated y
sin dependencias externas.

## Decision

Implementar el compilador en **C11 portable**, sin dependencias mas alla de la
biblioteca estandar de C.

## Razones

- Es el lenguaje canonico para compiladores y para el objetivo prioritario.
- Permite que FOC se autohospede (self-hosting) y forme parte de una
  toolchain minima para sistemas Freestanding.
- No requiere runtime de lenguaje: FOC puede enlazarse con una libc minima o
  ser Freestanding.
- El consumo de memoria y el tamano del binario quedan bajo control directo.
- El compilador anfitrion (cualquier C11) es solo una herramienta de
  construccion, nunca un frontend oculto del codigo de usuario.

## Restricciones

- Solo C11: sin VLA en el propio FOC, sin `goto` fuera de la limpieza, sin
  extensiones del compilador anfitrion.
- Sin GNU extensions: nada de `__attribute__`, `typeof`, statement
  expressions ni designadores inexistentes.
- Las unicas bibliotecas permitidas son C estandar y una capa de portabilidad
  propia (`foc/src/core`) para procesos, ficheros y rutas.
- Las invocaciones al ensamblador y al enlazador del sistema se aislan en
  `foc/src/driver/toolchain.c` para poder sustituirlas.

## Consecuencias

- El codigo del compilador es portable entre-hosted y Freestanding con poco
  esfuerzo.
- Se pierde el garbage collector: la gestion de memoria es explicita con una
  arena (`foc/src/core/util.c`), lo que ademas reduce el consumo y el tiempo de
  compilacion.
- El estilo se guiding por `docs/coding-standards.md`.
