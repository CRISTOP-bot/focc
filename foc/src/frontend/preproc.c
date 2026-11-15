#include "foc/preproc.h"

#include "foc/map.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Macros                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;
    size_t name_len;
    bool is_function;
    bool is_variadic;
    bool is_builtin;
    FOCToken *params;
    size_t param_count;
    FOCToken *body;
    size_t body_len;
} FOCMacro;

typedef struct FOCTokenSource FOCTokenSource;

struct FOCTokenSource {
    FOCToken *tokens;
    size_t len;
    size_t pos;
    FOCTokenSource *prev;
    bool from_macro;
    const char *macro_name; /* macro cuya expansion es esta fuente */
    char *dir;              /* directorio del fichero (solo para fuentes de fichero) */
};

typedef struct {
    FOCDiagEngine *diag;
    FOCSourceMgr *sources;
    const FOCPreprocOptions *options;
    FOCMap macros;
    FOCTokenList *out;
    FOCTokenSource *source;
    FOCArena arena; /* conserva los cuerpos de las macros predefinidas */
    FOCVec include_stack;   /* rutas ya incluidas (para #pragma once) */
    FOCVec once_files;      /* char* marcadas con #pragma once */
    FOCVec expanding;       /* char* macros en expansion (anti recursion) */
    int depth;
    int error_count;
} FOCPreproc;

void FOC_preproc_options_init(FOCPreprocOptions *options) {
    memset(options, 0, sizeof(*options));
    FOC_vec_init(&options->include_paths, sizeof(char *));
    FOC_vec_init(&options->defines, sizeof(char *));
    FOC_vec_init(&options->undefines, sizeof(char *));
    FOC_vec_init(&options->sys_include_paths, sizeof(char *));
    options->std_name = "c17";
    options->target = FOC_default_target();
}

void FOC_preproc_options_free(FOCPreprocOptions *options) {
    FOC_vec_free(&options->include_paths);
    FOC_vec_free(&options->defines);
    FOC_vec_free(&options->undefines);
    FOC_vec_free(&options->sys_include_paths);
}

static void vec_push_char(FOCVec *vec, const char *text) {
    char **slot = (char **)FOC_vec_push(vec);
    *slot = FOC_strdup(text);
}

/* ------------------------------------------------------------------ */
/* Pila de fuentes de tokens                                           */
/* ------------------------------------------------------------------ */

static FOCTokenSource *source_push(FOCPreproc *pp, FOCToken *tokens, size_t len, bool from_macro,
                                   const char *macro_name) {
    FOCTokenSource *src = (FOCTokenSource *)calloc(1, sizeof(FOCTokenSource));
    if (!src) {
        return NULL;
    }
    src->tokens = tokens;
    src->len = len;
    src->pos = 0;
    src->prev = pp->source;
    src->from_macro = from_macro;
    src->macro_name = macro_name;
    pp->source = src;
    return src;
}

/* Quita el nombre de una macro de la lista de expansiones en curso. */
static void expanding_remove(FOCPreproc *pp, const char *name) {
    for (size_t i = 0; i < pp->expanding.len; i++) {
        char *active = *(char **)FOC_VEC_AT(&pp->expanding, char *, i);
        if (strcmp(active, name) == 0) {
            free(active);
            for (size_t j = i + 1; j < pp->expanding.len; j++) {
                *(char **)FOC_VEC_AT(&pp->expanding, char *, j - 1) =
                    *(char **)FOC_VEC_AT(&pp->expanding, char *, j);
            }
            pp->expanding.len--;
            return;
        }
    }
}

static void source_pop(FOCPreproc *pp) {
    FOCTokenSource *src = pp->source;
    if (!src) {
        return;
    }
    pp->source = src->prev;
    if (src->macro_name) {
        expanding_remove(pp, src->macro_name);
    }
    free(src->tokens);
    free(src);
}

static bool source_has_next(FOCPreproc *pp) {
    /* Hay tokens si alguna fuente de la pila tiene posicion disponible. */
    FOCTokenSource *src = pp->source;
    while (src) {
        if (src->pos < src->len) {
            return true;
        }
        src = src->prev;
    }
    return false;
}

/* Devuelve el siguiente token disponible sin consumirlo. */
static FOCToken *peek_token(FOCPreproc *pp) {
    FOCTokenSource *src = pp->source;
    while (src) {
        if (src->pos < src->len) {
            return &src->tokens[src->pos];
        }
        if (src->from_macro) {
            return NULL;
        }
        src = src->prev;
    }
    return NULL;
}

static FOCToken next_token(FOCPreproc *pp) {
    FOCToken tok;
    memset(&tok, 0, sizeof(tok));
    tok.kind = FOC_TOK_EOF;
    for (;;) {
        FOCTokenSource *src = pp->source;
        if (!src) {
            break;
        }
        if (src->pos < src->len) {
            return src->tokens[src->pos++];
        }
        if (src->from_macro) {
            source_pop(pp);
            continue;
        }
        /* Fin de un fichero: se retira si hay mas fuentes apiladas. */
        if (src->prev) {
            source_pop(pp);
            continue;
        }
        break;
    }
    return tok;
}

/* Directorio del fichero que se esta procesando (para includes con comillas). */
static const char *current_dir(FOCPreproc *pp) {
    FOCTokenSource *src = pp->source;
    while (src) {
        if (!src->from_macro) {
            return src->dir ? src->dir : ".";
        }
        src = src->prev;
    }
    return ".";
}

/* ------------------------------------------------------------------ */
/* Tabla de macros                                                     */
/* ------------------------------------------------------------------ */

static FOCMacro *macro_find(FOCPreproc *pp, const char *name, size_t len) {
    return (FOCMacro *)FOC_map_get(&pp->macros, name, len);
}

static void macro_define(FOCPreproc *pp, FOCMacro *macro) {
    FOC_map_put(&pp->macros, macro->name, macro->name_len, macro);
}

static void macro_undefine(FOCPreproc *pp, const char *name, size_t len) {
    FOC_map_remove(&pp->macros, name, len);
}

/* ------------------------------------------------------------------ */
/* Utilidades de tokens                                                */
/* ------------------------------------------------------------------ */

static bool tok_is(const FOCToken *tok, const char *text) {
    if (!tok) {
        return false;
    }
    size_t len = strlen(text);
    return tok->len == len && memcmp(tok->text, text, len) == 0;
}

static bool tok_is_punct(const FOCToken *tok, const char *text) {
    return tok && tok->kind == FOC_TOK_PUNCT && tok_is(tok, text);
}

static bool tok_is_ident(const FOCToken *tok) {
    return tok && tok->kind == FOC_TOK_IDENT;
}

static char *tok_dup(const FOCToken *tok) {
    char *out = (char *)malloc(tok->len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, tok->text, tok->len);
    out[tok->len] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* Expansion de macros                                                 */
/* ------------------------------------------------------------------ */

static bool is_expanding(FOCPreproc *pp, const char *name, size_t len) {
    for (size_t i = 0; i < pp->expanding.len; i++) {
        char *active = *(char **)FOC_VEC_AT(&pp->expanding, char *, i);
        if (strlen(active) == len && memcmp(active, name, len) == 0) {
            return true;
        }
    }
    return false;
}

typedef struct {
    FOCToken *tokens;
    size_t len;
    size_t cap;
} TokenBuf;

static void buf_push(TokenBuf *buf, FOCToken tok) {
    if (buf->len == buf->cap) {
        size_t cap = buf->cap ? buf->cap * 2 : 16;
        FOCToken *data = (FOCToken *)realloc(buf->tokens, cap * sizeof(FOCToken));
        if (!data) {
            fprintf(stderr, "foc: agotamiento de memoria\n");
            abort();
        }
        buf->tokens = data;
        buf->cap = cap;
    }
    buf->tokens[buf->len++] = tok;
}

static FOCToken make_token(FOCTokenKind kind, const char *text, size_t len, FOCToken like) {
    FOCToken tok;
    memset(&tok, 0, sizeof(tok));
    tok.kind = kind;
    tok.text = text;
    tok.len = (uint32_t)len;
    tok.start = like.start;
    tok.end = like.end;
    tok.file_id = like.file_id;
    tok.space_before = like.space_before;
    tok.expanded = true;
    return tok;
}

/* Convierte una lista de tokens en texto (para la operador #). */
static char *stringize(const FOCToken *tokens, size_t len) {
    size_t cap = len * 2 + 3;
    char *out = (char *)malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t pos = 0;
    out[pos++] = '"';
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && tokens[i].space_before) {
            out[pos++] = ' ';
        }
        for (uint32_t j = 0; j < tokens[i].len; j++) {
            char c = tokens[i].text[j];
            if (c == '"' || c == '\\') {
                out[pos++] = '\\';
            }
            out[pos++] = c;
        }
    }
    out[pos++] = '"';
    out[pos] = '\0';
    return out;
}

/* Concatena el texto de dos tokens (operador ##). */
static FOCToken paste_tokens(FOCToken left, FOCToken right) {
    size_t len = left.len + right.len;
    char *text = (char *)malloc(len + 1);
    if (!text) {
        return left;
    }
    memcpy(text, left.text, left.len);
    memcpy(text + left.len, right.text, right.len);
    text[len] = '\0';
    FOCToken tok = make_token(left.kind, text, len, left);
    return tok;
}

/* Busca el indice de un parametro en la lista de la macro. */
static long param_index(const FOCMacro *macro, const FOCToken *tok) {
    if (!tok_is_ident(tok)) {
        return -1;
    }
    for (size_t i = 0; i < macro->param_count; i++) {
        if (macro->params[i].len == tok->len &&
            memcmp(macro->params[i].text, tok->text, tok->len) == 0) {
            return (long)i;
        }
    }
    return -1;
}

/* Argumentos de una macro de funcion: una lista de tokens por argumento. */
typedef struct {
    TokenBuf *args;
    size_t count;
    size_t capacity;
} MacroArgs;

static void macro_args_init(MacroArgs *args, size_t capacity) {
    args->args = (TokenBuf *)calloc(capacity, sizeof(TokenBuf));
    args->count = 0;
    args->capacity = capacity;
}

static void macro_args_free(MacroArgs *args) {
    if (!args->args) {
        return;
    }
    for (size_t i = 0; i < args->capacity; i++) {
        free(args->args[i].tokens);
    }
    free(args->args);
    args->args = NULL;
    args->count = 0;
    args->capacity = 0;
}

static bool macro_args_grow(MacroArgs *args, size_t needed) {
    if (needed < args->capacity) {
        return true;
    }
    size_t capacity = args->capacity ? args->capacity : 4;
    while (capacity <= needed) {
        capacity *= 2;
    }
    TokenBuf *data = (TokenBuf *)realloc(args->args, capacity * sizeof(TokenBuf));
    if (!data) {
        return false;
    }
    memset(data + args->capacity, 0, (capacity - args->capacity) * sizeof(TokenBuf));
    args->args = data;
    args->capacity = capacity;
    return true;
}

/* Recolecta los argumentos de una macro de funcion. */
static bool collect_arguments(FOCPreproc *pp, FOCMacro *macro, MacroArgs *out) {
    macro_args_init(out, macro->param_count + 1);
    size_t current = 0;
    int depth = 0;

    for (;;) {
        FOCToken *peek = peek_token(pp);
        if (!peek) {
            FOC_diag_error(pp->diag, (FOCSpan){-1, 0, 0}, "E0301",
                           "macro '%s': faltan argumentos", macro->name);
            return false;
        }
        FOCToken tok = next_token(pp);
        if (tok.kind == FOC_TOK_EOF) {
            return false;
        }
        if (tok.kind == FOC_TOK_NEWLINE) {
            continue;
        }
        if (tok_is_punct(&tok, "(")) {
            depth++;
        } else if (tok_is_punct(&tok, ")")) {
            if (depth == 0) {
                break;
            }
            depth--;
        } else if (tok_is_punct(&tok, ",") && depth == 0) {
            current++;
            if (!macro_args_grow(out, current)) {
                return false;
            }
            continue;
        }
        if (!macro_args_grow(out, current)) {
            return false;
        }
        buf_push(&out->args[current], tok);
    }
    out->count = current + 1;
    return true;
}

/* Sustituye los parametros en el cuerpo de la macro. */
static bool substitute_body(FOCPreproc *pp, FOCMacro *macro, MacroArgs *args,
                            FOCToken **out_tokens, size_t *out_len) {
    TokenBuf result;
    memset(&result, 0, sizeof(result));

    for (size_t i = 0; i < macro->body_len; i++) {
        FOCToken tok = macro->body[i];

        /* Operador # seguido de un parametro. */
        if (tok_is_punct(&tok, "#") && macro->is_function && i + 1 < macro->body_len) {
            long index = param_index(macro, &macro->body[i + 1]);
            if (index >= 0) {
                TokenBuf *arg = &args->args[index];
                char *text = stringize(arg->tokens, arg->len);
                FOCToken literal = make_token(FOC_TOK_STRING, text, strlen(text), tok);
                buf_push(&result, literal);
                i++;
                continue;
            }
        }

        /* Operador ##: concatena con el token anterior. */
        if (tok_is_punct(&tok, "##") && i + 1 < macro->body_len) {
            FOCToken next = macro->body[i + 1];
            long index = param_index(macro, &next);
            if (index >= 0) {
                TokenBuf *arg = &args->args[index];
                if (arg->len > 0 && result.len > 0) {
                    FOCToken left = result.tokens[result.len - 1];
                    result.len--;
                    result.tokens[result.len++] = paste_tokens(left, arg->tokens[0]);
                    for (size_t k = 1; k < arg->len; k++) {
                        buf_push(&result, arg->tokens[k]);
                    }
                }
                i++;
                continue;
            }
            if (result.len > 0) {
                FOCToken left = result.tokens[result.len - 1];
                result.len--;
                result.tokens[result.len++] = paste_tokens(left, next);
            } else {
                buf_push(&result, next);
            }
            i++;
            continue;
        }

        long index = param_index(macro, &tok);
        if (index >= 0) {
            TokenBuf *arg = &args->args[index];
            for (size_t k = 0; k < arg->len; k++) {
                buf_push(&result, arg->tokens[k]);
            }
            continue;
        }

        buf_push(&result, tok);
    }

    *out_tokens = result.tokens;
    *out_len = result.len;
    (void)pp;
    return true;
}

/* Expande una macro. Devuelve true si se expandio. */
static bool expand_macro(FOCPreproc *pp, FOCMacro *macro, FOCToken invocation) {
    if (is_expanding(pp, macro->name, macro->name_len)) {
        return false;
    }

    if (macro->is_builtin) {
        FOCToken tok = invocation;
        if (tok_is(&invocation, "__LINE__")) {
            FOCSourceFile *file = FOC_source_get(pp->sources, invocation.file_id);
            int line = file ? (int)FOC_source_line_of(file, invocation.start) + 1 : 0;
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%d", line);
            /* El buffer es local: se copia para que el token sobreviva. */
            size_t buffer_len = strlen(buffer);
            char *copy = (char *)malloc(buffer_len + 1);
            if (!copy) {
                return false;
            }
            memcpy(copy, buffer, buffer_len + 1);
            tok = make_token(FOC_TOK_INT, copy, buffer_len, invocation);
        } else if (tok_is(&invocation, "__FILE__")) {
            FOCSourceFile *file = FOC_source_get(pp->sources, invocation.file_id);
            const char *path = file ? file->path : "<archivo>";
            size_t len = strlen(path);
            char *text = (char *)malloc(len + 3);
            if (!text) {
                return false;
            }
            text[0] = '"';
            memcpy(text + 1, path, len);
            text[len + 1] = '"';
            text[len + 2] = '\0';
            tok = make_token(FOC_TOK_STRING, text, len + 2, invocation);
        }
        FOC_token_list_push(pp->out, tok);
        return true;
    }

    if (!macro->is_function) {
        size_t len = macro->body_len;
        FOCToken *tokens = (FOCToken *)malloc((len ? len : 1) * sizeof(FOCToken));
        if (!tokens) {
            return false;
        }
        for (size_t i = 0; i < len; i++) {
            tokens[i] = macro->body[i];
            tokens[i].expanded = true;
        }
        vec_push_char(&pp->expanding, macro->name);
        source_push(pp, tokens, len, true, macro->name);
        return true;
    }

    /* Macro de funcion: el '(' de la llamada se consume aqui. */
    FOCToken *open = peek_token(pp);
    if (!open || !tok_is_punct(open, "(")) {
        return false;
    }
    next_token(pp);
    MacroArgs args;
    if (!collect_arguments(pp, macro, &args)) {
        macro_args_free(&args);
        return false;
    }

    FOCToken *body = NULL;
    size_t body_len = 0;
    substitute_body(pp, macro, &args, &body, &body_len);
    macro_args_free(&args);

    vec_push_char(&pp->expanding, macro->name);
    source_push(pp, body, body_len, true, macro->name);
    return true;
}

/* ------------------------------------------------------------------ */
/* Evaluacion de condiciones                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    FOCToken *tokens;
    size_t len;
    size_t pos;
    FOCPreproc *pp;
} CondEval;

static long long cond_primary(CondEval *ev);
static long long cond_binary(CondEval *ev, int min_precedence);

static FOCToken *cond_peek(CondEval *ev) {
    if (ev->pos < ev->len) {
        return &ev->tokens[ev->pos];
    }
    return NULL;
}

static long long cond_primary(CondEval *ev) {
    FOCToken *tok = cond_peek(ev);
    if (!tok) {
        return 0;
    }
    if (tok_is_punct(tok, "(")) {
        ev->pos++;
        long long value = cond_binary(ev, 1);
        if (cond_peek(ev) && tok_is_punct(cond_peek(ev), ")")) {
            ev->pos++;
        }
        return value;
    }
    if (tok_is_punct(tok, "!")) {
        ev->pos++;
        return !cond_primary(ev);
    }
    if (tok_is_punct(tok, "-")) {
        ev->pos++;
        return -cond_primary(ev);
    }
    if (tok_is_punct(tok, "+")) {
        ev->pos++;
        return cond_primary(ev);
    }
    if (tok_is_punct(tok, "~")) {
        ev->pos++;
        return ~cond_primary(ev);
    }
    if (tok->kind == FOC_TOK_INT) {
        ev->pos++;
        const char *text = tok->text;
        size_t len = tok->len;
        unsigned long long value = 0;
        size_t i = 0;
        if (len >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
            for (i = 2; i < len; i++) {
                char c = text[i];
                int digit = (c >= '0' && c <= '9') ? c - '0'
                                                : (((c | 0x20) - 'a' + 10));
                value = value * 16 + (unsigned)digit;
            }
        } else {
            for (i = 0; i < len; i++) {
                char c = text[i];
                if (c < '0' || c > '9') {
                    break;
                }
                value = value * 10 + (unsigned)(c - '0');
            }
        }
        return (long long)value;
    }
    if (tok->kind == FOC_TOK_CHAR) {
        ev->pos++;
        /* Reutiliza el valor calculado por el parser a traves del texto. */
        const char *start = tok->text;
        size_t len = tok->len;
        size_t i = 0;
        while (i < len && start[i] != '\'') {
            i++;
        }
        i++;
        long long value = 0;
        if (i < len && start[i] == '\\') {
            i++;
            if (i < len) {
                switch (start[i]) {
                case 'n': value = '\n'; break;
                case 't': value = '\t'; break;
                case 'r': value = '\r'; break;
                case '0': value = 0; break;
                case '\\': value = '\\'; break;
                case '\'': value = '\''; break;
                default: value = (unsigned char)start[i]; break;
                }
            }
        } else if (i < len) {
            value = (signed char)start[i];
        }
        return value;
    }
    if (tok_is_ident(tok)) {
        /* Identificadores no expandidos valen 0 en las directivas. */
        ev->pos++;
        if (tok_is(tok, "true")) {
            return 1;
        }
        return 0;
    }
    ev->pos++;
    return 0;
}

static long long cond_binary(CondEval *ev, int min_precedence) {
    long long lhs = cond_primary(ev);
    for (;;) {
        FOCToken *tok = cond_peek(ev);
        if (!tok || tok->kind != FOC_TOK_PUNCT) {
            break;
        }
        const char *op = tok->text;
        int precedence = 0;
        bool op_is = false;
#define OP_IS(text) (op_is = (strcmp(op, text) == 0))
        if (OP_IS("||")) precedence = 1;
        else if (OP_IS("&&")) precedence = 2;
        else if (OP_IS("|")) precedence = 3;
        else if (OP_IS("^")) precedence = 4;
        else if (OP_IS("&")) precedence = 5;
        else if (OP_IS("==") || OP_IS("!=")) precedence = 6;
        else if (OP_IS("<") || OP_IS(">") || OP_IS("<=") || OP_IS(">=")) precedence = 7;
        else if (OP_IS("<<") || OP_IS(">>")) precedence = 8;
        else if (OP_IS("+") || OP_IS("-")) precedence = 9;
        else if (OP_IS("*") || OP_IS("/") || OP_IS("%")) precedence = 10;
        else break;
#undef OP_IS
        if (precedence < min_precedence) {
            break;
        }
        ev->pos++;
        long long rhs = cond_binary(ev, precedence + 1);
        if (strcmp(op, "||") == 0) lhs = (lhs || rhs);
        else if (strcmp(op, "&&") == 0) lhs = (lhs && rhs);
        else if (strcmp(op, "|") == 0) lhs = lhs | rhs;
        else if (strcmp(op, "^") == 0) lhs = lhs ^ rhs;
        else if (strcmp(op, "&") == 0) lhs = lhs & rhs;
        else if (strcmp(op, "==") == 0) lhs = (lhs == rhs);
        else if (strcmp(op, "!=") == 0) lhs = (lhs != rhs);
        else if (strcmp(op, "<") == 0) lhs = (lhs < rhs);
        else if (strcmp(op, ">") == 0) lhs = (lhs > rhs);
        else if (strcmp(op, "<=") == 0) lhs = (lhs <= rhs);
        else if (strcmp(op, ">=") == 0) lhs = (lhs >= rhs);
        else if (strcmp(op, "<<") == 0) lhs = (lhs << rhs);
        else if (strcmp(op, ">>") == 0) lhs = (lhs >> rhs);
        else if (strcmp(op, "+") == 0) lhs = lhs + rhs;
        else if (strcmp(op, "-") == 0) lhs = lhs - rhs;
        else if (strcmp(op, "*") == 0) lhs = lhs * rhs;
        else if (strcmp(op, "/") == 0) lhs = rhs ? lhs / rhs : 0;
        else if (strcmp(op, "%") == 0) lhs = rhs ? lhs % rhs : 0;
    }
    return lhs;
}

/* ------------------------------------------------------------------ */
/* Lectura de ficheros                                                 */
/* ------------------------------------------------------------------ */

static bool read_whole_file(const char *path, char **out_data, size_t *out_len) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        return false;
    }
    char *data = (char *)malloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        return false;
    }
    size_t read = fread(data, 1, (size_t)size, file);
    fclose(file);
    data[read] = '\0';
    *out_data = data;
    *out_len = read;
    return true;
}

static char *dir_of(const char *path) {
    const char *slash = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            slash = p;
        }
    }
    if (!slash) {
        return FOC_strdup(".");
    }
    size_t len = (size_t)(slash - path);
    char *dir = (char *)malloc(len + 1);
    if (!dir) {
        return FOC_strdup(".");
    }
    memcpy(dir, path, len);
    dir[len] = '\0';
    if (len == 0) {
        free(dir);
        return FOC_strdup("/");
    }
    return dir;
}

/* ------------------------------------------------------------------ */
/* Directivas                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    bool parent_active;
    bool active;
    bool seen_true;
    bool seen_else;
} CondState;

static void skip_line(FOCPreproc *pp) {
    for (;;) {
        FOCToken *tok = peek_token(pp);
        if (!tok || tok->kind == FOC_TOK_NEWLINE || tok->kind == FOC_TOK_EOF) {
            return;
        }
        next_token(pp);
    }
}

static void handle_define(FOCPreproc *pp) {
    FOCToken *name = peek_token(pp);
    if (!name || name->kind != FOC_TOK_IDENT) {
        FOC_diag_error(pp->diag, (FOCSpan){name ? name->file_id : -1, name ? name->start : 0,
                                            name ? name->end : 0},
                       "E0302", "se esperaba un nombre de macro");
        skip_line(pp);
        return;
    }
    next_token(pp);

    FOCMacro *macro = (FOCMacro *)calloc(1, sizeof(FOCMacro));
    macro->name = name->text;
    macro->name_len = name->len;

    /* ¿Macro de función? */
    FOCToken *after = peek_token(pp);
    if (after && after->kind == FOC_TOK_PUNCT && tok_is(after, "(") && !after->space_before) {
        macro->is_function = true;
        next_token(pp);
        size_t capacity = 8;
        macro->params = (FOCToken *)calloc(capacity, sizeof(FOCToken));
        if (!macro->params) {
            free(macro);
            return;
        }
        for (;;) {
            FOCToken *param = peek_token(pp);
            if (!param) {
                break;
            }
            if (tok_is_punct(param, ")")) {
                next_token(pp);
                break;
            }
            if (tok_is_punct(param, ",")) {
                next_token(pp);
                continue;
            }
            if (param->kind == FOC_TOK_IDENT) {
                next_token(pp);
                if (tok_is(param, "...")) {
                    macro->is_variadic = true;
                    FOCToken *va = peek_token(pp);
                    if (va && tok_is_ident(va)) {
                        next_token(pp);
                        if (macro->param_count == capacity) {
                            capacity *= 2;
                            macro->params = (FOCToken *)realloc(macro->params,
                                                                 capacity * sizeof(FOCToken));
                        }
                        macro->params[macro->param_count++] = *va;
                    }
                } else if (macro->param_count == capacity) {
                    capacity *= 2;
                    macro->params = (FOCToken *)realloc(macro->params, capacity * sizeof(FOCToken));
                }
                if (macro->param_count < capacity) {
                    macro->params[macro->param_count++] = *param;
                }
            } else {
                next_token(pp);
            }
        }
    }

    /* Cuerpo. */
    size_t capacity = 8;
    macro->body = (FOCToken *)calloc(capacity, sizeof(FOCToken));
    if (!macro->body) {
        free(macro);
        return;
    }
    for (;;) {
        FOCToken *tok = peek_token(pp);
        if (!tok || tok->kind == FOC_TOK_NEWLINE || tok->kind == FOC_TOK_EOF) {
            break;
        }
        next_token(pp);
        if (macro->body_len == capacity) {
            capacity *= 2;
            macro->body = (FOCToken *)realloc(macro->body, capacity * sizeof(FOCToken));
        }
        macro->body[macro->body_len++] = *tok;
    }
    macro_define(pp, macro);
}

static void handle_undef(FOCPreproc *pp) {
    FOCToken *name = peek_token(pp);
    if (!name || name->kind != FOC_TOK_IDENT) {
        skip_line(pp);
        return;
    }
    next_token(pp);
    macro_undefine(pp, name->text, name->len);
    skip_line(pp);
}

/* Expande los tokens de una linea de condicional y evalua el resultado. */
static long long evaluate_condition(FOCPreproc *pp) {
    /* Recolecta la linea. */
    TokenBuf line;
    memset(&line, 0, sizeof(line));
    for (;;) {
        FOCToken *tok = peek_token(pp);
        if (!tok || tok->kind == FOC_TOK_NEWLINE || tok->kind == FOC_TOK_EOF) {
            break;
        }
        next_token(pp);
        buf_push(&line, *tok);
    }

    /* Resuelve `defined X` antes de la expansion. */
    TokenBuf resolved;
    memset(&resolved, 0, sizeof(resolved));
    for (size_t i = 0; i < line.len; i++) {
        if (tok_is_ident(&line.tokens[i]) && tok_is(&line.tokens[i], "defined")) {
            size_t j = i + 1;
            bool paren = false;
            if (j < line.len && tok_is_punct(&line.tokens[j], "(")) {
                paren = true;
                j++;
            }
            if (j < line.len && tok_is_ident(&line.tokens[j])) {
                FOCMacro *macro = macro_find(pp, line.tokens[j].text, line.tokens[j].len);
                int value = macro ? 1 : 0;
                j++;
                if (paren && j < line.len && tok_is_punct(&line.tokens[j], ")")) {
                    j++;
                }
                char buffer[8];
                snprintf(buffer, sizeof(buffer), "%d", value);
                FOCToken tok = make_token(FOC_TOK_INT, buffer, strlen(buffer), line.tokens[i]);
                buf_push(&resolved, tok);
                i = j - 1;
                continue;
            }
        }
        buf_push(&resolved, line.tokens[i]);
    }
    free(line.tokens);

    /* Expande macros en el resultado. */
    FOCTokenList expanded;
    FOC_token_list_init(&expanded);
    FOCPreproc sub = *pp;
    sub.out = &expanded;
    source_push(&sub, resolved.tokens, resolved.len, true, NULL);
    for (;;) {
        FOCToken *tok = peek_token(&sub);
        if (!tok) {
            break;
        }
        FOCToken current = next_token(&sub);
        if (current.kind == FOC_TOK_EOF) {
            break;
        }
        if (current.kind == FOC_TOK_IDENT) {
            FOCMacro *macro = macro_find(&sub, current.text, current.len);
            if (macro && !is_expanding(&sub, macro->name, macro->name_len)) {
                if (expand_macro(&sub, macro, current)) {
                    continue;
                }
            }
        }
        FOC_token_list_push(&expanded, current);
    }
    /* La fuente se marca como expansion de macro: al agotar la pila se
     * liberan los tokens de `resolved` (no hace falta liberarlos aqui). */
    while (sub.source && sub.source->from_macro) {
        source_pop(&sub);
    }

    CondEval ev;
    ev.tokens = expanded.data;
    ev.len = expanded.len;
    ev.pos = 0;
    ev.pp = pp;
    long long value = cond_binary(&ev, 1);
    FOC_token_list_free(&expanded);
    return value;
}

/* ------------------------------------------------------------------ */
/* Inclusiones                                                         */
/* ------------------------------------------------------------------ */

static bool file_in_once_list(FOCPreproc *pp, const char *path) {
    for (size_t i = 0; i < pp->once_files.len; i++) {
        char *entry = *(char **)FOC_VEC_AT(&pp->once_files, char *, i);
        if (strcmp(entry, path) == 0) {
            return true;
        }
    }
    return false;
}

static void handle_include(FOCPreproc *pp, bool is_system) {
    /* Lee el nombre del fichero. */
    char *name = NULL;
    FOCToken *tok = peek_token(pp);
    FOCSpan include_span = tok ? (FOCSpan){tok->file_id, tok->start, tok->end}
                               : (FOCSpan){-1, 0, 0};
    if (tok && tok->kind == FOC_TOK_STRING) {
        next_token(pp);
        name = tok_dup(tok);
        /* Quita las comillas. */
        if (name[0] == '"') {
            memmove(name, name + 1, strlen(name));
            char *end = strrchr(name, '"');
            if (end) {
                *end = '\0';
            }
        }
    } else if (tok && tok->kind == FOC_TOK_PUNCT && tok_is(tok, "<")) {
        FOCBuf buf;
        FOC_buf_init(&buf);
        next_token(pp);
        for (;;) {
            FOCToken *part = peek_token(pp);
            if (!part || part->kind == FOC_TOK_NEWLINE || part->kind == FOC_TOK_EOF) {
                break;
            }
            next_token(pp);
            if (tok_is_punct(part, ">")) {
                break;
            }
            /* El nombre es literal entre < y >: solo hay espacio si el
             * fuente lo tenia. */
            if (part->space_before && buf.len > 0) {
                FOC_buf_putc(&buf, ' ');
            }
            FOC_buf_append(&buf, part->text, part->len);
        }
        name = FOC_buf_take(&buf);
    } else {
        /* Forma calculada: no soportada todavia. */
        skip_line(pp);
        return;
    }
    if (!name) {
        return;
    }
    skip_line(pp);

    if (pp->depth > 64) {
        FOC_diag_error(pp->diag, include_span, "E0303", "inclusion anidada demasiado profunda");
        free(name);
        return;
    }

    /* Construye la lista de rutas candidatas. */
    FOCVec candidates;
    FOC_vec_init(&candidates, sizeof(char *));
    if (!is_system) {
        FOCBuf buf;
        FOC_buf_init(&buf);
        FOC_buf_puts(&buf, current_dir(pp));
        FOC_buf_putc(&buf, '/');
        FOC_buf_puts(&buf, name);
        vec_push_char(&candidates, FOC_buf_take(&buf));
    }
    for (size_t i = 0; i < pp->options->include_paths.len; i++) {
        char *dir = *(char **)FOC_VEC_AT(&pp->options->include_paths, char *, i);
        FOCBuf buf;
        FOC_buf_init(&buf);
        FOC_buf_puts(&buf, dir);
        FOC_buf_putc(&buf, '/');
        FOC_buf_puts(&buf, name);
        vec_push_char(&candidates, FOC_buf_take(&buf));
    }
    for (size_t i = 0; i < pp->options->sys_include_paths.len; i++) {
        char *dir = *(char **)FOC_VEC_AT(&pp->options->sys_include_paths, char *, i);
        FOCBuf buf;
        FOC_buf_init(&buf);
        FOC_buf_puts(&buf, dir);
        FOC_buf_putc(&buf, '/');
        FOC_buf_puts(&buf, name);
        vec_push_char(&candidates, FOC_buf_take(&buf));
    }

    char *found = NULL;
    for (size_t i = 0; i < candidates.len; i++) {
        char *candidate = *(char **)FOC_VEC_AT(&candidates, char *, i);
        FILE *test = fopen(candidate, "rb");
        if (test) {
            fclose(test);
            found = candidate;
            break;
        }
    }
    for (size_t i = 0; i < candidates.len; i++) {
        char *candidate = *(char **)FOC_VEC_AT(&candidates, char *, i);
        if (candidate != found) {
            free(candidate);
        }
    }
    FOC_vec_free(&candidates);

    if (!found) {
        FOC_diag_error(pp->diag, include_span, "E0304",
                       "no se encuentra el fichero incluido '%s'", name ? name : "<desconocido>");
        free(name);
        return;
    }
    free(name);
    if (file_in_once_list(pp, found)) {
        free(found);
        return;
    }
    if (pp->depth > 64) {
        FOC_diag_error(pp->diag, include_span, "E0303", "inclusion anidada demasiado profunda");
        free(found);
        return;
    }

    /* Lee, registra y encola los tokens del fichero incluido. */
    char *data = NULL;
    size_t len = 0;
    if (!read_whole_file(found, &data, &len)) {
        FOC_diag_error(pp->diag, include_span, "E0305", "no se puede abrir '%s'", found);
        free(found);
        return;
    }
    FOCSourceFile *file = FOC_source_add(pp->sources, found, data, len);
    free(data);
    free(found);

    FOCTokenList tokens;
    FOC_token_list_init(&tokens);
    FOC_lex_file(file, pp->diag, &tokens);
    FOCTokenSource *src = source_push(pp, tokens.data, tokens.len, false, NULL);
    if (src) {
        src->dir = dir_of(file->path);
    }
}

/* ------------------------------------------------------------------ */
/* Macros predefinidas                                                 */
/* ------------------------------------------------------------------ */

static void define_builtin(FOCPreproc *pp, const char *name, const char *value) {
    FOCMacro *macro = (FOCMacro *)calloc(1, sizeof(FOCMacro));
    macro->name = name;
    macro->name_len = strlen(name);
    macro->is_builtin = true;
    (void)value;
    macro_define(pp, macro);
}

static void define_text_macro(FOCPreproc *pp, const char *name, const char *value) {
    FOCMacro *macro = (FOCMacro *)calloc(1, sizeof(FOCMacro));
    macro->name = name;
    macro->name_len = strlen(name);
    size_t len = strlen(value);
    macro->body = (FOCToken *)calloc(len ? len : 1, sizeof(FOCToken));
    macro->body_len = 0;
    /* El cuerpo se lexea como una linea virtual. La arena del preprocesador
     * mantiene vivo el texto durante toda la compilacion. */
    FOCSourceMgr tmp;
    FOC_source_init(&tmp, &pp->arena);
    FOCSourceFile *file = FOC_source_add(&tmp, "<builtin>", value, len);
    FOCTokenList tokens;
    FOC_token_list_init(&tokens);
    FOC_lex_file(file, NULL, &tokens);
    for (size_t i = 0; i < tokens.len; i++) {
        if (tokens.data[i].kind == FOC_TOK_NEWLINE || tokens.data[i].kind == FOC_TOK_EOF) {
            break;
        }
        macro->body[macro->body_len++] = tokens.data[i];
    }
    FOC_token_list_free(&tokens);
    macro_define(pp, macro);
}

static void define_predefined(FOCPreproc *pp) {
    define_text_macro(pp, "__FOC__", "1");
    define_text_macro(pp, "__STDC__", "1");
    define_text_macro(pp, "__STDC_HOSTED__", "1");
    define_text_macro(pp, "__STDC_VERSION__", "201710L");
    define_text_macro(pp, "__DATE__", "\"Jan  1 2024\"");
    define_text_macro(pp, "__TIME__", "\"00:00:00\"");
    define_text_macro(pp, "__FOC_VERSION__", "\"0.1.0\"");
    if (pp->options->target.os == FOC_OS_WINDOWS) {
        define_text_macro(pp, "_WIN32", "1");
    }
    define_builtin(pp, "__FILE__", NULL);
    define_builtin(pp, "__LINE__", NULL);
}

/* ------------------------------------------------------------------ */
/* Preprocesado principal                                              */
/* ------------------------------------------------------------------ */

bool FOC_preprocess(const FOCSourceFile *file, FOCSourceMgr *sources, FOCDiagEngine *diag,
                    const FOCPreprocOptions *options, FOCTokenList *out) {
    FOCPreproc pp;
    memset(&pp, 0, sizeof(pp));
    pp.diag = diag;
    pp.sources = sources;
    pp.options = options;
    pp.out = out;
    FOC_arena_init(&pp.arena);
    FOC_map_init(&pp.macros);
    FOC_vec_init(&pp.include_stack, sizeof(char *));
    FOC_vec_init(&pp.once_files, sizeof(char *));
    FOC_vec_init(&pp.expanding, sizeof(char *));

    define_predefined(&pp);

    /* Macros de la linea de ordenes. */
    for (size_t i = 0; i < options->defines.len; i++) {
        char *entry = *(char **)FOC_VEC_AT(&options->defines, char *, i);
        char *equals = strchr(entry, '=');
        if (equals) {
            *equals = '\0';
            define_text_macro(&pp, entry, equals + 1);
        } else {
            define_text_macro(&pp, entry, "1");
        }
    }
    for (size_t i = 0; i < options->undefines.len; i++) {
        char *entry = *(char **)FOC_VEC_AT(&options->undefines, char *, i);
        macro_undefine(&pp, entry, strlen(entry));
    }

    /* El fichero principal se procesa como una inclusion. */
    FOCTokenList tokens;
    FOC_token_list_init(&tokens);
    FOC_lex_file(file, diag, &tokens);
    FOCTokenSource *main_src = source_push(&pp, tokens.data, tokens.len, false, NULL);
    if (main_src) {
        /* El directorio del fichero principal tambien se busca en "". */
        main_src->dir = dir_of(file->path);
    }

    CondState *conds = (CondState *)calloc(64, sizeof(CondState));
    size_t cond_count = 0;

    while (source_has_next(&pp)) {
        FOCToken tok = next_token(&pp);
        if (tok.kind == FOC_TOK_EOF) {
            break;
        }
        if (tok.kind == FOC_TOK_NEWLINE) {
            continue;
        }

        bool active = true;
        for (size_t i = 0; i < cond_count; i++) {
            if (!conds[i].active) {
                active = false;
                break;
            }
        }

        /* Directiva. */
        if (tok.kind == FOC_TOK_PUNCT && tok_is(&tok, "#") && tok.at_line_start) {
            FOCToken *directive = peek_token(&pp);
            if (!directive) {
                break;
            }
            if (tok_is_ident(directive)) {
                if (tok_is(directive, "ifdef")) {
                    next_token(&pp);
                    FOCToken *name = peek_token(&pp);
                    bool defined = false;
                    if (name && name->kind == FOC_TOK_IDENT) {
                        next_token(&pp);
                        defined = macro_find(&pp, name->text, name->len) != NULL;
                    }
                    skip_line(&pp);
                    if (cond_count < 64) {
                        conds[cond_count].parent_active = active;
                        conds[cond_count].active = active && defined;
                        conds[cond_count].seen_true = active && defined;
                        conds[cond_count].seen_else = false;
                        cond_count++;
                    }
                    continue;
                }
                if (tok_is(directive, "ifndef")) {
                    next_token(&pp);
                    FOCToken *name = peek_token(&pp);
                    bool defined = false;
                    if (name && name->kind == FOC_TOK_IDENT) {
                        next_token(&pp);
                        defined = macro_find(&pp, name->text, name->len) == NULL;
                    }
                    skip_line(&pp);
                    if (cond_count < 64) {
                        conds[cond_count].parent_active = active;
                        conds[cond_count].active = active && defined;
                        conds[cond_count].seen_true = active && defined;
                        conds[cond_count].seen_else = false;
                        cond_count++;
                    }
                    continue;
                }
                if (tok_is(directive, "if")) {
                    next_token(&pp);
                    long long value = evaluate_condition(&pp);
                    if (cond_count < 64) {
                        conds[cond_count].parent_active = active;
                        conds[cond_count].active = active && (value != 0);
                        conds[cond_count].seen_true = conds[cond_count].active;
                        conds[cond_count].seen_else = false;
                        cond_count++;
                    }
                    continue;
                }
                if (tok_is(directive, "elif")) {
                    next_token(&pp);
                    if (cond_count > 0) {
                        CondState *state = &conds[cond_count - 1];
                        long long value = evaluate_condition(&pp);
                        if (state->seen_true) {
                            state->active = false;
                        } else {
                            state->active = state->parent_active && (value != 0);
                            state->seen_true = state->active;
                        }
                    }
                    continue;
                }
                if (tok_is(directive, "else")) {
                    next_token(&pp);
                    skip_line(&pp);
                    if (cond_count > 0) {
                        CondState *state = &conds[cond_count - 1];
                        if (state->seen_true) {
                            state->active = false;
                        } else {
                            state->active = state->parent_active;
                            state->seen_true = state->active;
                            state->seen_else = true;
                        }
                    }
                    continue;
                }
                if (tok_is(directive, "endif")) {
                    next_token(&pp);
                    skip_line(&pp);
                    if (cond_count > 0) {
                        cond_count--;
                    }
                    continue;
                }
                if (tok_is(directive, "define")) {
                    next_token(&pp);
                    if (active) {
                        handle_define(&pp);
                    } else {
                        skip_line(&pp);
                    }
                    continue;
                }
                if (tok_is(directive, "undef")) {
                    next_token(&pp);
                    if (active) {
                        handle_undef(&pp);
                    } else {
                        skip_line(&pp);
                    }
                    continue;
                }
                if (tok_is(directive, "include")) {
                    next_token(&pp);
                    if (active) {
                        handle_include(&pp, false);
                    } else {
                        skip_line(&pp);
                    }
                    continue;
                }
                if (tok_is(directive, "pragma")) {
                    next_token(&pp);
                    FOCToken *pragma = peek_token(&pp);
                    if (active && pragma && tok_is_ident(pragma) && tok_is(pragma, "once")) {
                        next_token(&pp);
                        const FOCSourceFile *current = NULL;
                        if (pp.sources->files.len) {
                            current = *(FOCSourceFile **)FOC_VEC_AT(&pp.sources->files,
                                                                    FOCSourceFile *,
                                                                    pp.sources->files.len - 1);
                        }
                        if (current) {
                            vec_push_char(&pp.once_files, current->path);
                        }
                    }
                    skip_line(&pp);
                    continue;
                }
                if (tok_is(directive, "error")) {
                    next_token(&pp);
                    if (active) {
                        FOC_diag_error(pp.diag, (FOCSpan){tok.file_id, tok.start, tok.end}, "E0306",
                                       "#error");
                    }
                    skip_line(&pp);
                    continue;
                }
                if (tok_is(directive, "warning")) {
                    next_token(&pp);
                    skip_line(&pp);
                    continue;
                }
                if (tok_is(directive, "line")) {
                    next_token(&pp);
                    skip_line(&pp);
                    continue;
                }
            }
            skip_line(&pp);
            continue;
        }

        if (!active) {
            continue;
        }

        /* Expansion de macros. */
        if (tok.kind == FOC_TOK_IDENT) {
            FOCMacro *macro = macro_find(&pp, tok.text, tok.len);
            if (macro && !is_expanding(&pp, macro->name, macro->name_len)) {
                if (expand_macro(&pp, macro, tok)) {
                    continue;
                }
            }
        }

        FOC_token_list_push(out, tok);
    }

    free(conds);
    while (pp.source) {
        source_pop(&pp);
    }
    /* El flujo de tokens termina con EOF. */
    {
        FOCToken eof;
        memset(&eof, 0, sizeof(eof));
        eof.kind = FOC_TOK_EOF;
        FOC_token_list_push(out, eof);
    }
    FOC_map_free(&pp.macros);
    FOC_vec_free(&pp.include_stack);
    FOC_vec_free(&pp.once_files);
    FOC_vec_free(&pp.expanding);
    return pp.error_count == 0;
}
