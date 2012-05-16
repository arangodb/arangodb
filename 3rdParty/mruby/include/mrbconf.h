/*
** mrbconf.h - mruby core configuration
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBYCONF_H
#define MRUBYCONF_H

#include <stdint.h>
#undef MRB_USE_FLOAT

#ifdef MRB_USE_FLOAT
typedef float mrb_float;
#else
typedef double mrb_float;
#endif
#define readfloat(p) (mrb_float)strtod((p),NULL)

typedef int mrb_int;
typedef intptr_t mrb_sym;
#define readint(p,base) strtol((p),NULL,(base))

#undef  INCLUDE_ENCODING   /* not use encoding classes (ascii only) */
#define INCLUDE_ENCODING   /* use UTF-8 encoding classes */

#undef  INCLUDE_REGEXP     /* not use regular expression classes */
#define INCLUDE_REGEXP     /* use regular expression classes */

#ifdef  INCLUDE_REGEXP
# define INCLUDE_ENCODING  /* Regexp depends Encoding */
#endif

#undef  HAVE_UNISTD_H /* WINDOWS */
#define HAVE_UNISTD_H /* LINUX */

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
