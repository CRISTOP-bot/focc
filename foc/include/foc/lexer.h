/* Lexer de C. */
#ifndef FOC_LEXER_H
#define FOC_LEXER_H

#include "foc/diag.h"
#include "foc/source.h"

typedef enum {
    FOC_TOK_INVALID = 0,
    FOC_TOK_EOF,
    FOC_TOK_NEWLINE,
    FOC_TOK_IDENT,
    FOC_TOK_INT,
    FOC_TOK_FLOAT,
    FOC_TOK_CHAR,
    FOC_TOK_STRING,
    FOC_TOK_PUNCT
} FOCTokenKind;

typedef struct {
    FOCTokenKind kind;
    const char *text;
    uint32_t len;
    uint32_t start;
    uint32_t end;
    int file_id;
    bool at_line_start;
    bool space_before;
    bool expanded; /* producido por expansión de macro */
} FOCToken;

typedef struct {
    FOCToken *data;
    size_t len;
    size_t cap;
} FOCTokenList;

void FOC_token_list_init(FOCTokenList *list);
void FOC_token_list_push(FOCTokenList *list, FOCToken token);
void FOC_token_list_free(FOCTokenList *list);

/* Tokeniza un fichero completo. */
void FOC_lex_file(const FOCSourceFile *file, FOCDiagEngine *diag, FOCTokenList *out);

/* ¿El token es el texto dado? */
bool FOC_tok_is(const FOCToken *tok, const char *text);
/* ¿El token es la palabra clave dada? */
bool FOC_tok_is_kw(const FOCToken *tok, const char *kw);
/* Copia el texto del token como cadena terminada en NUL (malloc). */
char *FOC_tok_text_dup(const FOCToken *tok);

/* Palabras clave de C99 (base de las demás versiones). */
bool FOC_is_keyword(const char *text, size_t len);

#endif /* FOC_LEXER_H */
