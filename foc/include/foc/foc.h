/* FOC - compilador de C y C++.
 *
 * Utilidades comunes: arena de memoria, vectores dinámicos y buffers.
 */
#ifndef FOC_H
#define FOC_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Arena de memoria                                                     */
/* ------------------------------------------------------------------ */

typedef struct FOCArenaBlock FOCArenaBlock;

typedef struct {
    FOCArenaBlock *head;
    size_t total_bytes;
    size_t block_size;
} FOCArena;

void FOC_arena_init(FOCArena *arena);
void *FOC_arena_alloc(FOCArena *arena, size_t size);
void *FOC_arena_calloc(FOCArena *arena, size_t count, size_t size);
char *FOC_arena_strdup(FOCArena *arena, const char *s);
char *FOC_arena_strndup(FOCArena *arena, const char *s, size_t n);
char *FOC_arena_printf(FOCArena *arena, const char *fmt, ...);
void FOC_arena_free(FOCArena *arena);

/* ------------------------------------------------------------------ */
/* Vector dinámico                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    void *data;
    size_t len;
    size_t cap;
    size_t elem_size;
} FOCVec;

void FOC_vec_init(FOCVec *v, size_t elem_size);
/* Añade un elemento cero y devuelve un puntero al hueco reservado. */
void *FOC_vec_push(FOCVec *v);
void FOC_vec_clear(FOCVec *v);
void FOC_vec_free(FOCVec *v);

#define FOC_VEC_AT(v, type, i) (&((type *)(v)->data)[(i)])
#define FOC_VEC_LEN(v) ((v)->len)

/* ------------------------------------------------------------------ */
/* Buffer de texto                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} FOCBuf;

void FOC_buf_init(FOCBuf *b);
void FOC_buf_append(FOCBuf *b, const char *s, size_t n);
void FOC_buf_puts(FOCBuf *b, const char *s);
void FOC_buf_putc(FOCBuf *b, char c);
void FOC_buf_printf(FOCBuf *b, const char *fmt, ...);
char *FOC_buf_take(FOCBuf *b); /* transfiere la propiedad; el buffer queda vacío */
void FOC_buf_free(FOCBuf *b);

/* ------------------------------------------------------------------ */
/* Cadenas auxiliary                                                    */
/* ------------------------------------------------------------------ */

char *FOC_strdup(const char *s);
char *FOC_strndup(const char *s, size_t n);
bool FOC_str_equal(const char *a, const char *b);
bool FOC_str_starts_with(const char *s, const char *prefix);
bool FOC_str_ends_with(const char *s, const char *suffix);
/* Une `count` cadenas con `sep`; reserva con malloc. */
char *FOC_str_join(const char **parts, size_t count, const char *sep);

#endif /* FOC_H */
