# Changelog

Todos los cambios notables de este proyecto se documentan aqui. El formato
esta basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/)
y el versionado sigue [SemVer](https://semver.org/lang/es/) mientras el
proyecto sea pre-alpha (la compatibilidad declarada llegara con la 1.0).

## [No publicado]

### Agregado

- Cabeceras estandar propias para la libc de FOC: `stdio.h`, `stdlib.h`,
  `string.h`; `va_list` en `stdarg.h`; declaraciones de `fprintf`,
  `vsnprintf` y `getenv`.
- Prueba de integracion `declaraciones.c` (declaradores multiples,
  declaracion dentro del `for-init`, punteros encadenados).
- Diagnosticos con posicion exacta y nombre del archivo incluido:
  E0303, E0304 y E0305 llevan codigo, mensaje en espanol y span concreto.

### Corregido

- Preprocesador: `#include <...>` unia los tokens con espacios
  (`stddef . h`); los EOF embebidos del fichero incluido cortaban la
  unidad de traduccion (y las expansiones de macro heredaban el EOF);
  `#include "..."` del fichero principal no buscaba en su directorio;
  en `defined()` el token resuelto colgaba de la pila y devolvia valores
  erroneos con la optimizacion (`-O0` funcionaba por casualidad).
- Parser: `for (int i = 0; ...)` consumia el `;` dos veces y fallaba con
  E0201; las declaraciones con varios declaradores (`int a = 1, b = 2`)
  perdian silenciosamente todos menos el primero (ahora se enlazan en
  `FOCDecl::next`); `sizeof(expr)` no consumia el parentesis de cierre;
  `for-init` con declaracion funciona.
- IR: el array de globales se realocaba por realloc invalidando los
  punteros `FOCIRGlobal*` guardados en simbolos; ahora `FOCIRModule`
  almacena un array de punteros (como `functions`).
- Backend: `movb`/`movw` con registro de origen del ancho correcto;
  array de `FOCIRValue*` sustituido por inicializacion explicita valida.
- Driver: los intermedios `.s`/`.o` son temporales y se borran tras
  enlazar (antes se quedaban junto al fuente y el runner de integracion
  los recompilaba como si fueran pruebas).
- Runner de integracion: reintento de enlace ante bloqueos transitorios
  del antivirus; omision honesta del fixture M4 sin GNU M4; lista
  blanca de extensiones.
- Backend: la capacidad de registros de la ABI se respeta al clasificar
  argumentos; en Windows x64 solo `rcx rdx r8 r9` se asignan a
  registros (antes se leian `int_regs[-1]` y se generaban instrucciones
  invalidas en cualquier llamada con mas de cuatro argumentos enteros,
  p. ej. `fprintf` de seis argumentos).
- `libcc1/passes.c` ahora compila completo con `foc` a un objeto
  x86-64 (las funciones `FOC_opt_constant_fold`, `FOC_opt_dead_code`,
  `FOC_opt_copy_propagation` existen y enlazan).
- `foc/src/core/diag.c` compila completo con `foc` a un objeto x86-64.

### Conocido

- El autoensamblado (self-host) **aun no funciona**: `foc/src/core/fs.c`
  necesita cabeceras de plataforma (`sys/stat.h`, `direct.h`, `_fullpath`)
  que la libc de FOC aun no provee; falta ademas la suite de conformidad C.
- `goto` no esta soportado (E0419). Sin ABI System V, sin punto flotante
  en el backend, `switch` incompleto, sin cobertura de conformidad.

## [0.1.0] - pre-alpha inicial

- Compilador C11 propio (sin GCC/Clang/LLVM como frontend ni backend):
  preprocesador, lexer, parser descendente, semantica, IR propia,
  optimizador minimo, backend x86-64 AT&T con ABI Microsoft.
- Ensamblado y enlace mediante las herramientas del sistema
  (`as`, `gcc`/`cc` detectados por PATH y verificados).
- Suite unitaria e integracion, `configure` + `Makefile`, herramientas
  `focdep`, `foccount`, `foc++`; CI de GitHub y Forgejo.
- Repositorio inicial (`c1895da`); soporte de ensamblador GNU y M4
  (`69870f1`).
