# FOCC (FOC) - Fucking Optimized C Compiler

[![CI](https://github.com/CRISTOP-bot/focc/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/CRISTOP-bot/focc/actions/workflows/build.yml)

FOCC, abreviado FOC, es un compilador propio, independiente y portable de C
(y C++ en desarrollo). No es un wrapper de GCC, Clang ni LLVM: el frontend,
el analisis semantico, la representacion intermedia, el optimizador y el
backend son implementaciones originales escritas en C.

> **Estado: pre-alpha.** FOCC es experimental: las interfaces, opciones y
> resultados pueden cambiar sin compatibilidad entre versiones. No se
> recomienda para produccion. Consulta
> [`docs/milestones.md`](docs/milestones.md) para saber que esta realmente
> implementado y verificado. FOCC no declara compatibilidad con un estandar
> hasta que las pruebas de conformidad lo respalden.

## Caracteristicas

- Nucleo propio en C11 portable, sin depender de LLVM ni de un frontend C
  externo.
- Frontend propio de C: lexer, preprocesador, parser, AST, sistema de tipos y
  analisis semantico.
- IR propia independiente de la arquitectura.
- Backends propios: x86-64 (System V y Windows x64); AArch64 y RISC-V en
  desarrollo.
- Driver con interfaz familiar:
  - `foc main.c -o main`
  - `foc -c main.c`
  - `foc -O2 main.c -o main`
  - `foc main.c helper.s -o main`
  - `foc -c helper.S` (preprocesado C y ensamblado GNU)
  - `foc -c macros.m4` (GNU M4 genera ensamblador GNU)
  - `foc --target=x86_64-linux-gnu main.c`
  - `foc --version`, `foc --help`
- Diagnosticos con localizacion exacta, fragmento de codigo y formato JSON.
- Entrada de ensamblador GNU: `.s` se ensambla directamente, `.S` pasa por
  CPP y `.m4` pasa por GNU M4; los objetos se pueden enlazar junto con C.

## Construccion

Requiere un compilador C11 (el compilador del sistema), GNU `as` y un
enlazador compatible (normalmente `cc`, GCC o Clang). `.S` requiere ademas el
preprocesador de ese compilador; `.m4` requiere GNU M4. El frontend de C y el
backend de FOC son propios; las herramientas externas se usan para construir
FOC y para ensamblar/preprocesar/enlazar las entradas de bajo nivel.

```sh
sh ./build.sh         # genera bin/foc
```

En Windows:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

Pruebas:

```sh
sh ./run_tests.sh
```

El runner ejecuta las pruebas unitarias, de libc y de integracion. Primero
construye el compilador con `./build.sh`. En Windows:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
powershell -ExecutionPolicy Bypass -File run_tests.ps1
```

En Linux, la configuracion `make check` tambien ejecuta la suite completa:

```sh
sh ./configure
make -j2
make check
```

La integracion continua verifica Linux con GCC y Clang, y Windows con
MinGW-w64.

## Arquitectura

```
foc/include/      cabeceras publicas
foc/src/core/     utilidades portables (fuente, diagnosticos, plataforma, ...)
foc/src/frontend/ lexer, preprocesador, parser, AST, tipos y semantica
foc/src/ir/       representacion intermedia propia
foc/src/optimizer/ optimizaciones sobre la IR
foc/src/backend/  backends por arquitectura y ABI
foc/src/driver/   orquestacion del pipeline, CLI y toolchain
libc/             biblioteca C minima
tests/            unitarias e integracion
docs/             arquitectura, decisiones y hitos
```

Detalles en [`docs/architecture.md`](docs/architecture.md).

## Principios

1. Sin frontend ni backend ajeno para compilar C; ASM/M4 usan herramientas GNU
   explicitas en vez de implementaciones incompletas propias.
2. Sin stubs: cada funcion contiene una implementacion real.
3. Ninguna caracteristica se da por terminada sin pruebas que la verifiquen.
4. Diagnosticos precisos, con localizacion y fragmentos de codigo.
5. Portabilidad y bajo consumo como objetivos de diseno.

## Licencia

El proyecto se distribuye bajo la licencia MIT. Consulta el archivo
[LICENSE](LICENSE) para ver el texto completo.

## Contribuir

Consulta [CONTRIBUTING.md](CONTRIBUTING.md) para preparar el entorno, ejecutar
las pruebas y enviar cambios.
