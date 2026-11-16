/* Deteccion de plataforma y definicion de targets. */
#ifndef FOC_PLATFORM_H
#define FOC_PLATFORM_H

#include "foc/foc.h"

typedef enum {
    FOC_OS_UNKNOWN = 0,
    FOC_OS_LINUX,
    FOC_OS_WINDOWS,
    FOC_OS_DARWIN,
    FOC_OS_FREEBSD,
    FOC_OS_NETBSD,
    FOC_OS_OPENBSD,
    FOC_OS_DRAGONFLY,
    FOC_OS_SOLARIS
} FOCOS;

typedef enum {
    FOC_ARCH_UNKNOWN = 0,
    FOC_ARCH_X86,
    FOC_ARCH_X86_64,
    FOC_ARCH_ARM,
    FOC_ARCH_AARCH64,
    FOC_ARCH_RISCV32,
    FOC_ARCH_RISCV64,
    FOC_ARCH_WASM32,
    FOC_ARCH_WASM64
} FOCArch;

typedef struct {
    FOCOS os;
    FOCArch arch;
} FOCHost;

typedef struct {
    FOCArch arch;
    FOCOS os;
    char *vendor;
    char *env;
} FOCTarget;

FOCHost FOC_host(void);
FOCTarget FOC_default_target(void);
/* Interpreta "x86_64-pc-linux-gnu". Devuelve false si el triple no existe. */
bool FOC_target_parse(const char *triple, FOCTarget *out);
/* Representacion canonica del triple. */
char *FOC_target_triple(const FOCTarget *target);
int FOC_arch_pointer_size(FOCArch arch);
bool FOC_arch_is_little_endian(FOCArch arch);
const char *FOC_os_name(FOCOS os);
const char *FOC_arch_name(FOCArch arch);

#endif /* FOC_PLATFORM_H */
