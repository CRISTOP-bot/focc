/* Parser de C. */
#ifndef FOC_PARSER_H
#define FOC_PARSER_H

#include "foc/ast.h"
#include "foc/diag.h"
#include "foc/lexer.h"

/* Analiza una lista de tokens y produce la unidad de traducción.
 * Devuelve false si hubo errores. */
bool FOC_parse(const FOCTokenList *tokens, FOCDiagEngine *diag, FOCUnit **out_unit);

#endif /* FOC_PARSER_H */
