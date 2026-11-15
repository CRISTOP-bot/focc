#include "foc/proc.h"

#include "foc/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

FOCProcResult FOC_proc_run(const char *program, char **args, size_t arg_count) {
    FOCProcResult result;
    memset(&result, 0, sizeof(result));

    char **argv = (char **)calloc(arg_count + 2, sizeof(char *));
    if (!argv) {
        result.failed = true;
        return result;
    }
    argv[0] = (char *)program;
    for (size_t i = 0; i < arg_count; i++) {
        argv[i + 1] = args[i];
    }

    FILE *out_pipe = NULL;
    FILE *err_pipe = NULL;
    out_pipe = tmpfile();
    err_pipe = tmpfile();
    if (!out_pipe || !err_pipe) {
        free(argv);
        result.failed = true;
        return result;
    }

#ifdef _WIN32
    /* En Windows se usa _popen para mantener la portabilidad del nucleo. */
    FOCBuf command;
    FOC_buf_init(&command);
    /* El programa no se entrecomilla: cmd.exe no lo admite. */
    FOC_buf_puts(&command, program);
    for (size_t i = 0; i < arg_count; i++) {
        FOC_buf_puts(&command, " \"");
        FOC_buf_puts(&command, args[i]);
        FOC_buf_puts(&command, "\"");
    }
    command.data[command.len] = '\0';
    FILE *pipe = _popen(command.data, "r");
    FOC_buf_free(&command);
    if (!pipe) {
        free(argv);
        result.failed = true;
        return result;
    }
    char buffer[4096];
    size_t total = 0;
    char *output = (char *)malloc(1);
    if (!output) {
        _pclose(pipe);
        free(argv);
        result.failed = true;
        return result;
    }
    output[0] = '\0';
    size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        char *bigger = (char *)realloc(output, total + read + 1);
        if (!bigger) {
            break;
        }
        output = bigger;
        memcpy(output + total, buffer, read);
        total += read;
        output[total] = '\0';
    }
    int status = _pclose(pipe);
    result.exit_code = status;
    result.output = output;
    result.failed = status != 0;
    free(argv);
    fclose(out_pipe);
    fclose(err_pipe);
    return result;
#else
    pid_t pid = fork();
    if (pid < 0) {
        free(argv);
        result.failed = true;
        return result;
    }
    if (pid == 0) {
        dup2(fileno(out_pipe), STDOUT_FILENO);
        dup2(fileno(err_pipe), STDERR_FILENO);
        execvp(program, argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    fclose(out_pipe);
    fclose(err_pipe);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result.failed = result.exit_code != 0;
    free(argv);
    return result;
#endif
}

bool FOC_proc_which(const char *program, char **out_path) {
    if (!program) {
        return false;
    }
    /* Si contiene separadores, se usa tal cual. */
    if (strchr(program, '/') || strchr(program, '\\')) {
        if (FOC_fs_exists(program)) {
            if (out_path) {
                *out_path = FOC_strdup(program);
            }
            return true;
        }
        return false;
    }
    const char *path = getenv("PATH");
    if (!path) {
        return false;
    }
    /* En Windows los ejecutables llevan extension (PATHEXT). */
    static const char *const extensions[] = {"", ".exe", ".bat", ".cmd", NULL};
    char *copy = FOC_strdup(path);
    char *cursor = copy;
    while (cursor && *cursor) {
        char *separator = strchr(cursor, ';');
        if (!separator) {
            separator = strchr(cursor, ':');
        }
        if (separator) {
            *separator = '\0';
        }
        for (size_t e = 0; extensions[e]; e++) {
            char *name = (char *)malloc(strlen(program) + strlen(extensions[e]) + 1);
            if (!name) {
                break;
            }
            sprintf(name, "%s%s", program, extensions[e]);
            char *candidate = FOC_fs_join(cursor, name);
            free(name);
            if (candidate && FOC_fs_is_file(candidate)) {
                if (out_path) {
                    *out_path = candidate;
                } else {
                    free(candidate);
                }
                free(copy);
                return true;
            }
            free(candidate);
        }
        cursor = separator ? separator + 1 : NULL;
    }
    free(copy);
    return false;
}
