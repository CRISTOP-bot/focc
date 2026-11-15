/* Driver de FOC: orquesta el pipeline de compilacion. */
#include "foc/driver.h"

#include "foc/backend.h"
#include "foc/fs.h"
#include "foc/ir.h"
#include "foc/lexer.h"
#include "foc/optimizer.h"
#include "foc/options.h"
#include "foc/parser.h"
#include "foc/preproc.h"
#include "foc/proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *language_of(const FOCOptions *options, const char *path) {
    if (options->language) {
        return options->language;
    }
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return "c";
    }
    if (!strcmp(dot, ".c")) {
        return "c";
    }
    if (!strcmp(dot, ".i")) {
        return "c";
    }
    if (!strcmp(dot, ".s")) {
        return "assembler";
    }
    if (!strcmp(dot, ".S")) {
        return "assembler-with-cpp";
    }
    if (!strcmp(dot, ".m4")) {
        return "m4";
    }
    if (!strcmp(dot, ".cpp") || !strcmp(dot, ".cc") || !strcmp(dot, ".cxx") ||
        !strcmp(dot, ".C") || !strcmp(dot, ".hpp")) {
        return "c++";
    }
    return "c";
}

static void dump_tokens(const FOCTokenList *tokens) {
    for (size_t i = 0; i < tokens->len; i++) {
        const FOCToken *tok = &tokens->data[i];
        switch (tok->kind) {
        case FOC_TOK_IDENT:
            printf("ident  %.*s\n", (int)tok->len, tok->text);
            break;
        case FOC_TOK_INT:
            printf("int    %.*s\n", (int)tok->len, tok->text);
            break;
        case FOC_TOK_FLOAT:
            printf("float  %.*s\n", (int)tok->len, tok->text);
            break;
        case FOC_TOK_CHAR:
            printf("char   %.*s\n", (int)tok->len, tok->text);
            break;
        case FOC_TOK_STRING:
            printf("string %.*s\n", (int)tok->len, tok->text);
            break;
        case FOC_TOK_PUNCT:
            printf("punct  %.*s\n", (int)tok->len, tok->text);
            break;
        case FOC_TOK_NEWLINE:
            printf("nl\n");
            break;
        case FOC_TOK_EOF:
            printf("eof\n");
            break;
        default:
            printf("other  %.*s\n", (int)tok->len, tok->text);
            break;
        }
    }
}

static void dump_ir(const FOCIRModule *module) {
    static const char *const names[] = {"const",   "constf", "load",   "store",  "add",   "sub",
                                        "mul",     "div",    "mod",    "and",    "or",    "xor",
                                        "shl",     "shr",    "neg",    "bitnot","not",   "eq",
                                        "ne",      "lt",     "le",     "gt",     "ge",    "cast",
                                        "call",    "ret",    "jmp",    "branch", "label", "alloca",
                                        "getelem","frame",  "global", "field",  "copy"};
    for (size_t f = 0; f < module->function_count; f++) {
        const FOCIRFunction *function = module->functions[f];
        printf("fn %s -> %s (%zu params)\n", function->name,
               FOC_ir_type_name(function->ret_type), function->param_count);
        for (size_t i = 0; i < function->inst_count; i++) {
            const FOCIRInst *inst = FOC_ir_inst_at((FOCIRFunction *)function, i);
            const char *name = (inst->op >= 0 && (size_t)inst->op < sizeof(names) / sizeof(names[0]))
                                   ? names[inst->op]
                                   : "?";
            printf("  %-8s %%v%d", name, inst->dest);
            if (inst->op == FOC_OP_CONST) {
                printf(" = %lld", inst->lhs.imm);
            } else if (inst->op == FOC_OP_LABEL || inst->op == FOC_OP_JMP) {
                printf(" .L%d", inst->label);
            } else if (inst->op == FOC_OP_BRANCH) {
                printf(" .L%d", inst->label);
            } else if (inst->op == FOC_OP_CALL && inst->symbol) {
                printf(" %s", inst->symbol);
            }
            printf("\n");
        }
    }
}

static bool is_assembly_language(const char *language) {
    return !strcmp(language, "assembler") || !strcmp(language, "asm") ||
           !strcmp(language, "assembler-with-cpp") || !strcmp(language, "asm-with-cpp") ||
           !strcmp(language, "m4");
}

static char *temporary_assembly_path(const char *object_path) {
    static unsigned long sequence;
    size_t capacity = strlen(object_path) + 64;
    for (size_t attempt = 0; attempt < 1000; attempt++) {
        char *path = (char *)malloc(capacity);
        if (!path) {
            return NULL;
        }
        snprintf(path, capacity, "%s.foc-tmp-%lu.s", object_path, ++sequence);
        if (!FOC_fs_exists(path)) {
            return path;
        }
        free(path);
    }
    return NULL;
}

static bool compile_assembly_one(FOCOptions *options, FOCDiagEngine *diag, const char *input,
                                 const char *language, char **out_object) {
    bool use_cpp = !strcmp(language, "assembler-with-cpp") || !strcmp(language, "asm-with-cpp");
    bool use_m4 = !strcmp(language, "m4");
    bool preprocess = use_cpp || use_m4;

    if (options->dependency_only) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0011",
                       "no se generan dependencias para entradas de ensamblador");
        return false;
    }

    if (options->mode == FOC_MODE_PREPROCESS || options->mode == FOC_MODE_ASSEMBLY) {
        char *output_path = options->output ? FOC_strdup(options->output)
                                            : FOC_fs_replace_extension(input, ".s");
        if (!output_path) {
            FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0012",
                           "no se pudo determinar la salida para '%s'", input);
            return false;
        }

        bool ok;
        if (preprocess) {
            ok = FOC_toolchain_preprocess_assembly(options, input, output_path, use_m4,
                                                   options->verbose ? stdout : NULL);
        } else {
            size_t length = 0;
            char *data = FOC_fs_read_file(input, &length);
            ok = data && FOC_fs_write_file(output_path, data, length);
            if (!data) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0006", "no se puede leer '%s'", input);
            } else if (!ok) {
                FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0013",
                               "no se pudo escribir '%s'", output_path);
            }
            free(data);
        }
        if (!ok && preprocess) {
            FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0014",
                           "no se pudo preprocesar '%s'", input);
        }
        free(output_path);
        return ok;
    }

    char *object_path = FOC_fs_replace_extension(input, ".o");
    if (options->mode == FOC_MODE_COMPILE && options->output) {
        free(object_path);
        object_path = FOC_strdup(options->output);
    }
    if (!object_path) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0012",
                       "no se pudo determinar el objeto de '%s'", input);
        return false;
    }

    char *asm_path;
    if (preprocess) {
        asm_path = temporary_assembly_path(object_path);
    } else {
        asm_path = FOC_strdup(input);
    }
    if (!asm_path) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0012",
                       "no se pudo preparar la entrada '%s'", input);
        free(object_path);
        return false;
    }

    bool ok = true;
    if (preprocess) {
        ok = FOC_toolchain_preprocess_assembly(options, input, asm_path, use_m4,
                                               options->verbose ? stdout : NULL);
        if (!ok) {
            FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0014",
                           "no se pudo preprocesar '%s'", input);
        }
    }
    if (ok) {
        ok = FOC_toolchain_assemble(options, asm_path, object_path,
                                    options->verbose ? stdout : NULL);
        if (!ok) {
            FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0008",
                           "no se pudo ensamblar '%s'", input);
        }
    }
    if (preprocess) {
        remove(asm_path);
    }
    free(asm_path);
    if (!ok) {
        free(object_path);
        return false;
    }
    if (out_object) {
        *out_object = object_path;
    } else {
        free(object_path);
    }
    return true;
}

/* Compila un fichero fuente hasta el objeto o el ejecutable. */
static bool compile_one(FOCOptions *options, FOCDiagEngine *diag, FOCSourceMgr *sources,
                        const FOCTarget *target, const char *input, char **out_object) {
    const char *language = language_of(options, input);
    if (is_assembly_language(language)) {
        return compile_assembly_one(options, diag, input, language, out_object);
    }
    if (strcmp(language, "c") != 0 && strcmp(language, "c++") != 0) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0004", "lenguaje no soportado: '%s'", language);
        return false;
    }
    if (strcmp(language, "c++") == 0) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0005",
                       "el frontend de C++ todavia no esta disponible");
        return false;
    }

    size_t length = 0;
    char *data = FOC_fs_read_file(input, &length);
    if (!data) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0006", "no se puede leer '%s'", input);
        return false;
    }
    FOCSourceFile *file = FOC_source_add(sources, input, data, length);
    free(data);

    /* Preprocesado. */
    FOCPreprocOptions preproc_options;
    FOC_preproc_options_init(&preproc_options);
    preproc_options.target = *target;
    for (size_t i = 0; i < options->include_paths.len; i++) {
        char **slot = (char **)FOC_vec_push(&preproc_options.include_paths);
        *slot = *(char **)FOC_VEC_AT(&options->include_paths, char *, i);
    }
    for (size_t i = 0; i < options->defines.len; i++) {
        char **slot = (char **)FOC_vec_push(&preproc_options.defines);
        *slot = *(char **)FOC_VEC_AT(&options->defines, char *, i);
    }
    for (size_t i = 0; i < options->undefines.len; i++) {
        char **slot = (char **)FOC_vec_push(&preproc_options.undefines);
        *slot = *(char **)FOC_VEC_AT(&options->undefines, char *, i);
    }
    for (size_t i = 0; i < options->system_include_paths.len; i++) {
        char **slot = (char **)FOC_vec_push(&preproc_options.sys_include_paths);
        *slot = *(char **)FOC_VEC_AT(&options->system_include_paths, char *, i);
    }

    FOCTokenList tokens;
    FOC_token_list_init(&tokens);
    FOC_preprocess(file, sources, diag, &preproc_options, &tokens);
    FOC_preproc_options_free(&preproc_options);

    if (options->dump_tokens) {
        dump_tokens(&tokens);
    }

    /* Dependencias. */
    if (options->dependency_only) {
        FILE *out = stdout;
        if (options->dep_file) {
            out = fopen(options->dep_file, "w");
            if (!out) {
                out = stdout;
            }
        }
        fprintf(out, "%s:", input);
        for (size_t i = 1; i < sources->files.len; i++) {
            const FOCSourceFile *dep = *(FOCSourceFile **)FOC_VEC_AT(&sources->files,
                                                                   FOCSourceFile *, i);
            fprintf(out, " %s", dep->path);
        }
        fputc('\n', out);
        if (out != stdout) {
            fclose(out);
        }
        FOC_token_list_free(&tokens);
        return true;
    }

    if (options->mode == FOC_MODE_PREPROCESS) {
        FOCBuf out;
        FOC_buf_init(&out);
        for (size_t i = 0; i < tokens.len; i++) {
            const FOCToken *tok = &tokens.data[i];
            if (tok->kind == FOC_TOK_EOF || tok->kind == FOC_TOK_NEWLINE) {
                continue;
            }
            if (tok->space_before) {
                FOC_buf_putc(&out, ' ');
            }
            FOC_buf_append(&out, tok->text, tok->len);
        }
        FOC_buf_putc(&out, '\n');
        if (options->output) {
            FOC_fs_write_file(options->output, out.data, out.len);
        } else {
            fwrite(out.data, 1, out.len, stdout);
        }
        FOC_buf_free(&out);
        FOC_token_list_free(&tokens);
        return true;
    }

    /* Analisis sintactico y semantico. */
    FOCUnit *unit = NULL;
    FOC_parse(&tokens, diag, &unit);
    FOC_token_list_free(&tokens);
    if (!unit) {
        return false;
    }

    FOCIRModule *module = FOC_irgen(unit, diag, sources, target);
    if (options->dump_ir) {
        dump_ir(module);
    }
    if (FOC_diag_has_errors(diag)) {
        return false;
    }

    FOC_optimize(module, options->opt_level);

    /* Generacion de ensamblador. */
    char *assembly = NULL;
    if (!FOC_emit_x86_64(module, target, &assembly, NULL)) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0007",
                       "no hay backend para el destino '%s'", target->os == FOC_OS_WINDOWS ? "windows" : "posix");
        return false;
    }

    char *base = FOC_fs_basename(input);
    char *asm_path = FOC_fs_replace_extension(input, ".s");
    if (options->mode == FOC_MODE_ASSEMBLY) {
        if (options->output) {
            FOC_fs_write_file(options->output, assembly, strlen(assembly));
        } else {
            FOC_fs_write_file(asm_path, assembly, strlen(assembly));
        }
        free(asm_path);
        free(base);
        free(assembly);
        return true;
    }

    char *object_path = FOC_fs_replace_extension(input, ".o");
    if (options->mode == FOC_MODE_COMPILE && options->output) {
        free(object_path);
        object_path = FOC_strdup(options->output);
    }
    FOC_fs_write_file(asm_path, assembly, strlen(assembly));
    free(assembly);

    bool ok = FOC_toolchain_assemble(options, asm_path, object_path, options->verbose ? stdout : NULL);
    free(asm_path);
    if (!ok) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0008", "no se pudo ensamblar '%s'", input);
        free(object_path);
        free(base);
        return false;
    }
    free(base);
    if (out_object) {
        *out_object = object_path;
    } else {
        free(object_path);
    }
    return true;
}

int FOC_driver_run(FOCOptions *options, FOCDiagEngine *diag, FOCSourceMgr *sources) {
    FOCTarget target;
    if (options->target_triple) {
        if (!FOC_target_parse(options->target_triple, &target)) {
            FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0009", "triple de destino invalido: '%s'",
                           options->target_triple);
            return 1;
        }
    } else {
        target = FOC_default_target();
    }

    if (options->verbose) {
        char *triple = FOC_target_triple(&target);
        fprintf(stderr, "foc: destino %s\n", triple);
        free(triple);
    }

    FOCVec objects;
    FOC_vec_init(&objects, sizeof(char *));

    for (size_t i = 0; i < options->inputs.len; i++) {
        const char *input = *(char **)FOC_VEC_AT(&options->inputs, char *, i);
        char *object = NULL;
        if (!compile_one(options, diag, sources, &target, input, &object)) {
            FOC_vec_free(&objects);
            return 1;
        }
        if (object) {
            char **slot = (char **)FOC_vec_push(&objects);
            *slot = object;
        }
    }

    if (options->mode != FOC_MODE_LINK) {
        FOC_vec_free(&objects);
        return FOC_diag_has_errors(diag) ? 1 : 0;
    }

    /* Enlazado. */
    char *output = options->output ? FOC_strdup(options->output)
                                   : FOC_strdup(target.os == FOC_OS_WINDOWS ? "a.exe" : "a.out");
    char **object_list = (char **)objects.data;
    bool ok = FOC_toolchain_link(options, output, object_list, objects.len,
                                 options->verbose ? stdout : NULL);
    free(output);
    FOC_vec_free(&objects);
    if (!ok) {
        FOC_diag_error(diag, (FOCSpan){-1, 0, 0}, "E0010", "no se pudo enlazar");
        return 1;
    }
    return FOC_diag_has_errors(diag) ? 1 : 0;
}
