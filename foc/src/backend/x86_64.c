/* Backend x86-64: genera ensamblador en sintaxis AT&T.
 *
 * El backend consume la IR intermedia, que es independiente de la
 * arquitectura. Cada registro virtual de la IR se materializa en una ranura
 * del marco de pila; las instrucciones se traducen a secuencias MOV.
 */
#include "foc/backend.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    FOCBuf text;
    const FOCTarget *target;
    const FOCABI *abi;
    bool elf; /* usa las directivas .type/.size propias de ELF */
    FOCIRFunction *function;
    int *vreg_offset; /* desplazamiento de cada registro virtual */
    int frame_base;   /* parte baja reservada para las variables locales */
    int *alloca_offset;
    int alloca_count;
} FOCEmitter;

static const char *const int_regs[16] = {"%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp",
                                         "%rsi", "%rdi", "%r8",  "%r9",  "%r10", "%r11",
                                         "%r12", "%r13", "%r14", "%r15"};
static const char *const int_regs32[16] = {"%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp",
                                            "%esi", "%edi", "%r8d", "%r9d", "%r10d", "%r11d",
                                            "%r12d", "%r13d", "%r14d", "%r15d"};

static const char *const sse_regs[16] = {"%xmm0", "%xmm1", "%xmm2",  "%xmm3",  "%xmm4",
                                         "%xmm5", "%xmm6", "%xmm7",  "%xmm8",  "%xmm9",
                                         "%xmm10", "%xmm11", "%xmm12", "%xmm13", "%xmm14",
                                         "%xmm15"};

/* Sufijo de instruccion segun el tamano en bytes. */
static const char *suffix_for(int size) {
    switch (size) {
    case 1:
        return "b";
    case 2:
        return "w";
    case 4:
        return "l";
    default:
        return "q";
    }
}

static bool is_float_type(FOCIRType type) {
    return type == FOC_IR_F32 || type == FOC_IR_F64;
}

static int type_size(FOCIRType type) { return FOC_ir_type_size(type); }

/* Emite la carga de un valor (registro virtual o constante) en %rax o %xmm0. */
static void emit_load_value(FOCEmitter *em, const FOCIRValue *value, FOCIRType type) {
    int size = type_size(type);
    if (value->reg < 0) {
        if (is_float_type(type)) {
            /* Constante flotante: se materializa desde .rodata. */
            FOC_buf_printf(&em->text, "  movq $0, %%rax\n");
        } else {
            FOC_buf_printf(&em->text, "  movq $%lld, %%rax\n", value->imm);
        }
        return;
    }
    int offset = em->vreg_offset[value->reg];
    if (is_float_type(type)) {
        if (size == 4) {
            FOC_buf_printf(&em->text, "  movss %d(%%rbp), %%xmm0\n", offset);
        } else {
            FOC_buf_printf(&em->text, "  movsd %d(%%rbp), %%xmm0\n", offset);
        }
        return;
    }
    if (size == 1) {
        FOC_buf_printf(&em->text, "  movzbl %d(%%rbp), %%eax\n", offset);
    } else if (size == 2) {
        FOC_buf_printf(&em->text, "  movzwl %d(%%rbp), %%eax\n", offset);
    } else if (size == 4) {
        FOC_buf_printf(&em->text, "  movl %d(%%rbp), %%eax\n", offset);
    } else {
        FOC_buf_printf(&em->text, "  movq %d(%%rbp), %%rax\n", offset);
    }
}

/* Almacena %rax o %xmm0 en el registro virtual destino. */
static void emit_store_dest(FOCEmitter *em, int dest, FOCIRType type) {
    if (dest < 0) {
        return;
    }
    int offset = em->vreg_offset[dest];
    if (is_float_type(type)) {
        if (type_size(type) == 4) {
            FOC_buf_printf(&em->text, "  movss %%xmm0, %d(%%rbp)\n", offset);
        } else {
            FOC_buf_printf(&em->text, "  movsd %%xmm0, %d(%%rbp)\n", offset);
        }
        return;
    }
    FOC_buf_printf(&em->text, "  movq %%rax, %d(%%rbp)\n", offset);
}

/* Emite el valor de un operando en un registro concreto. */
static void emit_load_into(FOCEmitter *em, const FOCIRValue *value, FOCIRType type,
                           const char *reg64, const char *reg32) {
    int size = type_size(type);
    if (value->reg < 0) {
        FOC_buf_printf(&em->text, "  movq $%lld, %s\n", value->imm, reg64);
        return;
    }
    int offset = em->vreg_offset[value->reg];
    if (size == 1) {
        FOC_buf_printf(&em->text, "  movzbl %d(%%rbp), %s\n", offset, reg32);
    } else if (size == 2) {
        FOC_buf_printf(&em->text, "  movzwl %d(%%rbp), %s\n", offset, reg32);
    } else if (size == 4) {
        FOC_buf_printf(&em->text, "  movl %d(%%rbp), %s\n", offset, reg32);
    } else {
        FOC_buf_printf(&em->text, "  movq %d(%%rbp), %s\n", offset, reg64);
    }
}

static void emit_function(FOCEmitter *em, FOCIRFunction *function, size_t function_index) {
    em->function = function;

    /* Asigna offsets a los registros virtuales y a los huecos temporales. */
    int base = function->frame_size;
    if (base < 0) {
        base = 0;
    }
    em->frame_base = base;
    em->vreg_offset = (int *)calloc(function->vreg_count ? function->vreg_count : 1, sizeof(int));
    int offset = -base;
    for (int i = 0; i < function->vreg_count; i++) {
        offset -= 8;
        em->vreg_offset[i] = offset;
    }
    /* Los huecos temporales (alloca) van debajo de los registros. */
    int alloca_offset = offset;
    em->alloca_count = 0;
    for (size_t i = 0; i < function->inst_count; i++) {
        if (FOC_ir_inst_at(function, i)->op == FOC_OP_ALLOCA) {
            alloca_offset -= (int)FOC_ir_inst_at(function, i)->imm;
            em->alloca_count++;
        }
    }
    em->alloca_offset = (int *)calloc(em->alloca_count ? em->alloca_count : 1, sizeof(int));
    int current = alloca_offset;
    int index = 0;
    for (size_t i = 0; i < function->inst_count; i++) {
        if (FOC_ir_inst_at(function, i)->op == FOC_OP_ALLOCA) {
            current += (int)FOC_ir_inst_at(function, i)->imm;
            em->alloca_offset[index++] = current;
        }
    }

    int frame = -current;
    frame = (frame + 15) & ~15;

    FOC_buf_printf(&em->text, "  .text\n");
    if (!function->name) {
        return;
    }
    FOC_buf_printf(&em->text, "  .globl %s\n", function->name);
    if (em->elf) {
        FOC_buf_printf(&em->text, "  .type %s, @function\n", function->name);
    }
    FOC_buf_printf(&em->text, "%s:\n", function->name);
    FOC_buf_printf(&em->text, "  pushq %%rbp\n");
    FOC_buf_printf(&em->text, "  movq %%rsp, %%rbp\n");
    if (frame > 0) {
        FOC_buf_printf(&em->text, "  subq $%d, %%rsp\n", frame);
    }

    /* Los parametros llegan en registros: se copian a su hueco en el marco
     * para que el cuerpo los use de forma uniforme. */
    {
        int int_index = 0;
        int sse_index = 0;
        for (size_t i = 0; i < function->param_count; i++) {
            FOCIRType param_type = function->params[i].type;
            int slot = 16 + (int)(i * 8);
            bool is_float = is_float_type(param_type);
            if (is_float) {
                if (sse_index >= 8) {
                    continue;
                }
                FOC_buf_printf(&em->text, "  movsd %s, %d(%%rbp)\n", sse_regs[sse_index], slot);
                sse_index++;
                continue;
            }
            if (int_index >= 6) {
                continue;
            }
            int reg = em->abi->int_args[int_index];
            FOC_buf_printf(&em->text, "  movq %s, %d(%%rbp)\n", int_regs[reg], slot);
            int_index++;
        }
    }

    int alloca_index = 0;
    for (size_t i = 0; i < function->inst_count; i++) {
        FOCIRInst *inst = FOC_ir_inst_at(function, i);
        if (!inst || inst->dead) {
            continue;
        }
        switch (inst->op) {
        case FOC_OP_CONST:
            if (inst->dest >= 0) {
                int dest_offset = em->vreg_offset[inst->dest];
                FOC_buf_printf(&em->text, "  movq $%lld, %d(%%rbp)\n", inst->lhs.imm, dest_offset);
            }
            break;
        case FOC_OP_CONST_FLOAT: {
            /* La constante se emite en .rodata y se carga desde ahi. */
            FOC_buf_printf(&em->text, "  movsd .LFC%zu_%zu(%%rip), %%xmm0\n", function_index, i);
            emit_store_dest(em, inst->dest, FOC_IR_F64);
            break;
        }
        case FOC_OP_LOAD: {
            int size = type_size(inst->type);
            emit_load_value(em, &inst->lhs, FOC_IR_PTR);
            if (size == 1) {
                FOC_buf_printf(&em->text, "  movzbl (%%rax), %%eax\n");
            } else if (size == 2) {
                FOC_buf_printf(&em->text, "  movzwl (%%rax), %%eax\n");
            } else if (size == 4) {
                FOC_buf_printf(&em->text, "  movl (%%rax), %%eax\n");
            } else {
                FOC_buf_printf(&em->text, "  movq (%%rax), %%rax\n");
            }
            emit_store_dest(em, inst->dest, inst->type);
            break;
        }
        case FOC_OP_STORE: {
            int size = type_size(inst->type);
            emit_load_value(em, &inst->lhs, inst->type);
            emit_load_into(em, &inst->rhs, FOC_IR_PTR, "%rcx", "%ecx");
            FOC_buf_printf(&em->text, "  mov%s %s, (%%rcx)\n", suffix_for(size),
                           size == 8 ? "%rax" : "%eax");
            break;
        }
        case FOC_OP_ADD:
        case FOC_OP_SUB:
        case FOC_OP_MUL:
        case FOC_OP_DIV:
        case FOC_OP_MOD:
        case FOC_OP_AND:
        case FOC_OP_OR:
        case FOC_OP_XOR:
        case FOC_OP_SHL:
        case FOC_OP_SHR: {
            const char *op = "add";
            if (inst->op == FOC_OP_SUB) {
                op = "sub";
            } else if (inst->op == FOC_OP_MUL) {
                op = "imul";
            } else if (inst->op == FOC_OP_DIV) {
                op = "idiv";
            } else if (inst->op == FOC_OP_MOD) {
                op = "idiv";
            } else if (inst->op == FOC_OP_AND) {
                op = "and";
            } else if (inst->op == FOC_OP_OR) {
                op = "or";
            } else if (inst->op == FOC_OP_XOR) {
                op = "xor";
            } else if (inst->op == FOC_OP_SHL) {
                op = "shl";
            } else if (inst->op == FOC_OP_SHR) {
                op = "sar";
            }
            if (is_float_type(inst->type)) {
                const char *fop = "addsd";
                if (inst->op == FOC_OP_SUB) {
                    fop = "subsd";
                } else if (inst->op == FOC_OP_MUL) {
                    fop = "mulsd";
                } else if (inst->op == FOC_OP_DIV) {
                    fop = "divsd";
                }
                emit_load_value(em, &inst->lhs, inst->type);
                FOC_buf_printf(&em->text, "  movsd %%xmm0, %%xmm1\n");
                emit_load_value(em, &inst->rhs, inst->type);
                FOC_buf_printf(&em->text, "  %s %%xmm1, %%xmm0\n", fop);
                emit_store_dest(em, inst->dest, inst->type);
                break;
            }
            int size = type_size(inst->type);
            const char *sfx = suffix_for(size);
            if (inst->op == FOC_OP_DIV || inst->op == FOC_OP_MOD) {
                /* idiv usa rdx:rax; se calcula con signo. */
                emit_load_value(em, &inst->rhs, inst->type);
                FOC_buf_printf(&em->text, "  movq %%rax, %%rdi\n");
                emit_load_value(em, &inst->lhs, inst->type);
                if (size == 8) {
                    FOC_buf_printf(&em->text, "  cqto\n");
                    FOC_buf_printf(&em->text, "  idivq %%rdi\n");
                } else {
                    FOC_buf_printf(&em->text, "  cltd\n");
                    FOC_buf_printf(&em->text, "  idivl %%edi\n");
                }
                if (inst->op == FOC_OP_MOD) {
                    FOC_buf_printf(&em->text, "  movq %%rdx, %%rax\n");
                }
                emit_store_dest(em, inst->dest, inst->type);
                break;
            }
            if (inst->op == FOC_OP_SHL || inst->op == FOC_OP_SHR) {
                emit_load_value(em, &inst->rhs, inst->type);
                FOC_buf_printf(&em->text, "  movq %%rax, %%rdi\n");
                emit_load_value(em, &inst->lhs, inst->type);
                FOC_buf_printf(&em->text, "  %s%s %%cl, %s\n", op, sfx, size == 8 ? "%rax" : "%eax");
                emit_store_dest(em, inst->dest, inst->type);
                break;
            }
            if (inst->op == FOC_OP_MUL) {
                emit_load_value(em, &inst->rhs, inst->type);
                FOC_buf_printf(&em->text, "  movq %%rax, %%rdi\n");
                emit_load_value(em, &inst->lhs, inst->type);
                if (size == 8) {
                    FOC_buf_printf(&em->text, "  imulq %%rdi, %%rax\n");
                } else {
                    FOC_buf_printf(&em->text, "  imull %%edi, %%eax\n");
                }
                emit_store_dest(em, inst->dest, inst->type);
                break;
            }
            emit_load_value(em, &inst->lhs, inst->type);
            emit_load_into(em, &inst->rhs, inst->type, "%rdi", "%edi");
            FOC_buf_printf(&em->text, "  %s%s %s, %s\n", op, sfx, size == 8 ? "%rdi" : "%edi",
                           size == 8 ? "%rax" : "%eax");
            emit_store_dest(em, inst->dest, inst->type);
            break;
        }
        case FOC_OP_NEG:
            emit_load_value(em, &inst->lhs, inst->type);
            if (is_float_type(inst->type)) {
                FOC_buf_printf(&em->text, "  movq $0, %%rax\n");
                FOC_buf_printf(&em->text, "  movq %%rax, %%xmm1\n");
                FOC_buf_printf(&em->text, "  subsd %%xmm0, %%xmm1\n");
                FOC_buf_printf(&em->text, "  movsd %%xmm1, %%xmm0\n");
            } else {
                FOC_buf_printf(&em->text, "  negq %%rax\n");
            }
            emit_store_dest(em, inst->dest, inst->type);
            break;
        case FOC_OP_BITNOT:
            emit_load_value(em, &inst->lhs, inst->type);
            FOC_buf_printf(&em->text, "  notq %%rax\n");
            emit_store_dest(em, inst->dest, inst->type);
            break;
        case FOC_OP_NOT:
            emit_load_value(em, &inst->lhs, inst->type);
            FOC_buf_printf(&em->text, "  cmpq $0, %%rax\n");
            FOC_buf_printf(&em->text, "  sete %%al\n");
            FOC_buf_printf(&em->text, "  movzbl %%al, %%eax\n");
            emit_store_dest(em, inst->dest, FOC_IR_I32);
            break;
        case FOC_OP_EQ:
        case FOC_OP_NE:
        case FOC_OP_LT:
        case FOC_OP_LE:
        case FOC_OP_GT:
        case FOC_OP_GE: {
            const char *cc = "sete";
            if (inst->op == FOC_OP_NE) {
                cc = "setne";
            } else if (inst->op == FOC_OP_LT) {
                cc = "setl";
            } else if (inst->op == FOC_OP_LE) {
                cc = "setle";
            } else if (inst->op == FOC_OP_GT) {
                cc = "setg";
            } else if (inst->op == FOC_OP_GE) {
                cc = "setge";
            }
            if (is_float_type(inst->type)) {
                emit_load_value(em, &inst->lhs, inst->type);
                FOC_buf_printf(&em->text, "  movsd %%xmm0, %%xmm1\n");
                emit_load_value(em, &inst->rhs, inst->type);
                const char *fcc = "e";
                if (inst->op == FOC_OP_NE) {
                    fcc = "ne";
                } else if (inst->op == FOC_OP_LT) {
                    fcc = "b";
                } else if (inst->op == FOC_OP_LE) {
                    fcc = "be";
                } else if (inst->op == FOC_OP_GT) {
                    fcc = "a";
                } else if (inst->op == FOC_OP_GE) {
                    fcc = "ae";
                }
                FOC_buf_printf(&em->text, "  ucomisd %%xmm1, %%xmm0\n");
                FOC_buf_printf(&em->text, "  %s %%al\n", fcc);
            } else {
                emit_load_value(em, &inst->lhs, inst->type);
                FOC_buf_printf(&em->text, "  movq %%rax, %%rdi\n");
                emit_load_value(em, &inst->rhs, inst->type);
                FOC_buf_printf(&em->text, "  cmpq %%rax, %%rdi\n");
                FOC_buf_printf(&em->text, "  %s %%al\n", cc);
            }
            FOC_buf_printf(&em->text, "  movzbl %%al, %%eax\n");
            emit_store_dest(em, inst->dest, FOC_IR_I32);
            break;
        }
        case FOC_OP_CAST: {
            FOCIRType from = inst->lhs.type;
            FOCIRType to = inst->type;
            emit_load_value(em, &inst->lhs, from);
            if (is_float_type(from) && !is_float_type(to)) {
                FOC_buf_printf(&em->text, "  cvttsd2si %%xmm0, %%rax\n");
            } else if (!is_float_type(from) && is_float_type(to)) {
                FOC_buf_printf(&em->text, "  cvtsi2sd %%rax, %%xmm0\n");
            } else if (is_float_type(from) && is_float_type(to)) {
                /* f32 <-> f64 */
                if (type_size(from) == 4 && type_size(to) == 8) {
                    FOC_buf_printf(&em->text, "  cvtss2sd %%xmm0, %%xmm0\n");
                } else {
                    FOC_buf_printf(&em->text, "  cvtsd2ss %%xmm0, %%xmm0\n");
                }
            } else {
                int from_size = type_size(from);
                int to_size = type_size(to);
                if (from_size < to_size) {
                    if (from_size == 1) {
                        FOC_buf_printf(&em->text, "  movsbl %%al, %%eax\n");
                    } else if (from_size == 2) {
                        FOC_buf_printf(&em->text, "  movswl %%ax, %%eax\n");
                    } else {
                        FOC_buf_printf(&em->text, "  movl %%eax, %%eax\n");
                    }
                } else if (from_size > to_size) {
                    /* El truncamiento es implicito al almacenar. */
                }
            }
            emit_store_dest(em, inst->dest, to);
            break;
        }
        case FOC_OP_CALL: {
            /* Se separa el espacio de sombra y los argumentos de pila. */
            int int_count = 0;
            int sse_count = 0;
            FOCIRValue stack_args[8];
            int stack_count = 0;
            int register_args[8];
            bool register_is_float[8];
            int register_count = 0;

            for (int i = 0; i < inst->arg_count; i++) {
                FOCIRValue arg = inst->args[i];
                bool is_float = is_float_type(arg.type);
                bool in_register = false;
                if (is_float) {
                    if (sse_count < 8) {
                        in_register = true;
                    }
                } else {
                    if (int_count < 6) {
                        in_register = true;
                    }
                }
                if (in_register) {
                    register_args[register_count] = i;
                    register_is_float[register_count] = is_float;
                    register_count++;
                    if (is_float) {
                        sse_count++;
                    } else {
                        int_count++;
                    }
                    continue;
                }
                if (stack_count < 8) {
                    stack_args[stack_count++] = arg;
                }
            }

            int shadow = (em->abi->windows && inst->arg_count > 0) ? 32 : 0;
            int stack_bytes = stack_count * 8;
            int total = (shadow + stack_bytes + 15) & ~15;
            if (total > 0) {
                FOC_buf_printf(&em->text, "  subq $%d, %%rsp\n", total);
            }
            /* Los argumentos de pila se colocan encima del espacio de sombra. */
            for (int i = 0; i < stack_count; i++) {
                emit_load_into(em, &stack_args[i], stack_args[i].type, "%rax", "%eax");
                FOC_buf_printf(&em->text, "  movq %%rax, %d(%%rsp)\n", shadow + i * 8);
            }
            /* Los argumentos de registro se cargan en sus registros. */
            int int_index = 0;
            int sse_index = 0;
            for (int i = 0; i < register_count; i++) {
                FOCIRValue arg = inst->args[register_args[i]];
                if (register_is_float[i]) {
                    emit_load_into(em, &arg, arg.type, "%rax", "%eax");
                    FOC_buf_printf(&em->text, "  movq %%rax, %s\n", sse_regs[sse_index]);
                    sse_index++;
                    continue;
                }
                int reg = em->abi->int_args[int_index];
                emit_load_into(em, &arg, arg.type, int_regs[reg], int_regs32[reg]);
                int_index++;
            }
            if (inst->variadic) {
                FOC_buf_printf(&em->text, "  movl $%d, %%eax\n", sse_count);
            }
            if (inst->indirect) {
                emit_load_value(em, &inst->lhs, FOC_IR_PTR);
                FOC_buf_printf(&em->text, "  call *%%rax\n");
            } else {
                FOC_buf_printf(&em->text, "  call %s\n", inst->symbol ? inst->symbol : "abort");
            }
            if (total > 0) {
                FOC_buf_printf(&em->text, "  addq $%d, %%rsp\n", total);
            }
            if (inst->dest >= 0) {
                if (is_float_type(inst->type)) {
                    FOC_buf_printf(&em->text, "  movsd %%xmm0, %d(%%rbp)\n",
                                   em->vreg_offset[inst->dest]);
                } else {
                    FOC_buf_printf(&em->text, "  movq %%rax, %d(%%rbp)\n",
                                   em->vreg_offset[inst->dest]);
                }
            }
            break;
        }
        case FOC_OP_RET:
            if (inst->has_value) {
                emit_load_value(em, &inst->lhs, inst->type);
            }
            FOC_buf_printf(&em->text, "  leave\n");
            FOC_buf_printf(&em->text, "  ret\n");
            break;
        case FOC_OP_JMP:
            FOC_buf_printf(&em->text, "  jmp .L%d\n", inst->label);
            break;
        case FOC_OP_BRANCH:
            /* La instruccion tiene dos destinos: si la condicion es cierta va
             * a `label`, si es falsa a `label_false`. */
            emit_load_value(em, &inst->lhs, FOC_IR_I32);
            FOC_buf_printf(&em->text, "  cmpq $0, %%rax\n");
            FOC_buf_printf(&em->text, "  jne .L%d\n", inst->label);
            FOC_buf_printf(&em->text, "  jmp .L%d\n", inst->label_false);
            break;
        case FOC_OP_LABEL:
            FOC_buf_printf(&em->text, ".L%d:\n", inst->label);
            break;
        case FOC_OP_ALLOCA: {
            int slot = em->alloca_offset[alloca_index++];
            FOC_buf_printf(&em->text, "  leaq %d(%%rbp), %%rax\n", slot);
            emit_store_dest(em, inst->dest, FOC_IR_PTR);
            break;
        }
        case FOC_OP_FRAME_ADDR:
            FOC_buf_printf(&em->text, "  leaq %lld(%%rbp), %%rax\n", inst->imm);
            emit_store_dest(em, inst->dest, FOC_IR_PTR);
            break;
        case FOC_OP_GLOBAL_ADDR:
            FOC_buf_printf(&em->text, "  leaq %s(%%rip), %%rax\n", inst->symbol);
            emit_store_dest(em, inst->dest, FOC_IR_PTR);
            break;
        case FOC_OP_FIELD_ADDR:
            emit_load_value(em, &inst->lhs, FOC_IR_PTR);
            FOC_buf_printf(&em->text, "  addq $%lld, %%rax\n", inst->imm);
            emit_store_dest(em, inst->dest, FOC_IR_PTR);
            break;
        case FOC_OP_GETELEM:
            emit_load_value(em, &inst->lhs, FOC_IR_PTR);
            FOC_buf_printf(&em->text, "  movq %%rax, %%rdi\n");
            emit_load_value(em, &inst->rhs, FOC_IR_I64);
            FOC_buf_printf(&em->text, "  imulq $%lld, %%rax\n", inst->imm);
            FOC_buf_printf(&em->text, "  addq %%rax, %%rdi\n");
            FOC_buf_printf(&em->text, "  movq %%rdi, %%rax\n");
            emit_store_dest(em, inst->dest, FOC_IR_PTR);
            break;
        case FOC_OP_COPY:
            /* Copia de memoria: se realiza byte a byte. */
            emit_load_value(em, &inst->lhs, FOC_IR_PTR);
            FOC_buf_printf(&em->text, "  movq %%rax, %%rdi\n");
            emit_load_value(em, &inst->rhs, FOC_IR_PTR);
            FOC_buf_printf(&em->text, "  movq %%rax, %%rsi\n");
            FOC_buf_printf(&em->text, "  movl $%lld, %%ecx\n", inst->imm);
            FOC_buf_printf(&em->text, "  rep movsb\n");
            break;
        }
    }

    FOC_buf_printf(&em->text, "  leave\n");
    FOC_buf_printf(&em->text, "  ret\n");
    if (em->elf) {
        FOC_buf_printf(&em->text, "  .size %s, .-%s\n", function->name, function->name);
    }

    free(em->vreg_offset);
    free(em->alloca_offset);
    em->vreg_offset = NULL;
    em->alloca_offset = NULL;
}

bool FOC_emit_x86_64(FOCIRModule *module, const FOCTarget *target, char **out_text,
                     size_t *out_len) {
    FOCEmitter em;
    memset(&em, 0, sizeof(em));
    FOC_buf_init(&em.text);
    em.target = target;
    em.abi = FOC_abi_for(target);
    em.elf = target->os != FOC_OS_WINDOWS && target->os != FOC_OS_DARWIN;

    FOC_buf_printf(&em.text, "  .file 1 \"%s\"\n", "source");
    for (size_t i = 0; i < module->global_count; i++) {
        FOCIRGlobal *global = module->globals[i];
        if (global->is_extern && !global->data) {
            continue;
        }
        if (em.elf) {
            /* .local solo existe en ELF; en COFF basta con no exportar. */
            FOC_buf_printf(&em.text, "%s %s\n", global->is_static ? ".local" : ".globl",
                           global->name);
        } else if (!global->is_static) {
            FOC_buf_printf(&em.text, "  .globl %s\n", global->name);
        }
        if (em.elf) {
            FOC_buf_printf(&em.text, "  .type %s, @object\n", global->name);
            FOC_buf_printf(&em.text, "  .size %s, %d\n", global->name, global->size);
        }
        if (global->data) {
            FOC_buf_printf(&em.text, "  .section .rodata\n");
            FOC_buf_printf(&em.text, "%s:\n", global->name);
            FOC_buf_printf(&em.text, "  .byte ");
            for (size_t j = 0; j < global->data_len; j++) {
                FOC_buf_printf(&em.text, "%s%d", j ? "," : "", (unsigned char)global->data[j]);
            }
            FOC_buf_printf(&em.text, "\n");
        } else if (global->has_init) {
            FOC_buf_printf(&em.text, "  .data\n");
            FOC_buf_printf(&em.text, "  .align %d\n", global->size >= 8 ? 8 : 4);
            FOC_buf_printf(&em.text, "%s:\n", global->name);
            if (global->init_symbol) {
                FOC_buf_printf(&em.text, "  .quad %s\n", global->init_symbol);
            } else {
                FOC_buf_printf(&em.text, "  .quad %lld\n", global->init_int);
            }
        } else {
            FOC_buf_printf(&em.text, "  .bss\n");
            FOC_buf_printf(&em.text, "  .align %d\n", global->size >= 8 ? 8 : 4);
            FOC_buf_printf(&em.text, "%s:\n", global->name);
            FOC_buf_printf(&em.text, "  .zero %d\n", global->size > 0 ? global->size : 1);
        }
    }

    /* Constantes flotantes. */
    bool has_float = false;
    for (size_t i = 0; i < module->function_count; i++) {
        FOCIRFunction *function = module->functions[i];
        for (size_t j = 0; j < function->inst_count; j++) {
            if (FOC_ir_inst_at(function, j)->op == FOC_OP_CONST_FLOAT) {
                has_float = true;
            }
        }
    }
    if (has_float) {
        FOC_buf_printf(&em.text, "  .section .rodata\n");
        for (size_t i = 0; i < module->function_count; i++) {
            FOCIRFunction *function = module->functions[i];
            for (size_t j = 0; j < function->inst_count; j++) {
                if (FOC_ir_inst_at(function, j)->op == FOC_OP_CONST_FLOAT) {
                    unsigned long long bits = 0;
                    double value = FOC_ir_inst_at(function, j)->fimm;
                    memcpy(&bits, &value, sizeof(bits));
                    FOC_buf_printf(&em.text, "  .align 8\n");
                    FOC_buf_printf(&em.text, ".LFC%zu_%zu:\n", i, j);
                    FOC_buf_printf(&em.text, "  .quad %llu\n", bits);
                }
            }
        }
    }

    for (size_t i = 0; i < module->function_count; i++) {
        emit_function(&em, module->functions[i], i);
    }

    if (em.elf) {
        /* Marca la pila como no ejecutable (solo ELF). */
        FOC_buf_printf(&em.text, "  .section .note.GNU-stack,\"\",@progbits\n");
    }

    char *text = FOC_buf_take(&em.text);
    size_t text_length = strlen(text);
    if (out_text) {
        *out_text = text;
    } else {
        free(text);
    }
    if (out_len) {
        *out_len = text_length;
    }
    return true;
}
