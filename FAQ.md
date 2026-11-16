# Preguntas frecuentes (FAQ)

## ?Que es FOCC?

FOCC (abreviado FOC) es un compilador de C propio, escrito en C11
portatil. El frontend, el analisis semantico, la IR, el optimizador y el
backend son implementaciones originales. **No** usa GCC, Clang ni LLVM
como frontend ni como backend del codigo que compila.

## ?Puedo usarlo en produccion?

No. FOCC esta en pre-alpha: no hay conformidad con un estandar, no hay
suite de conformidad completa y el comportamiento puede cambiar entre
versiones. Usalo solo para experimentar.

## ?Que C soporta?

Un subconjunto de C11. A dia de hoy: aritmetica, funciones, punteros,
arrays, structs/unions/enums, `char *`, `sizeof`, preprocesador completo
(`#define`, `#include`, condicionales, `#pragma once`, `defined`,
`__LINE__`, `__FILE__`, `#` y `##`), `for` con declaracion en el
inicializador y declaraciones con varios declaradores.

No esta soportado: `goto` (E0419), `switch` completo con
fall-through, `_Atomic`, `_Generic`, `typeof`, bitfields, `va_arg` y
punto flotante (float/double) en el backend.

## ?Compila su propio codigo (self-host)?

Todavia no. FOCC compila `libcc1/src/passes.c` a un objeto x86-64
correcto (contiene `FOC_opt_constant_fold`, `FOC_opt_dead_code`,
`FOC_opt_copy_propagation` con simbolos). El autoensamblado completo
(`bin/foc.exe` compilado por `bin/foc.exe`) todavia no funciona: el
backend genera instrucciones `mov` invalidas para llamadas variadicas
como `fprintf`. Esta documentado en
[`docs/milestones.md`](docs/milestones.md).

## ?Por que existe si ya estan GCC y Clang?

Objetivos del proyecto:

- Un compilador que se entienda pieza a pieza, util para aprender como
  funcionan los compiladores por dentro.
- Control total sobre diagnosticos, IR y backend, sin depender de las
  decisiones de un backend externo.
- Un backend minimo y auditable (x86-64 Windows x64 ABI Microsoft).

## ?Donde esta la documentacion?

- [`README.md`](README.md) - panorama y construccion.
- [`docs/architecture.md`](docs/architecture.md) - diseno y decisiones.
- [`docs/milestones.md`](docs/milestones.md) - estado real y verificado.
- [`docs/decisions/`](docs/decisions) - registros de decisiones (ADR).
- [`CHANGELOG.md`](CHANGELOG.md) - historial de cambios.
- [`CONTRIBUTING.md`](CONTRIBUTING.md) - como contribuir.

## ?Como se publica una version?

Ver [`RELEASING.md`](RELEASING.md).

## ?Como reporto un bug?

Con una prueba de regresion si es posible (ver `CONTRIBUTING.md`) o, si
crees que es una vulnerabilidad, con el canal descrito en
[`SECURITY.md`](SECURITY.md).
