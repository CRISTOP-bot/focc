# Changelog

Todos los cambios notables de este proyecto se documentan aquí. El formato
está basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/)
y el versionado sigue [SemVer](https://semver.org/lang/es/) mientras el
proyecto sea pre-alpha (la compatibilidad declarada llegará con la 1.0).

## [No publicado]

### Agregado

- Cabeceras estándar propias para la libc de FOC: `stdio.h`, `stdlib.h`,
  `string.h`; `va_list` en `stdarg.h`; declaraciones de `fprintf`,
  `vsnprintf` y `getenv`.
- Prueba de integración `declaraciones.c` (declaradores múltiples,
  declaración dentro del `for-init`, punteros encadenados).
- Diagnósticos con posición exacta y nombre del archivo incluido:
  E0303, E0304 y E0305 llevan código, mensaje en español y span concreto.

### Corregido

- Preprocesador: `#include <...>` unía los tokens con espacios
  (`stddef . h`); los EOF embebidos del fichero incluido cortaban la
  unidad de traducción (y las expansiones de macro heredaban el EOF);
  `#include "..."` del fichero principal no buscaba en su directorio;
  en `defined()` el token resuelto colgaba de la pila y devolvía valores
  erróneos con la optimización (`-O0` funcionaba por casualidad).
- Parser: `for (int i = 0; ...)` consumía el `;` dos veces y fallaba con
  E0201; las declaraciones con varios declaradores (`int a = 1, b = 2`)
  perdían silenciosamente todos menos el primero (ahora se enlazan en
  `FOCDecl::next`); `sizeof(expr)` no consumía el paréntesis de cierre;
  `for-init` con declaración funciona.
- IR: el array de globales se realocaba por realloc invalidando los
  punteros `FOCIRGlobal*` guardados en símbolos; ahora `FOCIRModule`
  almacena un array de punteros (como `functions`).
- Backend: `movb`/`movw` con registro de origen del ancho correcto;
  array de `FOCIRValue*` sustituido por inicialización explícita válida.
- Driver: los intermedios `.s`/`.o` son temporales y se borran tras
  enlazar (antes se quedaban junto al fuente y el runner de integración
  los recompilaba como si fueran pruebas).
- Runner de integración: reintento de enlace ante bloqueos transitorios
  del antivirus; omisión honesta del fixture M4 sin GNU M4; lista
  blanca de extensiones.
- Backend: la capacidad de registros de la ABI se respeta al clasificar
  argumentos; en Windows x64 solo `rcx rdx r8 r9` se asignan a
  registros (antes se leían `int_regs[-1]` y se generaban instrucciones
  inválidas en cualquier llamada con más de cuatro argumentos enteros,
  p. ej. `fprintf` de seis argumentos).
- `libcc1/passes.c` ahora compila completo con `foc` a un objeto
  x86-64 (las funciones `FOC_opt_constant_fold`, `FOC_opt_dead_code`,
  `FOC_opt_copy_propagation` existen y enlazan).
- `foc/src/core/diag.c` compila completo con `foc` a un objeto x86-64.

### Conocido

- El autoensamblado (self-host) **aún no funciona**: `foc/src/core/fs.c`
  necesita cabeceras de plataforma (`sys/stat.h`, `direct.h`, `_fullpath`)
  que la libc de FOC aún no provee; falta además la suite de conformidad C.
- `goto` no está soportado (E0419). Sin ABI System V, sin punto flotante
  en el backend, `switch` incompleto, sin cobertura de conformidad.

## [0.1.0] — pre-alpha inicial

- Compilador C11 propio (sin GCC/Clang/LLVM como frontend ni backend):
  preprocesador, lexer, parser descendente, semántica, IR propia,
  optimizador mínimo, backend x86-64 AT&T con ABI Microsoft.
- Ensamblado y enlace mediante las herramientas del sistema
  (`as`, `gcc`/`cc` detectados por PATH y verificados).
- Suite unitaria e integración, `configure` + `Makefile`, herramientas
  `focdep`, `foccount`, `foc++`; CI de GitHub y Forgejo.
- Repositorio inicial (`c1895da`); soporte de ensamblador GNU y M4
  (`69870f1`).
