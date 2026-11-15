# Cómo se publica una versión

## Versión actual y fuente de verdad

La versión se define en **un único sitio**:

```
foc/src/driver/options.c  →  FOC_options_print_version()
```

Es `0.1.0`. No hay `VERSION` ni constantes en cabeceras: es intencionado
(un proyecto pequeño con un solo Makefile). Cuando llegue la 1.0 habrá
que generar el badge y los tarballs desde el tag.

## Cuándo cambiar la versión

- **PATCH** (x.y.Z): solo correcciones sin cambios observables de la
  CLI ni de la IR.
- **MINOR** (x.Y.0): nuevas capacidades visibles (p. ej. `switch`
  completo, `goto`, otro backend).
- **MAJOR** (X.0.0): 1.0 = conformidad declarada con un estándar de C.

Mientras el proyecto sea pre-alpha, los cambios de interfaz son posibles;
la disciplina de changelog es la que protege al usuario.

## Procedimiento

1. Todo el trabajo pasa por `main` (o PR equivalente). El árbol debe
   estar limpio; `make check` y la suite de integración pasan.
2. Actualiza la versión en `options.c`.
3. Actualiza `CHANGELOG.md`: mueve el bloque `[No publicado]` a la nueva
   versión y añade el encabezado de sección con fecha.
4. Revisa `docs/milestones.md`: quita lo que ya esté hecho y añade lo
   pendiente conocido.
5. Commit: `chore(release): vX.Y.Z`.
6. Tag: `git tag -a vX.Y.Z -m "FOCC vX.Y.Z"`.
7. Push con los tags: `git push origin main --follow-tags`.
8. En GitHub: crea la release desde el tag y pega la sección
   `CHANGELOG.md` correspondiente (+ notas de `KNOWN ISSUES`).
9. Si el CI genera artefactos (binarios), adjúntalos a la release.

## Después de publicar

- Añade una sección `## [No publicado]` vacía al inicio de
  `CHANGELOG.md`.
- Actualiza el badge/CI si la política de soporte cambió.
- Verifica que `foc --version` reporta la nueva versión.
