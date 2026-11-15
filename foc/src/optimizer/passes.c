/* Punto de entrada del optimizador: decide que pasadas se aplican. */
#include "foc/optimizer.h"

void FOC_optimize(FOCIRModule *module, int level) {
    if (!module || level <= 0) {
        return;
    }
    for (size_t i = 0; i < module->function_count; i++) {
        FOCIRFunction *function = module->functions[i];
        FOC_opt_constant_fold(function);
        FOC_opt_dead_code(function);
        if (level >= 2) {
            FOC_opt_copy_propagation(function);
        }
        if (level >= 3) {
            FOC_opt_constant_fold(function);
            FOC_opt_dead_code(function);
        }
    }
}
