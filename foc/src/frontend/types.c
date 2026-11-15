#include "foc/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_type_ids;

FOCType *FOC_type_new(FOCTypeKind kind) {
    FOCType *type = (FOCType *)calloc(1, sizeof(FOCType));
    if (!type) {
        fprintf(stderr, "foc: agotamiento de memoria\n");
        abort();
    }
    type->kind = kind;
    type->id = ++g_type_ids;
    type->array_len = -1;
    return type;
}

FOCType *FOC_type_basic(FOCTypeKind kind) {
    static FOCType *cache[FOC_TYPE_ENUM + 1];
    if (kind < 0 || kind > FOC_TYPE_ENUM) {
        return NULL;
    }
    if (!cache[kind]) {
        FOCType *type = FOC_type_new(kind);
        /* Los tipos basicos llevan los tamanos por defecto; la plataforma
         * los puede sobrescribir al crear el tipo concreto. */
        int pointer_size = 8;
        type->size = FOC_type_default_size(kind, pointer_size);
        type->align = FOC_type_default_align(kind, pointer_size);
        if (type->size == 0) {
            type->size = 1;
            type->align = 1;
        }
        cache[kind] = type;
    }
    return cache[kind];
}

FOCType *FOC_type_pointer(FOCType *elem) {
    FOCType *type = FOC_type_new(FOC_TYPE_POINTER);
    type->elem = elem;
    type->size = 8; /* se ajusta según el tamaño de puntero del target */
    type->align = 8;
    return type;
}

FOCType *FOC_type_array(FOCType *elem, int len) {
    FOCType *type = FOC_type_new(FOC_TYPE_ARRAY);
    type->elem = elem;
    type->array_len = len;
    if (len >= 0 && elem) {
        type->size = elem->size * len;
        type->align = elem->align;
    }
    return type;
}

FOCType *FOC_type_function(FOCType *ret, FOCType **params, size_t param_count, bool variadic) {
    FOCType *type = FOC_type_new(FOC_TYPE_FUNCTION);
    type->ret = ret;
    type->params = params;
    type->param_count = param_count;
    type->variadic = variadic;
    type->size = 1;
    type->align = 1;
    return type;
}

FOCType *FOC_type_qualify(FOCType *base, bool is_const, bool is_volatile, bool is_restrict) {
    if (!is_const && !is_volatile && !is_restrict) {
        return base;
    }
    if (base->kind == FOC_TYPE_STRUCT || base->kind == FOC_TYPE_UNION ||
        base->kind == FOC_TYPE_ENUM) {
        /* Los calificadores no dividen los tipos etiquetados. */
        return base;
    }
    FOCType *type = FOC_type_new(base->kind);
    type->is_const = is_const;
    type->is_volatile = is_volatile;
    type->is_restrict = is_restrict;
    type->size = base->size;
    type->align = base->align;
    type->elem = base->elem;
    type->ret = base->ret;
    type->params = base->params;
    type->param_count = base->param_count;
    type->variadic = base->variadic;
    type->name = base->name;
    type->complete = base->complete;
    type->fields = base->fields;
    type->field_count = base->field_count;
    type->values = base->values;
    type->value_count = base->value_count;
    type->array_len = base->array_len;
    return type;
}

bool FOC_type_is_integer(const FOCType *type) {
    if (!type) {
        return false;
    }
    switch (type->kind) {
    case FOC_TYPE_BOOL:
    case FOC_TYPE_CHAR:
    case FOC_TYPE_SCHAR:
    case FOC_TYPE_UCHAR:
    case FOC_TYPE_SHORT:
    case FOC_TYPE_USHORT:
    case FOC_TYPE_INT:
    case FOC_TYPE_UINT:
    case FOC_TYPE_LONG:
    case FOC_TYPE_ULONG:
    case FOC_TYPE_LLONG:
    case FOC_TYPE_ULLONG:
    case FOC_TYPE_ENUM:
        return true;
    default:
        return false;
    }
}

bool FOC_type_is_float(const FOCType *type) {
    return type && (type->kind == FOC_TYPE_FLOAT || type->kind == FOC_TYPE_DOUBLE ||
                    type->kind == FOC_TYPE_LDOUBLE);
}

bool FOC_type_is_arithmetic(const FOCType *type) {
    return FOC_type_is_integer(type) || FOC_type_is_float(type);
}

bool FOC_type_is_pointer(const FOCType *type) { return type && type->kind == FOC_TYPE_POINTER; }

bool FOC_type_is_array(const FOCType *type) { return type && type->kind == FOC_TYPE_ARRAY; }

bool FOC_type_is_function(const FOCType *type) { return type && type->kind == FOC_TYPE_FUNCTION; }

bool FOC_type_is_scalar(const FOCType *type) {
    return FOC_type_is_arithmetic(type) || FOC_type_is_pointer(type);
}

bool FOC_type_is_aggregate(const FOCType *type) {
    return type && (type->kind == FOC_TYPE_ARRAY || type->kind == FOC_TYPE_STRUCT ||
                    type->kind == FOC_TYPE_UNION);
}

bool FOC_type_is_void(const FOCType *type) { return type && type->kind == FOC_TYPE_VOID; }

bool FOC_type_is_signed(const FOCType *type) {
    if (!type) {
        return false;
    }
    switch (type->kind) {
    case FOC_TYPE_CHAR:
    case FOC_TYPE_SCHAR:
    case FOC_TYPE_SHORT:
    case FOC_TYPE_INT:
    case FOC_TYPE_LONG:
    case FOC_TYPE_LLONG:
    case FOC_TYPE_ENUM:
        return true;
    default:
        return false;
    }
}

bool FOC_type_is_unsigned(const FOCType *type) {
    if (!type) {
        return false;
    }
    switch (type->kind) {
    case FOC_TYPE_BOOL:
    case FOC_TYPE_UCHAR:
    case FOC_TYPE_USHORT:
    case FOC_TYPE_UINT:
    case FOC_TYPE_ULONG:
    case FOC_TYPE_ULLONG:
        return true;
    default:
        return false;
    }
}

bool FOC_type_is_char(const FOCType *type) {
    return type && (type->kind == FOC_TYPE_CHAR || type->kind == FOC_TYPE_SCHAR ||
                    type->kind == FOC_TYPE_UCHAR);
}

bool FOC_type_is_bool(const FOCType *type) { return type && type->kind == FOC_TYPE_BOOL; }

typedef struct {
    const FOCType *a;
    const FOCType *b;
} FOCTypePair;

bool FOC_type_equal(const FOCType *a, const FOCType *b) {
    /* Pila explícita para evitar recursión infinita en tipos recursivos. */
    FOCTypePair stack[64];
    size_t sp = 0;
    stack[sp++] = (FOCTypePair){a, b};
    while (sp > 0) {
        FOCTypePair pair = stack[--sp];
        const FOCType *x = pair.a;
        const FOCType *y = pair.b;
        if (x == y) {
            continue;
        }
        if (!x || !y) {
            return false;
        }
        if (x->kind != y->kind) {
            return false;
        }
        if (x->is_const != y->is_const || x->is_volatile != y->is_volatile) {
            return false;
        }
        switch (x->kind) {
        case FOC_TYPE_STRUCT:
        case FOC_TYPE_UNION:
        case FOC_TYPE_ENUM:
            if (x->id != y->id) {
                return false;
            }
            break;
        case FOC_TYPE_POINTER:
            if (sp < 64) {
                stack[sp++] = (FOCTypePair){x->elem, y->elem};
            }
            break;
        case FOC_TYPE_ARRAY:
            if (x->array_len != y->array_len) {
                return false;
            }
            if (sp < 64) {
                stack[sp++] = (FOCTypePair){x->elem, y->elem};
            }
            break;
        case FOC_TYPE_FUNCTION:
            if (x->variadic != y->variadic || x->param_count != y->param_count) {
                return false;
            }
            if (sp < 64) {
                stack[sp++] = (FOCTypePair){x->ret, y->ret};
                for (size_t i = 0; i < x->param_count; i++) {
                    stack[sp++] = (FOCTypePair){x->params[i], y->params[i]};
                }
            }
            break;
        default:
            break;
        }
    }
    return true;
}

int FOC_type_default_size(FOCTypeKind kind, int pointer_size) {
    switch (kind) {
    case FOC_TYPE_VOID:
        return 1;
    case FOC_TYPE_BOOL:
    case FOC_TYPE_CHAR:
    case FOC_TYPE_SCHAR:
    case FOC_TYPE_UCHAR:
        return 1;
    case FOC_TYPE_SHORT:
    case FOC_TYPE_USHORT:
        return 2;
    case FOC_TYPE_INT:
    case FOC_TYPE_UINT:
    case FOC_TYPE_FLOAT:
        return 4;
    case FOC_TYPE_LONG:
    case FOC_TYPE_ULONG:
    case FOC_TYPE_LLONG:
    case FOC_TYPE_ULLONG:
    case FOC_TYPE_DOUBLE:
        return 8;
    case FOC_TYPE_LDOUBLE:
        return 16;
    case FOC_TYPE_POINTER:
        return pointer_size ? pointer_size : 8;
    default:
        return 0;
    }
}

int FOC_type_default_align(FOCTypeKind kind, int pointer_size) {
    int size = FOC_type_default_size(kind, pointer_size);
    if (size > 8) {
        return 8;
    }
    return size ? size : 1;
}

const char *FOC_type_kind_name(FOCTypeKind kind) {
    switch (kind) {
    case FOC_TYPE_VOID:
        return "void";
    case FOC_TYPE_BOOL:
        return "_Bool";
    case FOC_TYPE_CHAR:
        return "char";
    case FOC_TYPE_SCHAR:
        return "signed char";
    case FOC_TYPE_UCHAR:
        return "unsigned char";
    case FOC_TYPE_SHORT:
        return "short";
    case FOC_TYPE_USHORT:
        return "unsigned short";
    case FOC_TYPE_INT:
        return "int";
    case FOC_TYPE_UINT:
        return "unsigned int";
    case FOC_TYPE_LONG:
        return "long";
    case FOC_TYPE_ULONG:
        return "unsigned long";
    case FOC_TYPE_LLONG:
        return "long long";
    case FOC_TYPE_ULLONG:
        return "unsigned long long";
    case FOC_TYPE_FLOAT:
        return "float";
    case FOC_TYPE_DOUBLE:
        return "double";
    case FOC_TYPE_LDOUBLE:
        return "long double";
    case FOC_TYPE_POINTER:
        return "pointer";
    case FOC_TYPE_ARRAY:
        return "array";
    case FOC_TYPE_FUNCTION:
        return "function";
    case FOC_TYPE_STRUCT:
        return "struct";
    case FOC_TYPE_UNION:
        return "union";
    case FOC_TYPE_ENUM:
        return "enum";
    default:
        return "desconocido";
    }
}

void FOC_type_print(const FOCType *type, char *out, size_t out_size) {
    if (!type) {
        snprintf(out, out_size, "(null)");
        return;
    }
    if (type->kind == FOC_TYPE_POINTER) {
        char inner[128];
        FOC_type_print(type->elem, inner, sizeof(inner));
        snprintf(out, out_size, "%s *", inner);
        return;
    }
    if (type->kind == FOC_TYPE_ARRAY) {
        char inner[128];
        FOC_type_print(type->elem, inner, sizeof(inner));
        snprintf(out, out_size, "%s[%d]", inner, type->array_len);
        return;
    }
    if (type->kind == FOC_TYPE_FUNCTION) {
        char inner[128];
        FOC_type_print(type->ret, inner, sizeof(inner));
        snprintf(out, out_size, "%s ()", inner);
        return;
    }
    if ((type->kind == FOC_TYPE_STRUCT || type->kind == FOC_TYPE_UNION ||
         type->kind == FOC_TYPE_ENUM) &&
        type->name) {
        snprintf(out, out_size, "%s %s", FOC_type_kind_name(type->kind), type->name);
        return;
    }
    snprintf(out, out_size, "%s", FOC_type_kind_name(type->kind));
}
