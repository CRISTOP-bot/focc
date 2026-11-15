---
name: foc-backend
description: >
  Cómo trabajar en el backend x86-64 de FOC: ABI de llamada, marco de pila,
  Emitiendo ensamblador AT&T y diferencias entre ELF y COFF. Usar al tocar
  src/backend o el runtime.
---

# Backend x86-64 de FOC

## ABI

- **Windows x64**: argumentos en `rcx, rdx, r8, r9` y `xmm0-3`; 32 bytes de
  espacio de sombra que el llamante reserva; el llamado puede usarlo para
  guardar los argumentos de registro.
- **System V AMD64**: argumentos en `rdi, rsi, rdx, rcx, r8, r9` y
  `xmm0-7`; en las llamadas variádicas `al` debe contener el número de
  registros vectoriales usados.

Los parámetros se copian de los registros a su hueco del marco (16(%rbp) en
Windows) al entrar en la función, y el cuerpo los usa de forma uniforme.

## Secuencia de llamada

1. `subq $total, %rsp` con `total = sombra + pila` redondeado a 16 bytes.
2. Los argumentos de pila se escriben **encima** de la sombra (`rsp+32` en
   Windows), no mediante `push` (eso rompería la alineación y la posición).
3. Se cargan los argumentos de registro.
4. `call`.
5. `addq $total, %rsp`.

## Marco de pila

- Locales en offsets negativos; parámetros en offsets positivos (área del
  llamante).
- Cada registro virtual de la IR ocupa 8 bytes por debajo de los locales.
- Los huecos temporales (`FOC_OP_ALLOCA`) se asignan por debajo de los
  registros virtuales.

## Sintaxis de salida

- GNU (AT&T): `movq %rax, -8(%rbp)`, `addl %edi, %eax`.
- `.type`/`.size`/`@function` y `.note.GNU-stack` son exclusivos de ELF: en
  COFF (Windows) y Mach-O se omiten.

## Portabilidad

El backend recibe un `FOCTarget`; nunca consulta el anfitrión. Toda
diferencia entre plataformas va en tablas (`FOC_abi_for`).
