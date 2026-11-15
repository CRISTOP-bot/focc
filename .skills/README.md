# FOC — habilidades del proyecto.

Este directorio contiene las habilidades (`SKILL.md`) que describen cómo
trabajar en cada parte del compilador. Un agente debe cargar la habilidad
relevante antes de tocar el código correspondiente.

| Habilidad | Ámbito |
|---|---|
| `token-discipline` | Reglas generales de alcance, consumo de tokens y comunicación. |
| `foc-frontend` | Lexer, preprocesador, parser, AST, tipos y semántica. |
| `foc-backend` | Backend x86-64, ABI, marco de pila y emisión de ensamblador. |
| `foc-testing` | Suite de pruebas, pruebas de regresión y depuración. |

Cada habilidad es un fichero `SKILL.md` con metadatos YAML (`name`,
`description`) para que las herramientas puedan indexarlas.
