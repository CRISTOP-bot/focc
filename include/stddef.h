/* stddef.h —cabecera estandar de FOC (freestanding). */
#ifndef _FOC_STDDEF_H
#define _FOC_STDDEF_H

typedef unsigned long size_t;
typedef long ptrdiff_t;
typedef int wchar_t;
typedef long double max_align_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(type, member) ((size_t) & (((type *)0)->member))

#endif /* _FOC_STDDEF_H */
