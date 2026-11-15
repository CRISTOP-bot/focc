/* Traduccion del AST a la IR, con analisis semantico integrated.
 *
 * Este modulo resuelve nombres y tipos, comprueba las expresiones y produce
 * la representacion intermedia. Es el unico punto donde el frontend conoce
 * la semantica del lenguaje; el backend solo ve la IR.
 */
#include "foc/ir.h"

#include "foc/map.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Simbolos                                                            */
/* ------------------------------------------------------------------ */

typedef enum {
    FOC_SYM_VARIABLE = 0,
    FOC_SYM_FUNCTION,
    FOC_SYM_TYPEDEF,
    FOC_SYM_ENUM_CONSTANT
} FOCSymbolKind;

typedef struct FOCSymbol {
    const char *name;
    size_t name_len;
    FOCSymbolKind kind;
    FOCType *type;
    bool is_global;
    bool is_static;
    bool is_extern;
    bool is_defined;
    bool is_local;
    int frame_offset;
    FOCIRGlobal *global;
    long long enum_value;
    FOCSpan span;
} FOCSymbol;

typedef struct FOCScope {
    FOCMap vars;
    FOCMap tags;
    struct FOCScope *parent;
} FOCScope;

typedef struct {
    FOCDiagEngine *diag;
    FOCSourceMgr *sources;
    FOCScope *scope;
    FOCIRModule *module;
    FOCIRFunction *function;
    int pointer_size;
    int frame_offset;
    int max_frame;
    int error_count;
    int string_counter;
    /* Contextos de control de flujo. */
    struct LoopContext *loop;
    struct SwitchContext *switch_ctx;
} FOCSema;

typedef struct LoopContext {
    struct LoopContext *parent;
    int break_label;
    int continue_label;
} FOCLoopContext;

typedef struct FOCSwitchCase {
    FOCStmt *stmt;
    int label;
    int compare_label;
} FOCSwitchCase;

typedef struct SwitchContext {
    struct SwitchContext *parent;
    int break_label;
    FOCType *cond_type;
    FOCSwitchCase *cases;
    size_t case_count;
} FOCSwitchContext;

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

static void sema_error(FOCSema *sema, FOCSpan span, const char *code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char message[512];
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    FOC_diag_error(sema->diag, span, code, "%s", message);
    sema->error_count++;
}

static FOCScope *scope_push(FOCSema *sema) {
    FOCScope *scope = (FOCScope *)calloc(1, sizeof(FOCScope));
    if (!scope) {
        return NULL;
    }
    FOC_map_init(&scope->vars);
    FOC_map_init(&scope->tags);
    scope->parent = sema->scope;
    sema->scope = scope;
    return scope;
}

static void scope_pop(FOCSema *sema) {
    FOCScope *scope = sema->scope;
    if (!scope) {
        return;
    }
    sema->scope = scope->parent;
    FOC_map_free(&scope->vars);
    FOC_map_free(&scope->tags);
    free(scope);
}

static FOCSymbol *scope_lookup(FOCSema *sema, const char *name, size_t len) {
    for (FOCScope *scope = sema->scope; scope; scope = scope->parent) {
        FOCSymbol *symbol = (FOCSymbol *)FOC_map_get(&scope->vars, name, len);
        if (symbol) {
            return symbol;
        }
    }
    return NULL;
}

static FOCSymbol *scope_lookup_local(FOCScope *scope, const char *name, size_t len) {
    return (FOCSymbol *)FOC_map_get(&scope->vars, name, len);
}

static FOCType *scope_lookup_tag(FOCSema *sema, const char *name, size_t len) {
    for (FOCScope *scope = sema->scope; scope; scope = scope->parent) {
        FOCType *type = (FOCType *)FOC_map_get(&scope->tags, name, len);
        if (type) {
            return type;
        }
    }
    return NULL;
}

static void scope_define(FOCSema *sema, FOCSymbol *symbol) {
    FOC_map_put(&sema->scope->vars, symbol->name, symbol->name_len, symbol);
}

static FOCSymbol *symbol_new(const char *name, size_t len, FOCSymbolKind kind) {
    FOCSymbol *symbol = (FOCSymbol *)calloc(1, sizeof(FOCSymbol));
    if (!symbol) {
        return NULL;
    }
    symbol->name = name;
    symbol->name_len = len;
    symbol->kind = kind;
    return symbol;
}

/* ------------------------------------------------------------------ */
/* Resolucion de tipos                                                 */
/* ------------------------------------------------------------------ */

static FOCType *resolve_spec_type(FOCSema *sema, FOCDeclSpec *spec);
static FOCType *resolve_declarator_type(FOCSema *sema, FOCDeclSpec *spec, FOCDeclarator *decl);

static void layout_struct(FOCType *type, int pointer_size) {
    int offset = 0;
    int max_align = 1;
    for (size_t i = 0; i < type->field_count; i++) {
        FOCField *field = &type->fields[i];
        int size = field->type ? field->type->size : 0;
        int align = field->type ? field->type->align : 1;
        if (align < 1) {
            align = 1;
        }
        if (align > max_align) {
            max_align = align;
        }
        if (field->bit_width > 0) {
            /* Los bitfields se alinean a la unidad natural. */
            int unit = size > 0 ? size : 4;
            if (offset % unit) {
                offset += unit - (offset % unit);
            }
            field->offset = offset;
            offset += unit;
            continue;
        }
        if (align > 1 && (offset % align)) {
            offset += align - (offset % align);
        }
        field->offset = offset;
        offset += size;
    }
    if (max_align > 1 && (offset % max_align)) {
        offset += max_align - (offset % max_align);
    }
    type->size = offset;
    type->align = max_align;
    (void)pointer_size;
}

static FOCType *complete_struct_type(FOCSema *sema, FOCType *type, FOCStructSpec *spec) {
    if (type->complete) {
        return type;
    }
    type->complete = true;
    if (!spec || !spec->has_body) {
        return type;
    }
    size_t count = spec->field_count;
    type->fields = (FOCField *)calloc(count ? count : 1, sizeof(FOCField));
    if (!type->fields) {
        return type;
    }
    type->field_count = 0;
    for (size_t i = 0; i < count; i++) {
        FOCFieldDecl *field_decl = spec->fields[i];
        for (size_t j = 0; j < field_decl->declarator_count; j++) {
            FOCDeclarator *declarator = field_decl->declarators[j];
            FOCType *field_type = field_decl->spec ? resolve_spec_type(sema, field_decl->spec) : NULL;
            if (declarator) {
                field_type = resolve_declarator_type(sema, field_decl->spec, declarator);
            }
            FOCField *field = &type->fields[type->field_count++];
            field->name = declarator ? declarator->name : "";
            field->name_len = declarator ? declarator->name_len : 0;
            field->type = field_type;
            field->span = declarator ? declarator->name_span : field_decl->span;
            if (field_decl->bit_widths && field_decl->bit_widths[j]) {
                field->bit_width = 1; /* el ancho exacto se evalua despues */
            }
        }
    }
    layout_struct(type, sema->pointer_size);
    return type;
}

static FOCType *resolve_spec_type(FOCSema *sema, FOCDeclSpec *spec) {
    if (!spec) {
        return FOC_type_basic(FOC_TYPE_INT);
    }
    if (spec->struct_spec) {
        FOCStructSpec *struct_spec = spec->struct_spec;
        bool is_union = struct_spec->is_union;
        FOCType *existing = NULL;
        if (struct_spec->tag && struct_spec->tag_len) {
            existing = scope_lookup_tag(sema, struct_spec->tag, struct_spec->tag_len);
        }
        FOCType *type;
        if (existing && (existing->kind == FOC_TYPE_STRUCT || existing->kind == FOC_TYPE_UNION)) {
            type = existing;
        } else {
            type = FOC_type_new(is_union ? FOC_TYPE_UNION : FOC_TYPE_STRUCT);
            if (struct_spec->tag && struct_spec->tag_len) {
                type->name = struct_spec->tag;
                FOC_map_put(&sema->scope->tags, struct_spec->tag, struct_spec->tag_len, type);
            }
        }
        if (struct_spec->has_body) {
            complete_struct_type(sema, type, struct_spec);
        } else if (!type->complete && !existing) {
            type->size = 0;
            type->align = 1;
        }
        return type;
    }
    if (spec->enum_spec) {
        FOCEnumSpec *enum_spec = spec->enum_spec;
        FOCType *type = NULL;
        if (enum_spec->tag && enum_spec->tag_len) {
            type = scope_lookup_tag(sema, enum_spec->tag, enum_spec->tag_len);
        }
        if (!type) {
            type = FOC_type_new(FOC_TYPE_ENUM);
            if (enum_spec->tag && enum_spec->tag_len) {
                type->name = enum_spec->tag;
                FOC_map_put(&sema->scope->tags, enum_spec->tag, enum_spec->tag_len, type);
            }
        }
        if (enum_spec->has_body && type->value_count == 0) {
            long long next = 0;
            size_t capacity = enum_spec->item_count;
            type->values = (FOCEnumValue *)calloc(capacity ? capacity : 1, sizeof(FOCEnumValue));
            if (!type->values) {
                return type;
            }
            for (size_t i = 0; i < enum_spec->item_count; i++) {
                FOCEnumItem *item = enum_spec->items[i];
                long long value = next;
                if (item->value) {
                    /* Los valores de enumeracion deben ser constantes. */
                    FOCExpr copy = *item->value;
                    FOCExpr *expr = &copy;
                    /* Evaluacion simple de literales. */
                    if (expr->kind == FOC_EXPR_INT) {
                        value = expr->int_lit.value;
                    } else if (expr->kind == FOC_EXPR_CHAR) {
                        value = expr->char_lit.value;
                    } else if (expr->kind == FOC_EXPR_IDENT) {
                        FOCSymbol *symbol =
                            scope_lookup(sema, expr->ident.name, expr->ident.name_len);
                        if (symbol && symbol->kind == FOC_SYM_ENUM_CONSTANT) {
                            value = symbol->enum_value;
                        }
                    } else if (expr->kind == FOC_EXPR_UNARY && expr->unary.op &&
                               strcmp(expr->unary.op, "-") == 0) {
                        if (expr->unary.operand && expr->unary.operand->kind == FOC_EXPR_INT) {
                            value = -expr->unary.operand->int_lit.value;
                        }
                    } else {
                        sema_error(sema, expr->span, "E0401",
                                   "el valor de un enumerador debe ser una constante entera");
                    }
                }
                type->values[i].name = item->name;
                type->values[i].value = value;
                type->values[i].span = item->name_span;
                type->value_count = i + 1;

                FOCSymbol *symbol = symbol_new(item->name, item->name_len, FOC_SYM_ENUM_CONSTANT);
                if (symbol) {
                    symbol->type = type;
                    symbol->enum_value = value;
                    symbol->span = item->name_span;
                    scope_define(sema, symbol);
                }
                next = value + 1;
            }
        }
        type->size = 4;
        type->align = 4;
        return type;
    }
    if (spec->typedef_name) {
        FOCSymbol *symbol = scope_lookup(sema, spec->typedef_name, spec->typedef_len);
        if (symbol && symbol->kind == FOC_SYM_TYPEDEF) {
            return symbol->type;
        }
        sema_error(sema, spec->typedef_span, "E0402", "tipo desconocido '%.*s'", (int)spec->typedef_len, spec->typedef_name);
        return FOC_type_basic(FOC_TYPE_INT);
    }

    /* Tipo basico combinado. */
    FOCTypeKind kind;
    if (spec->t_void) {
        kind = FOC_TYPE_VOID;
    } else if (spec->t_bool) {
        kind = FOC_TYPE_BOOL;
    } else if (spec->t_float) {
        kind = FOC_TYPE_FLOAT;
    } else if (spec->t_double) {
        kind = spec->long_count ? FOC_TYPE_LDOUBLE : FOC_TYPE_DOUBLE;
    } else if (spec->t_char) {
        kind = spec->t_unsigned ? FOC_TYPE_UCHAR : (spec->t_signed ? FOC_TYPE_SCHAR : FOC_TYPE_CHAR);
    } else if (spec->t_short) {
        kind = spec->t_unsigned ? FOC_TYPE_USHORT : FOC_TYPE_SHORT;
    } else if (spec->long_count >= 2) {
        kind = spec->t_unsigned ? FOC_TYPE_ULLONG : FOC_TYPE_LLONG;
    } else if (spec->long_count == 1) {
        kind = spec->t_unsigned ? FOC_TYPE_ULONG : FOC_TYPE_LONG;
    } else if (spec->t_unsigned) {
        kind = FOC_TYPE_UINT;
    } else if (spec->t_signed) {
        kind = FOC_TYPE_INT;
    } else {
        kind = FOC_TYPE_INT;
    }
    FOCType *type = FOC_type_basic(kind);
    FOCType *result = FOC_type_qualify(type, spec->is_const, spec->is_volatile, spec->is_restrict);
    result->size = FOC_type_default_size(kind, sema->pointer_size);
    result->align = FOC_type_default_align(kind, sema->pointer_size);
    return result;
}

static FOCType *resolve_param_type(FOCSema *sema, FOCParam *param) {
    FOCType *type = resolve_spec_type(sema, param->spec);
    if (param->declarator) {
        type = resolve_declarator_type(sema, param->spec, param->declarator);
    }
    /* Los arrays y funciones se ajustan a punteros en los parametros. */
    if (type && type->kind == FOC_TYPE_ARRAY) {
        FOCType *pointer = FOC_type_pointer(type->elem);
        pointer->size = sema->pointer_size;
        pointer->align = sema->pointer_size;
        return pointer;
    }
    if (type && type->kind == FOC_TYPE_FUNCTION) {
        FOCType *pointer = FOC_type_pointer(type);
        pointer->size = sema->pointer_size;
        pointer->align = sema->pointer_size;
        return pointer;
    }
    return type;
}

static FOCType *resolve_declarator_type(FOCSema *sema, FOCDeclSpec *spec, FOCDeclarator *decl) {
    FOCType *type = resolve_spec_type(sema, spec);
    if (!decl) {
        return type;
    }
    for (size_t i = 0; i < decl->ctor_count; i++) {
        FOCDeclCtor *ctor = &decl->ctors[i];
        switch (ctor->kind) {
        case FOC_CTOR_POINTER: {
            FOCType *pointer = FOC_type_pointer(type);
            pointer->size = sema->pointer_size;
            pointer->align = sema->pointer_size;
            pointer->is_const = ctor->is_const;
            pointer->is_volatile = ctor->is_volatile;
            pointer->is_restrict = ctor->is_restrict;
            type = pointer;
            break;
        }
        case FOC_CTOR_ARRAY: {
            int length = -1;
            if (ctor->array_len) {
                FOCExpr *expr = ctor->array_len;
                if (expr->kind == FOC_EXPR_INT) {
                    length = (int)expr->int_lit.value;
                } else if (expr->kind == FOC_EXPR_CHAR) {
                    length = (int)expr->char_lit.value;
                } else {
                    sema_error(sema, expr->span, "E0403",
                               "el tamano del array debe ser una constante");
                }
            }
            FOCType *array = FOC_type_array(type, length);
            array->size = (length >= 0 && type) ? type->size * length : 0;
            array->align = type ? type->align : 1;
            type = array;
            break;
        }
        case FOC_CTOR_FUNCTION: {
            size_t count = ctor->param_count;
            FOCType **params = (FOCType **)calloc(count ? count : 1, sizeof(FOCType *));
            if (!params) {
                return type;
            }
            for (size_t i = 0; i < count; i++) {
                params[i] = resolve_param_type(sema, ctor->params[i]);
            }
            FOCType *function = FOC_type_function(type, params, count, ctor->variadic);
            type = function;
            break;
        }
        }
    }
    return type;
}

/* ------------------------------------------------------------------ */
/* Valores y expresiones                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    FOCIRValue value;
    FOCIRValue address; /* valido si is_lvalue */
    FOCType *type;
    bool is_lvalue;
} FOCLowered;

static FOCLowered lower_expr(FOCSema *sema, FOCExpr *expr);
static void lower_stmt(FOCSema *sema, FOCStmt *stmt);

static FOCIRValue ir_const(FOCIRType type, long long value) {
    FOCIRValue v;
    v.reg = -1;
    v.type = type;
    v.imm = value;
    return v;
}

static FOCIRInst *emit(FOCSema *sema, FOCIROp op) {
    FOCIRInst *inst = FOC_ir_add_inst(sema->function);
    inst->op = op;
    inst->type = FOC_IR_I32;
    return inst;
}

static FOCIRType ir_type_of(FOCSema *sema, const FOCType *type) {
    return FOC_ir_type_of(type, sema->pointer_size);
}

static bool type_is_float(const FOCType *type) {
    return type && (type->kind == FOC_TYPE_FLOAT || type->kind == FOC_TYPE_DOUBLE ||
                    type->kind == FOC_TYPE_LDOUBLE);
}

/* Reserva un hueco temporal en el marco de pila (para fusionar valores). */
static FOCIRValue alloca_temp(FOCSema *sema, int size) {
    FOCIRInst *inst = emit(sema, FOC_OP_ALLOCA);
    inst->imm = size;
    inst->type = FOC_IR_PTR;
    inst->dest = FOC_ir_new_reg(sema->function, FOC_IR_PTR);
    FOCIRValue v;
    v.reg = inst->dest;
    v.type = FOC_IR_PTR;
    v.imm = 0;
    return v;
}

/* Devuelve la direccion de una variable local (marco de pila). */
static FOCIRValue frame_address(FOCSema *sema, int offset) {
    FOCIRInst *inst = emit(sema, FOC_OP_FRAME_ADDR);
    inst->imm = offset;
    inst->dest = FOC_ir_new_reg(sema->function, FOC_IR_PTR);
    inst->type = FOC_IR_PTR;
    FOCIRValue v;
    v.reg = inst->dest;
    v.type = FOC_IR_PTR;
    v.imm = 0;
    return v;
}

static FOCIRValue global_address(FOCSema *sema, FOCIRGlobal *global) {
    FOCIRInst *inst = emit(sema, FOC_OP_GLOBAL_ADDR);
    inst->symbol = global->name;
    inst->dest = FOC_ir_new_reg(sema->function, FOC_IR_PTR);
    inst->type = FOC_IR_PTR;
    FOCIRValue v;
    v.reg = inst->dest;
    v.type = FOC_IR_PTR;
    v.imm = 0;
    return v;
}

/* Crea un literal de cadena como global. */
static FOCIRGlobal *make_string_global(FOCSema *sema, const char *data, size_t len) {
    char name[64];
    snprintf(name, sizeof(name), ".LC%d", sema->string_counter++);
    FOCIRGlobal *global = FOC_ir_add_global(sema->module, name);
    global->is_static = true;
    global->data = (char *)malloc(len + 1);
    if (global->data) {
        memcpy(global->data, data, len);
        global->data[len] = '\0';
    }
    global->data_len = len + 1;
    global->size = (int)global->data_len;
    global->type = FOC_IR_PTR;
    return global;
}

static FOCLowered lower_ident(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;

    FOCSymbol *symbol = scope_lookup(sema, expr->ident.name, expr->ident.name_len);
    if (!symbol) {
        sema_error(sema, expr->span, "E0404", "identificador no declarado: '%.*s'", (int)expr->ident.name_len, expr->ident.name);
        out.type = FOC_type_basic(FOC_TYPE_INT);
        out.value = ir_const(FOC_IR_I32, 0);
        return out;
    }
    if (symbol->kind == FOC_SYM_ENUM_CONSTANT) {
        out.type = symbol->type;
        out.value = ir_const(ir_type_of(sema, symbol->type), symbol->enum_value);
        return out;
    }
    if (symbol->kind == FOC_SYM_FUNCTION) {
        out.type = symbol->type;
        FOCIRValue v;
        v.reg = -1;
        v.type = FOC_IR_PTR;
        v.imm = 0;
        out.value = v;
        out.is_lvalue = false;
        return out;
    }

    out.type = symbol->type;
    /* Un array decae a un puntero a su primer elemento: el valor es la
     * direccion del array, no los bytes cargados. */
    if (symbol->type && symbol->type->kind == FOC_TYPE_ARRAY) {
        FOCType *pointer = FOC_type_pointer(symbol->type->elem);
        pointer->size = sema->pointer_size;
        pointer->align = sema->pointer_size;
        out.type = pointer;
        if (symbol->is_local) {
            out.address = frame_address(sema, symbol->frame_offset);
        } else {
            out.address = global_address(sema, symbol->global);
        }
        out.value = out.address;
        out.is_lvalue = true;
        return out;
    }
    FOCIRType ir_type = ir_type_of(sema, symbol->type);
    if (symbol->is_local) {
        FOCIRValue address = frame_address(sema, symbol->frame_offset);
        out.address = address;
        out.is_lvalue = true;
        FOCIRInst *load = emit(sema, FOC_OP_LOAD);
        load->lhs = address;
        load->dest = FOC_ir_new_reg(sema->function, ir_type);
        load->type = ir_type;
        out.value.reg = load->dest;
        out.value.type = ir_type;
    } else {
        FOCIRValue address = global_address(sema, symbol->global);
        out.address = address;
        out.is_lvalue = true;
        FOCIRInst *load = emit(sema, FOC_OP_LOAD);
        load->lhs = address;
        load->dest = FOC_ir_new_reg(sema->function, ir_type);
        load->type = ir_type;
        out.value.reg = load->dest;
        out.value.type = ir_type;
    }
    return out;
}

static FOCLowered lower_call(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;

    FOCExpr *callee = expr->call.callee;
    FOCType *func_type = callee->type;
    const char *callee_name = NULL;

    if (callee->kind == FOC_EXPR_IDENT) {
        FOCSymbol *symbol = scope_lookup(sema, callee->ident.name, callee->ident.name_len);
        if (symbol && symbol->kind == FOC_SYM_FUNCTION) {
            callee_name = FOC_strndup(symbol->name, symbol->name_len);
            func_type = symbol->type;
        } else {
            sema_error(sema, callee->span, "E0405", "función desconocida: '%.*s'",
                       (int)callee->ident.name_len, callee->ident.name);
        }
    }

    size_t param_count = (func_type && func_type->kind == FOC_TYPE_FUNCTION) ? func_type->param_count : 0;
    size_t count = expr->call.arg_count;
    if (param_count && count < param_count) {
        sema_error(sema, expr->span, "E0406",
                   "la función espera %zu argumentos pero se han dado %zu", param_count, count);
    }

    /* Los argumentos se evaluan antes de emitir la llamada. */
    FOCIRValue args[8];
    int arg_count = 0;
    for (size_t i = 0; i < count && i < 8; i++) {
        FOCLowered arg = lower_expr(sema, expr->call.args[i]);
        args[i] = arg.value;
        arg_count++;
    }

    FOCIRInst *inst = emit(sema, FOC_OP_CALL);
    inst->symbol = callee_name;
    if (!callee_name) {
        FOCLowered fn = lower_expr(sema, callee);
        inst->indirect = true;
        inst->lhs = fn.value;
    }
    for (int i = 0; i < arg_count; i++) {
        inst->args[i] = args[i];
    }
    inst->arg_count = arg_count;

    FOCIRType ret = FOC_IR_VOID;
    if (func_type && func_type->kind == FOC_TYPE_FUNCTION && func_type->ret) {
        ret = ir_type_of(sema, func_type->ret);
    }
    inst->type = ret;
    inst->variadic = func_type && func_type->kind == FOC_TYPE_FUNCTION && func_type->variadic;
    if (ret != FOC_IR_VOID) {
        inst->dest = FOC_ir_new_reg(sema->function, ret);
    }
    out.value.reg = inst->dest;
    out.value.type = ret;
    out.type = (func_type && func_type->kind == FOC_TYPE_FUNCTION) ? func_type->ret : NULL;
    return out;
}

static FOCType *arith_result_type(FOCType *lhs, FOCType *rhs) {
    if (!lhs) {
        return rhs;
    }
    if (!rhs) {
        return lhs;
    }
    /* Un puntero mas un entero produce el mismo tipo de puntero. */
    if (lhs->kind == FOC_TYPE_POINTER) {
        return lhs;
    }
    if (rhs->kind == FOC_TYPE_POINTER) {
        return rhs;
    }
    if (type_is_float(lhs) || type_is_float(rhs)) {
        return FOC_type_basic(FOC_TYPE_DOUBLE);
    }
    if (lhs && lhs->kind == FOC_TYPE_LLONG) {
        return lhs;
    }
    if (rhs && rhs->kind == FOC_TYPE_LLONG) {
        return rhs;
    }
    if (lhs && (lhs->kind == FOC_TYPE_LONG || lhs->kind == FOC_TYPE_ULONG)) {
        return lhs;
    }
    if (rhs && (rhs->kind == FOC_TYPE_LONG || rhs->kind == FOC_TYPE_ULONG)) {
        return rhs;
    }
    return FOC_type_basic(FOC_TYPE_INT);
}

static FOCLowered lower_binary(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;

    const char *op = expr->binary.op;
    if (strcmp(op, ",") == 0) {
        lower_expr(sema, expr->binary.lhs);
        FOCLowered rhs = lower_expr(sema, expr->binary.rhs);
        out = rhs;
        return out;
    }
    if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        bool is_and = strcmp(op, "&&") == 0;
        int label_true = FOC_ir_new_label(sema->module);
        int label_false = FOC_ir_new_label(sema->module);
        int label_end = FOC_ir_new_label(sema->module);
        int label_next = FOC_ir_new_label(sema->module);
        FOCIRValue slot = alloca_temp(sema, 4);

        FOCLowered lhs = lower_expr(sema, expr->binary.lhs);
        FOCIRInst *branch = emit(sema, FOC_OP_BRANCH);
        branch->lhs = lhs.value;
        branch->label = is_and ? label_next : label_true;
        branch->label_false = is_and ? label_false : label_next;

        FOCIRInst *next_label = emit(sema, FOC_OP_LABEL);
        next_label->label = label_next;
        FOCLowered rhs = lower_expr(sema, expr->binary.rhs);
        FOCIRInst *branch2 = emit(sema, FOC_OP_BRANCH);
        branch2->lhs = rhs.value;
        branch2->label = is_and ? label_true : label_true;
        branch2->label_false = label_false;

        FOCIRInst *true_label = emit(sema, FOC_OP_LABEL);
        true_label->label = label_true;
        FOCIRInst *store_true = emit(sema, FOC_OP_STORE);
        store_true->lhs = ir_const(FOC_IR_I32, 1);
        store_true->rhs = slot;
        store_true->type = FOC_IR_I32;
        FOCIRInst *jmp = emit(sema, FOC_OP_JMP);
        jmp->label = label_end;

        FOCIRInst *false_label = emit(sema, FOC_OP_LABEL);
        false_label->label = label_false;
        FOCIRInst *store_false = emit(sema, FOC_OP_STORE);
        store_false->lhs = ir_const(FOC_IR_I32, 0);
        store_false->rhs = slot;
        store_false->type = FOC_IR_I32;

        FOCIRInst *end_label = emit(sema, FOC_OP_LABEL);
        end_label->label = label_end;
        FOCIRInst *load = emit(sema, FOC_OP_LOAD);
        load->lhs = slot;
        load->dest = FOC_ir_new_reg(sema->function, FOC_IR_I32);
        load->type = FOC_IR_I32;
        out.value.reg = load->dest;
        out.value.type = FOC_IR_I32;
        out.type = FOC_type_basic(FOC_TYPE_INT);
        return out;
    }

    FOCLowered lhs = lower_expr(sema, expr->binary.lhs);
    FOCLowered rhs = lower_expr(sema, expr->binary.rhs);

    /* Aritmetica de punteros: el desplazamiento se escala por el tamano del
     * tipo apuntado. */
    FOCType *pointer_type = NULL;
    FOCType *integer_type = NULL;
    if (lhs.type && lhs.type->kind == FOC_TYPE_POINTER) {
        pointer_type = lhs.type;
        integer_type = rhs.type;
    } else if (rhs.type && rhs.type->kind == FOC_TYPE_POINTER &&
               strcmp(op, "+") == 0) {
        pointer_type = rhs.type;
        integer_type = lhs.type;
    } else if (lhs.type && rhs.type && lhs.type->kind == FOC_TYPE_POINTER &&
               rhs.type->kind == FOC_TYPE_POINTER && strcmp(op, "-") == 0) {
        /* Diferencia de punteros: el resultado se divide por el tamano. */
        FOCIRType ptr_ir_type = ir_type_of(sema, lhs.type);
        FOCIRInst *sub = emit(sema, FOC_OP_SUB);
        sub->lhs = lhs.value;
        sub->rhs = rhs.value;
        sub->type = ptr_ir_type;
        sub->dest = FOC_ir_new_reg(sema->function, ptr_ir_type);
        FOCIRInst *div = emit(sema, FOC_OP_DIV);
        div->lhs.reg = sub->dest;
        div->lhs.type = ptr_ir_type;
        div->rhs = ir_const(FOC_IR_I64, lhs.type->elem ? lhs.type->elem->size : 1);
        div->type = ptr_ir_type;
        div->dest = FOC_ir_new_reg(sema->function, ptr_ir_type);
        out.value.reg = div->dest;
        out.value.type = ptr_ir_type;
        out.type = FOC_type_basic(FOC_TYPE_LONG);
        return out;
    }
    if (pointer_type && integer_type) {
        int element_size = pointer_type->elem ? pointer_type->elem->size : 1;
        if (element_size != 1) {
            FOCIRType int_ir_type = ir_type_of(sema, integer_type);
            FOCType *scaled_type = FOC_type_basic(FOC_TYPE_LONG);
            FOCIRInst *mul = emit(sema, FOC_OP_MUL);
            if (pointer_type == lhs.type) {
                mul->lhs = rhs.value;
            } else {
                mul->lhs = lhs.value;
            }
            mul->rhs = ir_const(FOC_IR_I64, element_size);
            mul->type = FOC_IR_I64;
            mul->dest = FOC_ir_new_reg(sema->function, FOC_IR_I64);
            /* El valor escalado se reasigna al operando entero. */
            if (pointer_type == lhs.type) {
                rhs.value.reg = mul->dest;
                rhs.value.type = FOC_IR_I64;
            } else {
                lhs.value.reg = mul->dest;
                lhs.value.type = FOC_IR_I64;
            }
            (void)scaled_type;
            (void)int_ir_type;
        }
    }

    bool is_compare = strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || strcmp(op, "<") == 0 ||
                      strcmp(op, "<=") == 0 || strcmp(op, ">") == 0 || strcmp(op, ">=") == 0;
    FOCType *result_type = is_compare ? FOC_type_basic(FOC_TYPE_INT) : arith_result_type(lhs.type, rhs.type);
    bool as_float = type_is_float(result_type);
    FOCIRType ir_type = as_float ? FOC_IR_F64 : ir_type_of(sema, result_type);

    FOCIROp ir_op = FOC_OP_ADD;
    if (strcmp(op, "+") == 0) {
        ir_op = FOC_OP_ADD;
    } else if (strcmp(op, "-") == 0) {
        ir_op = FOC_OP_SUB;
    } else if (strcmp(op, "*") == 0) {
        ir_op = FOC_OP_MUL;
    } else if (strcmp(op, "/") == 0) {
        ir_op = FOC_OP_DIV;
    } else if (strcmp(op, "%") == 0) {
        ir_op = FOC_OP_MOD;
    } else if (strcmp(op, "&") == 0) {
        ir_op = FOC_OP_AND;
    } else if (strcmp(op, "|") == 0) {
        ir_op = FOC_OP_OR;
    } else if (strcmp(op, "^") == 0) {
        ir_op = FOC_OP_XOR;
    } else if (strcmp(op, "<<") == 0) {
        ir_op = FOC_OP_SHL;
    } else if (strcmp(op, ">>") == 0) {
        ir_op = FOC_OP_SHR;
    } else if (strcmp(op, "==") == 0) {
        ir_op = FOC_OP_EQ;
    } else if (strcmp(op, "!=") == 0) {
        ir_op = FOC_OP_NE;
    } else if (strcmp(op, "<") == 0) {
        ir_op = FOC_OP_LT;
    } else if (strcmp(op, "<=") == 0) {
        ir_op = FOC_OP_LE;
    } else if (strcmp(op, ">") == 0) {
        ir_op = FOC_OP_GT;
    } else if (strcmp(op, ">=") == 0) {
        ir_op = FOC_OP_GE;
    } else {
        sema_error(sema, expr->span, "E0407", "operador no soportado: '%s'", op);
    }

    FOCIRInst *inst = emit(sema, ir_op);
    inst->lhs = lhs.value;
    inst->rhs = rhs.value;
    inst->type = ir_type;
    if (is_compare) {
        inst->dest = FOC_ir_new_reg(sema->function, FOC_IR_I32);
        out.value.reg = inst->dest;
        out.value.type = FOC_IR_I32;
        out.type = FOC_type_basic(FOC_TYPE_INT);
    } else {
        inst->dest = FOC_ir_new_reg(sema->function, ir_type);
        out.value.reg = inst->dest;
        out.value.type = ir_type;
        out.type = result_type;
    }
    return out;
}

static FOCLowered lower_assign(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;

    const char *op = expr->assign.op;
    FOCLowered lhs = lower_expr(sema, expr->assign.lhs);
    if (!lhs.is_lvalue) {
        sema_error(sema, expr->assign.lhs->span, "E0408",
                   "no se puede asignar a una expresión que no es un lvalor");
        return out;
    }
    FOCLowered rhs = lower_expr(sema, expr->assign.rhs);
    FOCIRType ir_type = ir_type_of(sema, lhs.type);

    FOCIRValue value = rhs.value;
    if (strcmp(op, "=") != 0) {
        /* Operadores compuestos: calculamos lhs op rhs. */
        FOCIROp ir_op = FOC_OP_ADD;
        if (strcmp(op, "+=") == 0) {
            ir_op = FOC_OP_ADD;
        } else if (strcmp(op, "-=") == 0) {
            ir_op = FOC_OP_SUB;
        } else if (strcmp(op, "*=") == 0) {
            ir_op = FOC_OP_MUL;
        } else if (strcmp(op, "/=") == 0) {
            ir_op = FOC_OP_DIV;
        } else if (strcmp(op, "%=") == 0) {
            ir_op = FOC_OP_MOD;
        } else if (strcmp(op, "&=") == 0) {
            ir_op = FOC_OP_AND;
        } else if (strcmp(op, "|=") == 0) {
            ir_op = FOC_OP_OR;
        } else if (strcmp(op, "^=") == 0) {
            ir_op = FOC_OP_XOR;
        } else if (strcmp(op, "<<=") == 0) {
            ir_op = FOC_OP_SHL;
        } else if (strcmp(op, ">>=") == 0) {
            ir_op = FOC_OP_SHR;
        }
        FOCIRInst *op_inst = emit(sema, ir_op);
        op_inst->lhs = lhs.value;
        op_inst->rhs = rhs.value;
        op_inst->type = ir_type;
        op_inst->dest = FOC_ir_new_reg(sema->function, ir_type);
        value.reg = op_inst->dest;
        value.type = ir_type;
    }

    FOCIRInst *store = emit(sema, FOC_OP_STORE);
    store->lhs = value;
    store->rhs = lhs.address;
    store->type = ir_type;
    out.value = value;
    out.type = lhs.type;
    return out;
}

static FOCLowered lower_unary(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;

    const char *op = expr->unary.op;
    if (strcmp(op, "&") == 0) {
        FOCLowered operand = lower_expr(sema, expr->unary.operand);
        if (!operand.is_lvalue) {
            sema_error(sema, expr->span, "E0409", "no se puede tomar la direccion de esta expresion");
            out.value = ir_const(FOC_IR_PTR, 0);
            out.type = FOC_type_pointer(FOC_type_basic(FOC_TYPE_INT));
            return out;
        }
        out.value = operand.address;
        out.type = FOC_type_pointer(operand.type);
        return out;
    }
    if (strcmp(op, "*") == 0) {
        FOCLowered operand = lower_expr(sema, expr->unary.operand);
        FOCType *type = operand.type && operand.type->kind == FOC_TYPE_POINTER ? operand.type->elem : NULL;
        FOCIRType ir_type = ir_type_of(sema, type);
        FOCIRInst *load = emit(sema, FOC_OP_LOAD);
        load->lhs = operand.value;
        load->dest = FOC_ir_new_reg(sema->function, ir_type);
        load->type = ir_type;
        out.value.reg = load->dest;
        out.value.type = ir_type;
        out.type = type;
        out.is_lvalue = true;
        out.address = operand.value;
        return out;
    }
    if (strcmp(op, "++") == 0 || strcmp(op, "--") == 0) {
        FOCLowered operand = lower_expr(sema, expr->unary.operand);
        if (!operand.is_lvalue) {
            sema_error(sema, expr->span, "E0410", "operando no modificable");
            return out;
        }
        FOCIRType ir_type = ir_type_of(sema, operand.type);
        FOCIRInst *inst = emit(sema, strcmp(op, "++") == 0 ? FOC_OP_ADD : FOC_OP_SUB);
        inst->lhs = operand.value;
        inst->rhs = ir_const(ir_type, 1);
        inst->type = ir_type;
        inst->dest = FOC_ir_new_reg(sema->function, ir_type);
        FOCIRInst *store = emit(sema, FOC_OP_STORE);
        store->lhs.reg = inst->dest;
        store->lhs.type = ir_type;
        store->rhs = operand.address;
        store->type = ir_type;
        out.value.reg = inst->dest;
        out.value.type = ir_type;
        out.type = operand.type;
        if (expr->unary.postfix) {
            out.value = operand.value;
        }
        return out;
    }
    if (strcmp(op, "!") == 0) {
        FOCLowered operand = lower_expr(sema, expr->unary.operand);
        FOCIRInst *inst = emit(sema, FOC_OP_NOT);
        inst->lhs = operand.value;
        inst->type = FOC_IR_I32;
        inst->dest = FOC_ir_new_reg(sema->function, FOC_IR_I32);
        out.value.reg = inst->dest;
        out.value.type = FOC_IR_I32;
        out.type = FOC_type_basic(FOC_TYPE_INT);
        return out;
    }
    if (strcmp(op, "~") == 0) {
        FOCLowered operand = lower_expr(sema, expr->unary.operand);
        FOCIRType ir_type = ir_type_of(sema, operand.type);
        FOCIRInst *inst = emit(sema, FOC_OP_BITNOT);
        inst->lhs = operand.value;
        inst->type = ir_type;
        inst->dest = FOC_ir_new_reg(sema->function, ir_type);
        out.value.reg = inst->dest;
        out.value.type = ir_type;
        out.type = operand.type;
        return out;
    }
    if (strcmp(op, "-") == 0) {
        FOCLowered operand = lower_expr(sema, expr->unary.operand);
        FOCIRType ir_type = ir_type_of(sema, operand.type);
        FOCIRInst *inst = emit(sema, FOC_OP_NEG);
        inst->lhs = operand.value;
        inst->type = ir_type;
        inst->dest = FOC_ir_new_reg(sema->function, ir_type);
        out.value.reg = inst->dest;
        out.value.type = ir_type;
        out.type = operand.type;
        return out;
    }
    if (strcmp(op, "+") == 0) {
        return lower_expr(sema, expr->unary.operand);
    }

    sema_error(sema, expr->span, "E0411", "operador unario no soportado: '%s'", op);
    return out;
}

static FOCField *find_field(FOCType *type, const char *name, size_t len) {
    if (!type) {
        return NULL;
    }
    for (size_t i = 0; i < type->field_count; i++) {
        if (type->fields[i].name_len == len &&
            memcmp(type->fields[i].name, name, len) == 0) {
            return &type->fields[i];
        }
    }
    return NULL;
}

static FOCLowered lower_member(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;

    FOCLowered base = lower_expr(sema, expr->member.base);
    FOCType *type = base.type;
    if (expr->member.arrow) {
        if (!type || type->kind != FOC_TYPE_POINTER) {
            sema_error(sema, expr->span, "E0412", "operador '->' sobre un valor que no es puntero");
            return out;
        }
        type = type->elem;
    }
    if (!type || (type->kind != FOC_TYPE_STRUCT && type->kind != FOC_TYPE_UNION)) {
        sema_error(sema, expr->span, "E0413", "acceso a miembro de un tipo que no es struct o union");
        return out;
    }
    FOCField *field = find_field(type, expr->member.name, expr->member.name_len);
    if (!field) {
        sema_error(sema, expr->member.name_span, "E0414", "miembro desconocido: '%s'",
                   expr->member.name);
        return out;
    }
    FOCIRType ir_type = ir_type_of(sema, field->type);
    FOCIRValue address;
    if (expr->member.arrow) {
        FOCIRInst *inst = emit(sema, FOC_OP_FIELD_ADDR);
        inst->lhs = base.value;
        inst->imm = field->offset;
        inst->dest = FOC_ir_new_reg(sema->function, FOC_IR_PTR);
        inst->type = FOC_IR_PTR;
        address.reg = inst->dest;
        address.type = FOC_IR_PTR;
        address.imm = 0;
    } else {
        FOCIRInst *inst = emit(sema, FOC_OP_FIELD_ADDR);
        inst->lhs = base.address;
        inst->imm = field->offset;
        inst->dest = FOC_ir_new_reg(sema->function, FOC_IR_PTR);
        inst->type = FOC_IR_PTR;
        address.reg = inst->dest;
        address.type = FOC_IR_PTR;
        address.imm = 0;
    }
    FOCIRInst *load = emit(sema, FOC_OP_LOAD);
    load->lhs = address;
    load->dest = FOC_ir_new_reg(sema->function, ir_type);
    load->type = ir_type;
    out.value.reg = load->dest;
    out.value.type = ir_type;
    out.address = address;
    out.is_lvalue = true;
    out.type = field->type;
    return out;
}

static FOCLowered lower_index(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;

    FOCLowered base = lower_expr(sema, expr->index.base);
    FOCLowered index = lower_expr(sema, expr->index.index);
    FOCType *elem = NULL;
    if (base.type && base.type->kind == FOC_TYPE_ARRAY) {
        elem = base.type->elem;
    } else if (base.type && base.type->kind == FOC_TYPE_POINTER) {
        elem = base.type->elem;
    } else {
        sema_error(sema, expr->span, "E0415", "no se puede indexar un valor de este tipo");
        return out;
    }
    int elem_size = elem ? elem->size : 1;
    FOCIRInst *inst = emit(sema, FOC_OP_GETELEM);
    /* El indice se aplica sobre el puntero: en arrays decae a la direccion
     * del primer elemento; en punteros sobre el valor cargado. */
    inst->lhs = base.value;
    inst->rhs = index.value;
    inst->imm = elem_size;
    inst->dest = FOC_ir_new_reg(sema->function, FOC_IR_PTR);
    inst->type = FOC_IR_PTR;
    FOCIRValue address;
    address.reg = inst->dest;
    address.type = FOC_IR_PTR;
    address.imm = 0;

    FOCIRType ir_type = ir_type_of(sema, elem);
    FOCIRInst *load = emit(sema, FOC_OP_LOAD);
    load->lhs = address;
    load->dest = FOC_ir_new_reg(sema->function, ir_type);
    load->type = ir_type;
    out.value.reg = load->dest;
    out.value.type = ir_type;
    out.address = address;
    out.is_lvalue = true;
    out.type = elem;
    return out;
}

static FOCLowered lower_cast(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;

    FOCType *target = resolve_declarator_type(sema, expr->cast.type_name->spec,
                                              expr->cast.type_name->declarator);
    FOCLowered operand = lower_expr(sema, expr->cast.expr);
    FOCIRType from = ir_type_of(sema, operand.type);
    FOCIRType to = ir_type_of(sema, target);
    if (from == to) {
        out.value = operand.value;
        out.type = target;
        return out;
    }
    FOCIRInst *inst = emit(sema, FOC_OP_CAST);
    inst->lhs = operand.value;
    inst->type = to;
    inst->dest = FOC_ir_new_reg(sema->function, to);
    out.value.reg = inst->dest;
    out.value.type = to;
    out.type = target;
    return out;
}

static FOCLowered lower_cond(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;

    int label_else = FOC_ir_new_label(sema->module);
    int label_then = FOC_ir_new_label(sema->module);
    int label_end = FOC_ir_new_label(sema->module);
    FOCIRValue slot = alloca_temp(sema, 8);

    FOCLowered cond = lower_expr(sema, expr->cond.cond);
    FOCIRInst *branch = emit(sema, FOC_OP_BRANCH);
    branch->lhs = cond.value;
    branch->label = label_then;
    branch->label_false = label_else;

    FOCIRInst *then_label = emit(sema, FOC_OP_LABEL);
    then_label->label = label_then;
    FOCLowered then_expr = lower_expr(sema, expr->cond.then_expr);
    FOCIRInst *store_then = emit(sema, FOC_OP_STORE);
    store_then->lhs = then_expr.value;
    store_then->rhs = slot;
    store_then->type = ir_type_of(sema, then_expr.type);
    FOCIRInst *jmp = emit(sema, FOC_OP_JMP);
    jmp->label = label_end;

    FOCIRInst *else_label = emit(sema, FOC_OP_LABEL);
    else_label->label = label_else;
    FOCLowered else_expr = lower_expr(sema, expr->cond.else_expr);
    FOCIRInst *store_else = emit(sema, FOC_OP_STORE);
    store_else->lhs = else_expr.value;
    store_else->rhs = slot;
    store_else->type = ir_type_of(sema, else_expr.type);

    FOCIRInst *end_label = emit(sema, FOC_OP_LABEL);
    end_label->label = label_end;
    FOCIRType ir_type = ir_type_of(sema, then_expr.type);
    FOCIRInst *load = emit(sema, FOC_OP_LOAD);
    load->lhs = slot;
    load->dest = FOC_ir_new_reg(sema->function, ir_type);
    load->type = ir_type;
    out.value.reg = load->dest;
    out.value.type = ir_type;
    out.type = then_expr.type;
    return out;
}

static FOCLowered lower_expr(FOCSema *sema, FOCExpr *expr) {
    FOCLowered out;
    memset(&out, 0, sizeof(out));
    out.value.reg = -1;
    if (!expr) {
        out.value = ir_const(FOC_IR_I32, 0);
        out.type = FOC_type_basic(FOC_TYPE_INT);
        return out;
    }
    switch (expr->kind) {
    case FOC_EXPR_INT: {
        FOCType *type = FOC_type_basic(expr->int_lit.is_unsigned ? FOC_TYPE_UINT : FOC_TYPE_INT);
        out.type = type;
        out.value = ir_const(FOC_IR_I32, expr->int_lit.value);
        return out;
    }
    case FOC_EXPR_CHAR: {
        out.type = FOC_type_basic(FOC_TYPE_INT);
        out.value = ir_const(FOC_IR_I32, expr->char_lit.value);
        return out;
    }
    case FOC_EXPR_FLOAT: {
        out.type = FOC_type_basic(FOC_TYPE_DOUBLE);
        out.value.type = FOC_IR_F64;
        out.value.reg = -1;
        out.value.imm = 0;
        FOCIRInst *inst = emit(sema, FOC_OP_CONST_FLOAT);
        inst->fimm = expr->float_lit.value;
        inst->type = FOC_IR_F64;
        inst->dest = FOC_ir_new_reg(sema->function, FOC_IR_F64);
        out.value.reg = inst->dest;
        return out;
    }
    case FOC_EXPR_STRING: {
        FOCIRGlobal *global = make_string_global(sema, expr->string_lit.value, expr->string_lit.value_len);
        out.type = FOC_type_pointer(FOC_type_basic(FOC_TYPE_CHAR));
        out.value = global_address(sema, global);
        out.address = out.value;
        out.is_lvalue = true;
        return out;
    }
    case FOC_EXPR_IDENT:
        return lower_ident(sema, expr);
    case FOC_EXPR_PAREN:
        return lower_expr(sema, expr->paren.expr);
    case FOC_EXPR_UNARY:
        return lower_unary(sema, expr);
    case FOC_EXPR_BINARY:
        return lower_binary(sema, expr);
    case FOC_EXPR_ASSIGN:
        return lower_assign(sema, expr);
    case FOC_EXPR_COND:
        return lower_cond(sema, expr);
    case FOC_EXPR_CALL:
        return lower_call(sema, expr);
    case FOC_EXPR_MEMBER:
        return lower_member(sema, expr);
    case FOC_EXPR_INDEX:
        return lower_index(sema, expr);
    case FOC_EXPR_CAST:
        return lower_cast(sema, expr);
    case FOC_EXPR_SIZEOF: {
        FOCType *type = NULL;
        if (expr->sizeof_expr.type_name) {
            type = resolve_declarator_type(sema, expr->sizeof_expr.type_name->spec,
                                           expr->sizeof_expr.type_name->declarator);
        } else {
            FOCLowered operand = lower_expr(sema, expr->sizeof_expr.expr);
            type = operand.type;
        }
        out.type = FOC_type_basic(FOC_TYPE_ULONG);
        out.value = ir_const(FOC_IR_I64, type ? type->size : 0);
        return out;
    }
    default:
        sema_error(sema, expr->span, "E0416", "expresion no soportada todavia");
        out.value = ir_const(FOC_IR_I32, 0);
        out.type = FOC_type_basic(FOC_TYPE_INT);
        return out;
    }
}

/* ------------------------------------------------------------------ */
/* Sentencias                                                          */
/* ------------------------------------------------------------------ */

static void lower_block(FOCSema *sema, FOCStmt *stmt) {
    scope_push(sema);
    for (size_t i = 0; i < stmt->block.count; i++) {
        lower_stmt(sema, stmt->block.items[i]);
    }
    scope_pop(sema);
}

static void lower_local_decl(FOCSema *sema, FOCDecl *decl) {
    FOCType *type = resolve_declarator_type(sema, decl->spec, decl->declarator);
    if (!decl->declarator || !decl->declarator->has_name) {
        return;
    }
    FOCSymbol *symbol = symbol_new(decl->declarator->name, decl->declarator->name_len,
                                   FOC_SYM_VARIABLE);
    if (!symbol) {
        return;
    }
    symbol->type = type;
    symbol->is_local = true;
    symbol->span = decl->declarator->name_span;
    if (type && type->kind == FOC_TYPE_FUNCTION) {
        symbol->kind = FOC_SYM_FUNCTION;
    }

    /* Reserva espacio en el marco de pila. */
    int size = type ? type->size : 0;
    if (size < 1) {
        size = 1;
    }
    int align = type ? type->align : 1;
    if (align < 1) {
        align = 1;
    }
    sema->frame_offset += size;
    if (sema->frame_offset % align) {
        sema->frame_offset += align - (sema->frame_offset % align);
    }
    symbol->frame_offset = -sema->frame_offset;
    if (-sema->frame_offset > sema->max_frame) {
        sema->max_frame = -sema->frame_offset;
    }
    scope_define(sema, symbol);

    if (decl->init) {
        FOCLowered init = lower_expr(sema, decl->init);
        /* La direccion del hueco se calcula antes de emitir el store. */
        FOCIRValue address = frame_address(sema, symbol->frame_offset);
        FOCIRInst *store = emit(sema, FOC_OP_STORE);
        store->lhs = init.value;
        store->rhs = address;
        store->type = ir_type_of(sema, type);
    }
}

/* Recolecta las sentencias `case` del cuerpo de un switch. */
static void collect_cases(FOCStmt *stmt, FOCStmt **cases, size_t *count, size_t capacity) {
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
    case FOC_STMT_BLOCK:
        for (size_t i = 0; i < stmt->block.count; i++) {
            collect_cases(stmt->block.items[i], cases, count, capacity);
        }
        break;
    case FOC_STMT_CASE:
        if (*count < capacity) {
            cases[(*count)++] = stmt;
        }
        collect_cases(stmt->case_stmt.body, cases, count, capacity);
        break;
    case FOC_STMT_DEFAULT:
        collect_cases(stmt->default_stmt.body, cases, count, capacity);
        break;
    default:
        break;
    }
}

static void lower_stmt(FOCSema *sema, FOCStmt *stmt) {
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
    case FOC_STMT_BLOCK:
        lower_block(sema, stmt);
        break;
    case FOC_STMT_EXPR:
        lower_expr(sema, stmt->expr.expr);
        break;
    case FOC_STMT_DECL:
        lower_local_decl(sema, stmt->decl_stmt.decl);
        break;
    case FOC_STMT_IF: {
        int label_then = FOC_ir_new_label(sema->module);
        int label_else = FOC_ir_new_label(sema->module);
        int label_end = FOC_ir_new_label(sema->module);
        FOCLowered cond = lower_expr(sema, stmt->if_stmt.cond);
        FOCIRInst *branch = emit(sema, FOC_OP_BRANCH);
        branch->lhs = cond.value;
        branch->label = label_then;
        branch->label_false = label_else;
        FOCIRInst *then_label = emit(sema, FOC_OP_LABEL);
        then_label->label = label_then;
        lower_stmt(sema, stmt->if_stmt.then_body);
        if (stmt->if_stmt.else_body) {
            FOCIRInst *jmp = emit(sema, FOC_OP_JMP);
            jmp->label = label_end;
        }
        FOCIRInst *else_label = emit(sema, FOC_OP_LABEL);
        else_label->label = label_else;
        lower_stmt(sema, stmt->if_stmt.else_body);
        FOCIRInst *end_label = emit(sema, FOC_OP_LABEL);
        end_label->label = label_end;
        break;
    }
    case FOC_STMT_WHILE: {
        int label_cond = FOC_ir_new_label(sema->module);
        int label_body = FOC_ir_new_label(sema->module);
        int label_end = FOC_ir_new_label(sema->module);
        FOCIRInst *cond_label = emit(sema, FOC_OP_LABEL);
        cond_label->label = label_cond;
        FOCLowered cond = lower_expr(sema, stmt->while_stmt.cond);
        FOCIRInst *branch = emit(sema, FOC_OP_BRANCH);
        branch->lhs = cond.value;
        branch->label = label_body;
        branch->label_false = label_end;
        FOCIRInst *body_label = emit(sema, FOC_OP_LABEL);
        body_label->label = label_body;
        FOCLoopContext loop;
        loop.parent = sema->loop;
        loop.break_label = label_end;
        loop.continue_label = label_cond;
        sema->loop = &loop;
        lower_stmt(sema, stmt->while_stmt.body);
        sema->loop = loop.parent;
        FOCIRInst *jmp = emit(sema, FOC_OP_JMP);
        jmp->label = label_cond;
        FOCIRInst *end_label = emit(sema, FOC_OP_LABEL);
        end_label->label = label_end;
        break;
    }
    case FOC_STMT_DO: {
        int label_body = FOC_ir_new_label(sema->module);
        int label_cond = FOC_ir_new_label(sema->module);
        int label_end = FOC_ir_new_label(sema->module);
        FOCIRInst *body_label = emit(sema, FOC_OP_LABEL);
        body_label->label = label_body;
        FOCLoopContext loop;
        loop.parent = sema->loop;
        loop.break_label = label_end;
        loop.continue_label = label_cond;
        sema->loop = &loop;
        lower_stmt(sema, stmt->do_stmt.body);
        sema->loop = loop.parent;
        FOCIRInst *cond_label = emit(sema, FOC_OP_LABEL);
        cond_label->label = label_cond;
        FOCLowered cond = lower_expr(sema, stmt->do_stmt.cond);
        FOCIRInst *branch = emit(sema, FOC_OP_BRANCH);
        branch->lhs = cond.value;
        branch->label = label_body;
        branch->label_false = label_end;
        FOCIRInst *end_label = emit(sema, FOC_OP_LABEL);
        end_label->label = label_end;
        break;
    }
    case FOC_STMT_FOR: {
        int label_cond = FOC_ir_new_label(sema->module);
        int label_body = FOC_ir_new_label(sema->module);
        int label_post = FOC_ir_new_label(sema->module);
        int label_end = FOC_ir_new_label(sema->module);
        scope_push(sema);
        if (stmt->for_stmt.init) {
            lower_stmt(sema, stmt->for_stmt.init);
        }
        FOCIRInst *cond_label = emit(sema, FOC_OP_LABEL);
        cond_label->label = label_cond;
        if (stmt->for_stmt.cond) {
            FOCLowered cond = lower_expr(sema, stmt->for_stmt.cond);
            FOCIRInst *branch = emit(sema, FOC_OP_BRANCH);
            branch->lhs = cond.value;
            branch->label = label_body;
            branch->label_false = label_end;
        }
        FOCIRInst *body_label = emit(sema, FOC_OP_LABEL);
        body_label->label = label_body;
        FOCLoopContext loop;
        loop.parent = sema->loop;
        loop.break_label = label_end;
        loop.continue_label = label_post;
        sema->loop = &loop;
        lower_stmt(sema, stmt->for_stmt.body);
        sema->loop = loop.parent;
        FOCIRInst *post_label = emit(sema, FOC_OP_LABEL);
        post_label->label = label_post;
        if (stmt->for_stmt.post) {
            lower_expr(sema, stmt->for_stmt.post);
        }
        FOCIRInst *jmp = emit(sema, FOC_OP_JMP);
        jmp->label = label_cond;
        FOCIRInst *end_label = emit(sema, FOC_OP_LABEL);
        end_label->label = label_end;
        scope_pop(sema);
        break;
    }
    case FOC_STMT_SWITCH: {
        int label_end = FOC_ir_new_label(sema->module);
        int label_default = FOC_ir_new_label(sema->module);
        FOCLowered cond = lower_expr(sema, stmt->switch_stmt.cond);

        size_t capacity = 32;
        FOCStmt **cases = (FOCStmt **)calloc(capacity, sizeof(FOCStmt *));
        size_t case_count = 0;
        if (cases) {
            collect_cases(stmt->switch_stmt.body, cases, &case_count, capacity);
        }

        FOCSwitchContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.parent = sema->switch_ctx;
        ctx.break_label = label_end;
        ctx.cond_type = cond.type;
        ctx.cases = (FOCSwitchCase *)calloc(case_count + 1, sizeof(FOCSwitchCase));
        ctx.case_count = case_count;

        for (size_t i = 0; i < case_count; i++) {
            ctx.cases[i].stmt = cases[i];
            ctx.cases[i].label = FOC_ir_new_label(sema->module);
            ctx.cases[i].compare_label = FOC_ir_new_label(sema->module);
        }
        /* Compara la condicion con cada valor de caso. */
        for (size_t i = 0; i < case_count; i++) {
            FOCIRInst *cmp_label = emit(sema, FOC_OP_LABEL);
            cmp_label->label = ctx.cases[i].compare_label;
            FOCLowered value = lower_expr(sema, cases[i]->case_stmt.value);
            FOCIRInst *cmp = emit(sema, FOC_OP_EQ);
            cmp->lhs = cond.value;
            cmp->rhs = value.value;
            cmp->type = FOC_IR_I32;
            cmp->dest = FOC_ir_new_reg(sema->function, FOC_IR_I32);
            FOCIRInst *branch = emit(sema, FOC_OP_BRANCH);
            branch->lhs.reg = cmp->dest;
            branch->lhs.type = FOC_IR_I32;
            branch->label = ctx.cases[i].label;
            branch->label_false =
                (i + 1 < case_count) ? ctx.cases[i + 1].compare_label : label_default;
        }
        FOCIRInst *jmp_default = emit(sema, FOC_OP_JMP);
        jmp_default->label = label_default;

        sema->switch_ctx = &ctx;
        lower_stmt(sema, stmt->switch_stmt.body);
        sema->switch_ctx = ctx.parent;

        FOCIRInst *default_label = emit(sema, FOC_OP_LABEL);
        default_label->label = label_default;
        FOCIRInst *end_label = emit(sema, FOC_OP_LABEL);
        end_label->label = label_end;
        free(ctx.cases);
        free(cases);
        break;
    }
    case FOC_STMT_CASE: {
        /* Emite la etiqueta del caso antes de su cuerpo. */
        if (sema->switch_ctx) {
            for (size_t i = 0; i < sema->switch_ctx->case_count; i++) {
                if (sema->switch_ctx->cases[i].stmt == stmt) {
                    FOCIRInst *label = emit(sema, FOC_OP_LABEL);
                    label->label = sema->switch_ctx->cases[i].label;
                    break;
                }
            }
        }
        lower_stmt(sema, stmt->case_stmt.body);
        break;
    }
    case FOC_STMT_DEFAULT:
        lower_stmt(sema, stmt->default_stmt.body);
        break;
    case FOC_STMT_BREAK: {
        if (sema->loop) {
            FOCIRInst *jmp = emit(sema, FOC_OP_JMP);
            jmp->label = sema->loop->break_label;
        } else if (sema->switch_ctx) {
            FOCIRInst *jmp = emit(sema, FOC_OP_JMP);
            jmp->label = sema->switch_ctx->break_label;
        } else {
            sema_error(sema, stmt->span, "E0417", "'break' fuera de un bucle o switch");
        }
        break;
    }
    case FOC_STMT_CONTINUE: {
        if (sema->loop) {
            FOCIRInst *jmp = emit(sema, FOC_OP_JMP);
            jmp->label = sema->loop->continue_label;
        } else {
            sema_error(sema, stmt->span, "E0418", "'continue' fuera de un bucle");
        }
        break;
    }
    case FOC_STMT_RETURN: {
        /* El valor se genera antes que la instruccion de retorno. */
        FOCLowered value;
        memset(&value, 0, sizeof(value));
        if (stmt->return_stmt.value) {
            value = lower_expr(sema, stmt->return_stmt.value);
        }
        FOCIRInst *inst = emit(sema, FOC_OP_RET);
        if (stmt->return_stmt.value) {
            inst->lhs = value.value;
            inst->type = ir_type_of(sema, value.type);
            inst->has_value = true;
        } else {
            inst->type = FOC_IR_VOID;
        }
        break;
    }
    case FOC_STMT_GOTO:
        sema_error(sema, stmt->span, "E0419", "'goto' todavia no esta soportado");
        break;
    case FOC_STMT_LABEL:
        lower_stmt(sema, stmt->label_stmt.body);
        break;
    case FOC_STMT_NULL:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Declaraciones globales                                              */
/* ------------------------------------------------------------------ */

static void lower_function(FOCSema *sema, FOCDecl *decl) {
    FOCType *type = resolve_declarator_type(sema, decl->spec, decl->declarator);
    if (!decl->declarator || !decl->declarator->has_name) {
        return;
    }
    const char *name = FOC_strndup(decl->declarator->name, decl->declarator->name_len);

    FOCSymbol *symbol = symbol_new(name, decl->declarator->name_len, FOC_SYM_FUNCTION);
    if (!symbol) {
        return;
    }
    symbol->type = type;
    symbol->is_global = true;
    symbol->is_static = decl->spec->is_static;
    symbol->is_extern = decl->spec->is_extern;
    symbol->span = decl->declarator->name_span;

    /* Una definicion no puede repetir el nombre de otra ya definida. */
    FOCSymbol *previous = scope_lookup_local(sema->scope, name, decl->declarator->name_len);
    if (previous && previous->is_defined && decl->body) {
        sema_error(sema, decl->span, "E0420", "definicion duplicada de '%.*s'",
                   (int)decl->declarator->name_len, name);
        return;
    }
    symbol->is_defined = decl->body != NULL;
    scope_define(sema, symbol);

    if (!decl->body) {
        return;
    }

    FOCIRFunction *function = FOC_ir_function_new(name);
    if (!function) {
        return;
    }
    function->ret_type = type && type->ret ? FOC_ir_type_of(type->ret, sema->pointer_size) : FOC_IR_I32;
    if (type && type->kind == FOC_TYPE_FUNCTION) {
        function->param_count = type->param_count;
        function->variadic = type->variadic;
        if (type->param_count) {
            function->params = (FOCIRParam *)calloc(type->param_count, sizeof(FOCIRParam));
            for (size_t i = 0; i < type->param_count; i++) {
                function->params[i].type = FOC_ir_type_of(type->params[i], sema->pointer_size);
                function->params[i].name = "";
            }
        }
    }

    sema->function = function;
    sema->frame_offset = 0;
    sema->max_frame = 0;
    sema->loop = NULL;
    sema->switch_ctx = NULL;

    scope_push(sema);
    /* Los parametros se allocating como variables locales. */
    if (type && type->kind == FOC_TYPE_FUNCTION) {
        for (size_t i = 0; i < type->param_count; i++) {
            FOCParam *param = NULL;
            /* Se recuperan del declarador. */
            for (size_t c = 0; c < decl->declarator->ctor_count; c++) {
                if (decl->declarator->ctors[c].kind == FOC_CTOR_FUNCTION &&
                    decl->declarator->ctors[c].param_count > i) {
                    param = decl->declarator->ctors[c].params[i];
                    break;
                }
            }
            if (!param || !param->declarator || !param->declarator->has_name) {
                continue;
            }
            FOCSymbol *param_symbol =
                symbol_new(param->declarator->name, param->declarator->name_len,
                           FOC_SYM_VARIABLE);
            if (!param_symbol) {
                continue;
            }
            param_symbol->type = type->params[i];
            param_symbol->is_local = true;
            param_symbol->frame_offset = 16 + (int)(i * 8);
            param_symbol->span = param->declarator->name_span;
            scope_define(sema, param_symbol);
        }
    }

    lower_stmt(sema, decl->body);
    scope_pop(sema);

    function->frame_size = sema->max_frame;
    sema->function = NULL;

    FOCIRFunction **functions =
        (FOCIRFunction **)realloc(sema->module->functions,
                                  (sema->module->function_count + 1) * sizeof(FOCIRFunction *));
    if (functions) {
        sema->module->functions = functions;
        sema->module->functions[sema->module->function_count++] = function;
    }
}

static void lower_global(FOCSema *sema, FOCDecl *decl) {
    FOCType *type = resolve_declarator_type(sema, decl->spec, decl->declarator);
    if (!decl->declarator || !decl->declarator->has_name) {
        return;
    }
    FOCSymbol *symbol = symbol_new(decl->declarator->name, decl->declarator->name_len,
                                   FOC_SYM_VARIABLE);
    if (!symbol) {
        return;
    }
    symbol->type = type;
    symbol->is_global = true;
    symbol->is_static = decl->spec->is_static;
    symbol->is_extern = decl->spec->is_extern;
    symbol->span = decl->declarator->name_span;
    scope_define(sema, symbol);

    /* El nombre del fuente no esta terminado: se copia con su longitud. */
    char *name = FOC_strndup(decl->declarator->name, decl->declarator->name_len);
    FOCIRGlobal *global = FOC_ir_add_global(sema->module, name);
    global->is_static = decl->spec->is_static;
    global->is_extern = decl->spec->is_extern;
    global->size = type ? type->size : 0;
    symbol->global = global;

    if (decl->init) {
        /* Los inicializadores de cadenas se resuelven como relocaciones:
         * la cadena se materializa como global y el puntero apunta a ella. */
        if (decl->init->kind == FOC_EXPR_STRING) {
            FOCIRGlobal *string_global =
                make_string_global(sema, decl->init->string_lit.value,
                                   decl->init->string_lit.value_len);
            global->init_symbol = string_global->name;
            global->has_init = true;
            global->size = sema->pointer_size;
            return;
        }
        FOCLowered init = lower_expr(sema, decl->init);
        if (init.value.reg < 0) {
            global->init_int = init.value.imm;
            global->has_init = true;
        } else if (init.value.type == FOC_IR_F64) {
            global->init_value = init.value;
            global->has_init = true;
        } else {
            sema_error(sema, decl->init->span, "E0421",
                       "el inicializador de una variable global debe ser una constante");
        }
    }
}

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */

FOCIRModule *FOC_irgen(FOCUnit *unit, FOCDiagEngine *diag, FOCSourceMgr *sources,
                        const FOCTarget *target) {
    FOCSema sema;
    memset(&sema, 0, sizeof(sema));
    sema.diag = diag;
    sema.sources = sources;
    sema.module = FOC_ir_module_new();
    sema.pointer_size = FOC_arch_pointer_size(target->arch);
    if (sema.pointer_size == 0) {
        sema.pointer_size = 8;
    }
    scope_push(&sema);

    for (size_t i = 0; i < unit->count; i++) {
        FOCDecl *decl = unit->decls[i];
        if (decl->kind == FOC_DCL_FUNCTION) {
            lower_function(&sema, decl);
        } else {
            lower_global(&sema, decl);
        }
    }
    scope_pop(&sema);
    return sema.module;
}
