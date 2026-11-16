# FOCC (FOC) - instrucciones para agentes automaticos.

FOCC, abreviado FOC (Fucking Optimized C Compiler), es un compilador propio de
C escrito en C11 portable. **No** usa GCC, Clang ni LLVM como frontend o
backend del codigo de usuario: el lexer, el preprocesador, el parser, el
analisis semantico, la IR, el optimizador y el backend son propios.

## Estructura

```
foc/src/core/      utilidades portables (arena, fuentes, diagnosticos, procesos)
foc/src/frontend/  lexer, preprocesador, parser, AST, tipos, semantica
foc/src/ir/        IR propia y lowering del AST
foc/src/optimizer/ seleccion de pasadas
foc/src/backend/   backends por arquitectura (x86-64) y ABI
foc/src/driver/    CLI, pipeline y toolchain
libc/ libgcc/ libcc1/  runtime, ayudantes y pasadas de la IR
include/           cabeceras estandar que FOC ofrece a los programas
tests/             pruebas unitarias, de integracion y de conformidad
```

## Reglas obligatorias

1. Nada de GNU extensions: solo C11. Ni `typeof`, ni `__attribute__`, ni
   statement expressions.
2. Las llamadas al sistema van en `src/core/` y detras de una interfaz.
3. Los nombres que vienen del fuente no estan terminados en NUL: copiarlos
   con `FOC_strndup` antes de usarlos como cadenas.
4. En el lowering, los operandos se generan ANTES que la instruccion que los
   consume.
5. Los diagnosticos llevan codigo, mensaje en espanol y posicion exacta.
6. Cada correccion de error incluye una prueba de regresion.

## Comandos

```sh
./configure && make        # construir
make check                 # pruebas
./bin/foc main.c -o main   # compilar
```

## Uso eficiente de tokens

Este repositorio es grande y el contexto es el recurso escaso. Antes de
cualquier tarea, carga la habilidad correspondiente de `.skills/`:

| Habilidad | Cuando |
|---|---|
| `.skills/token-discipline/SKILL.md` | Siempre (reglas de alcance y de consumo de tokens) |
| `.skills/foc-frontend/SKILL.md` | Al tocar `src/frontend` o `src/ir` |
| `.skills/foc-backend/SKILL.md` | Al tocar `src/backend` o el runtime |
| `.skills/foc-testing/SKILL.md` | Al anadir o depurar pruebas |

Resumen de las reglas:

- Localiza con `grep`/`glob` y lee solo el rango necesario; no vuelques
  ficheros completos ni generes JSON como formato de trabajo.
- Un cambio que toca mas de unos pocos ficheros suele ser otro encargo:
  pregunta antes.
- No crees andamiaje "por si acaso" (stubs, TODOs, ejemplos vacios) ni
  anadas dependencias u opciones no solicitadas.
- No reformatees codigo ajeno al cambio.
- Informa de forma breve: que cambiaste, por que y con que comando se
  verifica.

## Documentacion

- `docs/architecture.md`: arquitectura y decisiones.
- `docs/decisions/`: registros de decisiones (ADR).
- `docs/milestones.md`: estado real de cada hito.
