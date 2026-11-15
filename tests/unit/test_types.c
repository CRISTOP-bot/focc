/* Pruebas del sistema de tipos. */
#include "test.h"

#include "foc/types.h"

void test_types(void) {
    foc_test_begin("tipos");

    FOCType *int_type = FOC_type_basic(FOC_TYPE_INT);
    FOCType *char_type = FOC_type_basic(FOC_TYPE_CHAR);
    FOCType *double_type = FOC_type_basic(FOC_TYPE_DOUBLE);

    CHECK(FOC_type_is_integer(int_type));
    CHECK(FOC_type_is_integer(char_type));
    CHECK(!FOC_type_is_integer(double_type));
    CHECK(FOC_type_is_float(double_type));
    CHECK(FOC_type_is_arithmetic(int_type));
    CHECK(FOC_type_is_signed(int_type));
    CHECK(FOC_type_is_unsigned(FOC_type_basic(FOC_TYPE_UINT)));

    FOCType *pointer = FOC_type_pointer(int_type);
    CHECK(FOC_type_is_pointer(pointer));
    CHECK(pointer->elem == int_type);

    FOCType *array = FOC_type_array(char_type, 10);
    CHECK(FOC_type_is_array(array));
    CHECK_INT(array->array_len, 10);
    CHECK_INT(array->size, 10);

    CHECK(FOC_type_equal(int_type, int_type));
    CHECK(!FOC_type_equal(int_type, char_type));
    CHECK(FOC_type_equal(pointer, pointer));

    /* Una fonction con parametros. */
    FOCType *params[1];
    params[0] = int_type;
    FOCType *function = FOC_type_function(int_type, params, 1, false);
    CHECK(FOC_type_is_function(function));
    CHECK(function->ret == int_type);
    CHECK_INT(function->param_count, 1);

    /* Tamanos por defecto en x86-64. */
    CHECK_INT(FOC_type_default_size(FOC_TYPE_CHAR, 8), 1);
    CHECK_INT(FOC_type_default_size(FOC_TYPE_SHORT, 8), 2);
    CHECK_INT(FOC_type_default_size(FOC_TYPE_INT, 8), 4);
    CHECK_INT(FOC_type_default_size(FOC_TYPE_LONG, 8), 8);
    CHECK_INT(FOC_type_default_size(FOC_TYPE_POINTER, 8), 8);
}
