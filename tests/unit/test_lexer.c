/* Pruebas del lexer. */
#include "test.h"

#include "foc/diag.h"
#include "foc/lexer.h"
#include "foc/source.h"

static FOCTokenList lex_text(const char *text) {
    static FOCArena arena;
    static FOCSourceMgr sources;
    static FOCDiagEngine diag;
    FOC_arena_init(&arena);
    FOC_source_init(&sources, &arena);
    FOC_diag_init(&diag, &sources, &arena, stderr);

    FOCSourceFile *file = FOC_source_add(&sources, "<test>", text, strlen(text));
    FOCTokenList tokens;
    FOC_token_list_init(&tokens);
    FOC_lex_file(file, &diag, &tokens);
    return tokens;
}

void test_lexer(void) {
    foc_test_begin("lexer");

    /* Identificadores y palabras clave. */
    FOCTokenList tokens = lex_text("int variable = 42;");
    CHECK(tokens.len > 0);
    CHECK(tokens.data[0].kind == FOC_TOK_IDENT);
    CHECK(FOC_tok_is(&tokens.data[0], "int"));
    CHECK(FOC_is_keyword(tokens.data[0].text, tokens.data[0].len));
    CHECK(tokens.data[1].kind == FOC_TOK_IDENT);
    CHECK(!FOC_is_keyword(tokens.data[1].text, tokens.data[1].len));
    CHECK(tokens.data[3].kind == FOC_TOK_INT);
    FOC_token_list_free(&tokens);

    /* Numeros. */
    tokens = lex_text("0x1F 077 42 3.5 1e10");
    CHECK(tokens.data[0].kind == FOC_TOK_INT);
    CHECK(tokens.data[1].kind == FOC_TOK_INT);
    CHECK(tokens.data[2].kind == FOC_TOK_INT);
    CHECK(tokens.data[3].kind == FOC_TOK_FLOAT);
    CHECK(tokens.data[4].kind == FOC_TOK_FLOAT);
    FOC_token_list_free(&tokens);

    /* Cadenas y caracteres. */
    tokens = lex_text("\"hola\" 'a' '\\n'");
    CHECK(tokens.data[0].kind == FOC_TOK_STRING);
    CHECK(tokens.data[1].kind == FOC_TOK_CHAR);
    CHECK(tokens.data[2].kind == FOC_TOK_CHAR);
    FOC_token_list_free(&tokens);

    /* Operadores multicaracter. */
    tokens = lex_text("-> ++ << >>= == && ...");
    CHECK(FOC_tok_is(&tokens.data[0], "->"));
    CHECK(FOC_tok_is(&tokens.data[1], "++"));
    CHECK(FOC_tok_is(&tokens.data[2], "<<"));
    CHECK(FOC_tok_is(&tokens.data[3], ">>="));
    CHECK(FOC_tok_is(&tokens.data[4], "=="));
    CHECK(FOC_tok_is(&tokens.data[5], "&&"));
    CHECK(FOC_tok_is(&tokens.data[6], "..."));
    FOC_token_list_free(&tokens);

    /* Los comentarios se ignoran pero no separan identificadores. */
    tokens = lex_text("a/* x */b");
    CHECK(tokens.len == 3);
    CHECK(FOC_tok_is(&tokens.data[0], "a"));
    CHECK(FOC_tok_is(&tokens.data[1], "b"));
    FOC_token_list_free(&tokens);

    /* El operador de punto y coma no se confunde con el digito. */
    tokens = lex_text("x;y");
    CHECK(FOC_tok_is(&tokens.data[1], ";"));
    FOC_token_list_free(&tokens);
}
