#include "foc/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define FOC_MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define FOC_MKDIR(path) mkdir(path, 0755)
#endif

char *FOC_fs_absolute(const char *path) {
    if (!path) {
        return NULL;
    }
    char buffer[4096];
#ifdef _WIN32
    if (_fullpath(buffer, path, sizeof(buffer))) {
        return FOC_strdup(buffer);
    }
#else
    if (realpath(path, buffer)) {
        return FOC_strdup(buffer);
    }
#endif
    return FOC_strdup(path);
}

char *FOC_fs_join(const char *dir, const char *name) {
    if (!dir || !*dir) {
        return FOC_strdup(name ? name : "");
    }
    if (!name || !*name) {
        return FOC_strdup(dir);
    }
    size_t len = strlen(dir);
    bool has_separator = (dir[len - 1] == '/' || dir[len - 1] == '\\');
    size_t total = len + (has_separator ? 0 : 1) + strlen(name) + 1;
    char *out = (char *)malloc(total);
    if (!out) {
        return NULL;
    }
    if (has_separator) {
        snprintf(out, total, "%s%s", dir, name);
    } else {
        snprintf(out, total, "%s/%s", dir, name);
    }
    return out;
}

char *FOC_fs_dirname(const char *path) {
    if (!path) {
        return NULL;
    }
    const char *slash = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            slash = p;
        }
    }
    if (!slash) {
        return FOC_strdup(".");
    }
    size_t len = (size_t)(slash - path);
    if (len == 0) {
        return FOC_strdup("/");
    }
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

char *FOC_fs_basename(const char *path) {
    if (!path) {
        return NULL;
    }
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    const char *dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, base, len);
    out[len] = '\0';
    return out;
}

char *FOC_fs_replace_extension(const char *path, const char *extension) {
    if (!path) {
        return NULL;
    }
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    const char *dot = strrchr(base, '.');
    size_t keep = dot ? (size_t)(dot - path) : strlen(path);
    size_t total = keep + strlen(extension) + 1;
    char *out = (char *)malloc(total);
    if (!out) {
        return NULL;
    }
    memcpy(out, path, keep);
    strcpy(out + keep, extension);
    return out;
}

bool FOC_fs_write_file(const char *path, const char *data, size_t len) {
    char *dir = FOC_fs_dirname(path);
    if (dir) {
        FOC_fs_mkdir(dir);
        free(dir);
    }
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    size_t written = len ? fwrite(data, 1, len, file) : 0;
    fclose(file);
    return written == len;
}

char *FOC_fs_read_file(const char *path, size_t *out_len) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    char *data = (char *)malloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }
    size_t read = fread(data, 1, (size_t)size, file);
    fclose(file);
    data[read] = '\0';
    if (out_len) {
        *out_len = read;
    }
    return data;
}

bool FOC_fs_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool FOC_fs_is_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool FOC_fs_is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool FOC_fs_mkdir(const char *path) {
    if (!path || !*path) {
        return false;
    }
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }
    char *parent = FOC_fs_dirname(path);
    if (parent && strcmp(parent, path) != 0 && strcmp(parent, ".") != 0) {
        FOC_fs_mkdir(parent);
    }
    free(parent);
    return FOC_MKDIR(path) == 0;
}
