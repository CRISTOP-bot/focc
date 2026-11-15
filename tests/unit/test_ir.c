#include <stdlib.h>
/* Pruebas de la IR y del lowering. */
#include "test.h"

#include "foc/diag.h"
#include "foc/ir.h"
#include "foc/lexer.h"
#include "foc/parser.h"
#include "foc/platform.h"
#include "foc/source.h"

static FOCIRModule *build_module(const char *text) {
    FOCArena *arena = (FOCArena *)malloc(sizeof(FOCArena));
    FOC_arena_init(arena);
    FOCSourceMgr *sources = (FOCSourceMgr *)malloc(sizeof(FOCSourceMgr));
    FOC_source_init(sources, arena);
    FOCDiagEngine *diag = (FOCDiagEngine *)malloc(sizeof(FOCDiagEngine));
    FOC_diag_init(diag, sources, arena, stderr);

    FOCSourceFile *file = FOC_source_add(sources, "<test>", text, strlen(text));
    FOCTokenList tokens;
    FOC_token_list_init(&tokens);
    FOC_lex_file(file, diag, &tokens);
    FOCUnit *unit = NULL;
    FOC_parse(&tokens, diag, &unit);
    FOC_token_list_free(&tokens);

    FOCTarget target = FOC_default_target();
    FOCIRModule *module = FOC_irgen(unit, diag, sources, &target);
    free(diag);
    free(sources);
    free(arena);
    return module;
}

void test_ir(void) {
    foc_test_begin("ir");

    FOCIRModule *module = build_module("int suma(int a, int b) { return a + b; }");
    CHECK(module != NULL);
    CHECK_INT(module->function_count, 1);
    FOCIRFunction *function = module->functions[0];
    CHECK_STR(function->name, "suma");
    CHECK_INT(function->param_count, 2);
    CHECK_INT(function->ret_type, FOC_IR_I32);

    /* Debe existir una operacion de adicion. */
    bool found_add = false;
    for (size_t i = 0; i < function->inst_count; i++) {
        if (FOC_ir_inst_at(function, i)->op == FOC_OP_ADD) {
            found_add = true;
        }
    }
    CHECK(found_add);

    /* Variables globales. */
    module = build_module("int contador = 7;");
    CHECK_INT(module->global_count, 1);
    CHECK_STR(module->globals[0].name, "contador");
    CHECK_INT(module->globals[0].init_int, 7);
    CHECK(module->globals[0].has_init);
}
