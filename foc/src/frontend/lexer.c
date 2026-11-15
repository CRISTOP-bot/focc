#include "foc/lexer.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Lista de tokens                                                     */
/* ------------------------------------------------------------------ */

void FOC_token_list_init(FOCTokenList *list) {
    list->data = NULL;
    list->len = 0;
    list->cap = 0;
}

void FOC_token_list_push(FOCTokenList *list, FOCToken token) {
    if (list->len == list->cap) {
        size_t new_cap = list->cap ? list->cap * 2 : 64;
        FOCToken *data = (FOCToken *)realloc(list->data, new_cap * sizeof(FOCToken));
        if (!data) {
            fprintf(stderr, "foc: agotamiento de memoria\n");
            abort();
        }
        list->data = data;
        list->cap = new_cap;
    }
    list->data[list->len++] = token;
}

void FOC_token_list_free(FOCTokenList *list) {
    free(list->data);
    FOC_token_list_init(list);
}

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

bool FOC_tok_is(const FOCToken *tok, const char *text) {
    size_t len = strlen(text);
    return tok->len == len && memcmp(tok->text, text, len) == 0;
}

bool FOC_tok_is_kw(const FOCToken *tok, const char *kw) {
    return tok->kind == FOC_TOK_IDENT && FOC_tok_is(tok, kw);
}

char *FOC_tok_text_dup(const FOCToken *tok) {
    char *out = (char *)malloc(tok->len + 1);
    if (!out) {
        fprintf(stderr, "foc: agotamiento de memoria\n");
        abort();
    }
    memcpy(out, tok->text, tok->len);
    out[tok->len] = '\0';
    return out;
}

static const char *const keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double",
    "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long",
    "register", "restrict", "return", "short", "signed", "sizeof", "static", "struct",
    "switch", "typedef", "union", "unsigned", "void", "volatile", "while",
    "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
    "_Noreturn", "_Static_assert", "_Thread_local",
    /* C23 */
    "alignas", "alignof", "bool", "constexpr", "false", "nullptr", "static_assert",
    "thread_local", "true", "typeof", "typeof_unqual",
    NULL,
};

bool FOC_is_keyword(const char *text, size_t len) {
    for (size_t i = 0; keywords[i]; i++) {
        if (strlen(keywords[i]) == len && memcmp(keywords[i], text, len) == 0) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Lexer                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    const FOCSourceFile *file;
    FOCDiagEngine *diag;
    FOCTokenList *out;
    size_t pos;
    bool line_has_token;
    bool space_before;
} FOCLexer;

static bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0x80;
}

static bool is_ident_part(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_hex(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static FOCSpan span_of(const FOCLexer *lex, uint32_t start, uint32_t end) {
    FOCSpan span;
    span.file_id = lex->file->id;
    span.start = start;
    span.end = end;
    return span;
}

static void lex_error(FOCLexer *lex, uint32_t start, uint32_t end, const char *code, const char *fmt, ...) {
    if (!lex->diag) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    char message[512];
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    FOC_diag_error(lex->diag, span_of(lex, start, end), code, "%s", message);
}

static FOCToken make_token(FOCLexer *lex, FOCTokenKind kind, uint32_t start, uint32_t end) {
    FOCToken tok;
    memset(&tok, 0, sizeof(tok));
    tok.kind = kind;
    tok.text = lex->file->data + start;
    tok.len = end - start;
    tok.start = start;
    tok.end = end;
    tok.file_id = lex->file->id;
    tok.at_line_start = !lex->line_has_token;
    tok.space_before = lex->space_before;
    lex->line_has_token = true;
    lex->space_before = false;
    return tok;
}

static void skip_line_comment(FOCLexer *lex) {
    while (lex->pos < lex->file->len && lex->file->data[lex->pos] != '\n') {
        lex->pos++;
    }
    lex->space_before = true;
}

static bool skip_block_comment(FOCLexer *lex) {
    uint32_t start = (uint32_t)lex->pos;
    lex->pos += 2;
    while (lex->pos < lex->file->len) {
        if (lex->file->data[lex->pos] == '*' && lex->pos + 1 < lex->file->len &&
            lex->file->data[lex->pos + 1] == '/') {
            lex->pos += 2;
            lex->space_before = true;
            return true;
        }
        if (lex->file->data[lex->pos] == '\n') {
            lex->line_has_token = false;
        }
        lex->pos++;
    }
    lex_error(lex, start, (uint32_t)lex->pos, "E0105", "comentario sin terminar");
    return false;
}

static void lex_number(FOCLexer *lex) {
    uint32_t start = (uint32_t)lex->pos;
    FOCTokenKind kind = FOC_TOK_INT;
    char c = lex->file->data[lex->pos];

    if (c == '0' && lex->pos + 1 < lex->file->len &&
        (lex->file->data[lex->pos + 1] == 'x' || lex->file->data[lex->pos + 1] == 'X')) {
        lex->pos += 2;
        while (lex->pos < lex->file->len &&
               (is_hex(lex->file->data[lex->pos]) || lex->file->data[lex->pos] == '\'')) {
            lex->pos++;
        }
        if (lex->pos < lex->file->len && lex->file->data[lex->pos] == '.') {
            kind = FOC_TOK_FLOAT;
            lex->pos++;
            while (lex->pos < lex->file->len &&
                   (is_hex(lex->file->data[lex->pos]) || lex->file->data[lex->pos] == '\'')) {
                lex->pos++;
            }
        }
        if (lex->pos < lex->file->len &&
            (lex->file->data[lex->pos] == 'p' || lex->file->data[lex->pos] == 'P')) {
            kind = FOC_TOK_FLOAT;
            lex->pos++;
            if (lex->pos < lex->file->len &&
                (lex->file->data[lex->pos] == '+' || lex->file->data[lex->pos] == '-')) {
                lex->pos++;
            }
            while (lex->pos < lex->file->len && is_digit(lex->file->data[lex->pos])) {
                lex->pos++;
            }
        }
    } else if (c == '0' && lex->pos + 1 < lex->file->len &&
               (lex->file->data[lex->pos + 1] == 'b' || lex->file->data[lex->pos + 1] == 'B')) {
        lex->pos += 2;
        while (lex->pos < lex->file->len &&
               (lex->file->data[lex->pos] == '0' || lex->file->data[lex->pos] == '1' ||
                lex->file->data[lex->pos] == '\'')) {
            lex->pos++;
        }
    } else {
        while (lex->pos < lex->file->len &&
               (is_digit(lex->file->data[lex->pos]) || lex->file->data[lex->pos] == '\'')) {
            lex->pos++;
        }
        if (lex->pos < lex->file->len && lex->file->data[lex->pos] == '.') {
            kind = FOC_TOK_FLOAT;
            lex->pos++;
            while (lex->pos < lex->file->len && is_digit(lex->file->data[lex->pos])) {
                lex->pos++;
            }
        }
        if (lex->pos < lex->file->len &&
            (lex->file->data[lex->pos] == 'e' || lex->file->data[lex->pos] == 'E')) {
            kind = FOC_TOK_FLOAT;
            lex->pos++;
            if (lex->pos < lex->file->len &&
                (lex->file->data[lex->pos] == '+' || lex->file->data[lex->pos] == '-')) {
                lex->pos++;
            }
            while (lex->pos < lex->file->len && is_digit(lex->file->data[lex->pos])) {
                lex->pos++;
            }
        }
    }

    /* Sufijos. */
    while (lex->pos < lex->file->len) {
        char s = lex->file->data[lex->pos];
        if (s == 'u' || s == 'U' || s == 'l' || s == 'L' || s == 'f' || s == 'F') {
            lex->pos++;
            continue;
        }
        break;
    }
    FOC_token_list_push(lex->out, make_token(lex, kind, start, (uint32_t)lex->pos));
}

static void lex_char_literal(FOCLexer *lex) {
    uint32_t start = (uint32_t)lex->pos;
    lex->pos++; /* el apostrofo inicial */
    while (lex->pos < lex->file->len) {
        char c = lex->file->data[lex->pos];
        if (c == '\n') {
            break;
        }
        if (c == '\\' && lex->pos + 1 < lex->file->len) {
            lex->pos += 2;
            continue;
        }
        if (c == '\'') {
            lex->pos++;
            FOC_token_list_push(lex->out, make_token(lex, FOC_TOK_CHAR, start, (uint32_t)lex->pos));
            return;
        }
        lex->pos++;
    }
    lex_error(lex, start, (uint32_t)lex->pos, "E0102", "literal de carácter sin terminar");
    FOC_token_list_push(lex->out, make_token(lex, FOC_TOK_INVALID, start, (uint32_t)lex->pos));
}

static void lex_string_literal(FOCLexer *lex) {
    uint32_t start = (uint32_t)lex->pos;
    lex->pos++; /* la comilla inicial */
    while (lex->pos < lex->file->len) {
        char c = lex->file->data[lex->pos];
        if (c == '\n') {
            break;
        }
        if (c == '\\' && lex->pos + 1 < lex->file->len) {
            lex->pos += 2;
            continue;
        }
        if (c == '"') {
            lex->pos++;
            FOC_token_list_push(lex->out, make_token(lex, FOC_TOK_STRING, start, (uint32_t)lex->pos));
            return;
        }
        lex->pos++;
    }
    lex_error(lex, start, (uint32_t)lex->pos, "E0103", "literal de cadena sin terminar");
    FOC_token_list_push(lex->out, make_token(lex, FOC_TOK_INVALID, start, (uint32_t)lex->pos));
}

static const char *const punctuators[] = {
    "<<=", ">>=", "...", "##", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
    "&&", "||", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "::", ".*",
    "<:", ":>", "<%", "%>", "%:",
    "#", "(", ")", "[", "]", "{", "}", ";", ",", ":", "?", ".", "&", "|", "^", "~",
    "!", "+", "-", "*", "/", "%", "<", ">", "=", "@", "$",
    NULL,
};

static void lex_punct(FOCLexer *lex) {
    uint32_t start = (uint32_t)lex->pos;
    for (size_t i = 0; punctuators[i]; i++) {
        size_t len = strlen(punctuators[i]);
        if (lex->pos + len <= lex->file->len &&
            memcmp(lex->file->data + lex->pos, punctuators[i], len) == 0) {
            lex->pos += len;
            FOC_token_list_push(lex->out, make_token(lex, FOC_TOK_PUNCT, start, (uint32_t)lex->pos));
            return;
        }
    }
    lex->pos++;
    lex_error(lex, start, (uint32_t)lex->pos, "E0101", "carácter inesperado 0x%02X",
              (unsigned char)lex->file->data[start]);
    FOC_token_list_push(lex->out, make_token(lex, FOC_TOK_INVALID, start, (uint32_t)lex->pos));
}

static void lex_next(FOCLexer *lex) {
    for (;;) {
        if (lex->pos >= lex->file->len) {
            FOC_token_list_push(lex->out, make_token(lex, FOC_TOK_EOF, (uint32_t)lex->pos,
                                                     (uint32_t)lex->pos));
            return;
        }
        char c = lex->file->data[lex->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f') {
            lex->pos++;
            lex->space_before = true;
            continue;
        }
        if (c == '\\' && lex->pos + 1 < lex->file->len && lex->file->data[lex->pos + 1] == '\n') {
            lex->pos += 2;
            lex->space_before = true;
            continue;
        }
        if (c == '/' && lex->pos + 1 < lex->file->len && lex->file->data[lex->pos + 1] == '/') {
            skip_line_comment(lex);
            continue;
        }
        if (c == '/' && lex->pos + 1 < lex->file->len && lex->file->data[lex->pos + 1] == '*') {
            if (!skip_block_comment(lex)) {
                FOC_token_list_push(lex->out, make_token(lex, FOC_TOK_EOF, (uint32_t)lex->pos,
                                                         (uint32_t)lex->pos));
                return;
            }
            continue;
        }
        break;
    }

    uint32_t start = (uint32_t)lex->pos;
    char c = lex->file->data[lex->pos];

    if (c == '\n') {
        lex->pos++;
        FOCToken tok = make_token(lex, FOC_TOK_NEWLINE, start, (uint32_t)lex->pos);
        lex->line_has_token = false;
        FOC_token_list_push(lex->out, tok);
        return;
    }

    /* Prefijos de literales anchos: L'x', u'x', U'x', u8"..." */
    if (c == 'L' || c == 'u' || c == 'U') {
        if (lex->pos + 1 < lex->file->len) {
            char n = lex->file->data[lex->pos + 1];
            if (n == '\'') {
                lex_char_literal(lex);
                return;
            }
            if (n == '"') {
                lex_string_literal(lex);
                return;
            }
            if (c == 'u' && n == '8' && lex->pos + 2 < lex->file->len) {
                char m = lex->file->data[lex->pos + 2];
                if (m == '"') {
                    lex_string_literal(lex);
                    return;
                }
                if (m == '\'') {
                    lex_char_literal(lex);
                    return;
                }
            }
        }
    }

    if (is_ident_start(c)) {
        while (lex->pos < lex->file->len && is_ident_part(lex->file->data[lex->pos])) {
            lex->pos++;
        }
        FOC_token_list_push(lex->out, make_token(lex, FOC_TOK_IDENT, start, (uint32_t)lex->pos));
        return;
    }

    if (is_digit(c) || (c == '.' && lex->pos + 1 < lex->file->len && is_digit(lex->file->data[lex->pos + 1]))) {
        lex_number(lex);
        return;
    }

    if (c == '\'') {
        lex_char_literal(lex);
        return;
    }

    if (c == '"') {
        lex_string_literal(lex);
        return;
    }

    lex_punct(lex);
}

void FOC_lex_file(const FOCSourceFile *file, FOCDiagEngine *diag, FOCTokenList *out) {
    FOCLexer lex;
    memset(&lex, 0, sizeof(lex));
    lex.file = file;
    lex.diag = diag;
    lex.out = out;
    for (;;) {
        size_t before = out->len;
        lex_next(&lex);
        if (out->len == before) {
            break;
        }
        if (out->data[out->len - 1].kind == FOC_TOK_EOF) {
            break;
        }
    }
}
