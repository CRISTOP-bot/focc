# FOC — Fucking Optimized C Compiler

FOC es un compilador propio, independiente y portable de C (y C++ en
desarrollo). No es un wrapper de GCC, Clang ni LLVM: el frontend, el análisis
semántico, la representación intermedia, el optimizador y el backend son
implementaciones originales escritas en C.

> **Estado:** desarrollo temprano. Consulta
> [`docs/milestones.md`](docs/milestones.md) para saber qué está realmente
> implementado y verificado. FOC no declara compatibilidad con un estándar
> hasta que las pruebas de conformidad lo respalden.

## Características

- Implementado en C11 portable, sin dependencias externas.
- Frontend propio de C: lexer, preprocesador, parser, AST, sistema de tipos y
  análisis semántico.
- IR propia independiente de la arquitectura.
- Backends propios: x86-64 (System V y Windows x64); AArch64 y RISC-V en
  desarrollo.
- Driver con interfaz familiar:
  - `foc main.c -o main`
  - `foc -c main.c`
  - `foc -O2 main.c -o main`
  - `foc --target=x86_64-linux-gnu main.c`
  - `foc --version`, `foc --help`
- Diagnósticos con localización exacta, fragmento de código y formato JSON.

## Construcción

Requiere un compilador C11 (el compilador del sistema). El compilador del
sistema se usa **solo para construir FOC** y, en la fase actual, como
ensamblador y enlazador del código generado; nunca como frontend del código
de usuario.

```sh
./build.sh            # genera bin/foc
```

En Windows:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

Pruebas:

```sh
./run_tests.sh
```

En Windows, construye primero el compilador y luego ejecuta las pruebas:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
powershell -ExecutionPolicy Bypass -File run_tests.ps1
```

## Arquitectura

```
foc/include/      cabeceras públicas
foc/src/core/     utilidades portables (fuente, diagnósticos, plataforma, ...)
foc/src/frontend/ lexer, preprocesador, parser, AST, tipos y semántica
foc/src/ir/       representación intermedia propia
foc/src/optimizer/ optimizaciones sobre la IR
foc/src/backend/  backends por arquitectura y ABI
foc/src/driver/   orquestación del pipeline, CLI y toolchain
libc/             biblioteca C mínima
tests/            unitarias e integración
docs/             arquitectura, decisiones y hitos
```

Detalles en [`docs/architecture.md`](docs/architecture.md).

## Principios

1. Sin frontend ni backend ajeno para compilar código de usuario.
2. Sin stubs: cada función contiene una implementación real.
3. Ninguna característica se da por terminada sin pruebas que la verifiquen.
4. Diagnósticos precisos, con localización y fragmentos de código.
5. Portabilidad y bajo consumo como objetivos de diseño.

## Licencia

Por determinar. El código es de momento un prototipo de ingeniería.
