/*
** mrbconf.h - mruby core configuration
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBYCONF_H
#define MRUBYCONF_H

#include <stdint.h>

/* configuration options: */
/* add -DMRB_USE_FLOAT to use float instead of double for floating point numbers */
//#define MRB_USE_FLOAT

/* represent mrb_value in boxed double; conflict with MRB_USE_FLOAT */
//#define MRB_NAN_BOXING

/* define on big endian machines; used by MRB_NAN_BOXING */
//#define MRB_ENDIAN_BIG

/* argv max size in mrb_funcall */
//#define MRB_FUNCALL_ARGC_MAX 16 

/* number of object per heap page */
//#define MRB_HEAP_PAGE_SIZE 1024

/* use segmented list for IV table */
//#define MRB_USE_IV_SEGLIST

/* initial size for IV khash; ignored when MRB_USE_IV_SEGLIST is set */
//#define MRB_IVHASH_INIT_SIZE 8

/* default size of khash table bucket */
//#define KHASH_DEFAULT_SIZE 32

/* allocated memory address alignment */
//#define POOL_ALIGNMENT 4

/* page size of memory pool */
//#define POOL_PAGE_SIZE 16000

/* -DDISABLE_XXXX to drop the feature */
#define DISABLE_REGEXP	        /* regular expression classes */
//#define DISABLE_SPRINTF	/* Kernel.sprintf method */
//#define DISABLE_MATH		/* Math functions */
//#define DISABLE_TIME		/* Time class */
//#define DISABLE_STRUCT	/* Struct class */
//#define DISABLE_STDIO		/* use of stdio */

#undef  HAVE_UNISTD_H /* WINDOWS */
#define HAVE_UNISTD_H /* LINUX */

/* end of configuration */

#ifdef MRB_USE_FLOAT
typedef float mrb_float;
#else
typedef double mrb_float;
#endif
#define readfloat(p) (mrb_float)strtod((p),NULL)

#ifdef MRB_NAN_BOXING
typedef int32_t mrb_int;
#else
typedef int mrb_int;
#endif
typedef short mrb_sym;

/* define ENABLE_XXXX from DISABLE_XXX */
#ifndef DISABLE_REGEXP
#define ENABLE_REGEXP
#endif
#ifndef DISABLE_SPRINTF
#define ENABLE_SPRINTF
#endif
#ifndef DISABLE_MATH
#define ENABLE_MATH
#endif
#ifndef DISABLE_TIME
#define ENABLE_TIME
#endif
#ifndef DISABLE_STRUCT
#define ENABLE_STRUCT
#endif
#ifndef DISABLE_STDIO
#define ENABLE_STDIO
#endif

#ifndef FALSE
# define FALSE 0
#endif

#ifndef TRUE
# define TRUE 1
#endif

#ifdef _MSC_VER
# define inline __inline
# define snprintf _snprintf
# define isnan _isnan
# define isinf(n) (!_finite(n) && !_isnan(n))
#endif

#endif  /* MRUBYCONF_H */
