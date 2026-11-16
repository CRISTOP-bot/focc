# Politica de seguridad

## Versiones soportadas

FOCC esta en **pre-alpha**. No hay versiones estables ni ramas con
manutencion de seguridad: el esfuerzo va a llegar primero a la correccion
del compilador y despues a la estabilidad.

| Version | Soporte                     |
| ------- | --------------------------- |
| main    | ultima fotografia del desarrollo |
| demas   | ninguna                     |

## Como reportar una vulnerabilidad

Si encuentras un fallo **en FOCC** (no en el codigo que compilas con
FOCC), reportalo con:

1. Una descripcion del problema y del impacto esperado.
2. Un caso minimo reproducible (fichero fuente, linea de ordenes, salida).
3. La version o el commit afectado (`foc --version`, `git rev-parse HEAD`).

Canales, en orden de preferencia:

- **GitHub**: usa un reporte privado en
  https://github.com/CRISTOP-bot/focc/security/advisories/new
- Si no tienes acceso al repositorio, abre un issue publico indicando
  solo que existe una vulnerabilidad y que necesitas un canal privado.

Por favor, no publiques publicamente detalles de una vulnerabilidad sin
que exista un arreglo o un plan acordado.

## Que entra en el alcance

- Fallos de memoria, desbordamientos o uso despues de liberado dentro
  del compilador (`foc`) o sus herramientas (`focdep`, `foccount`).
- Bugs que hagan que FOCC acepte codigo que en realidad no es valido C
  y produzca ejecutables con comportamiento malicioso no detectado.
- Fallos en la generacion de codigo que alteren el comportamiento de
  programas bien definidos sin diagnostico.

## Que queda fuera

- El codigo que FOCC compila y sus propias vulnerabilidades (un bug de
  tu programa compilado con FOCC es tu bug; FOCC no ofrece garantia de
  seguridad del binario resultante).
- Ausencia de `goto`, `switch` incompleto, falta de ABI System V: son
  limitaciones conocidas y documentadas, no vulnerabilidades.

## Cadencia de respuesta

Pre-alpha, sin compromiso de SLA: lo habitual es responder en dias, no
en horas. Los reportes se procesan por orden de entrada y de gravedad.
