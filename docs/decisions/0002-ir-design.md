# 0002 - Diseno de la IR

Estado: aceptada.

## Contexto

La IR debe ser propia, independiente de la arquitectura y apta para
optimizacion. Debe poder generar codigo para x86-64, AArch64 y RISC-V sin
cambiar el frontend.

## Decision

IR de tres direcciones con bloques basicos, registros virtuales y tipos
explicitos. Variables locales en `alloca`/`load`/`store`, con promocion a
registros como optimizacion.

## Forma

```
fn @nombre(tipo %param, ...) -> tipo {
bb0:
  %r = op tipo %a, %b
  br bb1
bb1:
  %s = phi tipo [bb0, %r], [bb2, %t]
  ret tipo %s
}
```

## Instrucciones

- Aritmeticas: `add sub mul div rem` (con variantes con/sin signo).
- Logicas: `and or xor shl ashr/lshr`.
- Comparaciones: `icmp`, `fcmp`.
- Conversiones: `trunc ext zext sitofp uitofp fptosi fptoui ptrtoint inttoptr bitcast`.
- Memoria: `alloca load store getelementptr memcpy memmove`.
- Control: `br`, `condbr`, `ret`, `unreachable`, `switch`.
- Llamadas: `call`, `callvoid`.
- Constantes: enteros, flotantes, `null`, `undef`, `globaladdr`.

## Tipos

`void i1 i8 i16 i32 i64 f32 f64 ptr` y agregados con layout (struct, array).
Los punteros no distinguen address space en esta version.

## Por que no SSA desde el inicio

Construir SSA directamente complica la semantica de punteros, arrays y
asignaciones. Empezar con `alloca` y aplicar mem2reg en el optimizador da un
punto de partida correcto y permite que O0 genere codigo simple.

## Optimizaciones previstas

- Plegado y propagacion de constantes.
- Eliminacion de codigo muerto e instrucciones redundantes.
- mem2reg.
- Inlining de funciones pequenas.
- Asignacion de registros por barrido lineal.
