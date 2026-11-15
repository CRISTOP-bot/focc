#include "foc/foc.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Arena                                                                */
/* ------------------------------------------------------------------ */

struct FOCArenaBlock {
    FOCArenaBlock *next;
    size_t used;
    size_t size;
    char data[];
};

#define FOC_ARENA_DEFAULT_BLOCK (64 * 1024)

void FOC_arena_init(FOCArena *arena) {
    arena->head = NULL;
    arena->total_bytes = 0;
    arena->block_size = FOC_ARENA_DEFAULT_BLOCK;
}

static void *arena_raw_alloc(FOCArena *arena, size_t size) {
    size_t block_size = arena->block_size;
    if (size > block_size / 2) {
        block_size = size + sizeof(FOCArenaBlock);
    }
    FOCArenaBlock *block = (FOCArenaBlock *)malloc(sizeof(FOCArenaBlock) + block_size);
    if (!block) {
        fprintf(stderr, "FOC: agotamiento de memoria\n");
        abort();
    }
    block->next = arena->head;
    /* La reserva queda contabilizada: el siguiente bloque empieza despues. */
    block->used = size;
    block->size = block_size;
    arena->head = block;
    arena->total_bytes += block_size;
    return block->data;
}

void *FOC_arena_alloc(FOCArena *arena, size_t size) {
    if (size == 0) {
        size = 1;
    }
    size = (size + 15u) & ~(size_t)15u; /* alineación a 16 bytes */
    FOCArenaBlock *block = arena->head;
    if (!block || block->used + size > block->size) {
        return arena_raw_alloc(arena, size);
    }
    void *ptr = block->data + block->used;
    block->used += size;
    return ptr;
}

void *FOC_arena_calloc(FOCArena *arena, size_t count, size_t size) {
    if (count != 0 && size > (size_t)-1 / count) {
        fprintf(stderr, "FOC: desbordamiento de tamaño en la reserva\n");
        abort();
    }
    size_t total = count * size;
    void *ptr = FOC_arena_alloc(arena, total);
    memset(ptr, 0, total ? total : 1);
    return ptr;
}

char *FOC_arena_strdup(FOCArena *arena, const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *copy = (char *)FOC_arena_alloc(arena, len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

char *FOC_arena_strndup(FOCArena *arena, const char *s, size_t n) {
    if (!s) {
        return NULL;
    }
    size_t len = 0;
    while (len < n && s[len]) {
        len++;
    }
    char *copy = (char *)FOC_arena_alloc(arena, len + 1);
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

char *FOC_arena_printf(FOCArena *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return FOC_arena_strdup(arena, "");
    }
    char *out = (char *)FOC_arena_alloc(arena, (size_t)needed + 1);
    vsnprintf(out, (size_t)needed + 1, fmt, args);
    va_end(args);
    return out;
}

void FOC_arena_free(FOCArena *arena) {
    FOCArenaBlock *block = arena->head;
    while (block) {
        FOCArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    arena->head = NULL;
    arena->total_bytes = 0;
}

/* ------------------------------------------------------------------ */
/* Vector                                                               */
/* ------------------------------------------------------------------ */

void FOC_vec_init(FOCVec *v, size_t elem_size) {
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
    v->elem_size = elem_size;
}

void *FOC_vec_push(FOCVec *v) {
    if (v->len == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 8;
        void *data = realloc(v->data, new_cap * v->elem_size);
        if (!data) {
            fprintf(stderr, "FOC: agotamiento de memoria\n");
            abort();
        }
        v->data = data;
        v->cap = new_cap;
    }
    void *slot = (char *)v->data + v->len * v->elem_size;
    memset(slot, 0, v->elem_size);
    v->len++;
    return slot;
}

void FOC_vec_clear(FOCVec *v) { v->len = 0; }

void FOC_vec_free(FOCVec *v) {
    free(v->data);
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

/* ------------------------------------------------------------------ */
/* Buffer                                                               */
/* ------------------------------------------------------------------ */

void FOC_buf_init(FOCBuf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void buf_reserve(FOCBuf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) {
        return;
    }
    size_t new_cap = b->cap ? b->cap : 64;
    while (new_cap < b->len + extra + 1) {
        new_cap *= 2;
    }
    char *data = (char *)realloc(b->data, new_cap);
    if (!data) {
        fprintf(stderr, "FOC: agotamiento de memoria\n");
        abort();
    }
    b->data = data;
    b->cap = new_cap;
}

void FOC_buf_append(FOCBuf *b, const char *s, size_t n) {
    if (!s || n == 0) {
        buf_reserve(b, 1);
        b->data[b->len] = '\0';
        return;
    }
    buf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void FOC_buf_puts(FOCBuf *b, const char *s) {
    if (s) {
        FOC_buf_append(b, s, strlen(s));
    }
}

void FOC_buf_putc(FOCBuf *b, char c) { FOC_buf_append(b, &c, 1); }

void FOC_buf_printf(FOCBuf *b, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return;
    }
    buf_reserve(b, (size_t)needed);
    vsnprintf(b->data + b->len, (size_t)needed + 1, fmt, args);
    b->len += (size_t)needed;
    va_end(args);
}

char *FOC_buf_take(FOCBuf *b) {
    if (!b->data) {
        buf_reserve(b, 1);
        b->data[0] = '\0';
    }
    char *data = b->data;
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    return data;
}

void FOC_buf_free(FOCBuf *b) {
    free(b->data);
    FOC_buf_init(b);
}

/* ------------------------------------------------------------------ */
/* Cadenas                                                              */
/* ------------------------------------------------------------------ */

char *FOC_strdup(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "FOC: agotamiento de memoria\n");
        abort();
    }
    memcpy(copy, s, len + 1);
    return copy;
}

char *FOC_strndup(const char *s, size_t n) {
    if (!s) {
        return NULL;
    }
    size_t len = 0;
    while (len < n && s[len]) {
        len++;
    }
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "FOC: agotamiento de memoria\n");
        abort();
    }
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

bool FOC_str_equal(const char *a, const char *b) {
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return false;
    }
    return strcmp(a, b) == 0;
}

bool FOC_str_starts_with(const char *s, const char *prefix) {
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

bool FOC_str_ends_with(const char *s, const char *suffix) {
    size_t ls = strlen(s);
    size_t lf = strlen(suffix);
    if (lf > ls) {
        return false;
    }
    return strcmp(s + ls - lf, suffix) == 0;
}

char *FOC_str_join(const char **parts, size_t count, const char *sep) {
    size_t sep_len = sep ? strlen(sep) : 0;
    size_t total = 1;
    for (size_t i = 0; i < count; i++) {
        total += parts[i] ? strlen(parts[i]) : 0;
        if (i + 1 < count) {
            total += sep_len;
        }
    }
    char *out = (char *)malloc(total);
    if (!out) {
        fprintf(stderr, "FOC: agotamiento de memoria\n");
        abort();
    }
    out[0] = '\0';
    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        if (parts[i]) {
            size_t n = strlen(parts[i]);
            memcpy(out + pos, parts[i], n);
            pos += n;
        }
        if (i + 1 < count && sep_len) {
            memcpy(out + pos, sep, sep_len);
            pos += sep_len;
        }
    }
    out[pos] = '\0';
    return out;
}
