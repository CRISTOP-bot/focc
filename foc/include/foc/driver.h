/* Driver de FOC. */
#ifndef FOC_DRIVER_H
#define FOC_DRIVER_H

#include "foc/diag.h"
#include "foc/options.h"
#include "foc/source.h"

/* Ejecuta la compilacion con las opciones dadas. Devuelve el codigo de salida. */
int FOC_driver_run(FOCOptions *options, FOCDiagEngine *diag, FOCSourceMgr *sources);

#endif /* FOC_DRIVER_H */
