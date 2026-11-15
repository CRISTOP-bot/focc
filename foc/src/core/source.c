#include "foc/source.h"

#include <stdlib.h>
#include <string.h>

void FOC_source_init(FOCSourceMgr *mgr, FOCArena *arena) {
    mgr->arena = arena;
    FOC_vec_init(&mgr->files, sizeof(FOCSourceFile *));
}

FOCSourceFile *FOC_source_add(FOCSourceMgr *mgr, const char *path, const char *data, size_t len) {
    FOCSourceFile *file = (FOCSourceFile *)FOC_arena_calloc(mgr->arena, 1, sizeof(FOCSourceFile));
    file->id = (int)mgr->files.len;
    file->path = FOC_arena_strdup(mgr->arena, path ? path : "<memoria>");
    file->len = len;
    file->data = (char *)FOC_arena_alloc(mgr->arena, len + 1);
    if (len && data) {
        memcpy(file->data, data, len);
    }
    file->data[len] = '\0';

    /* Índice de líneas. */
    size_t count = 1;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            count++;
        }
    }
    file->line_starts = (uint32_t *)FOC_arena_alloc(mgr->arena, count * sizeof(uint32_t));
    file->line_count = count;
    size_t idx = 0;
    file->line_starts[idx++] = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            file->line_starts[idx++] = (uint32_t)(i + 1);
        }
    }

    FOCSourceFile **slot = (FOCSourceFile **)FOC_vec_push(&mgr->files);
    *slot = file;
    return file;
}

FOCSourceFile *FOC_source_get(FOCSourceMgr *mgr, int id) {
    if (id < 0 || (size_t)id >= mgr->files.len) {
        return NULL;
    }
    return *(FOCSourceFile **)FOC_VEC_AT(&mgr->files, FOCSourceFile *, (size_t)id);
}

FOCSourceFile *FOC_source_by_path(FOCSourceMgr *mgr, const char *path) {
    for (size_t i = 0; i < mgr->files.len; i++) {
        FOCSourceFile *f = *(FOCSourceFile **)FOC_VEC_AT(&mgr->files, FOCSourceFile *, i);
        if (FOC_str_equal(f->path, path)) {
            return f;
        }
    }
    return NULL;
}

size_t FOC_source_count(FOCSourceMgr *mgr) { return mgr->files.len; }

size_t FOC_source_line_of(const FOCSourceFile *file, uint32_t offset) {
    if (!file || file->line_count == 0) {
        return 0;
    }
    if (offset > file->len) {
        offset = (uint32_t)file->len;
    }
    size_t lo = 0;
    size_t hi = file->line_count - 1;
    while (lo < hi) {
        size_t mid = (lo + hi + 1) / 2;
        if (file->line_starts[mid] <= offset) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo; /* 0-based */
}

uint32_t FOC_source_line_start(const FOCSourceFile *file, size_t line) {
    if (!file || line >= file->line_count) {
        return 0;
    }
    return file->line_starts[line];
}

const char *FOC_source_line_text(const FOCSourceFile *file, size_t line, size_t *out_len) {
    if (!file || line >= file->line_count) {
        if (out_len) {
            *out_len = 0;
        }
        return "";
    }
    uint32_t start = file->line_starts[line];
    uint32_t end = (line + 1 < file->line_count) ? file->line_starts[line + 1] : (uint32_t)file->len;
    while (end > start && (file->data[end - 1] == '\n' || file->data[end - 1] == '\r')) {
        end--;
    }
    if (out_len) {
        *out_len = end - start;
    }
    return file->data + start;
}

size_t FOC_source_column_of(const FOCSourceFile *file, uint32_t offset) {
    if (!file) {
        return 0;
    }
    size_t line = FOC_source_line_of(file, offset);
    uint32_t start = FOC_source_line_start(file, line);
    if (offset < start) {
        offset = start;
    }
    size_t col = 1;
    for (uint32_t i = start; i < offset; i++) {
        /* Cuenta UTF-8: los bytes de continuación noUME. */
        if (((unsigned char)file->data[i] & 0xC0) != 0x80) {
            col++;
        }
    }
    return col;
}
