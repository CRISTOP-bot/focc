/* Optimizaciones sobre la IR. */
#ifndef FOC_OPTIMIZER_H
#define FOC_OPTIMIZER_H

#include "foc/ir.h"

/* Aplica las optimizaciones correspondientes al nivel indicado.
 * level: 0 = sin optimizaciones, 1..3 = crecientes, 4 = tamano, 5 = minimo. */
void FOC_optimize(FOCIRModule *module, int level);

/* Pasadas individuales (implementadas en libcc1). */
void FOC_opt_constant_fold(FOCIRFunction *function);
void FOC_opt_dead_code(FOCIRFunction *function);
void FOC_opt_copy_propagation(FOCIRFunction *function);

#endif /* FOC_OPTIMIZER_H */
