/* foccount: cuenta tokens y lineas de un fichero fuente. */
#include "foc/diag.h"
#include "foc/fs.h"
#include "foc/lexer.h"
#include "foc/source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: foccount fichero.c ...\n");
        return 1;
    }
    FOCArena arena;
    FOC_arena_init(&arena);
    FOCSourceMgr sources;
    FOC_source_init(&sources, &arena);
    FOCDiagEngine diag;
    FOC_diag_init(&diag, &sources, &arena, stderr);

    for (int i = 1; i < argc; i++) {
        size_t length = 0;
        char *data = FOC_fs_read_file(argv[i], &length);
        if (!data) {
            fprintf(stderr, "foccount: no se puede leer '%s'\n", argv[i]);
            continue;
        }
        FOCSourceFile *file = FOC_source_add(&sources, argv[i], data, length);
        free(data);

        FOCTokenList tokens;
        FOC_token_list_init(&tokens);
        FOC_lex_file(file, &diag, &tokens);

        size_t identifiers = 0;
        size_t keywords = 0;
        size_t numbers = 0;
        size_t strings = 0;
        size_t punctuation = 0;
        for (size_t t = 0; t < tokens.len; t++) {
            switch (tokens.data[t].kind) {
            case FOC_TOK_IDENT:
                if (FOC_is_keyword(tokens.data[t].text, tokens.data[t].len)) {
                    keywords++;
                } else {
                    identifiers++;
                }
                break;
            case FOC_TOK_INT:
            case FOC_TOK_FLOAT:
            case FOC_TOK_CHAR:
                numbers++;
                break;
            case FOC_TOK_STRING:
                strings++;
                break;
            case FOC_TOK_PUNCT:
                punctuation++;
                break;
            default:
                break;
            }
        }
        printf("%s: lineas=%zu tokens=%zu identificadores=%zu palabras_clave=%zu numeros=%zu "
               "cadenas=%zu puntuacion=%zu\n",
               argv[i], file->line_count, tokens.len, identifiers, keywords, numbers, strings,
               punctuation);
        FOC_token_list_free(&tokens);
    }
    FOC_arena_free(&arena);
    return 0;
}
