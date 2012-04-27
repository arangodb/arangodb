/*
** numeric.c - Numeric, Integer, Float, Fixnum class
** 
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/numeric.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include <string.h>
#include "mruby/class.h"
#include "mruby/variable.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>

#ifdef INCLUDE_REGEXP
#include "encoding.h"
#endif

#if defined(__FreeBSD__) && __FreeBSD__ < 4
#include <floatingpoint.h>
#endif

#ifdef HAVE_FLOAT_H
#include <float.h>
#endif

#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif

#ifndef mrb_usascii_str_new2
  #ifdef INCLUDE_REGEXP
    #define mrb_usascii_str_new2 mrb_usascii_str_new_cstr
  #else
    #define mrb_usascii_str_new2 mrb_str_new_cstr
  #endif
#endif
#ifndef mrb_usascii_str_new2
  #ifdef INCLUDE_REGEXP
  #else
    #define mrb_usascii_str_new  mrb_str_new
  #endif
#endif

/* use IEEE 64bit values if not defined */
#ifndef FLT_RADIX
#define FLT_RADIX 2
#endif
#ifndef FLT_ROUNDS
#define FLT_ROUNDS 1
#endif
#ifndef DBL_MIN
#define DBL_MIN 2.2250738585072014e-308
#endif
#ifndef DBL_MAX
#define DBL_MAX 1.7976931348623157e+308
#endif
#ifndef DBL_MIN_EXP
#define DBL_MIN_EXP (-1021)
#endif
#ifndef DBL_MAX_EXP
#define DBL_MAX_EXP 1024
#endif
#ifndef DBL_MIN_10_EXP
#define DBL_MIN_10_EXP (-307)
#endif
#ifndef DBL_MAX_10_EXP
#define DBL_MAX_10_EXP 308
#endif
#ifndef DBL_DIG
#define DBL_DIG 15
#endif
#ifndef DBL_MANT_DIG
#define DBL_MANT_DIG 53
#endif
#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-16
#endif

#define mrb_rational_raw1(x) mrb_rational_raw(x, INT2FIX(1))

typedef uintptr_t VALUE;
typedef uintptr_t ID;
#define SIGNED_VALUE intptr_t

#ifdef HAVE_INFINITY
#elif BYTE_ORDER == LITTLE_ENDIAN
const unsigned char mrb_infinity[] = "\x00\x00\x80\x7f";
#else
const unsigned char mrb_infinity[] = "\x7f\x80\x00\x00";
#endif

#ifdef HAVE_NAN
#elif BYTE_ORDER == LITTLE_ENDIAN
const unsigned char mrb_nan[] = "\x00\x00\xc0\x7f";
#else
const unsigned char mrb_nan[] = "\x7f\xc0\x00\x00";
#endif

#ifdef MRB_USE_FLOAT
#define round(f) roundf(f)
#define floor(f) floorf(f)
#define ceil(f) ceilf(f)
#define floor(f) floorf(f)
#define fmod(x,y) fmodf(x,y)
#endif

void mrb_cmperr(mrb_state *mrb, mrb_value x, mrb_value y);

void
mrb_num_zerodiv(mrb_state *mrb)
{
    mrb_raise(mrb, E_ZERODIVISION_ERROR, "divided by 0");
}


/*
 *  call-seq:
 *     num.coerce(numeric)  ->  array
 *
 *  If <i>aNumeric</i> is the same type as <i>num</i>, returns an array
 *  containing <i>aNumeric</i> and <i>num</i>. Otherwise, returns an
 *  array with both <i>aNumeric</i> and <i>num</i> represented as
 *  <code>Float</code> objects. This coercion mechanism is used by
 *  Ruby to handle mixed-type numeric operations: it is intended to
 *  find a compatible common type between the two operands of the operator.
 *
 *     1.coerce(2.5)   #=> [2.5, 1.0]
 *     1.2.coerce(3)   #=> [3.0, 1.2]
 *     1.coerce(2)     #=> [2, 1]
 */

static mrb_value
num_coerce(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_get_args(mrb, "o", &y);

    //if (CLASS_OF(x) == CLASS_OF(y))
    if (mrb_class(mrb, x) == mrb_class(mrb, y))
        return mrb_assoc_new(mrb, y, x);
    x = mrb_Float(mrb, x);
    y = mrb_Float(mrb, y);
    return mrb_assoc_new(mrb, y, x);
}

static mrb_value
coerce_body(mrb_state *mrb, mrb_value *x)
{
    return mrb_funcall(mrb, x[1], "coerce", 1, x[0]);
}

static mrb_value
coerce_rescue(mrb_state *mrb, mrb_value *x)
{
    volatile mrb_value v = mrb_inspect(mrb, x[1]);

    mrb_raise(mrb, E_TYPE_ERROR, "%s can't be coerced into %s",
             mrb_special_const_p(x[1])?
             RSTRING_PTR(v):
             mrb_obj_classname(mrb, x[1]),
             mrb_obj_classname(mrb, x[0]));
    return mrb_nil_value();                /* dummy */
}

static int
do_coerce(mrb_state *mrb, mrb_value *x, mrb_value *y, int err)
{
    mrb_value ary;
    mrb_value a[2];

    a[0] = *x; a[1] = *y;

    ary = coerce_body(mrb, a);
    if (mrb_type(ary) != MRB_TT_ARRAY || RARRAY_LEN(ary) != 2) {
        if (err) {
            mrb_raise(mrb, E_TYPE_ERROR, "coerce must return [x, y]");
        }
        return FALSE;
    }

    *x = RARRAY_PTR(ary)[0];
    *y = RARRAY_PTR(ary)[1];
    return TRUE;
}

mrb_value
mrb_num_coerce_bin(mrb_state *mrb, mrb_value x, mrb_value y, char* func)
{
    do_coerce(mrb, &x, &y, TRUE);
    return mrb_funcall(mrb, x, func, 1, y);
}

mrb_value
mrb_num_coerce_cmp(mrb_state *mrb, mrb_value x, mrb_value y, char* func)
{
    if (do_coerce(mrb, &x, &y, FALSE))
        return mrb_funcall(mrb, x, func, 1, y);
    return mrb_nil_value();
}

mrb_value
mrb_num_coerce_relop(mrb_state *mrb, mrb_value x, mrb_value y, char* func)
{
    mrb_value c, x0 = x, y0 = y;

    if (!do_coerce(mrb, &x, &y, FALSE) ||
        mrb_nil_p(c = mrb_funcall(mrb, x, func, 1, y))) {
        mrb_cmperr(mrb, x0, y0);
        return mrb_nil_value();                /* not reached */
    }
    return c;
}

/*
 *  call-seq:
 *     +num  ->  num
 *
 *  Unary Plus---Returns the receiver's value.
 */

static mrb_value
num_uplus(mrb_state *mrb, mrb_value num)
{
    return num;
}

/*
 *  call-seq:
 *     -num  ->  numeric
 *
 *  Unary Minus---Returns the receiver's value, negated.
 */

static mrb_value
num_uminus(mrb_state *mrb, mrb_value num)
{
    mrb_value zero;

    zero = mrb_fixnum_value(0);
    do_coerce(mrb, &zero, &num, TRUE);

    return mrb_funcall(mrb, zero, "-", 1, num);
}

/*
 *  call-seq:
 *     num.quo(numeric)  ->  real
 *
 *  Returns most exact division (rational for integers, float for floats).
 */
static mrb_value
num_quo(mrb_state *mrb, mrb_value x)
{
    mrb_value y;

    mrb_get_args(mrb, "o", &y);
    return mrb_funcall(mrb, mrb_float_value((mrb_float)mrb_fixnum(x)), "/", 1, y);
}

/*
 *  call-seq:
 *     num.abs        ->  numeric
 *     num.magnitude  ->  numeric
 *
 *  Returns the absolute value of <i>num</i>.
 *
 *     12.abs         #=> 12
 *     (-34.56).abs   #=> 34.56
 *     -34.56.abs     #=> 34.56
 */

static mrb_value
num_abs(mrb_state *mrb, mrb_value num)
{
    if (mrb_test(mrb_funcall(mrb, num, "<", 1, mrb_fixnum_value(0)))) {
        return mrb_funcall(mrb, num, "-@", 0);
    }
    return num;
}

/********************************************************************
 *
 * Document-class: Float
 *
 *  <code>Float</code> objects represent inexact real numbers using
 *  the native architecture's double-precision floating point
 *  representation.
 */

mrb_value
mrb_float_new(mrb_float d)
{
  //NEWOBJ(flt, struct RFloat);
  //OBJSETUP(flt, mrb_cFloat, MRB_TT_FLOAT);

  //flt->float_value = d;
  //return (mrb_value)flt;
  return mrb_float_value(d);
}

/* 15.2.9.3.16(x) */
/*
 *  call-seq:
 *     flt.to_s  ->  string
 *
 *  Returns a string containing a representation of self. As well as a
 *  fixed or exponential form of the number, the call may return
 *  ``<code>NaN</code>'', ``<code>Infinity</code>'', and
 *  ``<code>-Infinity</code>''.
 */

static mrb_value
flo_to_s(mrb_state *mrb, mrb_value flt)
{
    char buf[32];
    mrb_float value = mrb_float(flt);
    char *p, *e;

    if (isinf(value))
      return mrb_str_new2(mrb, value < 0 ? "-Infinity" : "Infinity");
    else if(isnan(value))
      return mrb_str_new2(mrb, "NaN");

    sprintf(buf, "%#.15g", value); /* ensure to print decimal point */
    if (!(e = strchr(buf, 'e'))) {
      e = buf + strlen(buf);
    }
    if (!ISDIGIT(e[-1])) { /* reformat if ended with decimal point (ex 111111111111111.) */
      sprintf(buf, "%#.14e", value);
      if (!(e = strchr(buf, 'e'))) {
        e = buf + strlen(buf);
      }
    }
    p = e;
    while (p[-1]=='0' && ISDIGIT(p[-2]))
      p--;
    memmove(p, e, strlen(e)+1);
    return mrb_str_new2(mrb, buf);
}

/* 15.2.9.3.2  */
/*
 * call-seq:
 *   float - other  ->  float
 *
 * Returns a new float which is the difference of <code>float</code>
 * and <code>other</code>.
 */

static mrb_value
flo_minus(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);

    switch (mrb_type(y)) {
      case MRB_TT_FIXNUM:
        return mrb_float_value(mrb_float(x) - (mrb_float)mrb_fixnum(y));
      case MRB_TT_FLOAT:
        return mrb_float_value(mrb_float(x) - mrb_float(y));
      default:
        return mrb_num_coerce_bin(mrb, x, y, "-");
    }
}

/* 15.2.9.3.3  */
/*
 * call-seq:
 *   float * other  ->  float
 *
 * Returns a new float which is the product of <code>float</code>
 * and <code>other</code>.
 */

static mrb_value
flo_mul(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);

    switch (mrb_type(y)) {
      case MRB_TT_FIXNUM:
        return mrb_float_value(mrb_float(x) * (mrb_float)mrb_fixnum(y));
      case MRB_TT_FLOAT:
        return mrb_float_value(mrb_float(x) * mrb_float(y));
      default:
        return mrb_num_coerce_bin(mrb, x, y, "*");
    }
}

/* 15.2.9.3.4  */
/*
 * call-seq:
 *   float / other  ->  float
 *
 * Returns a new float which is the result of dividing
 * <code>float</code> by <code>other</code>.
 */

static mrb_value
flo_div(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  long f_y;

  mrb_get_args(mrb, "o", &y);

  switch (mrb_type(y)) {
    case MRB_TT_FIXNUM:
      f_y = mrb_fixnum(y);
      return mrb_float_value(mrb_float(x) / (mrb_float)f_y);
    case MRB_TT_FLOAT:
      return mrb_float_value(mrb_float(x) / mrb_float(y));
    default:
      return mrb_num_coerce_bin(mrb, x, y, "/");
  }
}

/*
 *  call-seq:
 *     float.quo(numeric)  ->  float
 *
 *  Returns float / numeric.
 */
static mrb_value
flo_quo(mrb_state *mrb, mrb_value x)
{
    mrb_value y;

    mrb_get_args(mrb, "o", &y);
    return mrb_funcall(mrb, x, "/", 1, y);
}

static void
flodivmod(mrb_state *mrb, mrb_float x, mrb_float y, mrb_float *divp, mrb_float *modp)
{
    mrb_float div, mod;

    if (y == 0.0) mrb_num_zerodiv(mrb);
    mod = fmod(x, y);
    if (isinf(x) && !isinf(y) && !isnan(y))
        div = x;
    else
        div = (x - mod) / y;
    if (y*mod < 0) {
        mod += y;
        div -= 1.0;
    }
    if (modp) *modp = mod;
    if (divp) *divp = div;
}

/* 15.2.9.3.5  */
/*
 *  call-seq:
 *     flt % other        ->  float
 *     flt.modulo(other)  ->  float
 *
 *  Return the modulo after division of <code>flt</code> by <code>other</code>.
 *
 *     6543.21.modulo(137)      #=> 104.21
 *     6543.21.modulo(137.24)   #=> 92.9299999999996
 */

static mrb_value
flo_mod(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_float fy, mod;
  mrb_get_args(mrb, "o", &y);

    switch (mrb_type(y)) {
      case MRB_TT_FIXNUM:
        fy = (mrb_float)mrb_fixnum(y);
        break;
      case MRB_TT_FLOAT:
        fy = mrb_float(y);
        break;
      default:
        return mrb_num_coerce_bin(mrb, x, y, "%");
    }
    flodivmod(mrb, mrb_float(x), fy, 0, &mod);
    return mrb_float_value(mod);
}

static mrb_value
flt2ival(mrb_float d)
{
    if (FIXABLE(d)) {
        d = round(d);
        return mrb_fixnum_value((long)d);
    }
    return mrb_nil_value();
}


/* 15.2.8.3.16 */
/*
 *  call-seq:
 *     num.eql?(numeric)  ->  true or false
 *
 *  Returns <code>true</code> if <i>num</i> and <i>numeric</i> are the
 *  same type and have equal values.
 *
 *     1 == 1.0          #=> true
 *     1.eql?(1.0)       #=> false
 *     (1.0).eql?(1.0)   #=> true
 */
static mrb_value
num_eql(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_get_args(mrb, "o", &y);
  if (mrb_type(x) != mrb_type(y)) return mrb_false_value();
  if (mrb_equal(mrb, x, y)) {
      return mrb_true_value();
  }
  else {
      return mrb_false_value();
  }
}

static mrb_value
num_equal(mrb_state *mrb, mrb_value x, mrb_value y)
{
    if (mrb_obj_equal(mrb, x, y)) return mrb_true_value();
    return mrb_funcall(mrb, y, "==", 1, x);
}

/* 15.2.9.3.7  */
/*
 *  call-seq:
 *     flt == obj  ->  true or false
 *
 *  Returns <code>true</code> only if <i>obj</i> has the same value
 *  as <i>flt</i>. Contrast this with <code>Float#eql?</code>, which
 *  requires <i>obj</i> to be a <code>Float</code>.
 *
 *     1.0 == 1   #=> true
 *
 */

static mrb_value
flo_eq(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  volatile mrb_float a, b;
  mrb_get_args(mrb, "o", &y);

    switch (mrb_type(y)) {
      case MRB_TT_FIXNUM:
        b = (mrb_float)mrb_fixnum(y);
        break;
      case MRB_TT_FLOAT:
        b = mrb_float(y);
#if defined(_MSC_VER) && _MSC_VER < 1300
        if (isnan(b)) return mrb_false_value();
#endif
        break;
      default:
        return num_equal(mrb, x, y);
    }
    a = mrb_float(x);
#if defined(_MSC_VER) && _MSC_VER < 1300
    if (isnan(a)) return mrb_false_value();
#endif
    return (a == b)?mrb_true_value():mrb_false_value();
}

/* 15.2.8.3.18 */
/*
 * call-seq:
 *   flt.hash  ->  integer
 *
 * Returns a hash code for this float.
 */
static mrb_value
flo_hash(mrb_state *mrb, mrb_value num)
{
  mrb_float d;
  char *c;
  int i, hash;

  d = (mrb_float)mrb_fixnum(num);
  if (d == 0) d = fabs(d);
  c = (char*)&d;
  for (hash=0, i=0; i<sizeof(mrb_float);i++) {
    hash = (hash * 971) ^ (unsigned char)c[i];
  }
  if (hash < 0) hash = -hash;
  return mrb_fixnum_value(hash);
}

mrb_value
mrb_flt_cmp(double a, double b)
{
    if (isnan(a) || isnan(b)) return mrb_nil_value();
    if (a == b) return mrb_fixnum_value(0);
    if (a > b) return mrb_fixnum_value(1);
    if (a < b) return mrb_fixnum_value(-1);
    return mrb_nil_value();
}

/* 15.2.9.3.13 */
/*
 * call-seq:
 *   flt.to_f  ->  self
 *
 * As <code>flt</code> is already a float, returns +self+.
 */

static mrb_value
flo_to_f(mrb_state *mrb, mrb_value num)
{
    return num;
}

/* 15.2.9.3.11 */
/*
 *  call-seq:
 *     flt.infinite?  ->  nil, -1, +1
 *
 *  Returns <code>nil</code>, -1, or +1 depending on whether <i>flt</i>
 *  is finite, -infinity, or +infinity.
 *
 *     (0.0).infinite?        #=> nil
 *     (-1.0/0.0).infinite?   #=> -1
 *     (+1.0/0.0).infinite?   #=> 1
 */

static mrb_value
flo_is_infinite_p(mrb_state *mrb, mrb_value num)
{
    mrb_float value = mrb_float(num);

    if (isinf(value)) {
        return mrb_fixnum_value( value < 0 ? -1 : 1 );
    }

    return mrb_nil_value();
}

/* 15.2.9.3.9  */
/*
 *  call-seq:
 *     flt.finite?  ->  true or false
 *
 *  Returns <code>true</code> if <i>flt</i> is a valid IEEE floating
 *  point number (it is not infinite, and <code>nan?</code> is
 *  <code>false</code>).
 *
 */

static mrb_value
flo_is_finite_p(mrb_state *mrb, mrb_value num)
{
    mrb_float value = mrb_float(num);

#if HAVE_FINITE
    if (!finite(value))
        return mrb_false_value();
#else
    if (isinf(value) || isnan(value))
        return mrb_false_value();
#endif

    return mrb_true_value();
}

/* 15.2.9.3.10 */
/*
 *  call-seq:
 *     flt.floor  ->  integer
 *
 *  Returns the largest integer less than or equal to <i>flt</i>.
 *
 *     1.2.floor      #=> 1
 *     2.0.floor      #=> 2
 *     (-1.2).floor   #=> -2
 *     (-2.0).floor   #=> -2
 */

static mrb_value
flo_floor(mrb_state *mrb, mrb_value num)
{
    mrb_float f = floor(mrb_float(num));
    long val;

    if (!FIXABLE(f)) {
        return mrb_flt2big(mrb, f);
    }
    val = (long)f;
    return mrb_fixnum_value(val);
}

/* 15.2.9.3.8  */
/*
 *  call-seq:
 *     flt.ceil  ->  integer
 *
 *  Returns the smallest <code>Integer</code> greater than or equal to
 *  <i>flt</i>.
 *
 *     1.2.ceil      #=> 2
 *     2.0.ceil      #=> 2
 *     (-1.2).ceil   #=> -1
 *     (-2.0).ceil   #=> -2
 */

static mrb_value
flo_ceil(mrb_state *mrb, mrb_value num)
{
    mrb_float f = ceil(mrb_float(num));
    long val;

    if (!FIXABLE(f)) {
        return mrb_flt2big(mrb, f);
    }
    val = (long)f;
    return mrb_fixnum_value(val);
}

/* 15.2.9.3.12 */
/*
 *  call-seq:
 *     flt.round([ndigits])  ->  integer or float
 *
 *  Rounds <i>flt</i> to a given precision in decimal digits (default 0 digits).
 *  Precision may be negative.  Returns a floating point number when ndigits
 *  is more than zero.
 *
 *     1.4.round      #=> 1
 *     1.5.round      #=> 2
 *     1.6.round      #=> 2
 *     (-1.5).round   #=> -2
 *
 *     1.234567.round(2)  #=> 1.23
 *     1.234567.round(3)  #=> 1.235
 *     1.234567.round(4)  #=> 1.2346
 *     1.234567.round(5)  #=> 1.23457
 *
 *     34567.89.round(-5) #=> 0
 *     34567.89.round(-4) #=> 30000
 *     34567.89.round(-3) #=> 35000
 *     34567.89.round(-2) #=> 34600
 *     34567.89.round(-1) #=> 34570
 *     34567.89.round(0)  #=> 34568
 *     34567.89.round(1)  #=> 34567.9
 *     34567.89.round(2)  #=> 34567.89
 *     34567.89.round(3)  #=> 34567.89
 *
 */

static mrb_value
flo_round(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value num)
{
  mrb_value nd;
  mrb_float number, f;
  int ndigits = 0, i;
  long val;
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);

    if (argc /*> 0 && mrb_scan_args(argc, argv, "01", &nd) */== 1) {
        nd = argv[0];
        ndigits = mrb_fixnum(nd);
    }
    number  = mrb_float(num);
    f = 1.0;
    i = abs(ndigits);
    while  (--i >= 0)
        f = f*10.0;

    if (isinf(f)) {
        if (ndigits < 0) number = 0;
    }
    else {
        if (ndigits < 0) number /= f;
        else number *= f;
        number = round(number);
        if (ndigits < 0) number *= f;
        else number /= f;
    }

    if (ndigits > 0) return mrb_float_value(number);

    if (!FIXABLE(number)) {
        return mrb_flt2big(mrb, number);
    }
    val = (long)number;
    return mrb_fixnum_value(val);
}

/* 15.2.9.3.14 */
/* 15.2.9.3.15 */
/*
 *  call-seq:
 *     flt.to_i      ->  integer
 *     flt.to_int    ->  integer
 *     flt.truncate  ->  integer
 *
 *  Returns <i>flt</i> truncated to an <code>Integer</code>.
 */

static mrb_value
flo_truncate(mrb_state *mrb, mrb_value num)
{
    mrb_float f = mrb_float(num);
    long val;

    if (f > 0.0) f = floor(f);
    if (f < 0.0) f = ceil(f);

    if (!FIXABLE(f)) {
        return mrb_flt2big(mrb, f);
    }
    val = (long)f;
    return mrb_fixnum_value(val);
}

/* 15.2.8.3.17 */
/*
 *  call-seq:
 *     num.floor  ->  integer
 *
 *  Returns the largest integer less than or equal to <i>num</i>.
 *  <code>Numeric</code> implements this by converting <i>anInteger</i>
 *  to a <code>Float</code> and invoking <code>Float#floor</code>.
 *
 *     1.floor      #=> 1
 *     (-1).floor   #=> -1
 */

static mrb_value
num_floor(mrb_state *mrb, mrb_value num)
{
    return flo_floor(mrb, mrb_Float(mrb, num));
}

/* 15.2.8.3.20 */
/*
 *  call-seq:
 *     num.round([ndigits])  ->  integer or float
 *
 *  Rounds <i>num</i> to a given precision in decimal digits (default 0 digits).
 *  Precision may be negative.  Returns a floating point number when ndigits
 *  is more than zero.  <code>Numeric</code> implements this by converting itself
 *  to a <code>Float</code> and invoking <code>Float#round</code>.
 */

static mrb_value
num_round(mrb_state *mrb, /*int argc, mrb_value* argv,*/ mrb_value num)
{
    return flo_round(mrb, /*argc, argv,*/ mrb_Float(mrb, num));
}

SIGNED_VALUE
mrb_num2long(mrb_state *mrb, mrb_value val)
{
  again:
    if (mrb_nil_p(val)) {
        mrb_raise(mrb, E_TYPE_ERROR, "no implicit conversion from nil to integer");
    }

    if (FIXNUM_P(val)) return mrb_fixnum(val);

    switch (mrb_type(val)) {
      case MRB_TT_FLOAT:
        if (mrb_float(val) <= (mrb_float)LONG_MAX
            && mrb_float(val) >= (mrb_float)LONG_MIN) {
            return (SIGNED_VALUE)(mrb_float(val));
        }
        else {
            char buf[24];
            char *s;

            snprintf(buf, sizeof(buf), "%-.10g", mrb_float(val));
            if ((s = strchr(buf, ' ')) != 0) *s = '\0';
            mrb_raise(mrb, E_RANGE_ERROR, "float %s out of range of integer", buf);
        }

      default:
        val = mrb_to_int(mrb, val);
        goto again;
    }
}

mrb_value
mrb_num2ulong(mrb_state *mrb, mrb_value val)
{
  again:
    if (mrb_nil_p(val)) {
       mrb_raise(mrb, E_TYPE_ERROR, "no implicit conversion from nil to integer");
    }

    if (FIXNUM_P(val)) return val; /* this is FIX2LONG, inteneded */

    switch (mrb_type(val)) {
      case MRB_TT_FLOAT:
       if (mrb_float(val) <= (mrb_float)LONG_MAX
           && mrb_float(val) >= (mrb_float)LONG_MIN) {
           return mrb_fixnum_value(mrb_float(val));
       }
       else {
           char buf[24];
           char *s;

           snprintf(buf, sizeof(buf), "%-.10g", mrb_float(val));
           if ((s = strchr(buf, ' ')) != 0) *s = '\0';
           mrb_raise(mrb, E_RANGE_ERROR, "float %s out of range of integer", buf);
       }

      default:
       val = mrb_to_int(mrb, val);
       goto again;
    }
}

long
mrb_num2int(mrb_state *mrb, mrb_value val)
{
    return mrb_num2long(mrb, val);
}

long
mrb_fix2int(mrb_value val)
{
    return mrb_fixnum(val);
}

mrb_value
mrb_num2fix(mrb_state *mrb, mrb_value val)
{
    long v;

    if (FIXNUM_P(val)) return val;

    v = mrb_num2long(mrb, val);
    if (!FIXABLE(v))
        mrb_raise(mrb, E_RANGE_ERROR, "integer %ld out of range of fixnum", v);
    return mrb_fixnum_value(v);
}

/*
 * Document-class: Integer
 *
 *  <code>Integer</code> is the basis for the two concrete classes that
 *  hold whole numbers, <code>Bignum</code> and <code>Fixnum</code>.
 *
 */


/* 15.2.8.3.14 */
/* 15.2.8.3.24 */
/* 15.2.8.3.26 */
/*
 *  call-seq:
 *     int.to_i      ->  integer
 *     int.to_int    ->  integer
 *     int.floor     ->  integer
 *     int.ceil      ->  integer
 *     int.round     ->  integer
 *     int.truncate  ->  integer
 *
 *  As <i>int</i> is already an <code>Integer</code>, all these
 *  methods simply return the receiver.
 */

static mrb_value
int_to_i(mrb_state *mrb, mrb_value num)
{
    return num;
}

/* 15.2.8.3.21 */
/*
 *  call-seq:
 *     fixnum.next  ->  integer
 *     fixnum.succ  ->  integer
 *
 *  Returns the <code>Integer</code> equal to <i>int</i> + 1.
 *
 *     1.next      #=> 2
 *     (-1).next   #=> 0
 */

static mrb_value
fix_succ(mrb_state *mrb, mrb_value num)
{
    long i = mrb_fixnum(num) + 1;
    return mrb_fixnum_value(i);
}

/* 15.2.8.3.19 */
/*
 *  call-seq:
 *     int.next  ->  integer
 *     int.succ  ->  integer
 *
 *  Returns the <code>Integer</code> equal to <i>int</i> + 1.
 *
 *     1.next      #=> 2
 *     (-1).next   #=> 0
 */
static mrb_value
int_succ(mrb_state *mrb, mrb_value num)
{
    if (FIXNUM_P(num)) {
        long i = mrb_fixnum(num) + 1;
        return mrb_fixnum_value(i);
    }
    return mrb_funcall(mrb, num, "+", 1, mrb_fixnum_value(1));
}

mrb_value
rb_fix2str(mrb_state *mrb, mrb_value x, int base)
{
    extern const char ruby_digitmap[];
    char buf[sizeof(mrb_int)*CHAR_BIT + 2], *b = buf + sizeof buf;
    long val = mrb_fixnum(x);
    int neg = 0;

    if (base < 2 || 36 < base) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid radix %d", base);
    }
    if (val == 0) {
        return mrb_usascii_str_new2(mrb, "0");
    }
    if (val < 0) {
        val = -val;
        neg = 1;
    }
    *--b = '\0';
    do {
        *--b = ruby_digitmap[(int)(val % base)];
    } while (val /= base);
    if (neg) {
        *--b = '-';
    }

    return mrb_usascii_str_new2(mrb, b);
}

#define SQRT_LONG_MAX ((SIGNED_VALUE)1<<((sizeof(intptr_t)*CHAR_BIT-1)/2))
/*tests if N*N would overflow*/
#define FIT_SQRT_LONG(n) (((n)<SQRT_LONG_MAX)&&((n)>=-SQRT_LONG_MAX))

/* 15.2.8.3.3  */
/*
 * call-seq:
 *   fix * numeric  ->  numeric_result
 *
 * Performs multiplication: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */

static mrb_value
fix_mul(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_get_args(mrb, "o", &y);

    if (FIXNUM_P(y)) {
#ifdef __HP_cc
/* avoids an optimization bug of HP aC++/ANSI C B3910B A.06.05 [Jul 25 2005] */
        volatile
#endif
        long a, b;
        long c;
        mrb_value r;

        a = mrb_fixnum(x);
        b = mrb_fixnum(y);

        if (FIT_SQRT_LONG(a) && FIT_SQRT_LONG(b))
            return mrb_fixnum_value(a*b);
        c = a * b;
        r = mrb_fixnum_value(c);

        if (a == 0) return x;
        if (mrb_fixnum(r) != c || c/a != b) {
            //r = mrb_big_mul(mrb_int2big(a), mrb_int2big(b));
            r = mrb_fixnum_value(a*b);
        }
        return r;
    }
    switch (mrb_type(y)) {
      case MRB_TT_FLOAT:
        return mrb_float_value((mrb_float)mrb_fixnum(x) * mrb_float(y));
      default:
        return mrb_num_coerce_bin(mrb, x, y, "*");
    }
}

static void
fixdivmod(mrb_state *mrb, long x, long y, long *divp, long *modp)
{
    long div, mod;

    if (y == 0) mrb_num_zerodiv(mrb);
    if (y < 0) {
        if (x < 0)
            div = -x / -y;
        else
            div = - (x / -y);
    }
    else {
        if (x < 0)
            div = - (-x / y);
        else
            div = x / y;
    }
    mod = x - div*y;
    if ((mod < 0 && y > 0) || (mod > 0 && y < 0)) {
        mod += y;
        div -= 1;
    }
    if (divp) *divp = div;
    if (modp) *modp = mod;
}

mrb_value rb_big_fdiv(mrb_value x, mrb_value y);

//mrb_value mrb_rational_reciprocal(mrb_value x);

static mrb_value
fix_divide(mrb_state *mrb, mrb_value x, mrb_value y, char* op)
{
    if (FIXNUM_P(y)) {
        long div;

        fixdivmod(mrb, mrb_fixnum(x), mrb_fixnum(y), &div, 0);
        return mrb_fixnum_value(div);
    }
    switch (mrb_type(y)) {
      case MRB_TT_FLOAT:
        {
            mrb_float div;

            if (*op == '/') {
                div = (mrb_float)mrb_fixnum(x) / mrb_float(y);
                return mrb_float_value(div);
            }
            else {
                if (mrb_float(y) == 0) mrb_num_zerodiv(mrb);
                div = (mrb_float)mrb_fixnum(x) / mrb_float(y);
                return mrb_flt2big(mrb, floor(div));
            }
        }
      //case MRB_TT_RATIONAL:
      //  if (op == '/' && mrb_fixnum(x) == 1)
      //      return mrb_rational_reciprocal(y);
        /* fall through */
      default:
        return mrb_num_coerce_bin(mrb, x, y, op);
    }
}

/* 15.2.8.3.4  */
/*
 * call-seq:
 *   fix / numeric  ->  numeric_result
 *
 * Performs division: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */

static mrb_value
fix_div(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_get_args(mrb, "o", &y);

  return fix_divide(mrb, x, y, "/");
}

/* 15.2.8.3.5  */
/*
 *  call-seq:
 *    fix % other        ->  real
 *    fix.modulo(other)  ->  real
 *
 *  Returns <code>fix</code> modulo <code>other</code>.
 *  See <code>numeric.divmod</code> for more information.
 */

static mrb_value
fix_mod(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_get_args(mrb, "o", &y);

    if (FIXNUM_P(y)) {
        long mod;

        fixdivmod(mrb, mrb_fixnum(x), mrb_fixnum(y), 0, &mod);
        return mrb_fixnum_value(mod);
    }
    switch (mrb_type(y)) {
      case MRB_TT_FLOAT:
        {
            mrb_float mod;

            flodivmod(mrb, (mrb_float)mrb_fixnum(x), mrb_float(y), 0, &mod);
            return mrb_float_value(mod);
        }
      default:
        return mrb_num_coerce_bin(mrb, x, y, "%");
    }
}

/*
 *  call-seq:
 *     fix.divmod(numeric)  ->  array
 *
 *  See <code>Numeric#divmod</code>.
 */
static mrb_value
fix_divmod(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_get_args(mrb, "o", &y);

    if (FIXNUM_P(y)) {
        long div, mod;

        fixdivmod(mrb, mrb_fixnum(x), mrb_fixnum(y), &div, &mod);

        return mrb_assoc_new(mrb, mrb_fixnum_value(div), mrb_fixnum_value(mod));
    }
    switch (mrb_type(y)) {
      case MRB_TT_FLOAT:
        {
            mrb_float div, mod;
            volatile mrb_value a, b;

            flodivmod(mrb, (mrb_float)mrb_fixnum(x), mrb_float(y), &div, &mod);
            a = flt2ival(div);
            b = mrb_float_value(mod);
            return mrb_assoc_new(mrb, a, b);
        }
      default:
        return mrb_num_coerce_bin(mrb, x, y, "divmod");
    }
}

/* 15.2.8.3.7  */
/*
 * call-seq:
 *   fix == other  ->  true or false
 *
 * Return <code>true</code> if <code>fix</code> equals <code>other</code>
 * numerically.
 *
 *   1 == 2      #=> false
 *   1 == 1.0    #=> true
 */

static mrb_value
fix_equal(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_get_args(mrb, "o", &y);

    if (mrb_obj_equal(mrb, x, y)) return mrb_true_value();
    if (FIXNUM_P(y)) return mrb_false_value();
    switch (mrb_type(y)) {
      case MRB_TT_FLOAT:
        return (mrb_float)mrb_fixnum(x) == mrb_float(y) ? mrb_true_value() : mrb_false_value();
      default:
        return num_equal(mrb, x, y);
    }
}

/* 15.2.8.3.8  */
/*
 * call-seq:
 *   ~fix  ->  integer
 *
 * One's complement: returns a number where each bit is flipped.
 *   ex.0---00001 (1)-> 1---11110 (-2)
 *   ex.0---00010 (2)-> 1---11101 (-3)
 *   ex.0---00100 (4)-> 1---11011 (-5)
 */

static mrb_value
fix_rev(mrb_state *mrb, mrb_value num)
{
    long val = mrb_fixnum(num);

    val = ~val;
    return mrb_fixnum_value(val);
}

static mrb_value
bit_coerce(mrb_state *mrb, mrb_value x)
{
    while (!FIXNUM_P(x)) {
        if (mrb_type(x) == MRB_TT_FLOAT) {
            mrb_raise(mrb, E_TYPE_ERROR, "can't convert Float into Integer");
        }
        x = mrb_to_int(mrb, x);
    }
    return x;
}

/* 15.2.8.3.9  */
/*
 * call-seq:
 *   fix & integer  ->  integer_result
 *
 * Bitwise AND.
 */

static mrb_value
fix_and(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  long val;
  mrb_get_args(mrb, "o", &y);

  //if (!FIXNUM_P(y = bit_coerce(mrb, y))) {
  //  return mrb_big_and(y, x);
  //}
  if (mrb_type(y) == MRB_TT_FLOAT) {
      mrb_raise(mrb, E_TYPE_ERROR, "can't convert Float into Integer");
  }
  y = bit_coerce(mrb, y);
  val = mrb_fixnum(x) & mrb_fixnum(y);
  return mrb_fixnum_value(val);
}

/* 15.2.8.3.10 */
/*
 * call-seq:
 *   fix | integer  ->  integer_result
 *
 * Bitwise OR.
 */

static mrb_value
fix_or(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  long val;
  mrb_get_args(mrb, "o", &y);

  //if (!FIXNUM_P(y = bit_coerce(mrb, y))) {
  //    return mrb_big_or(y, x);
  //}
  if (mrb_type(y) == MRB_TT_FLOAT) {
      mrb_raise(mrb, E_TYPE_ERROR, "can't convert Float into Integer");
  }
  y = bit_coerce(mrb, y);
  val = mrb_fixnum(x) | mrb_fixnum(y);
  return mrb_fixnum_value(val);
}

/* 15.2.8.3.11 */
/*
 * call-seq:
 *   fix ^ integer  ->  integer_result
 *
 * Bitwise EXCLUSIVE OR.
 */

static mrb_value
fix_xor(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  long val;
  mrb_get_args(mrb, "o", &y);

  //if (!FIXNUM_P(y = bit_coerce(mrb, y))) {
  //  return mrb_big_xor(y, x);
  //}
  if (mrb_type(y) == MRB_TT_FLOAT) {
      mrb_raise(mrb, E_TYPE_ERROR, "can't convert Float into Integer");
  }
  y = bit_coerce(mrb, y);
  val = mrb_fixnum(x) ^ mrb_fixnum(y);
  return mrb_fixnum_value(val);
}

static mrb_value fix_lshift(mrb_state *mrb, long, unsigned long);
static mrb_value fix_rshift(long, unsigned long);

/* 15.2.8.3.12 */
/*
 * call-seq:
 *   fix << count  ->  integer
 *
 * Shifts _fix_ left _count_ positions (right if _count_ is negative).
 */

static mrb_value
mrb_fix_lshift(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  long val, width;
  mrb_get_args(mrb, "o", &y);

  val = mrb_fixnum(x);
  //if (!FIXNUM_P(y))
  //    return mrb_big_lshift(mrb_int2big(val), y);
  if (mrb_type(y) == MRB_TT_FLOAT) {
      mrb_raise(mrb, E_TYPE_ERROR, "can't convert Float into Integer");
  }
  width = mrb_fixnum(y);
  if (width < 0)
      return fix_rshift(val, (unsigned long)-width);
  return fix_lshift(mrb, val, width);
}

static mrb_value
fix_lshift(mrb_state *mrb, long val, unsigned long width)
{
  if (width > (sizeof(intptr_t)*CHAR_BIT-1)
      || ((unsigned long)abs(val))>>(sizeof(intptr_t)*CHAR_BIT-1-width) > 0) {
      mrb_raise(mrb, E_RANGE_ERROR, "width(%d) > (sizeof(intptr_t)*CHAR_BIT-1)", width);
  }
  val = val << width;
  return mrb_fixnum_value(val);
}

/* 15.2.8.3.13 */
/*
 * call-seq:
 *   fix >> count  ->  integer
 *
 * Shifts _fix_ right _count_ positions (left if _count_ is negative).
 */

static mrb_value
mrb_fix_rshift(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  long i, val;
  mrb_get_args(mrb, "o", &y);

    val = mrb_fixnum(x);
    //if (!FIXNUM_P(y))
    //    return mrb_big_rshift(mrb_int2big(val), y);
    i = mrb_fixnum(y);
    if (i == 0) return x;
    if (i < 0)
        return fix_lshift(mrb, val, (unsigned long)-i);
    return fix_rshift(val, i);
}

static mrb_value
fix_rshift(long val, unsigned long i)
{
    if (i >= sizeof(long)*CHAR_BIT-1) {
        if (val < 0) return mrb_fixnum_value(-1);
        return mrb_fixnum_value(0);
    }
    val = RSHIFT(val, i);
    return mrb_fixnum_value(val);
}

/* 15.2.8.3.23 */
/*
 *  call-seq:
 *     fix.to_f  ->  float
 *
 *  Converts <i>fix</i> to a <code>Float</code>.
 *
 */

static mrb_value
fix_to_f(mrb_state *mrb, mrb_value num)
{
    mrb_float val;

    val = (mrb_float)mrb_fixnum(num);

    return mrb_float_value(val);
}

/*
 *  Document-class: ZeroDivisionError
 *
 *  Raised when attempting to divide an integer by 0.
 *
 *     42 / 0
 *
 *  <em>raises the exception:</em>
 *
 *     ZeroDivisionError: divided by 0
 *
 *  Note that only division by an exact 0 will raise that exception:
 *
 *     42 /  0.0 #=> Float::INFINITY
 *     42 / -0.0 #=> -Float::INFINITY
 *     0  /  0.0 #=> NaN
 */

/*
 *  Document-class: FloatDomainError
 *
 *  Raised when attempting to convert special float values
 *  (in particular infinite or NaN)
 *  to numerical classes which don't support them.
 *
 *     Float::INFINITY.to_r
 *
 *  <em>raises the exception:</em>
 *
 *     FloatDomainError: Infinity
 */
/* ------------------------------------------------------------------------*/
static mrb_int
flt2big(mrb_state *mrb, float d)
{
  //long i = 0;
  //BDIGIT c;
  //BDIGIT *digits;
  mrb_int z;

  if (isinf(d)) {
    mrb_raise(mrb, E_FLOATDOMAIN_ERROR, d < 0 ? "-Infinity" : "Infinity");
  }
  if (isnan(d)) {
    mrb_raise(mrb, E_FLOATDOMAIN_ERROR, "NaN");
  }
  z = (mrb_int)d;
  return z;
}

mrb_value
mrb_flt2big(mrb_state *mrb, float d)
{
    return mrb_fixnum_value(flt2big(mrb, d));
}

/* 15.2.8.3.1  */
/*
 * call-seq:
 *   fix + numeric  ->  numeric_result
 *
 * Performs addition: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */
static mrb_value
mrb_fixnum_plus(mrb_state *mrb, mrb_value self)
{
  mrb_int x, y;

  x = mrb_fixnum(self);
  mrb_get_args(mrb, "i", &y);

  DEBUG(printf("%d + %d = %d\n", x, y, x+y));
  return mrb_fixnum_value(x + y);
}

/* 15.2.8.3.2  */
/* 15.2.8.3.16 */
/*
 * call-seq:
 *   fix - numeric  ->  numeric_result
 *
 * Performs subtraction: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */
static mrb_value
mrb_fixnum_minus(mrb_state *mrb, mrb_value self)
{
  mrb_int x, y;

  x = mrb_fixnum(self);
  mrb_get_args(mrb, "i", &y);

  DEBUG(printf("%d - %d = %d\n", x, y, x-y));
  return mrb_fixnum_value(x - y);
}

/* 15.2.8.3.6  */
/*
 *  call-seq:
 *     self.i <=> other.i    => -1, 0, +1
 *             <  => -1
 *             =  =>  0
 *             >  => +1
 *  Comparison---Returns -1, 0, or +1 depending on whether <i>fix</i> is
 *  less than, equal to, or greater than <i>numeric</i>. This is the
 *  basis for the tests in <code>Comparable</code>.
 */
static mrb_value
mrb_fixnum_cmp(mrb_state *mrb, mrb_value self)
{
  mrb_int x, y;
  mrb_value vy;

  mrb_get_args(mrb, "o", &vy);
  if (FIXNUM_P(vy)) {
    x = mrb_fixnum(self);
    y = mrb_fixnum(vy);
    DEBUG(printf("%d <=> %d\n", x, y));
    if (x > y)
      return mrb_fixnum_value(1);
    else if (x < y)
      return mrb_fixnum_value(-1);
    else
      return mrb_fixnum_value(0);
  }
  else {
    return mrb_num_coerce_cmp(mrb, self, vy, "<=>");
  }

}

/* 15.2.8.3.29 (x) */
/*
 * call-seq:
 *   fix > other     => true or false
 *
 * Returns <code>true</code> if the value of <code>fix</code> is
 * greater than that of <code>other</code>.
 */

mrb_value
mrb_fix2str(mrb_state *mrb, mrb_value x, int base)
{
  char buf[64], *b = buf + sizeof buf;
  long val = mrb_fixnum(x);
  int neg = 0;

  if (base < 2 || 36 < base) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid radix %d", base);
  }
  if (val == 0) {
    return mrb_str_new2(mrb, "0");
  }
  if (val < 0) {
    val = -val;
    neg = 1;
  }
  *--b = '\0';
  do {
    *--b = ruby_digitmap[(int)(val % base)];
  } while (val /= base);
  if (neg) {
    *--b = '-';
  }

  return mrb_str_new2(mrb, b);
}

mrb_value
mrb_fix_to_s(mrb_state *mrb, mrb_value self, int argc, mrb_value *argv)
{
  int base;

  if (argc == 0) base = 10;
  else {
    //mrb_value b;

    //mrb_scan_args(argc, argv, "01", &b);
    base = mrb_fixnum(argv[0]);
  }

  return mrb_fix2str(mrb, self, base);
}

/* 15.2.8.3.25 */
/*
 *  call-seq:
 *     fix.to_s(base=10)  ->  string
 *
 *  Returns a string containing the representation of <i>fix</i> radix
 *  <i>base</i> (between 2 and 36).
 *
 *     12345.to_s       #=> "12345"
 *     12345.to_s(2)    #=> "11000000111001"
 *     12345.to_s(8)    #=> "30071"
 *     12345.to_s(10)   #=> "12345"
 *     12345.to_s(16)   #=> "3039"
 *     12345.to_s(36)   #=> "9ix"
 *
 */
static mrb_value
mrb_fixnum_to_s(mrb_state *mrb, mrb_value self) /* fix_to_s */
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_fix_to_s(mrb, self, argc, argv);
}

/* 15.2.9.3.6  */
/*
 * call-seq:
 *     self.f <=> other.f    => -1, 0, +1
 *             <  => -1
 *             =  =>  0
 *             >  => +1
 *  Comparison---Returns -1, 0, or +1 depending on whether <i>fix</i> is
 *  less than, equal to, or greater than <i>numeric</i>. This is the
 *  basis for the tests in <code>Comparable</code>.
 */
static mrb_value
mrb_float_cmp(mrb_state *mrb, mrb_value self)
{
  mrb_value vy;
  mrb_float x, y;

  x = mrb_float(self);
  mrb_get_args(mrb, "o", &vy);
  if (FIXNUM_P(vy)) {
    y = (mrb_float)mrb_fixnum(vy);
  }
  else {
    y = mrb_float(vy);
  }

  DEBUG(printf("%f <=> %f\n", x, y));
  if (x > y)
    return mrb_fixnum_value(1);
  else {
    if (x < y)
      return mrb_fixnum_value(-1);
    return mrb_fixnum_value(0);
  }
}

/* 15.2.9.3.1  */
/*
 * call-seq:
 *   float + other  ->  float
 *
 * Returns a new float which is the sum of <code>float</code>
 * and <code>other</code>.
 */
static mrb_value
mrb_float_plus(mrb_state *mrb, mrb_value self)
{
  mrb_float x,  y;

  x = mrb_float(self);
  mrb_get_args(mrb, "f", &y);

  return mrb_float_value(x + y);
}
/* ------------------------------------------------------------------------*/
void
mrb_init_numeric(mrb_state *mrb)
{
  struct RClass *numeric, *integer, *fixnum, *fl;
  /* Numeric Class */
  numeric = mrb_define_class(mrb, "Numeric",  mrb->object_class);
  mrb_include_module(mrb, numeric, mrb_class_get(mrb, "Comparable"));

  mrb_define_method(mrb, numeric, "+@",       num_uplus,      ARGS_REQ(1));  /* 15.2.7.4.1  */
  mrb_define_method(mrb, numeric, "-@",       num_uminus,     ARGS_REQ(1));  /* 15.2.7.4.2  */
  mrb_define_method(mrb, numeric, "abs",      num_abs,        ARGS_NONE());  /* 15.2.7.4.3  */
  mrb_define_method(mrb, numeric, "coerce",   num_coerce,     ARGS_REQ(1));  /* 15.2.7.4.4  */
  mrb_define_method(mrb, numeric, "quo",      num_quo,        ARGS_REQ(1));  /* 15.2.7.4.5 (x) */

  /* Integer Class */
  integer = mrb_define_class(mrb, "Integer",  numeric);
  fixnum = mrb->fixnum_class = mrb_define_class(mrb, "Fixnum", integer);

  mrb_define_method(mrb, fixnum,  "+",        mrb_fixnum_plus,   ARGS_REQ(1)); /* 15.2.8.3.1  */
  mrb_define_method(mrb, fixnum,  "-",        mrb_fixnum_minus,  ARGS_REQ(1)); /* 15.2.8.3.2  */
  mrb_define_method(mrb, fixnum,  "*",        fix_mul,           ARGS_REQ(1)); /* 15.2.8.3.3  */
  mrb_define_method(mrb, fixnum,  "/",        fix_div,           ARGS_REQ(1)); /* 15.2.8.3.4  */
  mrb_define_method(mrb, fixnum,  "%",        fix_mod,           ARGS_REQ(1)); /* 15.2.8.3.5  */
  mrb_define_method(mrb, fixnum,  "<=>",      mrb_fixnum_cmp,    ARGS_REQ(1)); /* 15.2.8.3.6  */
  mrb_define_method(mrb, fixnum,  "==",       fix_equal,         ARGS_REQ(1)); /* 15.2.8.3.7  */
  mrb_define_method(mrb, fixnum,  "~",        fix_rev,           ARGS_NONE()); /* 15.2.8.3.8  */
  mrb_define_method(mrb, fixnum,  "&",        fix_and,           ARGS_REQ(1)); /* 15.2.8.3.9  */
  mrb_define_method(mrb, fixnum,  "|",        fix_or,            ARGS_REQ(1)); /* 15.2.8.3.10 */
  mrb_define_method(mrb, fixnum,  "^",        fix_xor,           ARGS_REQ(1)); /* 15.2.8.3.11 */
  mrb_define_method(mrb, fixnum,  "<<",       mrb_fix_lshift,    ARGS_REQ(1)); /* 15.2.8.3.12 */
  mrb_define_method(mrb, fixnum,  ">>",       mrb_fix_rshift,    ARGS_REQ(1)); /* 15.2.8.3.13 */
  mrb_define_method(mrb, fixnum,  "ceil",     int_to_i,          ARGS_NONE()); /* 15.2.8.3.14 */
  mrb_define_method(mrb, fixnum,  "eql?",     num_eql,           ARGS_REQ(1)); /* 15.2.8.3.16 */
  mrb_define_method(mrb, fixnum,  "floor",    num_floor,         ARGS_NONE()); /* 15.2.8.3.17 */
  mrb_define_method(mrb, fixnum,  "hash",     flo_hash,          ARGS_NONE()); /* 15.2.8.3.18 */
  mrb_define_method(mrb, fixnum,  "next",     int_succ,          ARGS_NONE()); /* 15.2.8.3.19 */
  mrb_define_method(mrb, fixnum,  "round",    num_round,         ARGS_ANY());  /* 15.2.8.3.20 */
  mrb_define_method(mrb, fixnum,  "succ",     fix_succ,          ARGS_NONE()); /* 15.2.8.3.21 */
  mrb_define_method(mrb, fixnum,  "to_f",     fix_to_f,          ARGS_NONE()); /* 15.2.8.3.23 */
  mrb_define_method(mrb, fixnum,  "to_i",     int_to_i,          ARGS_NONE()); /* 15.2.8.3.24 */
  mrb_define_method(mrb, fixnum,  "to_s",     mrb_fixnum_to_s,   ARGS_NONE()); /* 15.2.8.3.25 */
  mrb_define_method(mrb, fixnum,  "truncate", int_to_i,          ARGS_NONE()); /* 15.2.8.3.26 */
  //mrb_define_method(mrb, fixnum,  "<",        mrb_fixnum_lt,     ARGS_REQ(1)); /* 15.2.8.3.28 (x) */
  //mrb_define_method(mrb, fixnum,  ">",        mrb_fixnum_gt,     ARGS_REQ(1)); /* 15.2.8.3.29 (x) */
  mrb_define_method(mrb, fixnum,  "divmod",   fix_divmod,        ARGS_REQ(1)); /* 15.2.8.3.30 (x) */

  /* Float Class */
  fl = mrb->float_class = mrb_define_class(mrb, "Float", numeric);
  mrb_define_method(mrb, fl,      "+",         mrb_float_plus,   ARGS_REQ(1)); /* 15.2.9.3.1  */
  mrb_define_method(mrb, fl,      "-",         flo_minus,        ARGS_REQ(1)); /* 15.2.9.3.2  */
  mrb_define_method(mrb, fl,      "*",         flo_mul,          ARGS_REQ(1)); /* 15.2.9.3.3  */
  mrb_define_method(mrb, fl,      "/",         flo_div,          ARGS_REQ(1)); /* 15.2.9.3.4  */
  mrb_define_method(mrb, fl,      "%",         flo_mod,          ARGS_REQ(1)); /* 15.2.9.3.5  */
  mrb_define_method(mrb, fl,      "<=>",       mrb_float_cmp,    ARGS_REQ(1)); /* 15.2.9.3.6  */
  mrb_define_method(mrb, fl,      "==",        flo_eq,           ARGS_REQ(1)); /* 15.2.9.3.7  */
  mrb_define_method(mrb, fl,      "ceil",      flo_ceil,         ARGS_NONE()); /* 15.2.9.3.8  */
  mrb_define_method(mrb, fl,      "finite?",   flo_is_finite_p,  ARGS_NONE()); /* 15.2.9.3.9  */
  mrb_define_method(mrb, fl,      "floor",     flo_floor,        ARGS_NONE()); /* 15.2.9.3.10 */
  mrb_define_method(mrb, fl,      "infinite?", flo_is_infinite_p,ARGS_NONE()); /* 15.2.9.3.11 */
  mrb_define_method(mrb, fl,      "round",     flo_round,        ARGS_ANY());  /* 15.2.9.3.12 */
  mrb_define_method(mrb, fl,      "to_f",      flo_to_f,         ARGS_NONE()); /* 15.2.9.3.13 */
  mrb_define_method(mrb, fl,      "to_i",      flo_truncate,     ARGS_NONE()); /* 15.2.9.3.14 */
  mrb_define_method(mrb, fl,      "truncate",  flo_truncate,     ARGS_NONE()); /* 15.2.9.3.15 */

  mrb_define_method(mrb, fl,      "to_s",      flo_to_s,         ARGS_NONE()); /* 15.2.9.3.16(x) */
  //mrb_define_method(mrb, fl,      "<",         flo_lt,           ARGS_REQ(1)); /* 15.2.9.3.17(x) */
  //mrb_define_method(mrb, fl,      ">",         flo_gt,           ARGS_REQ(1)); /* 15.2.9.3.18(x) */
  mrb_define_method(mrb, fl,      "quo",       flo_quo,          ARGS_REQ(1)); /* 15.2.9.3.19(x) */
}
