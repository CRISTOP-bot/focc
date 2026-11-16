/* Sistema de tipos de FOC. */
#ifndef FOC_TYPES_H
#define FOC_TYPES_H

#include "foc/source.h"

typedef enum {
    FOC_TYPE_VOID = 0,
    FOC_TYPE_BOOL,
    FOC_TYPE_CHAR,
    FOC_TYPE_SCHAR,
    FOC_TYPE_UCHAR,
    FOC_TYPE_SHORT,
    FOC_TYPE_USHORT,
    FOC_TYPE_INT,
    FOC_TYPE_UINT,
    FOC_TYPE_LONG,
    FOC_TYPE_ULONG,
    FOC_TYPE_LLONG,
    FOC_TYPE_ULLONG,
    FOC_TYPE_FLOAT,
    FOC_TYPE_DOUBLE,
    FOC_TYPE_LDOUBLE,
    FOC_TYPE_POINTER,
    FOC_TYPE_ARRAY,
    FOC_TYPE_FUNCTION,
    FOC_TYPE_STRUCT,
    FOC_TYPE_UNION,
    FOC_TYPE_ENUM
} FOCTypeKind;

typedef struct FOCType FOCType;

typedef struct {
    const char *name;
    size_t name_len;
    FOCType *type;
    int offset;
    int bit_width; /* 0 si no es bitfield */
    FOCSpan span;
} FOCField;

typedef struct {
    const char *name;
    long long value;
    FOCSpan span;
} FOCEnumValue;

struct FOCType {
    FOCTypeKind kind;
    int id; /* identidad unica para struct/union/enum */
    bool is_const;
    bool is_volatile;
    bool is_restrict;
    int size;
    int align;
    const char *name; /* nombre de la etiqueta */
    FOCType *elem;   /* puntero y array */
    int array_len;   /* -1 si es longitud desconocida */
    FOCType *ret;    /* retorno de funcion */
    FOCType **params;
    size_t param_count;
    bool variadic;
    FOCField *fields;
    size_t field_count;
    bool complete;
    FOCEnumValue *values;
    size_t value_count;
};

FOCType *FOC_type_new(FOCTypeKind kind);
FOCType *FOC_type_basic(FOCTypeKind kind);
FOCType *FOC_type_pointer(FOCType *elem);
FOCType *FOC_type_array(FOCType *elem, int len);
FOCType *FOC_type_function(FOCType *ret, FOCType **params, size_t param_count, bool variadic);
FOCType *FOC_type_qualify(FOCType *base, bool is_const, bool is_volatile, bool is_restrict);

bool FOC_type_is_integer(const FOCType *type);
bool FOC_type_is_float(const FOCType *type);
bool FOC_type_is_arithmetic(const FOCType *type);
bool FOC_type_is_pointer(const FOCType *type);
bool FOC_type_is_array(const FOCType *type);
bool FOC_type_is_function(const FOCType *type);
bool FOC_type_is_scalar(const FOCType *type);
bool FOC_type_is_aggregate(const FOCType *type);
bool FOC_type_is_void(const FOCType *type);
bool FOC_type_is_signed(const FOCType *type);
bool FOC_type_is_unsigned(const FOCType *type);
bool FOC_type_is_char(const FOCType *type);
bool FOC_type_is_bool(const FOCType *type);

/* Comparacion estructural con deteccion de ciclos. */
bool FOC_type_equal(const FOCType *a, const FOCType *b);

/* Tamano y alineacion por defecto segun el tipo y el tamano de puntero. */
int FOC_type_default_size(FOCTypeKind kind, int pointer_size);
int FOC_type_default_align(FOCTypeKind kind, int pointer_size);

const char *FOC_type_kind_name(FOCTypeKind kind);
void FOC_type_print(const FOCType *type, char *out, size_t out_size);

#endif /* FOC_TYPES_H */
