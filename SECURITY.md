# Política de seguridad

## Versiones soportadas

FOCC está en **pre-alpha**. No hay versiones estables ni ramas con
manutención de seguridad: el esfuerzo va a llegar primero a la corrección
del compilador y después a la estabilidad.

| Versión | Soporte                     |
| ------- | --------------------------- |
| main    | última fotografía del desarrollo |
| demás   | ninguna                     |

## Cómo reportar una vulnerabilidad

Si encuentras un fallo **en FOCC** (no en el código que compilas con
FOCC), repórtalo con:

1. Una descripción del problema y del impacto esperado.
2. Un caso mínimo reproducible (fichero fuente, línea de órdenes, salida).
3. La versión o el commit afectado (`foc --version`, `git rev-parse HEAD`).

Canales, en orden de preferencia:

- **GitHub**: usa un reporte privado en
  https://github.com/CRISTOP-bot/focc/security/advisories/new
- Si no tienes acceso al repositorio, abre un issue público indicando
  solo que existe una vulnerabilidad y que necesitas un canal privado.

Por favor, no publiques públicamente detalles de una vulnerabilidad sin
que exista un arreglo o un plan acordado.

## Qué entra en el alcance

- Fallos de memoria, desbordamientos o uso después de liberado dentro
  del compilador (`foc`) o sus herramientas (`focdep`, `foccount`).
- Bugs que hagan que FOCC acepte código que en realidad no es válido C
  y produzca ejecutables con comportamiento malicioso no detectado.
- Fallos en la generación de código que alteren el comportamiento de
  programas bien definidos sin diagnóstico.

## Qué queda fuera

- El código que FOCC compila y sus propias vulnerabilidades (un bug de
  tu programa compilado con FOCC es tu bug; FOCC no ofrece garantía de
  seguridad del binario resultante).
- Ausencia de `goto`, `switch` incompleto, falta de ABI System V: son
  limitaciones conocidas y documentadas, no vulnerabilidades.

## Cadencia de respuesta

Pre-alpha, sin compromiso de SLA: lo habitual es responder en días, no
en horas. Los reportes se procesan por orden de entrada y de gravedad.
