# Como se publica una version

## Version actual y fuente de verdad

La version se define en **un unico sitio**:

```
foc/src/driver/options.c  ->  FOC_options_print_version()
```

Es `0.1.0`. No hay `VERSION` ni constantes en cabeceras: es intencionado
(un proyecto pequeno con un solo Makefile). Cuando llegue la 1.0 habra
que generar el badge y los tarballs desde el tag.

## Cuando cambiar la version

- **PATCH** (x.y.Z): solo correcciones sin cambios observables de la
  CLI ni de la IR.
- **MINOR** (x.Y.0): nuevas capacidades visibles (p. ej. `switch`
  completo, `goto`, otro backend).
- **MAJOR** (X.0.0): 1.0 = conformidad declarada con un estandar de C.

Mientras el proyecto sea pre-alpha, los cambios de interfaz son posibles;
la disciplina de changelog es la que protege al usuario.

## Procedimiento

1. Todo el trabajo pasa por `main` (o PR equivalente). El arbol debe
   estar limpio; `make check` y la suite de integracion pasan.
2. Actualiza la version en `options.c`.
3. Actualiza `CHANGELOG.md`: mueve el bloque `[No publicado]` a la nueva
   version y anade el encabezado de seccion con fecha.
4. Revisa `docs/milestones.md`: quita lo que ya este hecho y anade lo
   pendiente conocido.
5. Commit: `chore(release): vX.Y.Z`.
6. Tag: `git tag -a vX.Y.Z -m "FOCC vX.Y.Z"`.
7. Push con los tags: `git push origin main --follow-tags`.
8. En GitHub: crea la release desde el tag y pega la seccion
   `CHANGELOG.md` correspondiente (+ notas de `KNOWN ISSUES`).
9. Si el CI genera artefactos (binarios), adjuntalos a la release.

## Despues de publicar

- Anade una seccion `## [No publicado]` vacia al inicio de
  `CHANGELOG.md`.
- Actualiza el badge/CI si la politica de soporte cambio.
- Verifica que `foc --version` reporta la nueva version.
