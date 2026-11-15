# 0002 �?" Diseño de la IR

Estado: aceptada.

## Contexto

La IR debe ser propia, independiente de la arquitectura y apta para
optimización. Debe poder generar código para x86-64, AArch64 y RISC-V sin
cambiar el frontend.

## Decisión

IR de tres direcciones con bloques básicos, registros virtuales y tipos
explícitos. Variables locales en `alloca`/`load`/`store`, con promoción a
registros como optimización.

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

- Aritméticas: `add sub mul div rem` (con variantes con/sin signo).
- Lógicas: `and or xor shl ashr/lshr`.
- Comparaciones: `icmp`, `fcmp`.
- Conversiones: `trunc ext zext sitofp uitofp fptosi fptoui ptrtoint inttoptr bitcast`.
- Memoria: `alloca load store getelementptr memcpy memmove`.
- Control: `br`, `condbr`, `ret`, `unreachable`, `switch`.
- Llamadas: `call`, `callvoid`.
- Constantes: enteros, flotantes, `null`, `undef`, `globaladdr`.

## Tipos

`void i1 i8 i16 i32 i64 f32 f64 ptr` y agregados con layout (struct, array).
Los punteros no distinguen address space en esta versión.

## Por qué no SSA desde el inicio

Construir SSA directamente complica la semántica de punteros, arrays y
asignaciones. Empezar con `alloca` y aplicar mem2reg en el optimizador da un
punto de partida correcto y permite que O0 genere código simple.

## Optimizaciones previstas

- Plegado y propagación de constantes.
- Eliminación de código muerto e instrucciones redundantes.
- mem2reg.
- Inlining de funciones pequeñas.
- Asignación de registros por barrido lineal.
