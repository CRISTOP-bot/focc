#include "foc/ir.h"

#include <stdlib.h>
#include <string.h>

FOCIRModule *FOC_ir_module_new(void) {
    FOCIRModule *module = (FOCIRModule *)calloc(1, sizeof(FOCIRModule));
    return module;
}

FOCIRFunction *FOC_ir_function_new(const char *name) {
    FOCIRFunction *function = (FOCIRFunction *)calloc(1, sizeof(FOCIRFunction));
    if (!function) {
        return NULL;
    }
    /* El nombre se copia: los nombres del fuente no estan terminados en NUL. */
    function->name = FOC_strdup(name ? name : "");
    return function;
}

FOCIRInst *FOC_ir_add_inst(FOCIRFunction *function) {
    if (!function->last_block || function->last_block->used == FOC_IR_BLOCK_SIZE) {
        FOCIRInstBlock *block = (FOCIRInstBlock *)calloc(1, sizeof(FOCIRInstBlock));
        if (!block) {
            fprintf(stderr, "foc: agotamiento de memoria\n");
            abort();
        }
        if (function->last_block) {
            function->last_block->next = block;
        } else {
            function->blocks = block;
        }
        function->last_block = block;
        block->used = 0;
    }
    FOCIRInstBlock *block = function->last_block;
    FOCIRInst *inst = &block->insts[block->used++];
    memset(inst, 0, sizeof(FOCIRInst));
    inst->dest = -1;
    inst->lhs.reg = -1;
    inst->rhs.reg = -1;
    inst->third.reg = -1;
    function->inst_count++;
    return inst;
}

FOCIRInst *FOC_ir_inst_at(FOCIRFunction *function, size_t index) {
    if (index >= function->inst_count) {
        return NULL;
    }
    size_t block_index = index / FOC_IR_BLOCK_SIZE;
    size_t offset = index % FOC_IR_BLOCK_SIZE;
    FOCIRInstBlock *block = function->blocks;
    for (size_t i = 0; i < block_index && block; i++) {
        block = block->next;
    }
    if (!block) {
        return NULL;
    }
    return &block->insts[offset];
}

int FOC_ir_new_reg(FOCIRFunction *function, FOCIRType type) {
    FOCIRType *types = (FOCIRType *)realloc(function->vreg_types,
                                             (function->vreg_count + 1) * sizeof(FOCIRType));
    if (!types) {
        fprintf(stderr, "foc: agotamiento de memoria\n");
        abort();
    }
    function->vreg_types = types;
    function->vreg_types[function->vreg_count] = type;
    return function->vreg_count++;
}

int FOC_ir_new_label(FOCIRModule *module) { return module->label_count++; }

FOCIRGlobal *FOC_ir_add_global(FOCIRModule *module, const char *name) {
    FOCIRGlobal **globals =
        (FOCIRGlobal **)realloc(module->globals, (module->global_count + 1) * sizeof(FOCIRGlobal *));
    if (!globals) {
        fprintf(stderr, "foc: agotamiento de memoria\n");
        abort();
    }
    module->globals = globals;
    FOCIRGlobal *global = (FOCIRGlobal *)calloc(1, sizeof(FOCIRGlobal));
    if (!global) {
        fprintf(stderr, "foc: agotamiento de memoria\n");
        abort();
    }
    global->name = FOC_strdup(name ? name : "");
    module->globals[module->global_count++] = global;
    return global;
}

const char *FOC_ir_type_name(FOCIRType type) {
    switch (type) {
    case FOC_IR_VOID:
        return "void";
    case FOC_IR_I1:
        return "i1";
    case FOC_IR_I8:
        return "i8";
    case FOC_IR_I16:
        return "i16";
    case FOC_IR_I32:
        return "i32";
    case FOC_IR_I64:
        return "i64";
    case FOC_IR_F32:
        return "f32";
    case FOC_IR_F64:
        return "f64";
    case FOC_IR_PTR:
        return "ptr";
    default:
        return "?";
    }
}

int FOC_ir_type_size(FOCIRType type) {
    switch (type) {
    case FOC_IR_VOID:
        return 0;
    case FOC_IR_I1:
    case FOC_IR_I8:
        return 1;
    case FOC_IR_I16:
        return 2;
    case FOC_IR_I32:
    case FOC_IR_F32:
        return 4;
    case FOC_IR_I64:
    case FOC_IR_F64:
    case FOC_IR_PTR:
        return 8;
    default:
        return 8;
    }
}

FOCIRType FOC_ir_type_of(const FOCType *type, int pointer_size) {
    if (!type) {
        return FOC_IR_I32;
    }
    switch (type->kind) {
    case FOC_TYPE_VOID:
        return FOC_IR_VOID;
    case FOC_TYPE_BOOL:
    case FOC_TYPE_CHAR:
    case FOC_TYPE_SCHAR:
    case FOC_TYPE_UCHAR:
        return FOC_IR_I8;
    case FOC_TYPE_SHORT:
    case FOC_TYPE_USHORT:
        return FOC_IR_I16;
    case FOC_TYPE_INT:
    case FOC_TYPE_UINT:
    case FOC_TYPE_ENUM:
        return FOC_IR_I32;
    case FOC_TYPE_LONG:
    case FOC_TYPE_ULONG:
    case FOC_TYPE_LLONG:
    case FOC_TYPE_ULLONG:
        return FOC_IR_I64;
    case FOC_TYPE_FLOAT:
        return FOC_IR_F32;
    case FOC_TYPE_DOUBLE:
    case FOC_TYPE_LDOUBLE:
        return FOC_IR_F64;
    case FOC_TYPE_POINTER:
    case FOC_TYPE_ARRAY:
    case FOC_TYPE_FUNCTION:
        return FOC_IR_PTR;
    case FOC_TYPE_STRUCT:
    case FOC_TYPE_UNION:
        return FOC_IR_PTR;
    default:
        return FOC_IR_I32;
    }
    (void)pointer_size;
}
