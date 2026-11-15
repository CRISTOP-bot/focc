/* stdarg.h —argumentos variables de FOC. */
#ifndef _FOC_STDARG_H
#define _FOC_STDARG_H

typedef char *foc_va_list;
typedef foc_va_list va_list;

#define va_start(ap, last) ((ap) = (foc_va_list) & (last) + sizeof(last))
#define va_arg(ap, type) (*(type *)((ap)))
#define va_end(ap) ((void)0)
#define va_copy(dest, src) ((dest) = (src))

#endif /* _FOC_STDARG_H */
