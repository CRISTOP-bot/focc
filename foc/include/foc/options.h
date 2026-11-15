/* Opciones de la linea de ordenes de FOC. */
#ifndef FOC_OPTIONS_H
#define FOC_OPTIONS_H

#include "foc/diag.h"
#include "foc/foc.h"
#include "foc/platform.h"

typedef enum {
    FOC_MODE_LINK = 0,  /* genera un ejecutable */
    FOC_MODE_COMPILE,   /* -c: genera un objeto */
    FOC_MODE_ASSEMBLY,  /* -S: genera ensamblador */
    FOC_MODE_PREPROCESS /* -E: solo preprocesa */
} FOCOutputMode;

typedef enum { FOC_OPT_O0 = 0, FOC_OPT_O1, FOC_OPT_O2, FOC_OPT_O3, FOC_OPT_OS, FOC_OPT_OZ } FOCOptLevel;

typedef struct {
    FOCOutputMode mode;
    FOCOptLevel opt_level;
    const char *output;
    const char *target_triple;
    const char *std;
    bool verbose;
    bool debug_info;
    bool nostdlib;
    bool static_link;
    bool shared;
    bool emit_ir;
    bool dump_ast;
    bool dump_tokens;
    bool dump_ir;
    bool no_warnings;
    bool warnings_as_errors;
    bool color_auto;
    bool color_always;
    bool color_never;
    bool json_diagnostics;
    bool dependency_only;
    bool print_version;
    bool print_help;
    const char *language; /* -x */
    const char *dep_file;

    FOCVec inputs;      /* char* */
    FOCVec include_paths; /* char* */
    FOCVec defines;     /* char* */
    FOCVec undefines;   /* char* */
    FOCVec libraries;   /* char* (-l) */
    FOCVec library_paths; /* char* (-L) */
    FOCVec system_include_paths;
} FOCOptions;

void FOC_options_init(FOCOptions *options);
void FOC_options_free(FOCOptions *options);
/* Analiza los argumentos. Devuelve false si hay un error de uso. */
bool FOC_options_parse(FOCOptions *options, int argc, char **argv, FOCDiagEngine *diag);
void FOC_options_print_help(FILE *out);
void FOC_options_print_version(FILE *out);

/* Ensambla y enlaza mediante las herramientas del sistema. */
bool FOC_toolchain_assemble(const FOCOptions *options, const char *asm_path,
                             const char *object_path, FILE *log);
/* Preprocesa ensamblador con CPP o GNU M4 y escribe el resultado en un fichero. */
bool FOC_toolchain_preprocess_assembly(const FOCOptions *options, const char *input_path,
                                       const char *output_path, bool use_m4, FILE *log);
bool FOC_toolchain_link(const FOCOptions *options, const char *output, char **objects,
                        size_t object_count, FILE *log);

#endif /* FOC_OPTIONS_H */
