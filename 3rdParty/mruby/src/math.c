/*
** math.c - Math module
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include <math.h>

#define domain_error(msg) \
    mrb_raise(mrb, E_RANGE_ERROR, "Numerical argument is out of domain - " #msg);


mrb_value
mrb_assoc_new(mrb_state *mrb, mrb_value car, mrb_value cdr);

/*
  TRIGONOMETRIC FUNCTIONS
*/

/*
 *  call-seq:
 *     Math.sin(x)    -> float
 *
 *  Computes the sine of <i>x</i> (expressed in radians). Returns
 *  -1..1.
 */
static mrb_value
math_sin(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = sin(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.cos(x)    -> float
 *
 *  Computes the cosine of <i>x</i> (expressed in radians). Returns
 *  -1..1.
 */
static mrb_value
math_cos(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = cos(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.tan(x)    -> float
 *
 *  Returns the tangent of <i>x</i> (expressed in radians).
 */
static mrb_value
math_tan(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = tan(x);

  return mrb_float_value(x);
}

/*
  INVERSE TRIGONOMETRIC FUNCTIONS
*/

/*
 *  call-seq:
 *     Math.asin(x)    -> float
 *
 *  Computes the arc sine of <i>x</i>. Returns -{PI/2} .. {PI/2}.
 */
static mrb_value
math_asin(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = asin(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.acos(x)    -> float
 *
 *  Computes the arc cosine of <i>x</i>. Returns 0..PI.
 */
static mrb_value
math_acos(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = acos(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.atan(x)    -> float
 *
 *  Computes the arc tangent of <i>x</i>. Returns -{PI/2} .. {PI/2}.
 */
static mrb_value
math_atan(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = atan(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.atan2(y, x)  -> float
 *
 *  Computes the arc tangent given <i>y</i> and <i>x</i>. Returns
 *  -PI..PI.
 *
 *    Math.atan2(-0.0, -1.0) #=> -3.141592653589793
 *    Math.atan2(-1.0, -1.0) #=> -2.356194490192345
 *    Math.atan2(-1.0, 0.0)  #=> -1.5707963267948966
 *    Math.atan2(-1.0, 1.0)  #=> -0.7853981633974483
 *    Math.atan2(-0.0, 1.0)  #=> -0.0
 *    Math.atan2(0.0, 1.0)   #=> 0.0
 *    Math.atan2(1.0, 1.0)   #=> 0.7853981633974483
 *    Math.atan2(1.0, 0.0)   #=> 1.5707963267948966
 *    Math.atan2(1.0, -1.0)  #=> 2.356194490192345
 *    Math.atan2(0.0, -1.0)  #=> 3.141592653589793
 *
 */
static mrb_value
math_atan2(mrb_state *mrb, mrb_value obj)
{
  mrb_float x, y;

  mrb_get_args(mrb, "ff", &x, &y);
  x = atan2(x, y);

  return mrb_float_value(x);
}



/*
  HYPERBOLIC TRIG FUNCTIONS
*/
/*
 *  call-seq:
 *     Math.sinh(x)    -> float
 *
 *  Computes the hyperbolic sine of <i>x</i> (expressed in
 *  radians).
 */
static mrb_value
math_sinh(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = sinh(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.cosh(x)    -> float
 *
 *  Computes the hyperbolic cosine of <i>x</i> (expressed in radians).
 */
static mrb_value
math_cosh(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = cosh(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.tanh()    -> float
 *
 *  Computes the hyperbolic tangent of <i>x</i> (expressed in
 *  radians).
 */
static mrb_value
math_tanh(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = tanh(x);

  return mrb_float_value(x);
}


/*
  EXPONENTIALS AND LOGARITHMS
*/
#if defined __CYGWIN__
# include <cygwin/version.h>
# if CYGWIN_VERSION_DLL_MAJOR < 1005
#  define nan(x) nan()
# endif
# define log(x) ((x) < 0.0 ? nan("") : log(x))
# define log10(x) ((x) < 0.0 ? nan("") : log10(x))
#endif

#ifndef log2
#ifndef HAVE_LOG2
double
log2(double x)
{
    return log10(x)/log10(2.0);
}
#else
extern double log2(double);
#endif
#endif

/*
 *  call-seq:
 *     Math.exp(x)    -> float
 *
 *  Returns e**x.
 *
 *    Math.exp(0)       #=> 1.0
 *    Math.exp(1)       #=> 2.718281828459045
 *    Math.exp(1.5)     #=> 4.4816890703380645
 *
 */
static mrb_value
math_exp(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = exp(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.log(numeric)    -> float
 *     Math.log(num,base)   -> float
 *
 *  Returns the natural logarithm of <i>numeric</i>.
 *  If additional second argument is given, it will be the base
 *  of logarithm.
 *
 *    Math.log(1)          #=> 0.0
 *    Math.log(Math::E)    #=> 1.0
 *    Math.log(Math::E**3) #=> 3.0
 *    Math.log(12,3)       #=> 2.2618595071429146
 *
 */
static mrb_value
math_log(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = log(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.log2(numeric)    -> float
 *
 *  Returns the base 2 logarithm of <i>numeric</i>.
 *
 *    Math.log2(1)      #=> 0.0
 *    Math.log2(2)      #=> 1.0
 *    Math.log2(32768)  #=> 15.0
 *    Math.log2(65536)  #=> 16.0
 *
 */
static mrb_value
math_log2(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = log2(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.log10(numeric)    -> float
 *
 *  Returns the base 10 logarithm of <i>numeric</i>.
 *
 *    Math.log10(1)       #=> 0.0
 *    Math.log10(10)      #=> 1.0
 *    Math.log10(10**100) #=> 100.0
 *
 */
static mrb_value
math_log10(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = log10(x);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.frexp(numeric)    -> [ fraction, exponent ]
 *
 *  Returns a two-element array containing the normalized fraction (a
 *  <code>Float</code>) and exponent (a <code>Fixnum</code>) of
 *  <i>numeric</i>.
 *
 *     fraction, exponent = Math.frexp(1234)   #=> [0.6025390625, 11]
 *     fraction * 2**exponent                  #=> 1234.0
 */
static mrb_value
math_frexp(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;
  int exp;
  
  mrb_get_args(mrb, "f", &x);
  x = frexp(x, &exp);

  return mrb_assoc_new(mrb, mrb_float_value(x), mrb_fixnum_value(exp));
}

/*
 *  call-seq:
 *     Math.ldexp(flt, int) -> float
 *
 *  Returns the value of <i>flt</i>*(2**<i>int</i>).
 *
 *     fraction, exponent = Math.frexp(1234)
 *     Math.ldexp(fraction, exponent)   #=> 1234.0
 */
static mrb_value
math_ldexp(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;
  mrb_int   i;

  mrb_get_args(mrb, "fi", &x, &i);
  x = ldexp(x, i);

  return mrb_float_value(x);
}

/*
 *  call-seq:
 *     Math.hypot(x, y)    -> float
 *
 *  Returns sqrt(x**2 + y**2), the hypotenuse of a right-angled triangle
 *  with sides <i>x</i> and <i>y</i>.
 *
 *     Math.hypot(3, 4)   #=> 5.0
 */
static mrb_value
math_hypot(mrb_state *mrb, mrb_value obj)
{
  mrb_float x, y;

  mrb_get_args(mrb, "ff", &x, &y);
  x = hypot(x, y);

  return mrb_float_value(x);
}

/* ------------------------------------------------------------------------*/
void
mrb_init_math(mrb_state *mrb)
{
  struct RClass *mrb_math;
  mrb_math = mrb_define_module(mrb, "Math");
  
  #ifdef M_PI
      mrb_define_const(mrb, mrb_math, "PI", mrb_float_value(M_PI));
  #else
      mrb_define_const(mrb, mrb_math, "PI", mrb_float_value(atan(1.0)*4.0));
  #endif
  
  #ifdef M_E
      mrb_define_const(mrb, mrb_math, "E", mrb_float_value(M_E));
  #else
      mrb_define_const(mrb, mrb_math, "E", mrb_float_value(exp(1.0)));
  #endif
  
  mrb_define_class_method(mrb, mrb_math, "sin", math_sin, 1);
  mrb_define_class_method(mrb, mrb_math, "cos", math_cos, 1);
  mrb_define_class_method(mrb, mrb_math, "tan", math_tan, 1);

  mrb_define_class_method(mrb, mrb_math, "asin", math_asin, 1);
  mrb_define_class_method(mrb, mrb_math, "acos", math_acos, 1);
  mrb_define_class_method(mrb, mrb_math, "atan", math_atan, 1);
  mrb_define_class_method(mrb, mrb_math, "atan2", math_atan2, 2);
  
  mrb_define_class_method(mrb, mrb_math, "sinh", math_sinh, 1);
  mrb_define_class_method(mrb, mrb_math, "cosh", math_cosh, 1);
  mrb_define_class_method(mrb, mrb_math, "tanh", math_tanh, 1);

  mrb_define_class_method(mrb, mrb_math, "exp", math_exp, 1);
  mrb_define_class_method(mrb, mrb_math, "log", math_log, -1);
  mrb_define_class_method(mrb, mrb_math, "log2", math_log2, 1);
  mrb_define_class_method(mrb, mrb_math, "log10", math_log10, 1);

  mrb_define_class_method(mrb, mrb_math, "frexp", math_frexp, 1);
  mrb_define_class_method(mrb, mrb_math, "ldexp", math_ldexp, 2);

  mrb_define_class_method(mrb, mrb_math, "hypot", math_hypot, 2);
}
