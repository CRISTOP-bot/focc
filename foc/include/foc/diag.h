/* Diagnósticos del compilador. */
#ifndef FOC_DIAG_H
#define FOC_DIAG_H

#include "foc/source.h"

typedef enum {
    FOC_SEV_FATAL = 0,
    FOC_SEV_ERROR,
    FOC_SEV_WARNING,
    FOC_SEV_NOTE,
    FOC_SEV_HELP
} FOCSeverity;

typedef struct {
    char *message;
    FOCSpan span;
} FOCDiagNote;

typedef struct {
    FOCSeverity severity;
    char *code;
    char *message;
    FOCSpan span;
    FOCDiagNote *notes;
    size_t note_count;
} FOCDiagnostic;

typedef enum { FOC_COLOR_AUTO = 0, FOC_COLOR_ALWAYS, FOC_COLOR_NEVER } FOCColorMode;

typedef struct {
    FOCSourceMgr *sources;
    FOCArena *arena;
    FILE *out; /* destino de los diagnósticos (stderr) */
    FOCColorMode color;
    bool json;
    bool warnings_as_errors;
    bool no_warn;
    int error_count;
    int warning_count;
    int fatal_count;
} FOCDiagEngine;

void FOC_diag_init(FOCDiagEngine *engine, FOCSourceMgr *sources, FOCArena *arena, FILE *out);
void FOC_diag_report(FOCDiagEngine *engine, FOCDiagnostic diag);
void FOC_diag_error(FOCDiagEngine *engine, FOCSpan span, const char *code, const char *fmt, ...);
void FOC_diag_warning(FOCDiagEngine *engine, FOCSpan span, const char *code, const char *fmt, ...);
void FOC_diag_note(FOCDiagEngine *engine, FOCSpan span, const char *fmt, ...);
void FOC_diag_fatal(FOCDiagEngine *engine, FOCSpan span, const char *code, const char *fmt, ...);
bool FOC_diag_has_errors(const FOCDiagEngine *engine);
const char *FOC_severity_name(FOCSeverity sev);

#endif /* FOC_DIAG_H */
