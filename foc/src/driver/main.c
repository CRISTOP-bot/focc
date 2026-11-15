/* Punto de entrada de FOC. */
#include "foc/diag.h"
#include "foc/driver.h"
#include "foc/options.h"
#include "foc/source.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    FOCOptions options;
    FOC_options_init(&options);

    /* Un diagnostico temporal para recoger los errores de uso. */
    FOCArena arena;
    FOC_arena_init(&arena);
    FOCSourceMgr sources;
    FOC_source_init(&sources, &arena);
    FOCDiagEngine diag;
    FOC_diag_init(&diag, &sources, &arena, stderr);

    if (!FOC_options_parse(&options, argc, argv, &diag)) {
        FOC_options_free(&options);
        FOC_arena_free(&arena);
        return 1;
    }

    if (options.print_help) {
        FOC_options_print_help(stdout);
        FOC_options_free(&options);
        FOC_arena_free(&arena);
        return 0;
    }
    if (options.print_version) {
        FOC_options_print_version(stdout);
        FOC_options_free(&options);
        FOC_arena_free(&arena);
        return 0;
    }

    diag.no_warn = options.no_warnings;
    diag.warnings_as_errors = options.warnings_as_errors;
    diag.json = options.json_diagnostics;
    if (options.color_always) {
        diag.color = FOC_COLOR_ALWAYS;
    } else if (options.color_never) {
        diag.color = FOC_COLOR_NEVER;
    } else {
        diag.color = FOC_COLOR_AUTO;
    }

    int status = FOC_driver_run(&options, &diag, &sources);

    FOC_options_free(&options);
    FOC_arena_free(&arena);
    return status;
}
