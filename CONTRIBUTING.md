# Contribuir a FOCC

FOCC (abreviado FOC) esta en pre-alpha. Las interfaces y el comportamiento
pueden cambiar; el proyecto todavia no declara conformidad con un estandar
de C ni esta recomendado para produccion.

## Preparar el entorno

Se necesita un compilador C11. Las pruebas de integracion tambien necesitan
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

- Manten cada cambio enfocado y explica el problema que resuelve.
- Anade una prueba de regresion para cada bug corregido.
- Ejecuta la suite relevante y anota los comandos y resultados en el pull
  request. Si no puedes ejecutar una prueba, indicalo explicitamente.
- Actualiza la documentacion cuando cambie el comportamiento observable.
- No presentes una caracteristica como completa o conforme a un estandar
  sin pruebas que lo verifiquen.
- No incluyas binarios ni otros artefactos generados en los cambios.

## CodeRabbit

Con la aplicacion de CodeRabbit instalada en GitHub, `.coderabbit.yaml`
habilita revisiones automaticas y analisis de los resultados de GitHub Checks.
Para pedir que repare un CI fallido en un pull request, comenta
`@coderabbitai fix-ci`; el modo predeterminado entrega la reparacion en un
pull request apilado para revision.

La reparacion automatica sin pedir el comando requiere habilitar **Auto-fix
PRs** en Triage de CodeRabbit. Esa automatizacion necesita Coding Agent y un
plan Team o superior, ademas de permisos de administrador de la organizacion.
