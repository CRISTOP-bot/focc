/* Preprocesador de C. */
#ifndef FOC_PREPROC_H
#define FOC_PREPROC_H

#include "foc/lexer.h"
#include "foc/platform.h"

typedef struct {
    FOCVec include_paths; /* char* */
    FOCVec defines;       /* char*: "NOMBRE" o "NOMBRE=valor" */
    FOCVec undefines;     /* char* */
    FOCVec sys_include_paths;
    const char *std_name;
    FOCTarget target;
} FOCPreprocOptions;

void FOC_preproc_options_init(FOCPreprocOptions *options);
void FOC_preproc_options_free(FOCPreprocOptions *options);

/* Preprocesa `file` y escribe los tokens resultantes en `out`. */
bool FOC_preprocess(const FOCSourceFile *file, FOCSourceMgr *sources, FOCDiagEngine *diag,
                    const FOCPreprocOptions *options, FOCTokenList *out);

#endif /* FOC_PREPROC_H */
