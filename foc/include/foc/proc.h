#ifndef FOC_PROC_H
#define FOC_PROC_H

#include "foc/foc.h"

typedef struct {
    int exit_code;
    char *output;  /* stdout capturado (puede ser NULL) */
    char *error;   /* stderr capturado (puede ser NULL) */
    bool failed;
} FOCProcResult;

/* Ejecuta un programa con los argumentos dados. */
FOCProcResult FOC_proc_run(const char *program, char **args, size_t arg_count);
/* Busca un ejecutable en el PATH. */
bool FOC_proc_which(const char *program, char **out_path);

#endif /* FOC_PROC_H */
