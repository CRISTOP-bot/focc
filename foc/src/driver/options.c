#include "foc/options.h"

#include "foc/diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void FOC_options_init(FOCOptions *options) {
    memset(options, 0, sizeof(*options));
    options->mode = FOC_MODE_LINK;
    options->opt_level = FOC_OPT_O0;
    options->std = "c17";
    FOC_vec_init(&options->inputs, sizeof(char *));
    FOC_vec_init(&options->include_paths, sizeof(char *));
    FOC_vec_init(&options->defines, sizeof(char *));
    FOC_vec_init(&options->undefines, sizeof(char *));
    FOC_vec_init(&options->libraries, sizeof(char *));
    FOC_vec_init(&options->library_paths, sizeof(char *));
    FOC_vec_init(&options->system_include_paths, sizeof(char *));
}

void FOC_options_free(FOCOptions *options) {
    FOC_vec_free(&options->inputs);
    FOC_vec_free(&options->include_paths);
    FOC_vec_free(&options->defines);
    FOC_vec_free(&options->undefines);
    FOC_vec_free(&options->libraries);
    FOC_vec_free(&options->library_paths);
    FOC_vec_free(&options->system_include_paths);
}

static void push_option(FOCVec *vec, const char *text) {
    char **slot = (char **)FOC_vec_push(vec);
    *slot = FOC_strdup(text);
}

void FOC_options_print_version(FILE *out) {
    fprintf(out, "FOC (Fucking Optimized C Compiler) %s\n", "0.1.0");
    fprintf(out, "Compilador de C independiente, sin frontend ni backend de terceros.\n");
}

void FOC_options_print_help(FILE *out) {
    fprintf(out,
            "Uso: foc [opciones] fichero...\n"
            "\n"
            "Opciones generales:\n"
            "  -o <fichero>        Escribe la salida en <fichero>\n"
            "  -c                  Compila y ensambla, pero no enlaza\n"
            "  -S                  Compila, pero no ensambla\n"
            "  -E                  Solo preprocesa\n"
            "  -I <dir>            Anade <dir> a la ruta de includes\n"
            "  -D <macro[=valor]>  Define una macro de preprocesador\n"
            "  -U <macro>          Elimina una macro de preprocesador\n"
            "  -L <dir>            Anade <dir> a la ruta de bibliotecas\n"
            "  -l <biblioteca>     Enlaza con <biblioteca>\n"
            "  -O0 -O1 -O2 -O3 -Os -Oz  Nivel de optimizacion\n"
            "  -g                  Genera informacion de depuracion\n"
            "  -std=<estandar>     Estandar de C (c89, c99, c11, c17, c23)\n"
            "  -Wall -Wextra       Activa avisos\n"
            "  -Werror             Convierte los avisos en errores\n"
            "  -w                  Desactiva los avisos\n"
            "  -nostdlib           No enlaza la biblioteca estandar\n"
            "  -static             Enlace estatico\n"
            "  -shared             Produce una biblioteca compartida\n"
            "  -x <lenguaje>       Fuerza c, c++, assembler, assembler-with-cpp o m4\n"
            "  -M -MM              Genera las dependencias del fichero\n"
            "  -MF <fichero>       Escribe las dependencias en <fichero>\n"
            "  -v                  Salida detallada\n"
            "\n"
            "Opciones propias:\n"
            "  --target=<triple>   Compila para otro destino\n"
            "  --emit-ir           Escribe la IR intermedia en <fichero>.ir\n"
            "  --dump-tokens       Muestra los tokens del preprocesador\n"
            "  --dump-ast          Muestra el AST\n"
            "  --dump-ir           Muestra la IR\n"
            "  --color=<modo>      auto, always o never\n"
            "  --json-diagnostics  Diagnosticos en formato JSON\n"
            "  --version           Muestra la version\n"
            "  --help              Muestra esta ayuda\n");
}

static bool parse_optimization(FOCOptions *options, const char *flag) {
    if (!strcmp(flag, "-O0")) {
        options->opt_level = FOC_OPT_O0;
    } else if (!strcmp(flag, "-O1")) {
        options->opt_level = FOC_OPT_O1;
    } else if (!strcmp(flag, "-O2")) {
        options->opt_level = FOC_OPT_O2;
    } else if (!strcmp(flag, "-O3")) {
        options->opt_level = FOC_OPT_O3;
    } else if (!strcmp(flag, "-Os")) {
        options->opt_level = FOC_OPT_OS;
    } else if (!strcmp(flag, "-Oz")) {
        options->opt_level = FOC_OPT_OZ;
    } else if (!strcmp(flag, "-O")) {
        options->opt_level = FOC_OPT_O1;
    } else {
        return false;
    }
    return true;
}

bool FOC_options_parse(FOCOptions *options, int argc, char **argv, FOCDiagEngine *diag) {
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (!strcmp(arg, "-o")) {
            if (i + 1 >= argc) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0001", "falta el argumento de -o");
                return false;
            }
            options->output = argv[++i];
            continue;
        }
        if (!strncmp(arg, "-o", 2) && arg[2]) {
            options->output = arg + 2;
            continue;
        }
        if (!strcmp(arg, "-c")) {
            options->mode = FOC_MODE_COMPILE;
            continue;
        }
        if (!strcmp(arg, "-S")) {
            options->mode = FOC_MODE_ASSEMBLY;
            continue;
        }
        if (!strcmp(arg, "-E")) {
            options->mode = FOC_MODE_PREPROCESS;
            continue;
        }
        if (!strcmp(arg, "-I")) {
            if (i + 1 >= argc) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0001", "falta el argumento de -I");
                return false;
            }
            push_option(&options->include_paths, argv[++i]);
            continue;
        }
        if (!strncmp(arg, "-I", 2) && arg[2]) {
            push_option(&options->include_paths, arg + 2);
            continue;
        }
        if (!strcmp(arg, "-D")) {
            if (i + 1 >= argc) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0001", "falta el argumento de -D");
                return false;
            }
            push_option(&options->defines, argv[++i]);
            continue;
        }
        if (!strncmp(arg, "-D", 2) && arg[2]) {
            push_option(&options->defines, arg + 2);
            continue;
        }
        if (!strcmp(arg, "-U")) {
            if (i + 1 >= argc) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0001", "falta el argumento de -U");
                return false;
            }
            push_option(&options->undefines, argv[++i]);
            continue;
        }
        if (!strncmp(arg, "-U", 2) && arg[2]) {
            push_option(&options->undefines, arg + 2);
            continue;
        }
        if (!strcmp(arg, "-L")) {
            if (i + 1 >= argc) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0001", "falta el argumento de -L");
                return false;
            }
            push_option(&options->library_paths, argv[++i]);
            continue;
        }
        if (!strncmp(arg, "-L", 2) && arg[2]) {
            push_option(&options->library_paths, arg + 2);
            continue;
        }
        if (!strcmp(arg, "-l")) {
            if (i + 1 >= argc) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0001", "falta el argumento de -l");
                return false;
            }
            push_option(&options->libraries, argv[++i]);
            continue;
        }
        if (!strncmp(arg, "-l", 2) && arg[2]) {
            push_option(&options->libraries, arg + 2);
            continue;
        }
        if (parse_optimization(options, arg)) {
            continue;
        }
        if (!strncmp(arg, "-std=", 5)) {
            options->std = arg + 5;
            continue;
        }
        if (!strcmp(arg, "-g") || !strncmp(arg, "-g", 2)) {
            options->debug_info = true;
            continue;
        }
        if (!strcmp(arg, "-v")) {
            options->verbose = true;
            continue;
        }
        if (!strcmp(arg, "-w")) {
            options->no_warnings = true;
            continue;
        }
        if (!strcmp(arg, "-Werror")) {
            options->warnings_as_errors = true;
            continue;
        }
        if (!strcmp(arg, "-Wall") || !strcmp(arg, "-Wextra") || !strncmp(arg, "-W", 2)) {
            continue; /* los avisos se gestionan de forma global por ahora */
        }
        if (!strcmp(arg, "-nostdlib")) {
            options->nostdlib = true;
            continue;
        }
        if (!strcmp(arg, "-static")) {
            options->static_link = true;
            continue;
        }
        if (!strcmp(arg, "-shared")) {
            options->shared = true;
            continue;
        }
        if (!strcmp(arg, "-x")) {
            if (i + 1 >= argc) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0001", "falta el argumento de -x");
                return false;
            }
            options->language = argv[++i];
            continue;
        }
        if (!strcmp(arg, "-M") || !strcmp(arg, "-MM")) {
            options->dependency_only = true;
            continue;
        }
        if (!strcmp(arg, "-MF")) {
            if (i + 1 >= argc) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0001", "falta el argumento de -MF");
                return false;
            }
            options->dep_file = argv[++i];
            continue;
        }
        if (!strncmp(arg, "--target=", 9)) {
            options->target_triple = arg + 9;
            continue;
        }
        if (!strcmp(arg, "--emit-ir")) {
            options->emit_ir = true;
            continue;
        }
        if (!strcmp(arg, "--dump-tokens")) {
            options->dump_tokens = true;
            continue;
        }
        if (!strcmp(arg, "--dump-ast")) {
            options->dump_ast = true;
            continue;
        }
        if (!strcmp(arg, "--dump-ir")) {
            options->dump_ir = true;
            continue;
        }
        if (!strncmp(arg, "--color=", 8)) {
            const char *mode = arg + 8;
            if (!strcmp(mode, "always")) {
                options->color_always = true;
            } else if (!strcmp(mode, "never")) {
                options->color_never = true;
            } else {
                options->color_auto = true;
            }
            continue;
        }
        if (!strcmp(arg, "--json-diagnostics")) {
            options->json_diagnostics = true;
            continue;
        }
        if (!strcmp(arg, "--version") || !strcmp(arg, "-v")) {
            options->print_version = true;
            continue;
        }
        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            options->print_help = true;
            continue;
        }
        if (arg[0] == '-' && arg[1]) {
            FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0002", "opcion desconocida: '%s'", arg);
            return false;
        }
        push_option(&options->inputs, arg);
    }

    if (options->inputs.len == 0 && !options->print_help && !options->print_version) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0003", "no se ha indicado ningun fichero");
        return false;
    }
    return true;
}
