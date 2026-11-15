/* Representacion intermedia de FOC (independiente de la arquitectura). */
#ifndef FOC_IR_H
#define FOC_IR_H

#include "foc/ast.h"
#include "foc/diag.h"
#include "foc/platform.h"

typedef enum {
    FOC_IR_VOID = 0,
    FOC_IR_I1,
    FOC_IR_I8,
    FOC_IR_I16,
    FOC_IR_I32,
    FOC_IR_I64,
    FOC_IR_F32,
    FOC_IR_F64,
    FOC_IR_PTR
} FOCIRType;

typedef enum {
    FOC_OP_CONST = 0,
    FOC_OP_CONST_FLOAT,
    FOC_OP_LOAD,
    FOC_OP_STORE,
    FOC_OP_ADD,
    FOC_OP_SUB,
    FOC_OP_MUL,
    FOC_OP_DIV,
    FOC_OP_MOD,
    FOC_OP_AND,
    FOC_OP_OR,
    FOC_OP_XOR,
    FOC_OP_SHL,
    FOC_OP_SHR,
    FOC_OP_NEG,
    FOC_OP_BITNOT,
    FOC_OP_NOT,
    FOC_OP_EQ,
    FOC_OP_NE,
    FOC_OP_LT,
    FOC_OP_LE,
    FOC_OP_GT,
    FOC_OP_GE,
    FOC_OP_CAST,
    FOC_OP_CALL,
    FOC_OP_RET,
    FOC_OP_JMP,
    FOC_OP_BRANCH,
    FOC_OP_LABEL,
    FOC_OP_ALLOCA,
    FOC_OP_GETELEM,
    FOC_OP_FRAME_ADDR,
    FOC_OP_GLOBAL_ADDR,
    FOC_OP_FIELD_ADDR,
    FOC_OP_COPY
} FOCIROp;

typedef struct {
    int reg; /* registro virtual; -1 si es una constante */
    FOCIRType type;
    long long imm; /* valor constante */
} FOCIRValue;

typedef struct {
    FOCIROp op;
    FOCIRType type;
    int dest; /* registro virtual destino (-1 si no produce valor) */
    FOCIRValue lhs;
    FOCIRValue rhs;
    FOCIRValue third;
    long long imm;        /* desplazamiento o constante entera */
    double fimm;
    const char *symbol;  /* nombre de funcion o etiqueta */
    int label;            /* id de etiqueta */
    int label_false;      /* etiqueta de la rama falsa (FOC_OP_BRANCH) */
    FOCIRValue args[8];  /* argumentos de llamada */
    int arg_count;
    bool indirect;        /* llamada indirecta */
    bool variadic;        /* llamada a una funcion variadica */
    bool has_value;       /* el operador produce un valor (p.ej. return) */
    bool dead;            /* marcada por el optimizador como inutil */
    FOCSpan span;
} FOCIRInst;

typedef struct {
    const char *name;
    FOCIRType type;
} FOCIRParam;

typedef struct {
    const char *name;
    FOCIRType type;
    int offset; /* desplazamiento dentro del bloque de globales */
    bool is_static;
    bool is_extern;
    long long init_int;
    bool has_init;
    FOCIRValue init_value;
    int size;
    char *data;     /* datos inicializadores (cadenas) */
    size_t data_len;
    const char *init_symbol; /* relocacion del valor inicial a una cadena */
} FOCIRGlobal;

/* Las instrucciones se guardan en bloques de tamaño fijo: de este modo los
 * punteros a instrucciones siguen siendo validos aunque la IR crezca, lo que
 * permite al generador mantener referencias mientras genera otras. */
#define FOC_IR_BLOCK_SIZE 64

typedef struct FOCIRInstBlock {
    struct FOCIRInstBlock *next;
    size_t used;
    FOCIRInst insts[FOC_IR_BLOCK_SIZE];
} FOCIRInstBlock;

typedef struct {
    const char *name;
    FOCIRType ret_type;
    FOCIRInstBlock *blocks;
    FOCIRInstBlock *last_block;
    size_t inst_count;
    FOCIRParam *params;
    size_t param_count;
    bool variadic;
    int vreg_count;
    FOCIRType *vreg_types;
    int frame_size;
} FOCIRFunction;

typedef struct {
    FOCIRFunction **functions;
    size_t function_count;
    FOCIRGlobal *globals;
    size_t global_count;
    int label_count;
} FOCIRModule;

/* ------------------------------------------------------------------ */
/* Constructores                                                       */
/* ------------------------------------------------------------------ */

FOCIRModule *FOC_ir_module_new(void);
FOCIRFunction *FOC_ir_function_new(const char *name);
FOCIRInst *FOC_ir_add_inst(FOCIRFunction *function);
/* Devuelve la instruccion numero `index` (direccion estable). */
FOCIRInst *FOC_ir_inst_at(FOCIRFunction *function, size_t index);
int FOC_ir_new_reg(FOCIRFunction *function, FOCIRType type);
int FOC_ir_new_label(FOCIRModule *module);
FOCIRGlobal *FOC_ir_add_global(FOCIRModule *module, const char *name);
const char *FOC_ir_type_name(FOCIRType type);
int FOC_ir_type_size(FOCIRType type);

/* Traduce un tipo del analisis semantico a un tipo de la IR. */
FOCIRType FOC_ir_type_of(const FOCType *type, int pointer_size);

/* Traduce la unidad de traduccion a IR (incluye el analisis semantico). */
FOCIRModule *FOC_irgen(FOCUnit *unit, FOCDiagEngine *diag, FOCSourceMgr *sources,
                        const FOCTarget *target);

#endif /* FOC_IR_H */
