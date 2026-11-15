#include "foc/proc.h"

#include "foc/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

static char *read_tmpfile(FILE *stream) {
    if (fseek(stream, 0, SEEK_END) != 0) {
        return NULL;
    }
    long length = ftell(stream);
    if (length < 0 || fseek(stream, 0, SEEK_SET) != 0) {
        return NULL;
    }
    char *data = (char *)malloc((size_t)length + 1);
    if (!data) {
        return NULL;
    }
    size_t read = fread(data, 1, (size_t)length, stream);
    if (read != (size_t)length || ferror(stream)) {
        free(data);
        return NULL;
    }
    data[read] = '\0';
    return data;
}
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
        if (out_pipe) {
            fclose(out_pipe);
        }
        if (err_pipe) {
            fclose(err_pipe);
        }
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
        fclose(out_pipe);
        fclose(err_pipe);
        free(argv);
        result.failed = true;
        return result;
    }
    char buffer[4096];
    size_t total = 0;
    char *output = (char *)malloc(1);
    if (!output) {
        _pclose(pipe);
        fclose(out_pipe);
        fclose(err_pipe);
        free(argv);
        result.failed = true;
        return result;
    }
    output[0] = '\0';
    size_t read;
    bool capture_failed = false;
    while ((read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        char *bigger = (char *)realloc(output, total + read + 1);
        if (!bigger) {
            capture_failed = true;
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
    result.failed = status != 0 || capture_failed;
    free(argv);
    fclose(out_pipe);
    fclose(err_pipe);
    return result;
#else
    pid_t pid = fork();
    if (pid < 0) {
        fclose(out_pipe);
        fclose(err_pipe);
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
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) {
        result.exit_code = -1;
        result.failed = true;
        fclose(out_pipe);
        fclose(err_pipe);
        free(argv);
        return result;
    }
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result.output = read_tmpfile(out_pipe);
    result.error = read_tmpfile(err_pipe);
    fclose(out_pipe);
    fclose(err_pipe);
    result.failed = result.exit_code != 0 || !result.output || !result.error;
    if (!result.output || !result.error) {
        free(result.output);
        free(result.error);
        result.output = NULL;
        result.error = NULL;
    }
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
