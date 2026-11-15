#include <stdlib.h>
/* Pruebas del parser y del analisis semantico. */
#include "test.h"

#include "foc/diag.h"
#include "foc/parser.h"
#include "foc/source.h"

static FOCUnit *parse_text(const char *text, int *errors) {
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
    *errors = diag->error_count;
    FOC_token_list_free(&tokens);
    free(diag);
    free(sources);
    free(arena);
    return unit;
}

void test_parser(void) {
    foc_test_begin("parser");

    /* Los nombres del fuente no estan terminados: se comprueban con longitud. */
    int errors = 0;
    FOCUnit *unit = parse_text("int main(void) { return 0; }", &errors);
    CHECK_INT(errors, 0);
    CHECK(unit != NULL);
    CHECK_INT(unit->count, 1);
    CHECK_INT(unit->decls[0]->kind, FOC_DCL_FUNCTION);
    CHECK_INT(unit->decls[0]->declarator->ctor_count, 1);
    CHECK_INT(unit->decls[0]->declarator->ctors[0].kind, FOC_CTOR_FUNCTION);
    CHECK_INT(unit->decls[0]->declarator->name_len, 4);
    CHECK(memcmp(unit->decls[0]->declarator->name, "main", 4) == 0);
    CHECK(unit->decls[0]->body != NULL);

    /* Variable global. */
    unit = parse_text("int contador = 3;", &errors);
    CHECK_INT(errors, 0);
    CHECK_INT(unit->count, 1);
    CHECK_INT(unit->decls[0]->kind, FOC_DCL_VARIABLE);
    CHECK_INT(unit->decls[0]->declarator->name_len, 8);
    CHECK(memcmp(unit->decls[0]->declarator->name, "contador", 8) == 0);

    /* Struct. */
    unit = parse_text("struct Punto { int x; int y; };", &errors);
    CHECK_INT(errors, 0);
    CHECK(unit->decls[0]->spec->struct_spec != NULL);
    CHECK_INT(unit->decls[0]->spec->struct_spec->field_count, 2);

    /* Enum. */
    unit = parse_text("enum Color { ROJO, VERDE = 5, AZUL };", &errors);
    CHECK_INT(errors, 0);
    CHECK(unit->decls[0]->spec->enum_spec != NULL);
    CHECK_INT(unit->decls[0]->spec->enum_spec->item_count, 3);

    /* Expresiones: la precedencia de multiplicacion sobre suma. */
    unit = parse_text("int f(void) { return 1 + 2 * 3; }", &errors);
    CHECK_INT(errors, 0);
    CHECK(unit->decls[0]->body->block.count == 1);

    /* Error recuperable: falta un punto y coma. */
    unit = parse_text("int f(void) { int x = 1 return x; }", &errors);
    CHECK(errors > 0);

    /* Declaradores complejos: puntero a array de 3 enteros. */
    unit = parse_text("int (*p)[3];", &errors);
    CHECK_INT(errors, 0);
    CHECK_INT(unit->decls[0]->declarator->ctor_count, 2);
    CHECK_INT(unit->decls[0]->declarator->ctors[0].kind, FOC_CTOR_ARRAY);
    CHECK_INT(unit->decls[0]->declarator->ctors[1].kind, FOC_CTOR_POINTER);

    /* Array de punteros a funcion. */
    unit = parse_text("int (*tabla[3])(int);", &errors);
    CHECK_INT(errors, 0);
    CHECK_INT(unit->decls[0]->declarator->ctor_count, 3);
    CHECK_INT(unit->decls[0]->declarator->ctors[0].kind, FOC_CTOR_FUNCTION);
    CHECK_INT(unit->decls[0]->declarator->ctors[1].kind, FOC_CTOR_POINTER);
    CHECK_INT(unit->decls[0]->declarator->ctors[2].kind, FOC_CTOR_ARRAY);
}
