/* Gestión de ficheros fuente y posiciones. */
#ifndef FOC_SOURCE_H
#define FOC_SOURCE_H

#include "foc/foc.h"

typedef struct {
    int file_id; /* -1 si no hay fichero */
    uint32_t start;
    uint32_t end;
} FOCSpan;

typedef struct {
    int id;
    char *path;    /* ruta tal y como se introdujo */
    char *data;    /* contenido con '\0' final */
    size_t len;
    uint32_t *line_starts;
    size_t line_count;
} FOCSourceFile;

typedef struct {
    FOCArena *arena;
    FOCVec files; /* FOCSourceFile* */
} FOCSourceMgr;

void FOC_source_init(FOCSourceMgr *mgr, FOCArena *arena);
FOCSourceFile *FOC_source_add(FOCSourceMgr *mgr, const char *path, const char *data, size_t len);
FOCSourceFile *FOC_source_get(FOCSourceMgr *mgr, int id);
FOCSourceFile *FOC_source_by_path(FOCSourceMgr *mgr, const char *path);
size_t FOC_source_count(FOCSourceMgr *mgr);

/* Línea (1-based) que contiene el offset. */
size_t FOC_source_line_of(const FOCSourceFile *file, uint32_t offset);
/* Texto de una línea sin el salto final. */
const char *FOC_source_line_text(const FOCSourceFile *file, size_t line, size_t *out_len);
/* Columna (1-based, en caracteres UTF-8) del offset. */
size_t FOC_source_column_of(const FOCSourceFile *file, uint32_t offset);
uint32_t FOC_source_line_start(const FOCSourceFile *file, size_t line);

#endif /* FOC_SOURCE_H */
