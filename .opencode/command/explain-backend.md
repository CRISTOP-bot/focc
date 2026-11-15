---
description: Explica como se genera el codigo de una funcion de FOC.
agent: build
---

Para un fichero dado:

1. Muestra el AST con `foc --dump-ast` (o el flujo de tokens con
   `--dump-tokens`).
2. Muestra la IR con `foc --dump-ir`.
3. Explica el papel de cada instruccion y como se traduce en el backend
   x86-64 (`foc/src/backend/x86_64.c`).
