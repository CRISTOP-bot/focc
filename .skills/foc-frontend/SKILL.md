---
name: foc-frontend
description: >
  Como trabajar en el frontend de FOC (lexer, preprocesador, parser, AST,
  tipos y semantica): invariantes del declarador en C, orden de la IR y
  trampas conocidas. Usar al tocar src/frontend o src/ir.
---

# Frontend de FOC

## Capas

`lexer` -> `preproc` -> `parser` -> `ast` -> `sema` (en `ir/irgen.c`) -> IR.

## Invariante 1: los nombres del fuente no estan terminados en NUL

El texto viene del bufer del fichero. Cualquier uso como cadena de C
(`%s`, `strcmp`, `strlen`) lee de mas.

```c
/* incorrecto */
symbol->name = decl->declarator->name;

/* correcto */
symbol->name = FOC_strndup(decl->declarator->name, decl->declarator->name_len);
```

## Invariante 2: el orden de la IR

Los operandos se generan **antes** que la instruccion que los consume. Si
emites la instruccion y luego generas sus operandos, el backend produce codigo
con el orden invertido (el caso real fue `total = f()` en una declaracion local:
la direccion del destino se calculaba despues del `store`).

```c
/* correcto */
FOCLowered init = lower_expr(sema, decl->init);
FOCIRValue address = frame_address(sema, symbol->frame_offset);
FOCIRInst *store = emit(sema, FOC_OP_STORE);
```

## Invariante 3: punteros estables en la IR

Las instrucciones viven en bloques de tamano fijo (`FOC_IR_BLOCK_SIZE`), asi
que un `FOCIRInst *` sigue siendo valido aunque la funcion crezca. Si
necesitas el indice, usa `FOC_ir_inst_at()`.

## Invariante 4: el declarador de C

Un declarador se modela como una lista **plana** de constructores en orden de
aplicacion (`FOCDeclCtor`): puntero, array o funcion.

- `int *a[3]` -> `[PUNTERO, ARRAY(3)]` (array de punteros).
- `int (*a)[3]` -> `[ARRAY(3), PUNTERO]` (puntero a array).
- `int (*a[3])(int)` -> `[FUNCION, PUNTERO, ARRAY(3)]`.

Dentro de un nivel: primero los punteros, luego los sufijos en orden inverso al
textual. Al cruzar un parentesis se invierte el anidamiento. No reescribas este
modelo: es lo que evita los errores clasico del declarador en C.

## Preprocesador

- Las macros se expanden con una pila de fuentes de tokens; la pila evita la
  recursion infinita.
- `#` (stringize) y `##` (paste) se resuelven en `substitute_body`.
- Los `#include` se resuelven por rutas y se registran en el gestor de
  fuentes, que es la base de `focdep` y de las reglas de dependencia.

## Diagnosticos

Codigo `E0xxx` (lexer/preprocesador) o `E04xx` (semantica), mensaje en
espanol y `FOCSpan` exacto. Nunca imprimas texto del fuente sin longitud.
