---
description: Ejecuta la suite completa de pruebas de FOC.
agent: build
---

Ejecuta:

```sh
make check
```

La suite incluye las pruebas unitarias (`tests/unit`) y las de integracion
(`tests/run-integration.sh`), que compilan y ejecutan programas reales.

Resume los fallos mostrando el fichero, la linea y el mensaje.
