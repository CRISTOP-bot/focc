#include "foc/diag.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RESET "\033[0m"

void FOC_diag_init(FOCDiagEngine *engine, FOCSourceMgr *sources, FOCArena *arena, FILE *out) {
    engine->sources = sources;
    engine->arena = arena;
    engine->out = out ? out : stderr;
    engine->color = FOC_COLOR_AUTO;
    engine->json = false;
    engine->warnings_as_errors = false;
    engine->no_warn = false;
    engine->error_count = 0;
    engine->warning_count = 0;
    engine->fatal_count = 0;
}

const char *FOC_severity_name(FOCSeverity sev) {
    switch (sev) {
    case FOC_SEV_FATAL:
        return "fatal error";
    case FOC_SEV_ERROR:
        return "error";
    case FOC_SEV_WARNING:
        return "warning";
    case FOC_SEV_NOTE:
        return "note";
    case FOC_SEV_HELP:
        return "help";
    default:
        return "unknown";
    }
}

bool FOC_diag_has_errors(const FOCDiagEngine *engine) {
    return engine->error_count > 0 || engine->fatal_count > 0;
}

static bool color_enabled(const FOCDiagEngine *engine) {
    switch (engine->color) {
    case FOC_COLOR_ALWAYS:
        return true;
    case FOC_COLOR_NEVER:
        return false;
    default:
        break;
    }
    if (getenv("NO_COLOR")) {
        return false;
    }
    const char *term = getenv("TERM");
    if (term && strcmp(term, "dumb") == 0) {
        return false;
    }
#if defined(_WIN32)
    /* En Windows solo se colorea si el destino parece una consola real. */
    return engine->out == stderr || engine->out == stdout;
#else
    return isatty(fileno(engine->out)) != 0;
#endif
}

static void json_escape(FILE *out, const char *s) {
    if (!s) {
        return;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':
            fputs("\\\"", out);
            break;
        case '\\':
            fputs("\\\\", out);
            break;
        case '\n':
            fputs("\\n", out);
            break;
        case '\r':
            fputs("\\r", out);
            break;
        case '\t':
            fputs("\\t", out);
            break;
        default:
            if (*p < 0x20) {
                fprintf(out, "\\u%04x", *p);
            } else {
                fputc(*p, out);
            }
        }
    }
}

static void render_json(FOCDiagEngine *engine, const FOCDiagnostic *diag) {
    FILE *out = engine->out;
    fputs("{\"severity\":\"", out);
    fputs(FOC_severity_name(diag->severity), out);
    fputs("\"", out);
    if (diag->code) {
        fputs(",\"code\":\"", out);
        json_escape(out, diag->code);
        fputs("\"", out);
    }
    fputs(",\"message\":\"", out);
    json_escape(out, diag->message);
    fputs("\"", out);
    FOCSourceFile *file = NULL;
    if (diag->span.file_id >= 0 && engine->sources) {
        file = FOC_source_get(engine->sources, diag->span.file_id);
    }
    if (file) {
        size_t line = FOC_source_line_of(file, diag->span.start) + 1;
        size_t col = FOC_source_column_of(file, diag->span.start);
        fputs(",\"file\":\"", out);
        json_escape(out, file->path);
        fprintf(out, "\",\"line\":%zu,\"column\":%zu,\"length\":%u", line, col,
                diag->span.end - diag->span.start);
    }
    if (diag->note_count > 0) {
        fputs(",\"notes\":[", out);
        for (size_t i = 0; i < diag->note_count; i++) {
            if (i) {
                fputc(',', out);
            }
            fputs("{\"message\":\"", out);
            json_escape(out, diag->notes[i].message);
            fputs("\"", out);
            if (diag->notes[i].span.file_id >= 0 && engine->sources) {
                FOCSourceFile *nfile = FOC_source_get(engine->sources, diag->notes[i].span.file_id);
                if (nfile) {
                    size_t line = FOC_source_line_of(nfile, diag->notes[i].span.start) + 1;
                    size_t col = FOC_source_column_of(nfile, diag->notes[i].span.start);
                    fputs(",\"file\":\"", out);
                    json_escape(out, nfile->path);
                    fprintf(out, "\",\"line\":%zu,\"column\":%zu", line, col);
                }
            }
            fputc('}', out);
        }
        fputc(']', out);
    }
    fputs("}\n", out);
}

/* Acceso perezoso al gestor de fuentes desde el renderizador. */
void FOC_diag_report(FOCDiagEngine *engine, FOCDiagnostic diag) {
    if (diag.severity == FOC_SEV_WARNING && engine->no_warn) {
        return;
    }
    if (engine->warnings_as_errors && diag.severity == FOC_SEV_WARNING) {
        diag.severity = FOC_SEV_ERROR;
    }
    switch (diag.severity) {
    case FOC_SEV_ERROR:
        engine->error_count++;
        break;
    case FOC_SEV_FATAL:
        engine->fatal_count++;
        break;
    case FOC_SEV_WARNING:
        engine->warning_count++;
        break;
    default:
        break;
    }

    if (engine->json) {
        render_json(engine, &diag);
        return;
    }

    const char *reset = "";
    const char *bold = "";
    const char *cyan = "";
    const char *head_color = "";
    switch (diag.severity) {
    case FOC_SEV_FATAL:
    case FOC_SEV_ERROR:
        head_color = COLOR_RED;
        break;
    case FOC_SEV_WARNING:
        head_color = COLOR_YELLOW;
        break;
    default:
        head_color = COLOR_CYAN;
        break;
    }
    if (color_enabled(engine)) {
        reset = COLOR_RESET;
        bold = COLOR_BOLD;
        cyan = COLOR_CYAN;
    }

    FILE *out = engine->out;
    fputs(bold, out);
    fputs(head_color, out);
    fputs(FOC_severity_name(diag.severity), out);
    if (diag.code && *diag.code) {
        fputc('[', out);
        fputs(diag.code, out);
        fputc(']', out);
    }
    fputs(": ", out);
    fputs(reset, out);
    fputs(diag.message ? diag.message : "", out);
    fputc('\n', out);

    FOCSourceFile *file = NULL;
    if (diag.span.file_id >= 0 && engine->sources) {
        file = FOC_source_get(engine->sources, diag.span.file_id);
    }
    if (!file) {
        return;
    }

    size_t line_index = FOC_source_line_of(file, diag.span.start);
    size_t line = line_index + 1;
    size_t col = FOC_source_column_of(file, diag.span.start);
    fprintf(out, "%s  --> %s:%zu:%zu%s\n", cyan, file->path, line, col, reset);

    size_t text_len = 0;
    const char *text = FOC_source_line_text(file, line_index, &text_len);
    if (text_len == 0) {
        return;
    }

    char number[24];
    snprintf(number, sizeof(number), "%zu", line);
    size_t pad = strlen(number);
    fprintf(out, "%s%*s |%s\n", cyan, (int)pad, "", reset);
    fprintf(out, "%s%s | %.*s\n", cyan, number, (int)text_len, text);

    /* Marcador de posición. */
    uint32_t start = file->line_starts[line_index];
    size_t offset = diag.span.start > start ? diag.span.start - start : 0;
    size_t end = diag.span.end > start ? diag.span.end - start : offset;
    if (end > text_len) {
        end = text_len;
    }
    if (end < offset) {
        end = offset;
    }
    size_t width = end - offset;
    if (width == 0) {
        width = 1;
    }
    fprintf(out, "%s%*s | ", cyan, (int)pad, "");
    for (size_t i = 0; i < offset; i++) {
        fputc(' ', out);
    }
    fputs(head_color, out);
    fputc('^', out);
    for (size_t i = 1; i < width; i++) {
        fputc('~', out);
    }
    fputs(reset, out);
    fputc('\n', out);
    fprintf(out, "%s%*s |%s\n", cyan, (int)pad, "", reset);

    for (size_t i = 0; i < diag.note_count; i++) {
        FOCSourceFile *nfile = NULL;
        if (diag.notes[i].span.file_id >= 0 && engine->sources) {
            nfile = FOC_source_get(engine->sources, diag.notes[i].span.file_id);
        }
        if (nfile) {
            size_t nline = FOC_source_line_of(nfile, diag.notes[i].span.start) + 1;
            size_t ncol = FOC_source_column_of(nfile, diag.notes[i].span.start);
            fprintf(out, "%s  = note: %s (%s:%zu:%zu)%s\n", cyan, diag.notes[i].message, nfile->path,
                    nline, ncol, reset);
        } else {
            fprintf(out, "%s  = note: %s%s\n", cyan, diag.notes[i].message, reset);
        }
    }
}

static char *format_message(FOCArena *arena, const char *fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        return FOC_arena_strdup(arena, "");
    }
    char *out = (char *)FOC_arena_alloc(arena, (size_t)needed + 1);
    vsnprintf(out, (size_t)needed + 1, fmt, args);
    return out;
}

void FOC_diag_error(FOCDiagEngine *engine, FOCSpan span, const char *code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FOCDiagnostic diag;
    memset(&diag, 0, sizeof(diag));
    diag.severity = FOC_SEV_ERROR;
    diag.code = (char *)code;
    diag.message = format_message(engine->arena, fmt, args);
    diag.span = span;
    va_end(args);
    FOC_diag_report(engine, diag);
}

void FOC_diag_warning(FOCDiagEngine *engine, FOCSpan span, const char *code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FOCDiagnostic diag;
    memset(&diag, 0, sizeof(diag));
    diag.severity = FOC_SEV_WARNING;
    diag.code = (char *)code;
    diag.message = format_message(engine->arena, fmt, args);
    diag.span = span;
    va_end(args);
    FOC_diag_report(engine, diag);
}

void FOC_diag_note(FOCDiagEngine *engine, FOCSpan span, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FOCDiagnostic diag;
    memset(&diag, 0, sizeof(diag));
    diag.severity = FOC_SEV_NOTE;
    diag.message = format_message(engine->arena, fmt, args);
    diag.span = span;
    va_end(args);
    FOC_diag_report(engine, diag);
}

void FOC_diag_fatal(FOCDiagEngine *engine, FOCSpan span, const char *code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FOCDiagnostic diag;
    memset(&diag, 0, sizeof(diag));
    diag.severity = FOC_SEV_FATAL;
    diag.code = (char *)code;
    diag.message = format_message(engine->arena, fmt, args);
    diag.span = span;
    va_end(args);
    FOC_diag_report(engine, diag);
}
