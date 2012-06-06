/*
** numeric.h - Numeric, Integer, Float, Fixnum class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_NUMERIC_H
#define MRUBY_NUMERIC_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <limits.h>

#define RSHIFT(x,y) ((x)>>(int)(y))
#define POSFIXABLE(f) ((f) <= INT_MAX)
#define NEGFIXABLE(f) ((f) >= INT_MIN)
#define FIXABLE(f) (POSFIXABLE(f) && NEGFIXABLE(f))

mrb_value mrb_flt2big(mrb_state *mrb, mrb_float d);
void mrb_num_zerodiv(mrb_state *mrb);
mrb_value mrb_fix2str(mrb_state *mrb, mrb_value x, int base);

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* MRUBY_NUMERIC_H */
