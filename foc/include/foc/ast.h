/* AST de FOC. */
#ifndef FOC_AST_H
#define FOC_AST_H

#include "foc/source.h"
#include "foc/types.h"

typedef struct FOCExpr FOCExpr;
typedef struct FOCStmt FOCStmt;
typedef struct FOCDecl FOCDecl;
typedef struct FOCDeclSpec FOCDeclSpec;
typedef struct FOCDeclarator FOCDeclarator;
typedef struct FOCStructSpec FOCStructSpec;
typedef struct FOCEnumSpec FOCEnumSpec;
typedef struct FOCParam FOCParam;
typedef struct FOCTypeName FOCTypeName;

/* ------------------------------------------------------------------ */
/* Especificadores de declaracion                                       */
/* ------------------------------------------------------------------ */

struct FOCDeclSpec {
    FOCSpan span;
    bool is_typedef;
    bool is_extern;
    bool is_static;
    bool is_auto;
    bool is_register;
    bool is_thread_local;
    bool is_const;
    bool is_volatile;
    bool is_restrict;
    bool is_inline;
    bool is_noreturn;
    /* Tipo basico (combinacion de palabras clave). */
    bool t_void;
    bool t_bool;
    bool t_char;
    bool t_int;
    bool t_float;
    bool t_double;
    bool t_signed;
    bool t_unsigned;
    bool t_short;
    bool t_complex;
    int long_count;
    /* Tipos etiquetados. */
    FOCStructSpec *struct_spec;
    FOCEnumSpec *enum_spec;
    /* Nombre de typedef. */
    const char *typedef_name;
    uint32_t typedef_len;
    FOCSpan typedef_span;
};

/* ------------------------------------------------------------------ */
/* Declaradores                                                        */
/* ------------------------------------------------------------------ */

/* Un declarador se modela como una lista de constructores de tipo en
 * orden de aplicacion (del mas lejano al mas cercano al identificador).
 * Asi `int *a[3]` -> [PUNTER, ARRAY(3)] y `int (*a)[3]` -> [ARRAY(3), PUNTER],
 * que reproduce exactamente la semantica de C sin ambiguedades. */
typedef enum {
    FOC_CTOR_POINTER = 0,
    FOC_CTOR_ARRAY,
    FOC_CTOR_FUNCTION
} FOCDeclCtorKind;

typedef struct {
    FOCDeclCtorKind kind;
    /* Puntero. */
    bool is_const;
    bool is_volatile;
    bool is_restrict;
    /* Array. */
    FOCExpr *array_len;
    bool array_static;
    bool array_const;
    bool array_volatile;
    bool array_restrict;
    /* Funcion. */
    FOCParam **params;
    size_t param_count;
    bool variadic;
    bool has_proto; /* la lista de parametros es explicita */
} FOCDeclCtor;

struct FOCDeclarator {
    FOCSpan span;
    const char *name;
    uint32_t name_len;
    FOCSpan name_span;
    bool has_name;
    FOCDeclCtor *ctors;
    size_t ctor_count;
};

struct FOCParam {
    FOCSpan span;
    FOCDeclSpec *spec;
    FOCDeclarator *declarator; /* puede ser NULL */
};

struct FOCStructSpec {
    FOCSpan span;
    bool is_union;
    const char *tag;
    uint32_t tag_len;
    FOCSpan tag_span;
    bool has_body;
    struct FOCFieldDecl **fields;
    size_t field_count;
};

typedef struct FOCFieldDecl {
    FOCSpan span;
    FOCDeclSpec *spec;
    FOCDeclarator **declarators;
    size_t declarator_count;
    FOCExpr **bit_widths; /* paralelo a declarators; puede ser NULL */
} FOCFieldDecl;

typedef struct FOCEnumItem {
    const char *name;
    uint32_t name_len;
    FOCSpan name_span;
    FOCExpr *value;
} FOCEnumItem;

struct FOCEnumSpec {
    FOCSpan span;
    const char *tag;
    uint32_t tag_len;
    FOCSpan tag_span;
    bool has_body;
    FOCEnumItem **items;
    size_t item_count;
};

struct FOCTypeName {
    FOCSpan span;
    FOCDeclSpec *spec;
    FOCDeclarator *declarator; /* puede ser NULL */
};

/* ------------------------------------------------------------------ */
/* Declaraciones                                                       */
/* ------------------------------------------------------------------ */

typedef enum { FOC_DCL_FUNCTION = 0, FOC_DCL_VARIABLE } FOCDeclKind;

struct FOCDecl {
    FOCDeclKind kind;
    FOCSpan span;
    FOCDeclSpec *spec;
    FOCDeclarator *declarator;
    FOCExpr *init;
    FOCStmt *body; /* cuerpo de la definicion de funcion */
    FOCDecl *next; /* resto de declaradores de la misma declaracion */
};

/* ------------------------------------------------------------------ */
/* Expresiones                                                         */
/* ------------------------------------------------------------------ */

typedef enum {
    FOC_EXPR_INT = 0,
    FOC_EXPR_FLOAT,
    FOC_EXPR_CHAR,
    FOC_EXPR_STRING,
    FOC_EXPR_IDENT,
    FOC_EXPR_PAREN,
    FOC_EXPR_UNARY,
    FOC_EXPR_BINARY,
    FOC_EXPR_ASSIGN,
    FOC_EXPR_COND,
    FOC_EXPR_CALL,
    FOC_EXPR_MEMBER,
    FOC_EXPR_INDEX,
    FOC_EXPR_CAST,
    FOC_EXPR_SIZEOF,
    FOC_EXPR_COMPOUND,
    FOC_EXPR_INIT_LIST,
    FOC_EXPR_STMT_EXPR
} FOCExprKind;

struct FOCExpr {
    FOCExprKind kind;
    FOCSpan span;
    FOCType *type; /* lo rellena el analisis semantico */
    union {
        struct {
            const char *text;
            uint32_t len;
            long long value;
            bool is_unsigned;
        } int_lit;
        struct {
            const char *text;
            uint32_t len;
            double value;
        } float_lit;
        struct {
            const char *text;
            uint32_t len;
            long long value;
        } char_lit;
        struct {
            const char *text;
            uint32_t len;
            const char *value; /* contenido sin comillas */
            size_t value_len;
        } string_lit;
        struct {
            const char *name;
            uint32_t name_len;
            FOCSpan name_span;
        } ident;
        struct {
            FOCExpr *expr;
        } paren;
        struct {
            const char *op;
            FOCExpr *operand;
            bool postfix;
        } unary;
        struct {
            const char *op;
            FOCExpr *lhs;
            FOCExpr *rhs;
        } binary;
        struct {
            const char *op;
            FOCExpr *lhs;
            FOCExpr *rhs;
        } assign;
        struct {
            FOCExpr *cond;
            FOCExpr *then_expr;
            FOCExpr *else_expr;
        } cond;
        struct {
            FOCExpr *callee;
            FOCExpr **args;
            size_t arg_count;
        } call;
        struct {
            FOCExpr *base;
            bool arrow;
            const char *name;
            uint32_t name_len;
            FOCSpan name_span;
        } member;
        struct {
            FOCExpr *base;
            FOCExpr *index;
        } index;
        struct {
            FOCTypeName *type_name;
            FOCExpr *expr;
        } cast;
        struct {
            FOCExpr *expr;    /* sizeof(expr) */
            FOCTypeName *type_name; /* sizeof(tipo) */
        } sizeof_expr;
        struct {
            FOCTypeName *type_name;
            FOCExpr *init;
        } compound;
        struct {
            FOCExpr **items;
            size_t item_count;
        } init_list;
        struct {
            FOCStmt *body;
        } stmt_expr;
    };
};

/* ------------------------------------------------------------------ */
/* Sentencias                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    FOC_STMT_BLOCK = 0,
    FOC_STMT_EXPR,
    FOC_STMT_IF,
    FOC_STMT_WHILE,
    FOC_STMT_DO,
    FOC_STMT_FOR,
    FOC_STMT_SWITCH,
    FOC_STMT_CASE,
    FOC_STMT_DEFAULT,
    FOC_STMT_BREAK,
    FOC_STMT_CONTINUE,
    FOC_STMT_RETURN,
    FOC_STMT_GOTO,
    FOC_STMT_LABEL,
    FOC_STMT_NULL,
    FOC_STMT_DECL
} FOCStmtKind;

struct FOCStmt {
    FOCStmtKind kind;
    FOCSpan span;
    FOCType *type;
    union {
        struct {
            FOCStmt **items;
            size_t count;
        } block;
        struct {
            FOCExpr *expr;
        } expr;
        struct {
            FOCExpr *cond;
            FOCStmt *then_body;
            FOCStmt *else_body;
        } if_stmt;
        struct {
            FOCExpr *cond;
            FOCStmt *body;
        } while_stmt;
        struct {
            FOCExpr *cond;
            FOCStmt *body;
        } do_stmt;
        struct {
            FOCStmt *init;
            FOCExpr *cond;
            FOCExpr *post;
            FOCStmt *body;
        } for_stmt;
        struct {
            FOCExpr *cond;
            FOCStmt *body;
        } switch_stmt;
        struct {
            FOCExpr *value;
            FOCStmt *body;
        } case_stmt;
        struct {
            FOCStmt *body;
        } default_stmt;
        struct {
            FOCExpr *value;
        } return_stmt;
        struct {
            const char *name;
            uint32_t name_len;
        } goto_stmt;
        struct {
            const char *name;
            uint32_t name_len;
            FOCStmt *body;
        } label_stmt;
        struct {
            FOCDecl *decl;
        } decl_stmt;
    };
};

/* ------------------------------------------------------------------ */
/* Unidad de traduccion                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    FOCDecl **decls;
    size_t count;
} FOCUnit;

/* Constructores (src/frontend/ast.c). */
FOCExpr *FOC_ast_expr(FOCExprKind kind, FOCSpan span);
FOCStmt *FOC_ast_stmt(FOCStmtKind kind, FOCSpan span);
FOCDecl *FOC_ast_decl(FOCDeclKind kind, FOCSpan span);
FOCDeclSpec *FOC_ast_decl_spec(FOCSpan span);
FOCTypeName *FOC_ast_type_name(FOCSpan span);
FOCUnit *FOC_ast_unit(void);

/* Asigna el tipo a una expresion (usado por el analisis semantico). */
void FOC_ast_set_type(FOCExpr *expr, FOCType *type);

#endif /* FOC_AST_H */
