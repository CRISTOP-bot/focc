# Preguntas frecuentes (FAQ)

## ¿Qué es FOCC?

FOCC (abreviado FOC) es un compilador de C propio, escrito en C11
portátil. El frontend, el análisis semántico, la IR, el optimizador y el
backend son implementaciones originales. **No** usa GCC, Clang ni LLVM
como frontend ni como backend del código que compila.

## ¿Puedo usarlo en producción?

No. FOCC está en pre-alpha: no hay conformidad con un estándar, no hay
suite de conformidad completa y el comportamiento puede cambiar entre
versiones. Úsalo solo para experimentar.

## ¿Qué C soporta?

Un subconjunto de C11. A día de hoy: aritmética, funciones, punteros,
arrays, structs/unions/enums, `char *`, `sizeof`, preprocesador completo
(`#define`, `#include`, condicionales, `#pragma once`, `defined`,
`__LINE__`, `__FILE__`, `#` y `##`), `for` con declaración en el
inicializador y declaraciones con varios declaradores.

No está soportado: `goto` (E0419), `switch` completo con
fall-through, `_Atomic`, `_Generic`, `typeof`, bitfields, `va_arg` y
punto flotante (float/double) en el backend.

## ¿Compila su propio código (self-host)?

Todavía no. FOCC compila `libcc1/src/passes.c` a un objeto x86-64
correcto (contiene `FOC_opt_constant_fold`, `FOC_opt_dead_code`,
`FOC_opt_copy_propagation` con símbolos). El autoensamblado completo
(`bin/foc.exe` compilado por `bin/foc.exe`) todavía no funciona: el
backend genera instrucciones `mov` inválidas para llamadas variádicas
como `fprintf`. Está documentado en
[`docs/milestones.md`](docs/milestones.md).

## ¿Por qué existe si ya están GCC y Clang?

Objetivos del proyecto:

- Un compilador que se entienda pieza a pieza, útil para aprender cómo
  funcionan los compiladores por dentro.
- Control total sobre diagnósticos, IR y backend, sin depender de las
  decisiones de un backend externo.
- Un backend mínimo y auditable (x86-64 Windows x64 ABI Microsoft).

## ¿Dónde está la documentación?

- [`README.md`](README.md) — panorama y construcción.
- [`docs/architecture.md`](docs/architecture.md) — diseño y decisiones.
- [`docs/milestones.md`](docs/milestones.md) — estado real y verificado.
- [`docs/decisions/`](docs/decisions) — registros de decisiones (ADR).
- [`CHANGELOG.md`](CHANGELOG.md) — historial de cambios.
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — cómo contribuir.

## ¿Cómo se publica una versión?

Ver [`RELEASING.md`](RELEASING.md).

## ¿Cómo reporto un bug?

Con una prueba de regresión si es posible (ver `CONTRIBUTING.md`) o, si
crees que es una vulnerabilidad, con el canal descrito en
[`SECURITY.md`](SECURITY.md).
