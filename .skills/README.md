# FOC - habilidades del proyecto.

Este directorio contiene las habilidades (`SKILL.md`) que describen como
trabajar en cada parte del compilador. Un agente debe cargar la habilidad
relevante antes de tocar el codigo correspondiente.

| Habilidad | Ambito |
|---|---|
| `token-discipline` | Reglas generales de alcance, consumo de tokens y comunicacion. |
| `foc-frontend` | Lexer, preprocesador, parser, AST, tipos y semantica. |
| `foc-backend` | Backend x86-64, ABI, marco de pila y emision de ensamblador. |
| `foc-testing` | Suite de pruebas, pruebas de regresion y depuracion. |

Cada habilidad es un fichero `SKILL.md` con metadatos YAML (`name`,
`description`) para que las herramientas puedan indexarlas.
