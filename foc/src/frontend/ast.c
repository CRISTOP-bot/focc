#include "foc/ast.h"

#include <stdlib.h>
#include <string.h>

static void *ast_alloc(size_t size) {
    void *ptr = calloc(1, size);
    if (!ptr) {
        fprintf(stderr, "foc: agotamiento de memoria\n");
        abort();
    }
    return ptr;
}

FOCExpr *FOC_ast_expr(FOCExprKind kind, FOCSpan span) {
    FOCExpr *expr = (FOCExpr *)ast_alloc(sizeof(FOCExpr));
    expr->kind = kind;
    expr->span = span;
    return expr;
}

FOCStmt *FOC_ast_stmt(FOCStmtKind kind, FOCSpan span) {
    FOCStmt *stmt = (FOCStmt *)ast_alloc(sizeof(FOCStmt));
    stmt->kind = kind;
    stmt->span = span;
    return stmt;
}

FOCDecl *FOC_ast_decl(FOCDeclKind kind, FOCSpan span) {
    FOCDecl *decl = (FOCDecl *)ast_alloc(sizeof(FOCDecl));
    decl->kind = kind;
    decl->span = span;
    return decl;
}

FOCDeclSpec *FOC_ast_decl_spec(FOCSpan span) {
    FOCDeclSpec *spec = (FOCDeclSpec *)ast_alloc(sizeof(FOCDeclSpec));
    spec->span = span;
    return spec;
}

FOCTypeName *FOC_ast_type_name(FOCSpan span) {
    FOCTypeName *name = (FOCTypeName *)ast_alloc(sizeof(FOCTypeName));
    name->span = span;
    return name;
}

FOCUnit *FOC_ast_unit(void) {
    FOCUnit *unit = (FOCUnit *)ast_alloc(sizeof(FOCUnit));
    return unit;
}

void FOC_ast_set_type(FOCExpr *expr, FOCType *type) {
    if (expr) {
        expr->type = type;
    }
}
