#include "foc/parser.h"

#include "foc/map.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const FOCTokenList *tokens;
    size_t pos;
    FOCDiagEngine *diag;
    FOCUnit *unit;
    FOCMap typedefs;
    int error_count;
    int quiet; /* suprime diagnosticos durante el analisis especulativo */
    int loop_depth;
    int switch_depth;
} FOCParser;

/* Declaraciones adelantadas. */
static FOCExpr *parse_expr(FOCParser *p);
static FOCExpr *parse_assign(FOCParser *p);
static FOCExpr *parse_cond(FOCParser *p);
static FOCTypeName *parse_type_name(FOCParser *p);
static FOCDeclarator *parse_declarator(FOCParser *p);
FOCDeclarator *FOC_parse_declarator_pub(FOCParser *p);
FOCExpr *FOC_parse_expression_pub(FOCParser *p);

/* ------------------------------------------------------------------ */
/* Acceso a tokens                                                     */
/* ------------------------------------------------------------------ */

static FOCToken *tok_at(FOCParser *p, size_t index) {
    if (!p->tokens->len) {
        return NULL;
    }
    if (index >= p->tokens->len) {
        index = p->tokens->len - 1;
    }
    return &p->tokens->data[index];
}

static FOCToken *cur(FOCParser *p) { return tok_at(p, p->pos); }

static FOCToken *peek(FOCParser *p, size_t n) { return tok_at(p, p->pos + n); }

static bool at_punct(FOCParser *p, const char *text) {
    FOCToken *tok = cur(p);
    return tok && tok->kind == FOC_TOK_PUNCT && FOC_tok_is(tok, text);
}

static bool at_kw(FOCParser *p, const char *kw) {
    FOCToken *tok = cur(p);
    return tok && tok->kind == FOC_TOK_IDENT && FOC_tok_is(tok, kw);
}

static bool at_ident(FOCParser *p) {
    FOCToken *tok = cur(p);
    return tok && tok->kind == FOC_TOK_IDENT && !FOC_is_keyword(tok->text, tok->len);
}

static void advance(FOCParser *p) {
    if (p->pos + 1 < p->tokens->len) {
        p->pos++;
    }
}

static bool eat_punct(FOCParser *p, const char *text) {
    if (at_punct(p, text)) {
        advance(p);
        return true;
    }
    return false;
}

/* Copia un texto del fuente como cadena terminada en NUL (operadores). */
static const char *dup_text(const char *text, size_t len) {
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        return "";
    }
    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}

static FOCSpan span_of(FOCParser *p, FOCToken *tok) {
    (void)p;
    FOCSpan span;    span.file_id = tok ? tok->file_id : -1;
    span.start = tok ? tok->start : 0;
    span.end = tok ? tok->end : 0;
    return span;
}

static void error_at(FOCParser *p, FOCToken *tok, const char *code, const char *fmt, ...) {
    if (p->quiet) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    char message[512];
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    FOC_diag_error(p->diag, span_of(p, tok), code, "%s", message);
    p->error_count++;
}

static FOCToken *expect_punct(FOCParser *p, const char *text) {
    if (at_punct(p, text)) {
        FOCToken *tok = cur(p);
        advance(p);
        return tok;
    }
    error_at(p, cur(p), "E0201", "se esperaba '%s'", text);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Utilidades de tipos                                                 */
/* ------------------------------------------------------------------ */

static bool is_typedef_name(FOCParser *p, FOCToken *tok) {
    if (!tok || tok->kind != FOC_TOK_IDENT) {
        return false;
    }
    return FOC_map_get(&p->typedefs, tok->text, tok->len) != NULL;
}

static bool starts_type_name(FOCParser *p) {
    FOCToken *tok = cur(p);
    if (!tok || tok->kind != FOC_TOK_IDENT) {
        return false;
    }
    static const char *type_keywords[] = {
        "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned",
        "_Bool", "bool", "_Complex", "_Imaginary", "struct", "union", "enum", "const",
        "volatile", "restrict", "_Atomic", "typedef", "static", "extern", "auto",
        "register", "inline", "_Noreturn", "_Thread_local", "thread_local", "_Alignas",
        "typeof", "typeof_unqual", NULL,
    };
    for (size_t i = 0; type_keywords[i]; i++) {
        if (FOC_tok_is(tok, type_keywords[i])) {
            return true;
        }
    }
    return is_typedef_name(p, tok);
}

static bool starts_declaration(FOCParser *p) {
    FOCToken *tok = cur(p);
    if (!tok || tok->kind != FOC_TOK_IDENT) {
        return false;
    }
    static const char *decl_keywords[] = {
        "typedef", "static", "extern", "auto", "register", "inline", "_Noreturn",
        "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned",
        "_Bool", "bool", "_Complex", "struct", "union", "enum", "const", "volatile",
        "restrict", "_Atomic", "_Thread_local", "thread_local", "_Alignas", "typeof",
        "typeof_unqual", NULL,
    };
    for (size_t i = 0; decl_keywords[i]; i++) {
        if (FOC_tok_is(tok, decl_keywords[i])) {
            return true;
        }
    }
    return is_typedef_name(p, tok);
}

/* ------------------------------------------------------------------ */
/* Especificadores de declaracion                                      */
/* ------------------------------------------------------------------ */

static FOCStructSpec *parse_struct_spec(FOCParser *p);
static FOCEnumSpec *parse_enum_spec(FOCParser *p);
static bool parse_decl_specs(FOCParser *p, FOCDeclSpec *spec);

static FOCStructSpec *parse_struct_spec(FOCParser *p) {
    bool is_union = at_kw(p, "union");
    FOCSpan start_span = span_of(p, cur(p));
    advance(p);

    FOCStructSpec *spec = (FOCStructSpec *)calloc(1, sizeof(FOCStructSpec));
    if (!spec) {
        return NULL;
    }
    spec->is_union = is_union;
    spec->span = start_span;

    if (at_ident(p)) {
        FOCToken *tag = cur(p);
        spec->tag = tag->text;
        spec->tag_len = tag->len;
        spec->tag_span = span_of(p, tag);
        advance(p);
    }

    if (!at_punct(p, "{")) {
        return spec;
    }
    advance(p);
    spec->has_body = true;
    size_t capacity = 16;
    spec->fields = (FOCFieldDecl **)calloc(capacity, sizeof(FOCFieldDecl *));
    if (!spec->fields) {
        return NULL;
    }

    while (!at_punct(p, "}") && cur(p) && cur(p)->kind != FOC_TOK_EOF) {
        if (eat_punct(p, ";")) {
            continue;
        }
        FOCFieldDecl *field = (FOCFieldDecl *)calloc(1, sizeof(FOCFieldDecl));
        if (!field) {
            return NULL;
        }
        field->span = span_of(p, cur(p));
        field->spec = FOC_ast_decl_spec(field->span);
        if (!parse_decl_specs(p, field->spec)) {
            return NULL;
        }
        size_t field_capacity = 8;
        field->declarators = (FOCDeclarator **)calloc(field_capacity, sizeof(FOCDeclarator *));
        field->bit_widths = (FOCExpr **)calloc(field_capacity, sizeof(FOCExpr *));
        if (!field->declarators || !field->bit_widths) {
            return NULL;
        }

        if (!eat_punct(p, ";")) {
            for (;;) {
                FOCDeclarator *declarator = NULL;
                if (!at_punct(p, ":")) {
                    declarator = parse_declarator(p);
                }
                FOCExpr *bit_width = NULL;
                if (eat_punct(p, ":")) {
                    bit_width = parse_expr(p);
                }
                if (field->declarator_count == field_capacity) {
                    field_capacity *= 2;
                    FOCDeclarator **nd =
                        (FOCDeclarator **)realloc(field->declarators, field_capacity * sizeof(void *));
                    FOCExpr **nb = (FOCExpr **)realloc(field->bit_widths, field_capacity * sizeof(void *));
                    if (!nd || !nb) {
                        return NULL;
                    }
                    field->declarators = nd;
                    field->bit_widths = nb;
                }
                field->declarators[field->declarator_count] = declarator;
                field->bit_widths[field->declarator_count] = bit_width;
                field->declarator_count++;
                if (eat_punct(p, ",")) {
                    continue;
                }
                break;
            }
            expect_punct(p, ";");
        }
        if (spec->field_count == capacity) {
            capacity *= 2;
            FOCFieldDecl **nf =
                (FOCFieldDecl **)realloc(spec->fields, capacity * sizeof(void *));
            if (!nf) {
                return NULL;
            }
            spec->fields = nf;
        }
        spec->fields[spec->field_count++] = field;
    }
    expect_punct(p, "}");
    return spec;
}

static FOCEnumSpec *parse_enum_spec(FOCParser *p) {
    FOCSpan start_span = span_of(p, cur(p));
    advance(p);

    FOCEnumSpec *spec = (FOCEnumSpec *)calloc(1, sizeof(FOCEnumSpec));
    if (!spec) {
        return NULL;
    }
    spec->span = start_span;

    if (at_ident(p)) {
        FOCToken *tag = cur(p);
        spec->tag = tag->text;
        spec->tag_len = tag->len;
        spec->tag_span = span_of(p, tag);
        advance(p);
    }

    if (!at_punct(p, "{")) {
        return spec;
    }
    advance(p);
    spec->has_body = true;
    size_t capacity = 16;
    spec->items = (FOCEnumItem **)calloc(capacity, sizeof(FOCEnumItem *));
    if (!spec->items) {
        return NULL;
    }

    while (!at_punct(p, "}") && cur(p) && cur(p)->kind != FOC_TOK_EOF) {
        FOCEnumItem *item = (FOCEnumItem *)calloc(1, sizeof(FOCEnumItem));
        if (!item) {
            return NULL;
        }
        if (!at_ident(p)) {
            error_at(p, cur(p), "E0202", "se esperaba un enumerador");
            return NULL;
        }
        FOCToken *name = cur(p);
        item->name = name->text;
        item->name_len = name->len;
        item->name_span = span_of(p, name);
        advance(p);
        if (eat_punct(p, "=")) {
            /* El valor del enum no puede tragar la coma del enumerador. */
            item->value = parse_assign(p);
        }
        if (spec->item_count == capacity) {
            capacity *= 2;
            FOCEnumItem **ni = (FOCEnumItem **)realloc(spec->items, capacity * sizeof(void *));
            if (!ni) {
                return NULL;
            }
            spec->items = ni;
        }
        spec->items[spec->item_count++] = item;
        if (!eat_punct(p, ",")) {
            break;
        }
    }
    expect_punct(p, "}");
    return spec;
}

static bool parse_decl_specs(FOCParser *p, FOCDeclSpec *spec) {
    bool any = false;
    for (;;) {
        FOCToken *tok = cur(p);
        if (!tok || tok->kind != FOC_TOK_IDENT) {
            break;
        }
        /* Las etiquetas y los cuerpos los consume el analizador correspondiente:
         * en ese caso no se avanza el token al final del bucle. */
        bool consumed = false;
        if (FOC_tok_is_kw(tok, "typedef")) {
            spec->is_typedef = true;
        } else if (FOC_tok_is_kw(tok, "extern")) {
            spec->is_extern = true;
        } else if (FOC_tok_is_kw(tok, "static")) {
            spec->is_static = true;
        } else if (FOC_tok_is_kw(tok, "auto")) {
            spec->is_auto = true;
        } else if (FOC_tok_is_kw(tok, "register")) {
            spec->is_register = true;
        } else if (FOC_tok_is_kw(tok, "_Thread_local") || FOC_tok_is_kw(tok, "thread_local")) {
            spec->is_thread_local = true;
        } else if (FOC_tok_is_kw(tok, "const")) {
            spec->is_const = true;
        } else if (FOC_tok_is_kw(tok, "volatile")) {
            spec->is_volatile = true;
        } else if (FOC_tok_is_kw(tok, "restrict")) {
            spec->is_restrict = true;
        } else if (FOC_tok_is_kw(tok, "inline")) {
            spec->is_inline = true;
        } else if (FOC_tok_is_kw(tok, "_Noreturn")) {
            spec->is_noreturn = true;
        } else if (FOC_tok_is_kw(tok, "void")) {
            spec->t_void = true;
        } else if (FOC_tok_is_kw(tok, "_Bool") || FOC_tok_is_kw(tok, "bool")) {
            spec->t_bool = true;
        } else if (FOC_tok_is_kw(tok, "char")) {
            spec->t_char = true;
        } else if (FOC_tok_is_kw(tok, "int")) {
            spec->t_int = true;
        } else if (FOC_tok_is_kw(tok, "short")) {
            spec->t_short = true;
        } else if (FOC_tok_is_kw(tok, "long")) {
            spec->long_count++;
        } else if (FOC_tok_is_kw(tok, "float")) {
            spec->t_float = true;
        } else if (FOC_tok_is_kw(tok, "double")) {
            spec->t_double = true;
        } else if (FOC_tok_is_kw(tok, "signed")) {
            spec->t_signed = true;
        } else if (FOC_tok_is_kw(tok, "unsigned")) {
            spec->t_unsigned = true;
        } else if (FOC_tok_is_kw(tok, "_Complex") || FOC_tok_is_kw(tok, "_Imaginary")) {
            spec->t_complex = true;
        } else if (FOC_tok_is_kw(tok, "struct") || FOC_tok_is_kw(tok, "union")) {
            spec->struct_spec = parse_struct_spec(p);
            if (!spec->struct_spec) {
                return false;
            }
            consumed = true;
        } else if (FOC_tok_is_kw(tok, "enum")) {
            spec->enum_spec = parse_enum_spec(p);
            if (!spec->enum_spec) {
                return false;
            }
            consumed = true;
        } else if (!spec->t_void && !spec->t_bool && !spec->t_char && !spec->t_int &&
                   !spec->t_float && !spec->t_double && !spec->t_signed && !spec->t_unsigned &&
                   !spec->t_short && spec->long_count == 0 && !spec->struct_spec &&
                   !spec->enum_spec && is_typedef_name(p, tok)) {
            spec->typedef_name = tok->text;
            spec->typedef_len = tok->len;
            spec->typedef_span = span_of(p, tok);
        } else {
            break;
        }
        if (!consumed) {
            advance(p);
        }
        any = true;
    }
    if (spec->span.file_id < 0) {
        spec->span = span_of(p, cur(p));
    }
    return any;
}

/* ------------------------------------------------------------------ */
/* Declaradores                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    FOCDeclCtor *data;
    size_t len;
    size_t cap;
} CtorList;

static bool parse_declarator_level(FOCParser *p, FOCDeclarator *decl, CtorList *level);

static void ctor_list_init(CtorList *list) {
    list->data = NULL;
    list->len = 0;
    list->cap = 0;
}

static void ctor_list_push(CtorList *list, FOCDeclCtor ctor) {
    if (list->len == list->cap) {
        size_t cap = list->cap ? list->cap * 2 : 8;
        FOCDeclCtor *data = (FOCDeclCtor *)realloc(list->data, cap * sizeof(FOCDeclCtor));
        if (!data) {
            fprintf(stderr, "foc: agotamiento de memoria\n");
            abort();
        }
        list->data = data;
        list->cap = cap;
    }
    list->data[list->len++] = ctor;
}

static void ctor_list_free(CtorList *list) {
    free(list->data);
    ctor_list_init(list);
}

/* Analiza la lista de parametros de una funcion. */
static bool parse_param_list(FOCParser *p, FOCDeclCtor *ctor) {
    memset(ctor, 0, sizeof(*ctor));
    ctor->kind = FOC_CTOR_FUNCTION;
    ctor->has_proto = true;
    if (at_punct(p, ")")) {
        ctor->has_proto = false;
        return true;
    }
    if (at_kw(p, "void") && peek(p, 1) && FOC_tok_is(peek(p, 1), ")")) {
        advance(p);
        return true;
    }
    size_t capacity = 8;
    ctor->params = (FOCParam **)calloc(capacity, sizeof(FOCParam *));
    if (!ctor->params) {
        return false;
    }
    for (;;) {
        if (at_punct(p, "...")) {
            advance(p);
            ctor->variadic = true;
            break;
        }
        FOCParam *param = (FOCParam *)calloc(1, sizeof(FOCParam));
        if (!param) {
            return false;
        }
        param->span = span_of(p, cur(p));
        param->spec = FOC_ast_decl_spec(param->span);
        if (!parse_decl_specs(p, param->spec)) {
            error_at(p, cur(p), "E0203", "se esperaba un tipo en la lista de parametros");
            free(param);
            return false;
        }
        if (!at_punct(p, ")") && !at_punct(p, ",") && !at_punct(p, "...")) {
            param->declarator = parse_declarator(p);
        }
        if (ctor->param_count == capacity) {
            capacity *= 2;
            FOCParam **np = (FOCParam **)realloc(ctor->params, capacity * sizeof(void *));
            if (!np) {
                return false;
            }
            ctor->params = np;
        }
        ctor->params[ctor->param_count++] = param;
        if (eat_punct(p, ",")) {
            continue;
        }
        break;
    }
    return true;
}

/* Sufijos del nivel actual (array y funcion), en orden textual. */
static bool parse_suffixes(FOCParser *p, CtorList *out) {
    for (;;) {
        if (at_punct(p, "[")) {
            advance(p);
            FOCDeclCtor ctor;
            memset(&ctor, 0, sizeof(ctor));
            ctor.kind = FOC_CTOR_ARRAY;
            for (;;) {
                if (at_kw(p, "static")) {
                    ctor.array_static = true;
                    advance(p);
                    continue;
                }
                if (at_kw(p, "const")) {
                    ctor.array_const = true;
                    advance(p);
                    continue;
                }
                if (at_kw(p, "volatile")) {
                    ctor.array_volatile = true;
                    advance(p);
                    continue;
                }
                if (at_kw(p, "restrict")) {
                    ctor.array_restrict = true;
                    advance(p);
                    continue;
                }
                break;
            }
            if (!at_punct(p, "]")) {
                ctor.array_len = parse_assign(p);
            }
            expect_punct(p, "]");
            ctor_list_push(out, ctor);
            continue;
        }
        if (at_punct(p, "(")) {
            advance(p);
            FOCDeclCtor ctor;
            if (!parse_param_list(p, &ctor)) {
                return false;
            }
            expect_punct(p, ")");
            ctor_list_push(out, ctor);
            continue;
        }
        break;
    }
    return true;
}

/* Los sufijos de un nivel se aplican en orden inverso al textual: el primero
 * en el texto es el mas externo del tipo. */
static bool parse_level_suffixes(FOCParser *p, CtorList *level) {
    CtorList suffixes;
    ctor_list_init(&suffixes);
    if (!parse_suffixes(p, &suffixes)) {
        ctor_list_free(&suffixes);
        return false;
    }
    for (size_t i = suffixes.len; i > 0; i--) {
        ctor_list_push(level, suffixes.data[i - 1]);
    }
    ctor_list_free(&suffixes);
    return true;
}

#define CORE_FAIL 0
#define CORE_NAMED 1
#define CORE_ABSTRACT 2

/* Analiza el nucleo de un declarador: nombre, grupo entre parentesis o lista
 * de parametros. Los constructores se anaden a `level` en orden de aplicacion. */
static int parse_declarator_core(FOCParser *p, FOCDeclarator *decl, CtorList *level) {
    if (at_punct(p, "(")) {
        size_t saved = p->pos;
        advance(p);

        CtorList inner;
        ctor_list_init(&inner);
        FOCDeclarator inner_decl;
        memset(&inner_decl, 0, sizeof(inner_decl));
        p->quiet++;
        int inner_status = parse_declarator_level(p, &inner_decl, &inner);
        p->quiet--;

        if (inner_status && at_punct(p, ")")) {
            FOCToken *after = peek(p, 1);
            bool is_suffix = after && after->kind == FOC_TOK_PUNCT &&
                             (FOC_tok_is(after, "(") || FOC_tok_is(after, "["));
            if (is_suffix) {
                /* Grupo: los sufijos externos se aplican antes que el grupo. */
                advance(p); /* ')' */
                if (!parse_level_suffixes(p, level)) {
                    ctor_list_free(&inner);
                    return CORE_FAIL;
                }
                for (size_t i = 0; i < inner.len; i++) {
                    ctor_list_push(level, inner.data[i]);
                }
                ctor_list_free(&inner);
                decl->name = inner_decl.name;
                decl->name_len = inner_decl.name_len;
                decl->name_span = inner_decl.name_span;
                decl->has_name = inner_decl.has_name;
                return inner_decl.has_name ? CORE_NAMED : CORE_ABSTRACT;
            }
        }
        ctor_list_free(&inner);
        p->pos = saved;
    }

    /* Lista de parametros: el declarador es abstracto. */
    if (at_punct(p, "(")) {
        advance(p);
        FOCDeclCtor ctor;
        if (!parse_param_list(p, &ctor)) {
            return CORE_FAIL;
        }
        expect_punct(p, ")");
        CtorList suffixes;
        ctor_list_init(&suffixes);
        ctor_list_push(&suffixes, ctor);
        if (!parse_suffixes(p, &suffixes)) {
            ctor_list_free(&suffixes);
            return CORE_FAIL;
        }
        for (size_t i = suffixes.len; i > 0; i--) {
            ctor_list_push(level, suffixes.data[i - 1]);
        }
        ctor_list_free(&suffixes);
        return CORE_ABSTRACT;
    }

    if (at_ident(p)) {
        FOCToken *name = cur(p);
        decl->name = name->text;
        decl->name_len = name->len;
        decl->name_span = span_of(p, name);
        decl->has_name = true;
        advance(p);
        if (!parse_level_suffixes(p, level)) {
            return CORE_FAIL;
        }
        return CORE_NAMED;
    }
    return CORE_FAIL;
}

/* Analiza un nivel completo de declarador: punteros, nucleo y sufijos. */
static bool parse_declarator_level(FOCParser *p, FOCDeclarator *decl, CtorList *level) {
    size_t before = level->len;
    while (at_punct(p, "*")) {
        advance(p);
        FOCDeclCtor ctor;
        memset(&ctor, 0, sizeof(ctor));
        ctor.kind = FOC_CTOR_POINTER;
        for (;;) {
            if (at_kw(p, "const")) {
                ctor.is_const = true;
                advance(p);
                continue;
            }
            if (at_kw(p, "volatile")) {
                ctor.is_volatile = true;
                advance(p);
                continue;
            }
            if (at_kw(p, "restrict")) {
                ctor.is_restrict = true;
                advance(p);
                continue;
            }
            break;
        }
        ctor_list_push(level, ctor);
    }
    int status = parse_declarator_core(p, decl, level);
    if (status == CORE_FAIL) {
        /* Un declarador abstracto puede consistir solo en punteros. */
        return level->len > before;
    }
    return true;
}

static FOCDeclarator *parse_declarator(FOCParser *p) {
    FOCDeclarator *decl = (FOCDeclarator *)calloc(1, sizeof(FOCDeclarator));
    if (!decl) {
        return NULL;
    }
    decl->span = span_of(p, cur(p));

    CtorList level;
    ctor_list_init(&level);
    if (!parse_declarator_level(p, decl, &level)) {
        ctor_list_free(&level);
        free(decl);
        return NULL;
    }
    decl->ctors = level.data;
    decl->ctor_count = level.len;
    return decl;
}

/* ------------------------------------------------------------------ */
/* Nombres de tipo                                                     */
/* ------------------------------------------------------------------ */

static FOCTypeName *parse_type_name(FOCParser *p) {
    FOCTypeName *name = FOC_ast_type_name(span_of(p, cur(p)));
    name->spec = FOC_ast_decl_spec(name->span);
    if (!parse_decl_specs(p, name->spec)) {
        return NULL;
    }
    name->declarator = parse_declarator(p);
    return name;
}

/* ------------------------------------------------------------------ */
/* Literales                                                           */
/* ------------------------------------------------------------------ */

static long long parse_escape(const char **cursor) {
    const char *c = *cursor;
    long long value = 0;
    switch (*c) {
    case 'n':
        value = '\n';
        c++;
        break;
    case 't':
        value = '\t';
        c++;
        break;
    case 'r':
        value = '\r';
        c++;
        break;
    case 'a':
        value = '\a';
        c++;
        break;
    case 'b':
        value = '\b';
        c++;
        break;
    case 'f':
        value = '\f';
        c++;
        break;
    case 'v':
        value = '\v';
        c++;
        break;
    case 'e':
        value = 27;
        c++;
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': {
        int digits = 0;
        while (digits < 3 && *c >= '0' && *c <= '7') {
            value = value * 8 + (*c - '0');
            c++;
            digits++;
        }
        break;
    }
    case 'x': {
        c++;
        while ((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f') || (*c >= 'A' && *c <= 'F')) {
            int digit = (*c <= '9') ? (*c - '0') : ((*c | 0x20) - 'a' + 10);
            value = value * 16 + digit;
            c++;
        }
        break;
    }
    default:
        value = (unsigned char)*c;
        c++;
        break;
    }
    *cursor = c;
    return value;
}

static FOCExpr *parse_int_literal(FOCParser *p, FOCToken *tok) {
    FOCExpr *expr = FOC_ast_expr(FOC_EXPR_INT, span_of(p, tok));
    expr->int_lit.text = tok->text;
    expr->int_lit.len = tok->len;

    const char *text = tok->text;
    size_t len = tok->len;
    unsigned long long value = 0;
    bool is_unsigned = false;
    size_t i = 0;

    if (len >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        for (i = 2; i < len; i++) {
            char c = text[i];
            if (c == '\'') {
                continue;
            }
            int digit;
            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if ((c | 0x20) >= 'a' && (c | 0x20) <= 'f') {
                digit = (c | 0x20) - 'a' + 10;
            } else {
                break;
            }
            value = value * 16 + (unsigned)digit;
        }
    } else if (len >= 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        for (i = 2; i < len; i++) {
            char c = text[i];
            if (c == '\'') {
                continue;
            }
            if (c != '0' && c != '1') {
                break;
            }
            value = value * 2 + (unsigned)(c - '0');
        }
    } else if (len >= 2 && text[0] == '0') {
        for (i = 1; i < len; i++) {
            char c = text[i];
            if (c == '\'') {
                continue;
            }
            if (c < '0' || c > '7') {
                break;
            }
            value = value * 8 + (unsigned)(c - '0');
        }
    } else {
        for (i = 0; i < len; i++) {
            char c = text[i];
            if (c == '\'') {
                continue;
            }
            if (c < '0' || c > '9') {
                break;
            }
            value = value * 10 + (unsigned)(c - '0');
        }
    }
    for (; i < len; i++) {
        char c = text[i];
        if (c == 'u' || c == 'U') {
            is_unsigned = true;
        } else if (c == 'l' || c == 'L') {
            if (i + 1 < len && (text[i + 1] == 'l' || text[i + 1] == 'L')) {
                i++;
            }
        } else {
            break;
        }
    }
    expr->int_lit.value = (long long)value;
    expr->int_lit.is_unsigned = is_unsigned;
    return expr;
}

static FOCExpr *parse_char_literal(FOCParser *p, FOCToken *tok) {
    FOCExpr *expr = FOC_ast_expr(FOC_EXPR_CHAR, span_of(p, tok));
    expr->char_lit.text = tok->text;
    expr->char_lit.len = tok->len;

    const char *start = tok->text;
    size_t len = tok->len;
    size_t i = 0;
    while (i < len && start[i] != '\'') {
        i++;
    }
    i++;
    long long value = 0;
    int count = 0;
    while (i < len && start[i] != '\'') {
        long long ch;
        if (start[i] == '\\') {
            i++;
            if (i >= len) {
                break;
            }
            const char *cursor = &start[i];
            ch = parse_escape(&cursor);
        } else {
            ch = (unsigned char)start[i];
            i++;
        }
        value = (value << 8) | (ch & 0xff);
        count++;
    }
    if (count == 1) {
        value = (long long)(signed char)(value & 0xff);
    } else if (count > 1) {
        value = (long long)(int)(unsigned)value;
    }
    expr->char_lit.value = value;
    return expr;
}

static FOCExpr *parse_string_literal(FOCParser *p, FOCToken *tok) {
    FOCExpr *expr = FOC_ast_expr(FOC_EXPR_STRING, span_of(p, tok));
    expr->string_lit.text = tok->text;
    expr->string_lit.len = tok->len;

    const char *start = tok->text;
    size_t len = tok->len;
    size_t open = 0;
    while (open < len && start[open] != '"') {
        open++;
    }
    size_t close = open + 1;
    while (close < len && start[close] != '"') {
        close++;
    }
    expr->string_lit.value = start + open + 1;
    expr->string_lit.value_len = close > open + 1 ? close - open - 1 : 0;

    /* Concatena literales adyacentes. */
    while (cur(p) && cur(p)->kind == FOC_TOK_STRING) {
        FOCToken *next = cur(p);
        advance(p);
        const char *nstart = next->text;
        size_t nlen = next->len;
        size_t nopen = 0;
        while (nopen < nlen && nstart[nopen] != '"') {
            nopen++;
        }
        size_t nclose = nopen + 1;
        while (nclose < nlen && nstart[nclose] != '"') {
            nclose++;
        }
        size_t extra = nclose > nopen + 1 ? nclose - nopen - 1 : 0;
        char *merged = (char *)malloc(expr->string_lit.value_len + extra + 1);
        if (!merged) {
            return expr;
        }
        memcpy(merged, expr->string_lit.value, expr->string_lit.value_len);
        memcpy(merged + expr->string_lit.value_len, nstart + nopen + 1, extra);
        merged[expr->string_lit.value_len + extra] = '\0';
        expr->string_lit.value = merged;
        expr->string_lit.value_len += extra;
        expr->string_lit.len += next->len;
    }
    return expr;
}

/* ------------------------------------------------------------------ */
/* Expresiones                                                         */
/* ------------------------------------------------------------------ */

static FOCExpr *parse_unary(FOCParser *p);
static FOCExpr *parse_postfix(FOCParser *p);
static FOCExpr *parse_primary(FOCParser *p);

static FOCExpr *parse_primary(FOCParser *p) {
    FOCToken *tok = cur(p);
    if (!tok) {
        return NULL;
    }
    switch (tok->kind) {
    case FOC_TOK_INT:
        advance(p);
        return parse_int_literal(p, tok);
    case FOC_TOK_FLOAT: {
        advance(p);
        FOCExpr *expr = FOC_ast_expr(FOC_EXPR_FLOAT, span_of(p, tok));
        expr->float_lit.text = tok->text;
        expr->float_lit.len = tok->len;
        char buffer[64];
        size_t n = tok->len < sizeof(buffer) - 1 ? tok->len : sizeof(buffer) - 1;
        memcpy(buffer, tok->text, n);
        buffer[n] = '\0';
        expr->float_lit.value = strtod(buffer, NULL);
        return expr;
    }
    case FOC_TOK_CHAR:
        advance(p);
        return parse_char_literal(p, tok);
    case FOC_TOK_STRING:
        advance(p);
        return parse_string_literal(p, tok);
    case FOC_TOK_IDENT: {
        if (FOC_tok_is_kw(tok, "sizeof")) {
            advance(p);
            if (at_punct(p, "(")) {
                size_t saved = p->pos;
                advance(p);
                p->quiet++;
                FOCTypeName *type_name = parse_type_name(p);
                p->quiet--;
                if (type_name && at_punct(p, ")")) {
                    advance(p);
                    FOCExpr *expr = FOC_ast_expr(FOC_EXPR_SIZEOF, span_of(p, tok));
                    expr->sizeof_expr.type_name = type_name;
                    return expr;
                }
                p->pos = saved;
                advance(p);
            }
            FOCExpr *operand = parse_unary(p);
            FOCExpr *expr = FOC_ast_expr(FOC_EXPR_SIZEOF, span_of(p, tok));
            expr->sizeof_expr.expr = operand;
            return expr;
        }
        if (FOC_tok_is_kw(tok, "_Alignof") || FOC_tok_is_kw(tok, "alignof")) {
            advance(p);
            expect_punct(p, "(");
            FOCTypeName *type_name = parse_type_name(p);
            expect_punct(p, ")");
            FOCExpr *expr = FOC_ast_expr(FOC_EXPR_SIZEOF, span_of(p, tok));
            expr->sizeof_expr.type_name = type_name;
            return expr;
        }
        advance(p);
        FOCExpr *expr = FOC_ast_expr(FOC_EXPR_IDENT, span_of(p, tok));
        expr->ident.name = tok->text;
        expr->ident.name_len = tok->len;
        expr->ident.name_span = span_of(p, tok);
        return expr;
    }
    case FOC_TOK_PUNCT:
        if (FOC_tok_is(tok, "(")) {
            advance(p);
            /* Literal compuesto: (tipo){...} */
            if (starts_type_name(p)) {
                size_t saved = p->pos;
                p->quiet++;
                FOCTypeName *type_name = parse_type_name(p);
                p->quiet--;
                if (type_name && at_punct(p, ")")) {
                    advance(p);
                    if (at_punct(p, "{")) {
                        FOCExpr *expr = FOC_ast_expr(FOC_EXPR_COMPOUND, span_of(p, tok));
                        expr->compound.type_name = type_name;
                        expr->compound.init = parse_expr(p);
                        return expr;
                    }
                }
                p->pos = saved;
                advance(p);
            }
            FOCExpr *inner = parse_expr(p);
            expect_punct(p, ")");
            if (!inner) {
                return NULL;
            }
            FOCExpr *expr = FOC_ast_expr(FOC_EXPR_PAREN, span_of(p, tok));
            expr->paren.expr = inner;
            return expr;
        }
        break;
    default:
        break;
    }
    error_at(p, tok, "E0204", "expresion inesperada");
    advance(p);
    return NULL;
}

static FOCExpr *parse_postfix(FOCParser *p) {
    FOCExpr *expr = parse_primary(p);
    if (!expr) {
        return NULL;
    }
    for (;;) {
        FOCToken *tok = cur(p);
        if (!tok) {
            break;
        }
        if (FOC_tok_is(tok, "(")) {
            advance(p);
            FOCExpr *call = FOC_ast_expr(FOC_EXPR_CALL, span_of(p, tok));
            call->call.callee = expr;
            size_t capacity = 4;
            call->call.args = (FOCExpr **)calloc(capacity, sizeof(FOCExpr *));
            if (!call->call.args) {
                return NULL;
            }
            if (!at_punct(p, ")")) {
                for (;;) {
                    FOCExpr *arg = parse_assign(p);
                    if (call->call.arg_count == capacity) {
                        capacity *= 2;
                        FOCExpr **na =
                            (FOCExpr **)realloc(call->call.args, capacity * sizeof(void *));
                        if (!na) {
                            return NULL;
                        }
                        call->call.args = na;
                    }
                    call->call.args[call->call.arg_count++] = arg;
                    if (eat_punct(p, ",")) {
                        continue;
                    }
                    break;
                }
            }
            expect_punct(p, ")");
            expr = call;
            continue;
        }
        if (FOC_tok_is(tok, "[")) {
            advance(p);
            FOCExpr *index = parse_expr(p);
            expect_punct(p, "]");
            FOCExpr *node = FOC_ast_expr(FOC_EXPR_INDEX, span_of(p, tok));
            node->index.base = expr;
            node->index.index = index;
            expr = node;
            continue;
        }
        if (FOC_tok_is(tok, ".") || FOC_tok_is(tok, "->")) {
            bool arrow = FOC_tok_is(tok, "->");
            advance(p);
            if (!at_ident(p)) {
                error_at(p, cur(p), "E0205", "se esperaba un nombre de miembro");
                return expr;
            }
            FOCToken *name = cur(p);
            advance(p);
            FOCExpr *node = FOC_ast_expr(FOC_EXPR_MEMBER, span_of(p, tok));
            node->member.base = expr;
            node->member.arrow = arrow;
            node->member.name = name->text;
            node->member.name_len = name->len;
            node->member.name_span = span_of(p, name);
            expr = node;
            continue;
        }
        if (FOC_tok_is(tok, "++") || FOC_tok_is(tok, "--")) {
            FOCExpr *node = FOC_ast_expr(FOC_EXPR_UNARY, span_of(p, tok));
            node->unary.op = FOC_tok_is(tok, "++") ? "++" : "--";
            node->unary.operand = expr;
            node->unary.postfix = true;
            advance(p);
            expr = node;
            continue;
        }
        break;
    }
    return expr;
}

static FOCExpr *parse_unary(FOCParser *p) {
    FOCToken *tok = cur(p);
    if (!tok) {
        return NULL;
    }
    if (tok->kind == FOC_TOK_PUNCT) {
        const char *ops[] = {"++", "--", "+", "-", "!", "~", "*", "&", NULL};
        for (size_t i = 0; ops[i]; i++) {
            if (FOC_tok_is(tok, ops[i])) {
                advance(p);
                FOCExpr *operand = parse_unary(p);
                FOCExpr *expr = FOC_ast_expr(FOC_EXPR_UNARY, span_of(p, tok));
                expr->unary.op = dup_text(ops[i], strlen(ops[i]));
                expr->unary.operand = operand;
                expr->unary.postfix = false;
                return expr;
            }
        }
        if (FOC_tok_is(tok, "(")) {
            /* Conversion: (nombre-de-tipo)expresion */
            size_t saved = p->pos;
            advance(p);
            p->quiet++;
            FOCTypeName *type_name = parse_type_name(p);
            p->quiet--;
            if (type_name && at_punct(p, ")")) {
                advance(p);
                FOCExpr *operand = parse_unary(p);
                FOCExpr *expr = FOC_ast_expr(FOC_EXPR_CAST, span_of(p, tok));
                expr->cast.type_name = type_name;
                expr->cast.expr = operand;
                return expr;
            }
            p->pos = saved;
        }
    }
    return parse_postfix(p);
}

static int binary_precedence(FOCParser *p) {
    FOCToken *tok = cur(p);
    if (!tok || tok->kind != FOC_TOK_PUNCT) {
        return 0;
    }
    if (FOC_tok_is(tok, "||")) return 1;
    if (FOC_tok_is(tok, "&&")) return 2;
    if (FOC_tok_is(tok, "|")) return 3;
    if (FOC_tok_is(tok, "^")) return 4;
    if (FOC_tok_is(tok, "&")) return 5;
    if (FOC_tok_is(tok, "==") || FOC_tok_is(tok, "!=")) return 6;
    if (FOC_tok_is(tok, "<") || FOC_tok_is(tok, ">") || FOC_tok_is(tok, "<=") ||
        FOC_tok_is(tok, ">=")) {
        return 7;
    }
    if (FOC_tok_is(tok, "<<") || FOC_tok_is(tok, ">>")) return 8;
    if (FOC_tok_is(tok, "+") || FOC_tok_is(tok, "-")) return 9;
    if (FOC_tok_is(tok, "*") || FOC_tok_is(tok, "/") || FOC_tok_is(tok, "%")) return 10;
    return 0;
}

static FOCExpr *parse_binary(FOCParser *p, int min_precedence) {
    FOCExpr *lhs = parse_unary(p);
    if (!lhs) {
        return NULL;
    }
    for (;;) {
        int precedence = binary_precedence(p);
        if (precedence == 0 || precedence < min_precedence) {
            break;
        }
        FOCToken *op_token = cur(p);
        FOCSpan span = span_of(p, op_token);
        advance(p);
        FOCExpr *rhs = parse_binary(p, precedence + 1);
        FOCExpr *expr = FOC_ast_expr(FOC_EXPR_BINARY, span);
        expr->binary.op = dup_text(op_token->text, op_token->len);
        expr->binary.lhs = lhs;
        expr->binary.rhs = rhs;
        lhs = expr;
    }
    return lhs;
}

static FOCExpr *parse_cond(FOCParser *p) {
    FOCExpr *cond = parse_binary(p, 1);
    if (!cond || !at_punct(p, "?")) {
        return cond;
    }
    FOCSpan span = span_of(p, cur(p));
    advance(p);
    FOCExpr *then_expr = parse_expr(p);
    expect_punct(p, ":");
    FOCExpr *else_expr = parse_cond(p);
    FOCExpr *expr = FOC_ast_expr(FOC_EXPR_COND, span);
    expr->cond.cond = cond;
    expr->cond.then_expr = then_expr;
    expr->cond.else_expr = else_expr;
    return expr;
}

static FOCExpr *parse_assign(FOCParser *p) {
    FOCExpr *lhs = parse_cond(p);
    if (!lhs) {
        return NULL;
    }
    FOCToken *tok = cur(p);
    if (!tok || tok->kind != FOC_TOK_PUNCT) {
        return lhs;
    }
    static const char *assign_ops[] = {"=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=",
                                       "&=", "|=", "^=", NULL};
    for (size_t i = 0; assign_ops[i]; i++) {
        if (FOC_tok_is(tok, assign_ops[i])) {
            FOCSpan span = span_of(p, tok);
            advance(p);
            FOCExpr *rhs = parse_assign(p);
            FOCExpr *expr = FOC_ast_expr(FOC_EXPR_ASSIGN, span);
            expr->assign.op = dup_text(assign_ops[i], strlen(assign_ops[i]));
            expr->assign.lhs = lhs;
            expr->assign.rhs = rhs;
            return expr;
        }
    }
    return lhs;
}

static FOCExpr *parse_expr(FOCParser *p) {
    FOCExpr *expr = parse_assign(p);
    while (at_punct(p, ",")) {
        FOCSpan span = span_of(p, cur(p));
        advance(p);
        FOCExpr *rhs = parse_assign(p);
        FOCExpr *node = FOC_ast_expr(FOC_EXPR_BINARY, span);
        node->binary.op = ",";
        node->binary.lhs = expr;
        node->binary.rhs = rhs;
        expr = node;
    }
    return expr;
}

/* ------------------------------------------------------------------ */
/* Sentencias                                                          */
/* ------------------------------------------------------------------ */

static FOCStmt *parse_stmt(FOCParser *p);
static FOCStmt *parse_block(FOCParser *p);

static FOCStmt *parse_block(FOCParser *p) {
    FOCSpan span = span_of(p, cur(p));
    expect_punct(p, "{");
    FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_BLOCK, span);
    size_t capacity = 8;
    stmt->block.items = (FOCStmt **)calloc(capacity, sizeof(FOCStmt *));
    if (!stmt->block.items) {
        return NULL;
    }
    while (!at_punct(p, "}")) {
        if (!cur(p) || cur(p)->kind == FOC_TOK_EOF) {
            error_at(p, cur(p), "E0206", "falta '}'");
            break;
        }
        FOCStmt *item = parse_stmt(p);
        if (!item) {
            while (cur(p) && cur(p)->kind != FOC_TOK_EOF && !at_punct(p, ";") &&
                   !at_punct(p, "}")) {
                advance(p);
            }
            eat_punct(p, ";");
            continue;
        }
        if (stmt->block.count == capacity) {
            capacity *= 2;
            FOCStmt **ni = (FOCStmt **)realloc(stmt->block.items, capacity * sizeof(void *));
            if (!ni) {
                return NULL;
            }
            stmt->block.items = ni;
        }
        stmt->block.items[stmt->block.count++] = item;
    }
    expect_punct(p, "}");
    return stmt;
}

static FOCStmt *parse_local_decl(FOCParser *p, FOCSpan span) {
    FOCDecl *decl = (FOCDecl *)calloc(1, sizeof(FOCDecl));
    if (!decl) {
        return NULL;
    }
    decl->kind = FOC_DCL_VARIABLE;
    decl->span = span;
    decl->spec = FOC_ast_decl_spec(span);
    if (!parse_decl_specs(p, decl->spec)) {
        error_at(p, cur(p), "E0210", "se esperaba un especificador de declaracion");
        free(decl);
        return NULL;
    }
    if (!at_punct(p, ";")) {
        decl->declarator = parse_declarator(p);
        if (!decl->declarator) {
            error_at(p, cur(p), "E0209", "se esperaba un declarador");
            free(decl);
            return NULL;
        }
        if (at_punct(p, "=")) {
            advance(p);
            decl->init = parse_assign(p);
        }
        /* Declaradores adicionales: se enlazan en la cadena `next`. */
        FOCDecl *tail = decl;
        while (eat_punct(p, ",")) {
            FOCDeclarator *extra = parse_declarator(p);
            if (!extra) {
                break;
            }
            FOCDecl *more = (FOCDecl *)calloc(1, sizeof(FOCDecl));
            more->kind = decl->kind;
            more->span = extra->span;
            more->spec = decl->spec;
            more->declarator = extra;
            if (decl->spec->is_typedef) {
                FOC_map_put(&p->typedefs, extra->name, extra->name_len, (void *)1);
            }
            if (at_punct(p, "=")) {
                advance(p);
                more->init = parse_assign(p);
            }
            tail->next = more;
            tail = more;
        }
    }
    expect_punct(p, ";");
    FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_DECL, span);
    stmt->decl_stmt.decl = decl;
    return stmt;
}

static FOCStmt *parse_stmt(FOCParser *p) {
    FOCToken *tok = cur(p);
    if (!tok) {
        return NULL;
    }

    if (FOC_tok_is(tok, "{")) {
        return parse_block(p);
    }
    if (FOC_tok_is(tok, ";")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        return FOC_ast_stmt(FOC_STMT_NULL, span);
    }
    if (FOC_tok_is_kw(tok, "if")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        expect_punct(p, "(");
        FOCExpr *cond = parse_expr(p);
        expect_punct(p, ")");
        FOCStmt *then_body = parse_stmt(p);
        FOCStmt *else_body = NULL;
        if (at_kw(p, "else")) {
            advance(p);
            else_body = parse_stmt(p);
        }
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_IF, span);
        stmt->if_stmt.cond = cond;
        stmt->if_stmt.then_body = then_body;
        stmt->if_stmt.else_body = else_body;
        return stmt;
    }
    if (FOC_tok_is_kw(tok, "while")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        expect_punct(p, "(");
        FOCExpr *cond = parse_expr(p);
        expect_punct(p, ")");
        p->loop_depth++;
        FOCStmt *body = parse_stmt(p);
        p->loop_depth--;
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_WHILE, span);
        stmt->while_stmt.cond = cond;
        stmt->while_stmt.body = body;
        return stmt;
    }
    if (FOC_tok_is_kw(tok, "do")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        p->loop_depth++;
        FOCStmt *body = parse_stmt(p);
        p->loop_depth--;
        if (at_kw(p, "while")) {
            advance(p);
        } else {
            error_at(p, cur(p), "E0207", "se esperaba 'while'");
        }
        expect_punct(p, "(");
        FOCExpr *cond = parse_expr(p);
        expect_punct(p, ")");
        expect_punct(p, ";");
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_DO, span);
        stmt->do_stmt.cond = cond;
        stmt->do_stmt.body = body;
        return stmt;
    }
    if (FOC_tok_is_kw(tok, "for")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        expect_punct(p, "(");
        FOCStmt *init = NULL;
        if (!at_punct(p, ";")) {
            if (starts_declaration(p)) {
                /* parse_local_decl consume el ';' final. */
                init = parse_local_decl(p, span_of(p, cur(p)));
            } else {
                FOCExpr *e = parse_expr(p);
                init = FOC_ast_stmt(FOC_STMT_EXPR, e ? e->span : span_of(p, tok));
                init->expr.expr = e;
                expect_punct(p, ";");
            }
        } else {
            advance(p);
        }
        FOCExpr *cond = at_punct(p, ";") ? NULL : parse_expr(p);
        expect_punct(p, ";");
        FOCExpr *post = at_punct(p, ")") ? NULL : parse_expr(p);
        expect_punct(p, ")");
        p->loop_depth++;
        FOCStmt *body = parse_stmt(p);
        p->loop_depth--;
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_FOR, span);
        stmt->for_stmt.init = init;
        stmt->for_stmt.cond = cond;
        stmt->for_stmt.post = post;
        stmt->for_stmt.body = body;
        return stmt;
    }
    if (FOC_tok_is_kw(tok, "switch")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        expect_punct(p, "(");
        FOCExpr *cond = parse_expr(p);
        expect_punct(p, ")");
        p->switch_depth++;
        FOCStmt *body = parse_stmt(p);
        p->switch_depth--;
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_SWITCH, span);
        stmt->switch_stmt.cond = cond;
        stmt->switch_stmt.body = body;
        return stmt;
    }
    if (FOC_tok_is_kw(tok, "case")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        FOCExpr *value = parse_cond(p);
        expect_punct(p, ":");
        FOCStmt *body = at_punct(p, "}") ? NULL : parse_stmt(p);
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_CASE, span);
        stmt->case_stmt.value = value;
        stmt->case_stmt.body = body;
        return stmt;
    }
    if (FOC_tok_is_kw(tok, "default")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        expect_punct(p, ":");
        FOCStmt *body = at_punct(p, "}") ? NULL : parse_stmt(p);
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_DEFAULT, span);
        stmt->default_stmt.body = body;
        return stmt;
    }
    if (FOC_tok_is_kw(tok, "break")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        expect_punct(p, ";");
        return FOC_ast_stmt(FOC_STMT_BREAK, span);
    }
    if (FOC_tok_is_kw(tok, "continue")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        expect_punct(p, ";");
        return FOC_ast_stmt(FOC_STMT_CONTINUE, span);
    }
    if (FOC_tok_is_kw(tok, "return")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        FOCExpr *value = at_punct(p, ";") ? NULL : parse_expr(p);
        expect_punct(p, ";");
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_RETURN, span);
        stmt->return_stmt.value = value;
        return stmt;
    }
    if (FOC_tok_is_kw(tok, "goto")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        if (!at_ident(p)) {
            error_at(p, cur(p), "E0208", "se esperaba una etiqueta");
            return NULL;
        }
        FOCToken *name = cur(p);
        advance(p);
        expect_punct(p, ";");
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_GOTO, span);
        stmt->goto_stmt.name = name->text;
        stmt->goto_stmt.name_len = name->len;
        return stmt;
    }
    if (FOC_tok_is_kw(tok, "_Static_assert") || FOC_tok_is_kw(tok, "static_assert")) {
        FOCSpan span = span_of(p, tok);
        advance(p);
        expect_punct(p, "(");
        FOCExpr *cond = parse_cond(p);
        if (eat_punct(p, ",")) {
            parse_expr(p);
        }
        expect_punct(p, ")");
        expect_punct(p, ";");
        FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_DECL, span);
        FOCDecl *decl = (FOCDecl *)calloc(1, sizeof(FOCDecl));
        decl->kind = FOC_DCL_VARIABLE;
        decl->span = span;
        decl->spec = FOC_ast_decl_spec(span);
        decl->init = cond;
        stmt->decl_stmt.decl = decl;
        return stmt;
    }

    /* Etiqueta: identificador seguido de ':' */
    if (tok->kind == FOC_TOK_IDENT && !FOC_is_keyword(tok->text, tok->len)) {
        FOCToken *next = peek(p, 1);
        if (next && next->kind == FOC_TOK_PUNCT && FOC_tok_is(next, ":")) {
            FOCSpan span = span_of(p, tok);
            advance(p);
            advance(p);
            FOCStmt *body = at_punct(p, "}") ? NULL : parse_stmt(p);
            FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_LABEL, span);
            stmt->label_stmt.name = tok->text;
            stmt->label_stmt.name_len = tok->len;
            stmt->label_stmt.body = body;
            return stmt;
        }
    }

    /* Declaracion dentro de un bloque. */
    if (starts_declaration(p)) {
        return parse_local_decl(p, span_of(p, cur(p)));
    }

    /* Sentencia de expresion. */
    FOCExpr *expr = parse_expr(p);
    expect_punct(p, ";");
    FOCStmt *stmt = FOC_ast_stmt(FOC_STMT_EXPR, expr ? expr->span : span_of(p, tok));
    stmt->expr.expr = expr;
    return stmt;
}

/* ------------------------------------------------------------------ */
/* Declaraciones externas                                              */
/* ------------------------------------------------------------------ */

static void unit_push(FOCUnit *unit, FOCDecl *decl) {
    FOCDecl **data = (FOCDecl **)realloc(unit->decls, (unit->count + 1) * sizeof(FOCDecl *));
    if (!data) {
        fprintf(stderr, "foc: agotamiento de memoria\n");
        abort();
    }
    unit->decls = data;
    unit->decls[unit->count++] = decl;
}

static FOCDecl *parse_external_decl(FOCParser *p) {
    FOCSpan span = span_of(p, cur(p));
    FOCDecl *decl = (FOCDecl *)calloc(1, sizeof(FOCDecl));
    if (!decl) {
        return NULL;
    }
    decl->kind = FOC_DCL_VARIABLE;
    decl->span = span;
    decl->spec = FOC_ast_decl_spec(span);
    if (!parse_decl_specs(p, decl->spec)) {
        error_at(p, cur(p), "E0210", "se esperaba un especificador de declaracion");
        free(decl);
        return NULL;
    }
    if (eat_punct(p, ";")) {
        return decl; /* declaracion de struct/enum sin declaradores */
    }

    decl->declarator = parse_declarator(p);
    if (!decl->declarator) {
        error_at(p, cur(p), "E0209", "se esperaba un declarador");
        free(decl);
        return NULL;
    }

    if (decl->spec->is_typedef) {
        FOC_map_put(&p->typedefs, decl->declarator->name, decl->declarator->name_len, (void *)1);
    }

    if (at_punct(p, "{")) {
        decl->kind = FOC_DCL_FUNCTION;
        decl->body = parse_block(p);
        return decl;
    }

    if (at_punct(p, "=")) {
        advance(p);
        decl->init = parse_assign(p);
    }
    /* Declaradores adicionales: se enlazan en la cadena `next`. */
    FOCDecl *tail = decl;
    while (eat_punct(p, ",")) {
        FOCDeclarator *extra = parse_declarator(p);
        if (!extra) {
            break;
        }
        FOCDecl *more = (FOCDecl *)calloc(1, sizeof(FOCDecl));
        more->kind = decl->kind;
        more->span = extra->span;
        more->spec = decl->spec;
        more->declarator = extra;
        if (decl->spec->is_typedef) {
            FOC_map_put(&p->typedefs, extra->name, extra->name_len, (void *)1);
        }
        if (at_punct(p, "=")) {
            advance(p);
            more->init = parse_assign(p);
        }
        tail->next = more;
        tail = more;
    }
    expect_punct(p, ";");
    return decl;
}

FOCDeclarator *FOC_parse_declarator_pub(FOCParser *p) { return parse_declarator(p); }

FOCExpr *FOC_parse_expression_pub(FOCParser *p) { return parse_expr(p); }

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */

bool FOC_parse(const FOCTokenList *tokens, FOCDiagEngine *diag, FOCUnit **out_unit) {
    FOCParser parser;
    memset(&parser, 0, sizeof(parser));
    parser.tokens = tokens;
    parser.diag = diag;
    parser.unit = FOC_ast_unit();
    FOC_map_init(&parser.typedefs);

    while (cur(&parser) && cur(&parser)->kind != FOC_TOK_EOF) {
        size_t before = parser.pos;
        FOCDecl *decl = parse_external_decl(&parser);
        if (decl) {
            unit_push(parser.unit, decl);
        }
        if (parser.pos == before) {
            error_at(&parser, cur(&parser), "E0211", "token inesperado");
            if (parser.pos + 1 < parser.tokens->len) {
                advance(&parser);
            } else {
                break;
            }
        }
    }

    FOC_map_free(&parser.typedefs);
    *out_unit = parser.unit;
    return parser.error_count == 0;
}
