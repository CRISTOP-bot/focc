# Configuracion de OpenCode para FOC.
#
# Este directorio contiene las definiciones propias del proyecto: agentes
# especializados y comandos reutilizables.

Agentes disponibles:
  - `foc-reviewer`   Revisa cambios del compilador contra las reglas de C11
                     y la arquitectura documentada.
  - `foc-tester`     Ejecuta la suite de pruebas y analiza los fallos.

Comandos disponibles:
  - /build           Configura y construye el compilador.
  - /check           Ejecuta las pruebas unitarias y de integracion.
  - /explain-backend Explica como se genera el codigo de una funcion.
