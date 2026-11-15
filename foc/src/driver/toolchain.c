/* Integracion con el ensamblador y el enlazador del sistema.
 *
 * Esta interfaz es deliberadamente pequena para poder sustituirla por una
 * implementacion propia (escritura directa de ELF/COFF/Mach-O) sin tocar el
 * resto del compilador.
 */
#include "foc/options.h"
#include "foc/proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void log_command(FILE *log, const char *program, char **args, size_t count) {
    if (!log) {
        return;
    }
    fprintf(log, "%s", program);
    for (size_t i = 0; i < count; i++) {
        fprintf(log, " %s", args[i]);
    }
    fputc('\n', log);
}

/* Busca una herramienta utilizable: debe existir en el PATH y responder a
 * --version. Esto evita elegir un fichero con el nombre pero no ejecutable. */
static const char *find_tool(const char **candidates) {
    for (size_t i = 0; candidates[i]; i++) {
        char *path = NULL;
        if (!FOC_proc_which(candidates[i], &path)) {
            continue;
        }
        free(path);
        char *args[2];
        args[0] = (char *)"--version";
        args[1] = NULL;
        FOCProcResult probe = FOC_proc_run(candidates[i], args, 1);
        /* Una herramienta valida responde a --version con alguma salida. */
        bool ok = !probe.failed && probe.output && *probe.output;
        free(probe.output);
        free(probe.error);
        if (ok) {
            return candidates[i];
        }
    }
    return NULL;
}

/* Ensambla `asm_path` en `object_path`. */
bool FOC_toolchain_assemble(const FOCOptions *options, const char *asm_path,
                             const char *object_path, FILE *log) {
    static const char *assemblers[] = {"as", "gas", "cc", "gcc", "clang", NULL};
    const char *assembler = find_tool(assemblers);
    if (!assembler) {
        fprintf(stderr, "foc: no se encuentra un ensamblador (as)\n");
        return false;
    }
    char *args[8];
    size_t count = 0;
    bool use_driver = strcmp(assembler, "as") != 0;
    if (use_driver) {
        args[count++] = (char *)"-c";
    } else {
        args[count++] = (char *)"--64";
    }
    args[count++] = (char *)"-o";
    args[count++] = (char *)object_path;
    args[count++] = (char *)asm_path;
    args[count] = NULL;

    log_command(log, assembler, args, count);
    FOCProcResult result = FOC_proc_run(assembler, args, count);
    if (result.failed) {
        fprintf(stderr, "foc: fallo al ensamblar '%s'\n", asm_path);
        if (result.output && *result.output) {
            fputs(result.output, stderr);
        }
        free(result.output);
        free(result.error);
        return false;
    }
    free(result.output);
    free(result.error);
    (void)options;
    return true;
}

/* Enlaza los objetos en `output`. */
bool FOC_toolchain_link(const FOCOptions *options, const char *output, char **objects,
                        size_t object_count, FILE *log) {
    static const char *candidates[] = {"cc", "gcc", "clang", "ld", NULL};
    const char *linker = find_tool(candidates);
    if (!linker) {
        fprintf(stderr, "foc: no se encuentra un enlazador (cc/gcc/clang)\n");
        return false;
    }

    size_t capacity = object_count + 16 + options->libraries.len * 2;
    char **args = (char **)calloc(capacity, sizeof(char *));
    if (!args) {
        return false;
    }
    size_t count = 0;
    args[count++] = (char *)"-o";
    args[count++] = (char *)output;
    if (options->static_link) {
        args[count++] = (char *)"-static";
    }
    if (options->shared) {
        args[count++] = (char *)"-shared";
    }
    if (options->nostdlib) {
        args[count++] = (char *)"-nostdlib";
    }
    for (size_t i = 0; i < options->library_paths.len; i++) {
        args[count++] = (char *)"-L";
        args[count++] = *(char **)FOC_VEC_AT(&options->library_paths, char *, i);
    }
    for (size_t i = 0; i < object_count; i++) {
        args[count++] = objects[i];
    }
    for (size_t i = 0; i < options->libraries.len; i++) {
        char *flag = (char *)malloc(strlen(*(char **)FOC_VEC_AT(&options->libraries, char *, i)) + 3);
        if (!flag) {
            break;
        }
        sprintf(flag, "-l%s", *(char **)FOC_VEC_AT(&options->libraries, char *, i));
        args[count++] = flag;
    }
    args[count] = NULL;

    log_command(log, linker, args, count);
    FOCProcResult result = FOC_proc_run(linker, args, count);
    bool ok = !result.failed;
    if (!ok) {
        fprintf(stderr, "foc: fallo al enlazar\n");
        if (result.output && *result.output) {
            fputs(result.output, stderr);
        }
    }
    free(result.output);
    free(result.error);
    free(args);
    return ok;
}
