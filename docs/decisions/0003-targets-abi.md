# 0003 - Targets y ABI

Estado: aceptada.

## Contexto

FOC debe compilar para arquitecturas y sistemas distintos del anfitrion. La
convencion de llamada y el formato de salida varian.

## Decision

Representar el destino con un triple canonico:

```
<arquitectura>-<vendor>-<sistema>-<entorno>
```

Ejemplos: `x86_64-pc-linux-gnu`, `x86_64-pc-windows-msvc`,
`aarch64-apple-darwin`, `riscv64-unknown-none-elf`.

## Arquitecturas

Prioridad: x86-64, AArch64, RISC-V. WebAssembly despues.

## ABI x86-64

- **System V AMD64** (Linux, macOS, BSD): argumentos en `rdi rsi rdx rcx r8
  r9` y `xmm0-7`; retorno en `rax` y `xmm0`; alineacion de pila de 16 bytes
  antes de `call`; registros callee-saved `rbx rbp r12-r15`.
- **Windows x64**: argumentos en `rcx rdx r8 r9` y `xmm0-3`; 32 bytes de
  shadow space; retorno en `rax`/`xmm0`; callee-saved `rbx rbp rdi rsi r12-r15`
  y `xmm6-15`.

Los structs se clasifican segun el tamano y los campos (reglas de clasificacion
INTEGER/SSE de SysV; registro de 8 bytes o memoria en Windows).

## Formatos de salida

- Ensamblador (AT&T para GNU `as`).
- Fichero objeto mediante el ensamblador del sistema, aislado detras de una
  interfaz. A medio plazo, FOC escribira ELF/COFF/Mach-O directamente.
- Enlace mediante el enlazador del sistema (`ld` o el driver del compilador
  del sistema solo como enlazador) o mediante el enlazador propio futuro.

## Portabilidad

El backend solo recibe `Target`. No consulta el anfitrion. Las diferencias de
SO (colecciones de registros, formato, prefijos de simbolos) viven en tablas
por target.
