# Hitos

Cada hito tiene criterios de aceptación verificables. No se marca como
completado sin ejecutar sus pruebas. El estado que aparece aquí refleja lo
que realmente pasa hoy por `make check`.

## Hito 1 — Fundación — COMPLETADO

- [x] Estructura del repositorio (layout tipo GCC, sin copiarla).
- [x] Driver con `--version`, `--help`, `--target`, `-o`, `-c`, `-S`, `-E`,
      `-I`, `-D`, `-U`, `-O0..Oz`, `-Wall`, `-Werror`, `-w`, `-M/-MF`,
      `--dump-tokens`, `--dump-ir`, `--emit-ir`.
- [x] Capa de portabilidad (`platform`, `fs`, `proc`, `timer`).
- [x] Diagnósticos con fragmento de código, posición y JSON.
- [x] `configure` + `Makefile.in` tipo proyecto grande; `build.ps1`/`build.sh`
      como atajos.
- [x] Infraestructura de pruebas: unitarias e integración (`make check`).
- [x] Documentación de arquitectura, ADRs y estándares.

## Hito 2 — Frontend de C — PARCIAL

Verificado por las pruebas existentes:

- [x] Lexer completo (números, cadenas, caracteres, comentarios, prefijos).
- [x] Preprocesador: `#define` (objetivo y de función), `#undef`,
      `#ifdef/#ifndef/#if/#elif/#else/#endif`, `defined`, `#include`,
      `#pragma once`, `#error`, `#line`, `#warning`, `#`/`##`, `__LINE__`,
      `__FILE__`, `__FOC__` y expansiones anidadas.
- [x] Parser de declaraciones, expresiones y sentencias (descenso recursivo).
- [x] Declaraciones con varios declaradores (cadena `FOCDecl::next`) y
      declaración dentro del `for-init`.
- [x] `sizeof(expr)` y `sizeof(tipo)`.
- [x] Prototipos de función visibles en la tabla de símbolos desde
      ficheros incluidos (`#include` resuelve y `<...>` concatena bien).
- [x] AST con spans y sistema de tipos (punteros, arrays, funciones, struct,
      union, enum, calificadores, `sizeof`, conversión de tipos).
- [x] Ámbitos y tabla de símbolos; análisis semántico con errores.
- [x] Pruebas unitarias e integración.

No implementado o sin verificar:

- [ ] Declaradores de bits, `_Atomic`, `_Generic`, `_Alignas/_Alignof`,
      `typeof`.
- [ ] Inicializadores designados y compound literals.
- [ ] Expresión `?:` de tipo diferente entre ramas, `va_args`.
- [ ] Conformidad real con C89/C99/C11 (falta la suite de conformidad).

## Hito 3 — Backend x86-64 — PARCIAL

- [x] IR propia (tres direcciones, bloques básicos, registros virtuales).
- [x] Generación de código x86-64 para Windows x64 (ABI Microsoft:
      argumentos en `rcx rdx r8 r9` + sombra de 32 bytes + pilas en orden).
- [x] Ensamblado y enlace mediante herramientas del sistema (`as`, `cc/gcc/
      clang` con sondeo por `PATH` y verificación con `--version`).
- [x] Entrada `.s`/`.S` integrada con GNU `as` y el preprocesador de
      ensamblador; hay pruebas de integración (`assembly` código 7,
      `assembly_cpp` código 8). La prueba M4 se omite con OMITIDO cuando
      GNU M4 no está instalado (no falla).
- [x] Ejecutables de prueba que se ejecutan y devuelven el código esperado.
- [x] Aritmética, comparaciones, llamadas, acceso a memoria, arrays, structs
      por valor, llamadas a funciones y `sizeof`.

No implementado:

- [ ] ABI System V (Linux/macOS) — el código usa siempre la convención
      Microsoft.
- [ ] `float`/`double` (SSE2) en la generación de código (la IR los soporta).
- [ ] PIC, ejecutables PIE/DEP, relocations en objetos ELF/COFF.
- [ ] `switch` completo con etiquetas fall-through entre casos.
- [ ] `goto`/etiquetas (el lowering las ignora: `goto` genera error E0419).
- [ ] Backends AArch64 y RISC-V.

## Hito 4 — Optimizador — MÍNIMO

- [x] Plegado de constantes básico.
- [x] Eliminación de código muerto (marcado de instrucciones muertas).
- [x] Propagación de copias elemental.
- [ ] mem2reg, inlining, propagación de constantes, DCE global, O2/O3.
- [ ] Benchmarks comparativos (no medidos, nada de afirmaciones).

## Hito 5 — Compatibilidad con C real — PARCIAL

- [x] `foc` compila `libcc1/src/passes.c` completo a objeto x86-64
      (funciones `FOC_opt_constant_fold`, `FOC_opt_dead_code`,
      `FOC_opt_copy_propagation` con símbolos reales).
- [ ] Self-host: `diag.c` genera `mov` inválidas para llamadas variádicas
      (`fprintf` con seis argumentos); está en investigación.
- [ ] Suite de conformidad C (no iniciada).

## Hito 6 — C++ — NO EMPEZADO

## Hito 7 — Multiplataforma — NO EMPEZADO

La compilación cruzada existe sólo como verificación: el backend x86-64
puede compilar para Windows x64 desde Windows x64 (host y target iguales).
