/* Convenciones de llamada (ABI) por arquitectura. */
#ifndef FOC_BACKEND_H
#define FOC_BACKEND_H

#include "foc/ir.h"
#include "foc/platform.h"

typedef struct {
    bool windows;     /* Windows x64 */
    bool sysv;        /* System V AMD64 */
    int int_args[6];  /* registros de argumentos enteros (indices) */
    int sse_args[8];  /* registros de argumentos de coma flotante */
    int shadow_space; /* bytes de espacio de sombra (Windows) */
    int stack_args_offset; /* desplazamiento del primer argumento en pila */
} FOCABI;

/* Devuelve la ABI correspondiente al destino. */
const FOCABI *FOC_abi_for(const FOCTarget *target);

/* Emite el ensamblador del modulo en el buffer indicado. */
bool FOC_emit_x86_64(FOCIRModule *module, const FOCTarget *target, char **out_text,
                     size_t *out_len);

/* Nombre de la plataforma de objeto (ELF, COFF, Mach-O). */
const char *FOC_object_format(const FOCTarget *target);

#endif /* FOC_BACKEND_H */
