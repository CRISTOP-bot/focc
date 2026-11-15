# Contribuir a FOCC

FOCC (abreviado FOC) está en pre-alpha. Las interfaces y el comportamiento
pueden cambiar; el proyecto todavía no declara conformidad con un estándar
de C ni está recomendado para producción.

## Preparar el entorno

Se necesita un compilador C11. Las pruebas de integración también necesitan
GNU `as`, un enlazador compatible y GNU M4; para `.S` se usa el preprocesador
del compilador C. En Linux o macOS:

```sh
sh ./build.sh
sh ./run_tests.sh
```

En Windows, instala MinGW-w64/Clang y GNU M4, y ejecuta:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
powershell -ExecutionPolicy Bypass -File run_tests.ps1
```

Para validar con el mismo flujo de `make check` usado en Linux CI:

```sh
sh ./configure
make -j2
make check
```

## Enviar cambios

- Mantén cada cambio enfocado y explica el problema que resuelve.
- Añade una prueba de regresión para cada bug corregido.
- Ejecuta la suite relevante y anota los comandos y resultados en el pull
  request. Si no puedes ejecutar una prueba, indícalo explícitamente.
- Actualiza la documentación cuando cambie el comportamiento observable.
- No presentes una característica como completa o conforme a un estándar
  sin pruebas que lo verifiquen.
- No incluyas binarios ni otros artefactos generados en los cambios.

## CodeRabbit

Con la aplicación de CodeRabbit instalada en GitHub, `.coderabbit.yaml`
habilita revisiones automáticas y análisis de los resultados de GitHub Checks.
Para pedir que repare un CI fallido en un pull request, comenta
`@coderabbitai fix-ci`; el modo predeterminado entrega la reparación en un
pull request apilado para revisión.

La reparación automática sin pedir el comando requiere habilitar **Auto-fix
PRs** en Triage de CodeRabbit. Esa automatización necesita Coding Agent y un
plan Team o superior, además de permisos de administrador de la organización.
