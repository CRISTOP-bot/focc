#include "foc/backend.h"

#include <stdlib.h>
#include <string.h>

/* Indices de registro del sistema x86-64. */
enum {
    FOC_REG_RAX = 0,
    FOC_REG_RCX,
    FOC_REG_RDX,
    FOC_REG_RBX,
    FOC_REG_RSP,
    FOC_REG_RBP,
    FOC_REG_RSI,
    FOC_REG_RDI,
    FOC_REG_R8,
    FOC_REG_R9,
    FOC_REG_R10,
    FOC_REG_R11,
    FOC_REG_R12,
    FOC_REG_R13,
    FOC_REG_R14,
    FOC_REG_R15
};

static const FOCABI sysv_amd64 = {
    false,
    true,
    {FOC_REG_RDI, FOC_REG_RSI, FOC_REG_RDX, FOC_REG_RCX, FOC_REG_R8, FOC_REG_R9},
    {0, 1, 2, 3, 4, 5, 6, 7},
    0,
    16
};

static const FOCABI windows_amd64 = {
    true,
    false,
    {FOC_REG_RCX, FOC_REG_RDX, FOC_REG_R8, FOC_REG_R9, -1, -1},
    {0, 1, 2, 3, -1, -1, -1, -1},
    32,
    32
};

const FOCABI *FOC_abi_for(const FOCTarget *target) {
    if (target->os == FOC_OS_WINDOWS) {
        return &windows_amd64;
    }
    return &sysv_amd64;
}

const char *FOC_object_format(const FOCTarget *target) {
    switch (target->os) {
    case FOC_OS_WINDOWS:
        return "coff";
    case FOC_OS_DARWIN:
        return "macho";
    default:
        return "elf";
    }
}
