# Arquitectura de FOCC (FOC)

## 1. Pipeline

```
fuente → preprocesador → lexer → parser → AST → semántica → IR → optimizador → backend → ensamblador → enlazador
```

Cada etapa tiene una interfaz explícita y un fichero propio. El driver
(`foc/src/driver/`) las orquesta.

Las entradas de bajo nivel siguen rutas separadas: `.s` va directamente a GNU
`as`, `.S` pasa primero por el preprocesador C (`cc -E -x assembler-with-cpp`)
y `.m4` pasa por GNU M4 antes de GNU `as`. Sus objetos pueden enlazarse con
objetos generados desde C; FOCC no implementa otro ensamblador ni M4.

## 2. Estructura del árbol

Layout al estilo de un proyecto grande (tipo GCC pero propio):

```
foc/                 el compilador
  src/core/            utilidades portables (arena, fuentes, diagnósticos, procesos)
  src/frontend/        lexer, preprocesador, parser, AST, tipos, semántica
  src/ir/              la IR propia y el lowering AST → IR
  src/optimizer/       selección de pasadas
  src/backend/         backends por arquitectura (x86-64) y ABI
  src/driver/          CLI, pipeline y toolchain
  include/foc/         cabeceras internas
include/             cabeceras estándar que FOC ofrece a los programas
libc/                libc de FOC (string.h, stdio.h mínimos)
libgcc/              runtime del compilador (ayudante de división 64 bits)
libcc1/              las pasadas de la IR (el "cerebro", enlazable como lib)
tools/               herramientas propias del proyecto
tests/               pruebas unitarias, de integración y de conformidad
docs/                arquitectura, ADRs, hitos
.skills/             habilidades para el agente
.opencode/           agentes y comandos propios
.vscode/             configuración de VS Code
```

## 3. Capa de portabilidad

- `platform`: detección de anfitrión y triples de destino.
- `fs`, `proc`: ficheros y procesos sin APIs específicas de SO.
- El núcleo del compilador no toca APIs de Windows/Linux directamente.

## 4. IR

```
fn @suma(i32 %a, i32 %b) -> i32 {
bb0:
  %0 = add i32 %a, %b
  ret i32 %0
}
```

Instrucciones de tres direcciones con bloques básicos, registros virtuales
tipados y `FOC_OP_ALLOCA` para variables locales. `alloca`/`load`/`store`
para ausencia de SSA; el optimizador puede promocionar más tarde.

## 5. Diagnósticos

Formato humano y JSON, con el fragmento de código fuente y la posición
exacta. Ver `docs/architecture.md` y `.skills/foc-testing/SKILL.md`.

## 6. Verificación

- `make check` — unitarias e integración.
- `tests/integration/*.{c,s,S,m4}` — programas que se compilan y ejecutan;
  el código de salida esperado está en `<nombre>.expected`.
- Cada error corregido añade una prueba de regresión.
