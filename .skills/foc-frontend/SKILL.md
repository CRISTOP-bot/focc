---
name: foc-frontend
description: >
  Cómo trabajar en el frontend de FOC (lexer, preprocesador, parser, AST,
  tipos y semántica): invariantes del declarador en C, orden de la IR y
  trampas conocidas. Usar al tocar src/frontend o src/ir.
---

# Frontend de FOC

## Capas

`lexer` → `preproc` → `parser` → `ast` → `sema` (en `ir/irgen.c`) → IR.

## Invariante 1: los nombres del fuente no están terminados en NUL

El texto viene del búfer del fichero. Cualquier uso como cadena de C
(`%s`, `strcmp`, `strlen`) lee de más.

```c
/* incorrecto */
symbol->name = decl->declarator->name;

/* correcto */
symbol->name = FOC_strndup(decl->declarator->name, decl->declarator->name_len);
```

## Invariante 2: el orden de la IR

Los operandos se generan **antes** que la instrucción que los consume. Si
emites la instrucción y luego generas sus operandos, el backend produce código
con el orden invertido (el caso real fue `total = f()` en una declaración local:
la dirección del destino se calculaba después del `store`).

```c
/* correcto */
FOCLowered init = lower_expr(sema, decl->init);
FOCIRValue address = frame_address(sema, symbol->frame_offset);
FOCIRInst *store = emit(sema, FOC_OP_STORE);
```

## Invariante 3: punteros estables en la IR

Las instrucciones viven en bloques de tamaño fijo (`FOC_IR_BLOCK_SIZE`), así
que un `FOCIRInst *` sigue siendo válido aunque la función crezca. Si
necesitas el índice, usa `FOC_ir_inst_at()`.

## Invariante 4: el declarador de C

Un declarador se modela como una lista **plana** de constructores en orden de
aplicación (`FOCDeclCtor`): puntero, array o función.

- `int *a[3]` → `[PUNTERO, ARRAY(3)]` (array de punteros).
- `int (*a)[3]` → `[ARRAY(3), PUNTERO]` (puntero a array).
- `int (*a[3])(int)` → `[FUNCION, PUNTERO, ARRAY(3)]`.

Dentro de un nivel: primero los punteros, luego los sufijos en orden inverso al
textual. Al cruzar un paréntesis se invierte el anidamiento. No reescribas este
modelo: es lo que evita los errores clásico del declarador en C.

## Preprocesador

- Las macros se expanden con una pila de fuentes de tokens; la pila evita la
  recursión infinita.
- `#` (stringize) y `##` (paste) se resuelven en `substitute_body`.
- Los `#include` se resuelven por rutas y se registran en el gestor de
  fuentes, que es la base de `focdep` y de las reglas de dependencia.

## Diagnósticos

Código `E0xxx` (lexer/preprocesador) o `E04xx` (semántica), mensaje en
español y `FOCSpan` exacto. Nunca imprimas texto del fuente sin longitud.
