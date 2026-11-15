/* Pruebas del backend x86-64. */
#include "test.h"

#include "foc/backend.h"
#include "foc/platform.h"

#include <stdlib.h>
#include <string.h>

void test_backend(void) {
    foc_test_begin("backend x86-64");

    FOCIRFunction function = {0};
    function.name = "main";
    FOCIRFunction *functions[] = {&function};
    FOCIRModule module = {0};
    module.functions = functions;
    module.function_count = 1;
    FOCTarget target = FOC_default_target();

    char *text = NULL;
    size_t expected_length = 0;
    CHECK(FOC_emit_x86_64(&module, &target, &text, &expected_length));
    CHECK(text != NULL);

    size_t measured_length = 0;
    CHECK(FOC_emit_x86_64(&module, &target, NULL, &measured_length));
    CHECK_INT(measured_length, expected_length);

    free(text);
}
