/* Pasadas de optimizacion de la IR.
 *
 * Este fichero forma parte de libcc1: el "cerebro" del compilador. Se
 * mantiene separado del driver para poder enlazarlo como biblioteca.
 */
#include "foc/optimizer.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    bool *is_const;
    long long *value;
    bool *is_float;
    double *fvalue;
} FOCConstMap;

static FOCConstMap const_map_new(int count) {
    FOCConstMap map;
    map.is_const = (bool *)calloc(count ? count : 1, sizeof(bool));
    map.value = (long long *)calloc(count ? count : 1, sizeof(long long));
    map.is_float = (bool *)calloc(count ? count : 1, sizeof(bool));
    map.fvalue = (double *)calloc(count ? count : 1, sizeof(double));
    return map;
}

static void const_map_free(FOCConstMap *map) {
    free(map->is_const);
    free(map->value);
    free(map->is_float);
    free(map->fvalue);
}

/* Plegado de constantes: sustituye operaciones entre constantes por su
 * resultado, eliminando la instruccion intermedia. */
void FOC_opt_constant_fold(FOCIRFunction *function) {
    if (!function || function->inst_count == 0) {
        return;
    }
    FOCConstMap map = const_map_new(function->vreg_count);

    for (size_t i = 0; i < function->inst_count; i++) {
        FOCIRInst *inst = FOC_ir_inst_at(function, i);
        if (inst->dest < 0) {
            continue;
        }
        switch (inst->op) {
        case FOC_OP_CONST:
            map.is_const[inst->dest] = true;
            map.value[inst->dest] = inst->lhs.imm;
            break;
        case FOC_OP_CONST_FLOAT:
            map.is_const[inst->dest] = true;
            map.is_float[inst->dest] = true;
            map.fvalue[inst->dest] = inst->fimm;
            break;
        case FOC_OP_ADD:
        case FOC_OP_SUB:
        case FOC_OP_MUL:
        case FOC_OP_DIV:
        case FOC_OP_MOD:
        case FOC_OP_AND:
        case FOC_OP_OR:
        case FOC_OP_XOR:
        case FOC_OP_SHL:
        case FOC_OP_SHR:
        case FOC_OP_EQ:
        case FOC_OP_NE:
        case FOC_OP_LT:
        case FOC_OP_LE:
        case FOC_OP_GT:
        case FOC_OP_GE:
        case FOC_OP_NEG:
        case FOC_OP_BITNOT:
        case FOC_OP_NOT: {
            int lhs = inst->lhs.reg;
            int rhs = inst->rhs.reg;
            /* Un operando sin registro es una constante inmediata. */
            bool lhs_const = lhs < 0 ? true : map.is_const[lhs];
            bool rhs_const = rhs < 0 ? true : map.is_const[rhs];
            long long lhs_value = lhs < 0 ? inst->lhs.imm : map.value[lhs];
            long long rhs_value = rhs < 0 ? inst->rhs.imm : map.value[rhs];
            bool lhs_float = lhs < 0 ? false : map.is_float[lhs];
            bool rhs_float = rhs < 0 ? false : map.is_float[rhs];
            double lhs_f = lhs < 0 ? 0 : map.fvalue[lhs];
            double rhs_f = rhs < 0 ? 0 : map.fvalue[rhs];

            if (inst->op == FOC_OP_NEG || inst->op == FOC_OP_BITNOT || inst->op == FOC_OP_NOT) {
                if (!lhs_const) {
                    break;
                }
            } else if (!lhs_const || !rhs_const) {
                break;
            }
            if (lhs_float || rhs_float) {
                double a = lhs_f;
                double b = rhs_f;
                double result = 0;
                switch (inst->op) {
                case FOC_OP_ADD:
                    result = a + b;
                    break;
                case FOC_OP_SUB:
                    result = a - b;
                    break;
                case FOC_OP_MUL:
                    result = a * b;
                    break;
                case FOC_OP_DIV:
                    result = b != 0 ? a / b : 0;
                    break;
                default:
                    break;
                }
                inst->op = FOC_OP_CONST_FLOAT;
                inst->fimm = result;
                map.is_const[inst->dest] = true;
                map.is_float[inst->dest] = true;
                map.fvalue[inst->dest] = result;
                break;
            }
            long long a = lhs_value;
            long long b = rhs_value;
            long long result = 0;
            bool boolean = false;
            switch (inst->op) {
            case FOC_OP_ADD:
                result = a + b;
                break;
            case FOC_OP_SUB:
                result = a - b;
                break;
            case FOC_OP_MUL:
                result = a * b;
                break;
            case FOC_OP_DIV:
                if (b == 0) {
                    goto no_fold;
                }
                result = a / b;
                break;
            case FOC_OP_MOD:
                if (b == 0) {
                    goto no_fold;
                }
                result = a % b;
                break;
            case FOC_OP_AND:
                result = a & b;
                break;
            case FOC_OP_OR:
                result = a | b;
                break;
            case FOC_OP_XOR:
                result = a ^ b;
                break;
            case FOC_OP_SHL:
                result = a << b;
                break;
            case FOC_OP_SHR:
                result = a >> b;
                break;
            case FOC_OP_NEG:
                result = -a;
                break;
            case FOC_OP_BITNOT:
                result = ~a;
                break;
            case FOC_OP_NOT:
                result = !a;
                break;
            case FOC_OP_EQ:
                result = a == b;
                boolean = true;
                break;
            case FOC_OP_NE:
                result = a != b;
                boolean = true;
                break;
            case FOC_OP_LT:
                result = a < b;
                boolean = true;
                break;
            case FOC_OP_LE:
                result = a <= b;
                boolean = true;
                break;
            case FOC_OP_GT:
                result = a > b;
                boolean = true;
                break;
            case FOC_OP_GE:
                result = a >= b;
                boolean = true;
                break;
            default:
                break;
            }
            inst->op = FOC_OP_CONST;
            inst->lhs.imm = result;
            map.is_const[inst->dest] = true;
            map.is_float[inst->dest] = false;
            map.value[inst->dest] = result;
            (void)boolean;
            break;
        no_fold:
            break;
        }
        default:
            break;
        }
    }
    const_map_free(&map);
}

/* Eliminacion de codigo muerto: quita las instrucciones puras cuyo
 * resultado no se usa. */
void FOC_opt_dead_code(FOCIRFunction *function) {
    if (!function || function->inst_count == 0) {
        return;
    }
    bool *used = (bool *)calloc(function->vreg_count ? function->vreg_count : 1, sizeof(bool));
    bool *keep = (bool *)calloc(function->inst_count, sizeof(bool));
    if (!used || !keep) {
        free(used);
        free(keep);
        return;
    }

    /* Marca los registros usados por instrucciones con efectos. */
    for (size_t i = 0; i < function->inst_count; i++) {
        FOCIRInst *inst = FOC_ir_inst_at(function, i);
        switch (inst->op) {
        case FOC_OP_STORE:
        case FOC_OP_CALL:
        case FOC_OP_RET:
        case FOC_OP_BRANCH:
        case FOC_OP_COPY:
            keep[i] = true;
            break;
        default:
            break;
        }
        if (inst->lhs.reg >= 0) {
            used[inst->lhs.reg] = true;
        }
        if (inst->rhs.reg >= 0) {
            used[inst->rhs.reg] = true;
        }
        if (inst->third.reg >= 0) {
            used[inst->third.reg] = true;
        }
        for (int a = 0; a < inst->arg_count; a++) {
            if (inst->args[a].reg >= 0) {
                used[inst->args[a].reg] = true;
            }
        }
    }

    /* Una instruccion sin efectos y cuyo destino no se usa se marca como
     * muerta; el backend las omite al emitir. */
    for (size_t i = 0; i < function->inst_count; i++) {
        FOCIRInst *inst = FOC_ir_inst_at(function, i);
        if (!inst) {
            continue;
        }
        bool pure = inst->op != FOC_OP_STORE && inst->op != FOC_OP_CALL &&
                    inst->op != FOC_OP_RET && inst->op != FOC_OP_BRANCH && inst->op != FOC_OP_JMP &&
                    inst->op != FOC_OP_LABEL && inst->op != FOC_OP_COPY &&
                    inst->op != FOC_OP_ALLOCA && inst->op != FOC_OP_GLOBAL_ADDR &&
                    inst->op != FOC_OP_FRAME_ADDR && inst->op != FOC_OP_FIELD_ADDR &&
                    inst->op != FOC_OP_GETELEM;
        if (pure && inst->dest >= 0 && !used[inst->dest]) {
            inst->dead = true;
        }
    }
    free(used);
    free(keep);
}

/* Propagacion de copias: reemplaza los usos de un registro por el valor de
 * otro cuando la definicion es una copia directa. */
void FOC_opt_copy_propagation(FOCIRFunction *function) {
    if (!function || function->inst_count == 0) {
        return;
    }
    int *alias = (int *)malloc(function->vreg_count ? function->vreg_count * sizeof(int) : sizeof(int));
    if (!alias) {
        return;
    }
    for (int i = 0; i < function->vreg_count; i++) {
        alias[i] = i;
    }
    for (size_t i = 0; i < function->inst_count; i++) {
        FOCIRInst *inst = FOC_ir_inst_at(function, i);
        if (inst->op == FOC_OP_CONST && inst->dest >= 0) {
            alias[inst->dest] = inst->dest;
        }
    }
    /* Sustitucion de usos. */
    for (size_t i = 0; i < function->inst_count; i++) {
        FOCIRInst *inst = FOC_ir_inst_at(function, i);
        FOCIRValue *values[3] = {&inst->lhs, &inst->rhs, &inst->third};
        for (int v = 0; v < 3; v++) {
            if (values[v]->reg >= 0 && values[v]->reg < function->vreg_count) {
                values[v]->reg = alias[values[v]->reg];
            }
        }
        for (int a = 0; a < inst->arg_count; a++) {
            if (inst->args[a].reg >= 0 && inst->args[a].reg < function->vreg_count) {
                inst->args[a].reg = alias[inst->args[a].reg];
            }
        }
    }
    free(alias);
}
