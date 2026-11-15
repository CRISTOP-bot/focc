---
name: token-discipline
description: >
  Reglas de trabajo para el agente:消耗 mínimo de tokens, sin volcados de
  ficheros ni JSON, sin cambios fuera del encargo y con informes breves.
  Usar al iniciar cualquier tarea en el repositorio FOC.
---

# Disciplina de tokens y de alcance

Este repositorio es grande; el contexto es el recurso escaso. Estas reglas
evitan que el agente invierta miles de tokens en acciones que nadie pidió.

## Antes de actuar

1. **Define el objetivo en una frase.** Si la petición es ambigua, pregunta
   antes de tocar 40 ficheros.
2. **Localiza, no leas todo.** Usa `grep`/`glob` para encontrar el fichero y la
   función concretos. Lee el rango mínimo con `read` (`offset`/`limit`).
3. **No vuelques ficheros completos.** Jamás imprimas un fichero entero, un
   volcado JSON de un AST o de un AST completo en la salida: son decenas de
   miles de tokens sin valor.
4. **No generes JSON como formato de trabajo.** El JSON en el chat es
   ilegible y caro. Usa tablas cortas, listas o fragmentos de 5-20 líneas.

## Durante el trabajo

- Prefiere `edit` (cambio puntual) a reescribir un fichero completo.
- Si un cambio toca más de ~3 ficheros, es probablemente otro encargo:
  pregunta primero.
- No crees ficheros de andamiaje "por si acaso" (stubs, TODOs, exemplos
  vacíos). Cada fichero nuevo debe tener contenido real y usado.
- No añadas dependencias, opciones de CLI ni infraestructura que no se hayan
  pedido.
- No reformatees código ajeno al cambio ni "mejores" el estilo del
  vecindario sin motivo.
- No ejecutes comandos destructivos ni de red sin necesidad.

## Al informar

- Responde en español, en frases cortas.
- Di **qué** cambiaste, **por qué** y **cómo se verifica** (comando exacto y
  resultado esperado).
- Si algo quedó sin hacer, dilo explícitamente.
- No repitas el diff completo: resume las decisiones.

## Verificación

- Prefiere el ciclo mínimo: compilar el módulo tocado y ejecutar la prueba
  concreta.
- Usa las pruebas del repositorio (`make check`) antes de dar algo por bueno.
- Si una prueba falla, pega el mensaje de error relevante (10 líneas como
  máximo), no la salida completa.
