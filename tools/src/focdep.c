/* focdep: genera las dependencias de un fichero fuente. */
#include "foc/diag.h"
#include "foc/fs.h"
#include "foc/lexer.h"
#include "foc/preproc.h"
#include "foc/source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *output = NULL;
    const char *input = NULL;
    FOCVec includes;
    FOC_vec_init(&includes, sizeof(char *));

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            output = argv[++i];
            continue;
        }
        if (!strncmp(argv[i], "-I", 2)) {
            const char *dir = argv[i][2] ? argv[i] + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (dir) {
                char **slot = (char **)FOC_vec_push(&includes);
                *slot = FOC_strdup(dir);
            }
            continue;
        }
        if (argv[i][0] == '-') {
            continue;
        }
        input = argv[i];
    }

    if (!input) {
        fprintf(stderr, "uso: focdep [-o fichero] [-Idir] fuente.c\n");
        return 1;
    }

    FOCArena arena;
    FOC_arena_init(&arena);
    FOCSourceMgr sources;
    FOC_source_init(&sources, &arena);
    FOCDiagEngine diag;
    FOC_diag_init(&diag, &sources, &arena, stderr);

    size_t length = 0;
    char *data = FOC_fs_read_file(input, &length);
    if (!data) {
        fprintf(stderr, "focdep: no se puede leer '%s'\n", input);
        return 1;
    }
    FOCSourceFile *file = FOC_source_add(&sources, input, data, length);
    free(data);

    FOCPreprocOptions options;
    FOC_preproc_options_init(&options);
    for (size_t i = 0; i < includes.len; i++) {
        char **slot = (char **)FOC_vec_push(&options.include_paths);
        *slot = *(char **)FOC_VEC_AT(&includes, char *, i);
    }

    FOCTokenList tokens;
    FOC_token_list_init(&tokens);
    FOC_preprocess(file, &sources, &diag, &options, &tokens);
    FOC_token_list_free(&tokens);
    FOC_preproc_options_free(&options);

    FOCBuf out;
    FOC_buf_init(&out);
    FOC_buf_puts(&out, input);
    FOC_buf_putc(&out, ':');
    for (size_t i = 1; i < sources.files.len; i++) {
        const FOCSourceFile *dep = *(FOCSourceFile **)FOC_VEC_AT(&sources.files, FOCSourceFile *, i);
        FOC_buf_putc(&out, ' ');
        FOC_buf_puts(&out, dep->path);
    }
    FOC_buf_putc(&out, '\n');

    if (output) {
        FOC_fs_write_file(output, out.data, out.len);
    } else {
        fwrite(out.data, 1, out.len, stdout);
    }
    FOC_buf_free(&out);
    FOC_arena_free(&arena);
    FOC_vec_free(&includes);
    return 0;
}
