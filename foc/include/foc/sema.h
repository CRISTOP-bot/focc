/* Analisis semantico: evaluacion de expresiones constantes y utilidades
 * de tipos compartidas con el frontend. */
#ifndef FOC_SEMA_H
#define FOC_SEMA_H

#include "foc/ast.h"
#include "foc/types.h"

/* Evalua una expresion constante entera. Devuelve true si lo es. */
bool FOC_sema_eval_int(const FOCExpr *expr, long long *out);

/* Indica si el tipo es un entero con signo. */
bool FOC_sema_is_signed(const FOCType *type);

/* Nombre legible de un tipo. */
void FOC_sema_type_name(const FOCType *type, char *out, size_t out_size);

#endif /* FOC_SEMA_H */
