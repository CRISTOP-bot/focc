#include "foc/platform.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define HOST_OS FOC_OS_WINDOWS
#define HOST_ARCH FOC_ARCH_X86_64
#elif defined(__linux__)
#define HOST_OS FOC_OS_LINUX
#if defined(__x86_64__)
#define HOST_ARCH FOC_ARCH_X86_64
#elif defined(__i386__)
#define HOST_ARCH FOC_ARCH_X86
#elif defined(__aarch64__)
#define HOST_ARCH FOC_ARCH_AARCH64
#elif defined(__arm__)
#define HOST_ARCH FOC_ARCH_ARM
#elif defined(__riscv)
#define HOST_ARCH FOC_ARCH_RISCV64
#endif
#elif defined(__APPLE__)
#define HOST_OS FOC_OS_DARWIN
#define HOST_ARCH FOC_ARCH_X86_64
#elif defined(__FreeBSD__)
#define HOST_OS FOC_OS_FREEBSD
#define HOST_ARCH FOC_ARCH_X86_64
#elif defined(__NetBSD__)
#define HOST_OS FOC_OS_NETBSD
#define HOST_ARCH FOC_ARCH_X86_64
#elif defined(__OpenBSD__)
#define HOST_OS FOC_OS_OPENBSD
#define HOST_ARCH FOC_ARCH_X86_64
#else
#define HOST_OS FOC_OS_UNKNOWN
#define HOST_ARCH FOC_ARCH_UNKNOWN
#endif

FOCHost FOC_host(void) {
    FOCHost host;
    host.os = HOST_OS;
    host.arch = HOST_ARCH;
    return host;
}

FOCTarget FOC_default_target(void) {
    FOCHost host = FOC_host();
    FOCTarget target;
    target.arch = host.arch;
    target.os = host.os;
    target.vendor = FOC_strdup("pc");
    switch (host.os) {
    case FOC_OS_WINDOWS:
        target.env = FOC_strdup("msvc");
        break;
    case FOC_OS_DARWIN:
        target.vendor = FOC_strdup("apple");
        target.env = FOC_strdup("darwin");
        break;
    case FOC_OS_LINUX:
        target.env = FOC_strdup("gnu");
        break;
    default:
        target.env = FOC_strdup("unknown");
        break;
    }
    return target;
}

static bool parse_arch(const char *s, FOCArch *out) {
    if (!strcmp(s, "i386") || !strcmp(s, "i486") || !strcmp(s, "i586") || !strcmp(s, "i686") ||
        !strcmp(s, "x86")) {
        *out = FOC_ARCH_X86;
    } else if (!strcmp(s, "x86_64") || !strcmp(s, "amd64") || !strcmp(s, "x64")) {
        *out = FOC_ARCH_X86_64;
    } else if (!strcmp(s, "arm") || !strcmp(s, "armv7") || !strcmp(s, "armv7l")) {
        *out = FOC_ARCH_ARM;
    } else if (!strcmp(s, "aarch64") || !strcmp(s, "arm64")) {
        *out = FOC_ARCH_AARCH64;
    } else if (!strcmp(s, "riscv32")) {
        *out = FOC_ARCH_RISCV32;
    } else if (!strcmp(s, "riscv64")) {
        *out = FOC_ARCH_RISCV64;
    } else if (!strcmp(s, "wasm32") || !strcmp(s, "wasm")) {
        *out = FOC_ARCH_WASM32;
    } else if (!strcmp(s, "wasm64")) {
        *out = FOC_ARCH_WASM64;
    } else {
        return false;
    }
    return true;
}

static bool parse_os(const char *s, FOCOS *out) {
    if (!strcmp(s, "linux")) {
        *out = FOC_OS_LINUX;
    } else if (!strcmp(s, "windows") || !strcmp(s, "win32") || !strcmp(s, "mingw32") || !strcmp(s, "msvc")) {
        *out = FOC_OS_WINDOWS;
    } else if (!strcmp(s, "darwin") || !strcmp(s, "macos") || !strcmp(s, "osx")) {
        *out = FOC_OS_DARWIN;
    } else if (!strcmp(s, "freebsd")) {
        *out = FOC_OS_FREEBSD;
    } else if (!strcmp(s, "netbsd")) {
        *out = FOC_OS_NETBSD;
    } else if (!strcmp(s, "openbsd")) {
        *out = FOC_OS_OPENBSD;
    } else if (!strcmp(s, "dragonfly")) {
        *out = FOC_OS_DRAGONFLY;
    } else if (!strcmp(s, "solaris") || !strcmp(s, "sunos")) {
        *out = FOC_OS_SOLARIS;
    } else {
        return false;
    }
    return true;
}

bool FOC_target_parse(const char *triple, FOCTarget *out) {
    if (!triple || !*triple) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    char *copy = FOC_strdup(triple);
    char *parts[8];
    size_t count = 0;
    char *cursor = copy;
    while (count < 8) {
        char *dash = strchr(cursor, '-');
        if (dash) {
            *dash = '\0';
        }
        parts[count++] = cursor;
        if (!dash) {
            break;
        }
        cursor = dash + 1;
    }
    if (count == 0 || !parse_arch(parts[0], &out->arch)) {
        free(copy);
        return false;
    }
    size_t os_index = (size_t)-1;
    for (size_t i = 1; i < count; i++) {
        FOCOS candidate;
        if (parse_os(parts[i], &candidate)) {
            out->os = candidate;
            os_index = i;
            break;
        }
    }
    if (os_index == (size_t)-1) {
        out->os = FOC_OS_UNKNOWN;
        out->vendor = FOC_strdup(count > 1 ? parts[1] : "pc");
        out->env = FOC_strdup("unknown");
        free(copy);
        return true;
    }
    if (os_index > 1) {
        size_t vendor_len = 0;
        for (size_t i = 1; i < os_index; i++) {
            vendor_len += strlen(parts[i]) + 1;
        }
        char *vendor = (char *)malloc(vendor_len + 1);
        vendor[0] = '\0';
        for (size_t i = 1; i < os_index; i++) {
            if (i > 1) {
                strcat(vendor, "-");
            }
            strcat(vendor, parts[i]);
        }
        out->vendor = vendor;
    } else {
        out->vendor = FOC_strdup("pc");
    }
    if (os_index + 1 < count) {
        size_t env_len = 0;
        for (size_t i = os_index + 1; i < count; i++) {
            env_len += strlen(parts[i]) + 1;
        }
        char *env = (char *)malloc(env_len + 1);
        env[0] = '\0';
        for (size_t i = os_index + 1; i < count; i++) {
            if (i > os_index + 1) {
                strcat(env, "-");
            }
            strcat(env, parts[i]);
        }
        out->env = env;
    } else {
        out->env = FOC_strdup("unknown");
    }
    free(copy);
    return true;
}

char *FOC_target_triple(const FOCTarget *target) {
    FOCBuf buf;
    FOC_buf_init(&buf);
    FOC_buf_puts(&buf, FOC_arch_name(target->arch));
    if (target->vendor && *target->vendor) {
        FOC_buf_putc(&buf, '-');
        FOC_buf_puts(&buf, target->vendor);
    }
    if (target->os != FOC_OS_UNKNOWN) {
        FOC_buf_putc(&buf, '-');
        FOC_buf_puts(&buf, FOC_os_name(target->os));
    }
    if (target->env && *target->env) {
        FOC_buf_putc(&buf, '-');
        FOC_buf_puts(&buf, target->env);
    }
    return FOC_buf_take(&buf);
}

int FOC_arch_pointer_size(FOCArch arch) {
    switch (arch) {
    case FOC_ARCH_X86:
    case FOC_ARCH_ARM:
    case FOC_ARCH_RISCV32:
    case FOC_ARCH_WASM32:
        return 4;
    case FOC_ARCH_X86_64:
    case FOC_ARCH_AARCH64:
    case FOC_ARCH_RISCV64:
    case FOC_ARCH_WASM64:
        return 8;
    default:
        return 0;
    }
}

bool FOC_arch_is_little_endian(FOCArch arch) {
    switch (arch) {
    case FOC_ARCH_X86:
    case FOC_ARCH_X86_64:
    case FOC_ARCH_ARM:
    case FOC_ARCH_AARCH64:
    case FOC_ARCH_RISCV32:
    case FOC_ARCH_RISCV64:
    case FOC_ARCH_WASM32:
    case FOC_ARCH_WASM64:
        return true;
    default:
        return false;
    }
}

const char *FOC_os_name(FOCOS os) {
    switch (os) {
    case FOC_OS_LINUX:
        return "linux";
    case FOC_OS_WINDOWS:
        return "windows";
    case FOC_OS_DARWIN:
        return "darwin";
    case FOC_OS_FREEBSD:
        return "freebsd";
    case FOC_OS_NETBSD:
        return "netbsd";
    case FOC_OS_OPENBSD:
        return "openbsd";
    case FOC_OS_DRAGONFLY:
        return "dragonfly";
    case FOC_OS_SOLARIS:
        return "solaris";
    default:
        return "unknown";
    }
}

const char *FOC_arch_name(FOCArch arch) {
    switch (arch) {
    case FOC_ARCH_X86:
        return "i386";
    case FOC_ARCH_X86_64:
        return "x86_64";
    case FOC_ARCH_ARM:
        return "arm";
    case FOC_ARCH_AARCH64:
        return "aarch64";
    case FOC_ARCH_RISCV32:
        return "riscv32";
    case FOC_ARCH_RISCV64:
        return "riscv64";
    case FOC_ARCH_WASM32:
        return "wasm32";
    case FOC_ARCH_WASM64:
        return "wasm64";
    default:
        return "unknown";
    }
}
