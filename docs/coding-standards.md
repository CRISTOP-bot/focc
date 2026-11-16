# Estandares de codigo

- Go idiomatico: `gofmt`, `go vet`, nombres en minusculas para paquetes.
- Paquetes pequenos con una responsabilidad clara.
- Errores envueltos con contexto (`fmt.Errorf("...: %w", err)`).
- Diagnosticos con `Span`, nunca imprimir directamente desde el frontend.
- Sin dependencias externas salvo decision documentada.
- Cada correccion de error incluye una prueba de regresion.
- Los tests confirman el comportamiento, no solo que compila.
- Comentarios en espanol para decisiones de diseno; nombres de codigo en
  ingles cuando correspondan a conceptos del estandar de C/C++.
