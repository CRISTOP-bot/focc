#include "foc/sema.h"

#include <stdio.h>
#include <string.h>

bool FOC_sema_is_signed(const FOCType *type) { return FOC_type_is_signed(type); }

void FOC_sema_type_name(const FOCType *type, char *out, size_t out_size) {
    FOC_type_print(type, out, out_size);
}

/* Evalua una expresion constante entera (C99 6.6). */
bool FOC_sema_eval_int(const FOCExpr *expr, long long *out) {
    if (!expr) {
        return false;
    }
    switch (expr->kind) {
    case FOC_EXPR_INT:
        *out = expr->int_lit.value;
        return true;
    case FOC_EXPR_CHAR:
        *out = expr->char_lit.value;
        return true;
    case FOC_EXPR_PAREN:
        return FOC_sema_eval_int(expr->paren.expr, out);
    case FOC_EXPR_UNARY: {
        long long value = 0;
        if (!FOC_sema_eval_int(expr->unary.operand, &value)) {
            return false;
        }
        if (!expr->unary.op) {
            return false;
        }
        if (!strcmp(expr->unary.op, "-")) {
            *out = -value;
            return true;
        }
        if (!strcmp(expr->unary.op, "+")) {
            *out = value;
            return true;
        }
        if (!strcmp(expr->unary.op, "~")) {
            *out = ~value;
            return true;
        }
        if (!strcmp(expr->unary.op, "!")) {
            *out = !value;
            return true;
        }
        return false;
    }
    case FOC_EXPR_BINARY: {
        long long lhs = 0;
        long long rhs = 0;
        if (!expr->binary.op) {
            return false;
        }
        if (!strcmp(expr->binary.op, "&&") || !strcmp(expr->binary.op, "||")) {
            if (!FOC_sema_eval_int(expr->binary.lhs, &lhs)) {
                return false;
            }
            if (!FOC_sema_eval_int(expr->binary.rhs, &rhs)) {
                return false;
            }
            *out = !strcmp(expr->binary.op, "&&") ? (lhs && rhs) : (lhs || rhs);
            return true;
        }
        if (!FOC_sema_eval_int(expr->binary.lhs, &lhs)) {
            return false;
        }
        if (!FOC_sema_eval_int(expr->binary.rhs, &rhs)) {
            return false;
        }
        const char *op = expr->binary.op;
        if (!strcmp(op, "+")) {
            *out = lhs + rhs;
        } else if (!strcmp(op, "-")) {
            *out = lhs - rhs;
        } else if (!strcmp(op, "*")) {
            *out = lhs * rhs;
        } else if (!strcmp(op, "/")) {
            if (rhs == 0) {
                return false;
            }
            *out = lhs / rhs;
        } else if (!strcmp(op, "%")) {
            if (rhs == 0) {
                return false;
            }
            *out = lhs % rhs;
        } else if (!strcmp(op, "&")) {
            *out = lhs & rhs;
        } else if (!strcmp(op, "|")) {
            *out = lhs | rhs;
        } else if (!strcmp(op, "^")) {
            *out = lhs ^ rhs;
        } else if (!strcmp(op, "<<")) {
            *out = lhs << rhs;
        } else if (!strcmp(op, ">>")) {
            *out = lhs >> rhs;
        } else if (!strcmp(op, "==")) {
            *out = lhs == rhs;
        } else if (!strcmp(op, "!=")) {
            *out = lhs != rhs;
        } else if (!strcmp(op, "<")) {
            *out = lhs < rhs;
        } else if (!strcmp(op, "<=")) {
            *out = lhs <= rhs;
        } else if (!strcmp(op, ">")) {
            *out = lhs > rhs;
        } else if (!strcmp(op, ">=")) {
            *out = lhs >= rhs;
        } else {
            return false;
        }
        return true;
    }
    case FOC_EXPR_COND: {
        long long cond = 0;
        if (!FOC_sema_eval_int(expr->cond.cond, &cond)) {
            return false;
        }
        return FOC_sema_eval_int(cond ? expr->cond.then_expr : expr->cond.else_expr, out);
    }
    case FOC_EXPR_SIZEOF:
        if (expr->sizeof_expr.type_name) {
            return false; /* requiere resolucion de tipos */
        }
        return false;
    default:
        return false;
    }
}
