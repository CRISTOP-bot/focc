#include <stdlib.h>
/* Pruebas del preprocesador. */
#include "test.h"

#include "foc/diag.h"
#include "foc/preproc.h"
#include "foc/source.h"

static char *preprocess_text(const char *text, size_t *out_len) {
    FOCArena arena;
    FOC_arena_init(&arena);
    FOCSourceMgr sources;
    FOC_source_init(&sources, &arena);
    FOCDiagEngine diag;
    FOC_diag_init(&diag, &sources, &arena, stderr);

    FOCSourceFile *file = FOC_source_add(&sources, "<test>", text, strlen(text));
    FOCPreprocOptions options;
    FOC_preproc_options_init(&options);

    FOCTokenList tokens;
    FOC_token_list_init(&tokens);
    FOC_preprocess(file, &sources, &diag, &options, &tokens);

    FOCBuf buffer;
    FOC_buf_init(&buffer);
    for (size_t i = 0; i < tokens.len; i++) {
        if (tokens.data[i].kind == FOC_TOK_NEWLINE || tokens.data[i].kind == FOC_TOK_EOF) {
            continue;
        }
        if (tokens.data[i].space_before) {
            FOC_buf_putc(&buffer, ' ');
        }
        FOC_buf_append(&buffer, tokens.data[i].text, tokens.data[i].len);
    }
    FOC_token_list_free(&tokens);
    FOC_preproc_options_free(&options);
    char *result = FOC_buf_take(&buffer);
    if (out_len) {
        *out_len = buffer.len;
    }
    FOC_arena_free(&arena);
    return result;
}

void test_preproc(void) {
    foc_test_begin("preprocesador");

    size_t length = 0;
    char *result = preprocess_text("#define VALOR 42\nint x = VALOR;", &length);
    CHECK_STR(result, "int x = 42;");
    free(result);

    /* Macro de funcion. Los tokens se conservan tal cual del cuerpo: la
     * expansion preserva el espacio del texto original. */
    result = preprocess_text("#define SUM(a, b) ((a) + (b))\nint y = SUM(1, 2);", &length);
    CHECK_STR(result, "int y = ((1) + ( 2));");
    free(result);

    /* Condicionales. */
    result = preprocess_text("#define SI 1\n#if SI\nint activo;\n#else\nint inactivo;\n#endif", &length);
    CHECK_STR(result, "int activo;");
    free(result);

    result = preprocess_text("#if 0\nint no;\n#else\nint si;\n#endif", &length);
    CHECK_STR(result, "int si;");
    free(result);

    /* Operador # y ## . */
    result = preprocess_text("#define TEXTO(x) #x\nchar *s = TEXTO(hola);", &length);
    CHECK_STR(result, "char *s = \"hola\";");
    free(result);

    result = preprocess_text("#define CONCAT(a, b) a##b\nint ab = 1;", &length);
    CHECK_STR(result, "int ab = 1;");
    free(result);

    /* Macro predefinida. */
    result = preprocess_text("int linea = __LINE__;", &length);
    CHECK_STR(result, "int linea = 1;");
    free(result);
}
