# Estándares de código

- Go idiomático: `gofmt`, `go vet`, nombres en minúsculas para paquetes.
- Paquetes pequeños con una responsabilidad clara.
- Errores envueltos con contexto (`fmt.Errorf("...: %w", err)`).
- Diagnósticos con `Span`, nunca imprimir directamente desde el frontend.
- Sin dependencias externas salvo decisión documentada.
- Cada corrección de error incluye una prueba de regresión.
- Los tests confirman el comportamiento, no solo que compila.
- Comentarios en español para decisiones de diseño; nombres de código en
  inglés cuando correspondan a conceptos del estándar de C/C++.
