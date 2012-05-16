/*
** numeric.h - Numeric, Integer, Float, Fixnum class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_NUMERIC_H
#define MRUBY_NUMERIC_H

#include <limits.h>

#define RSHIFT(x,y) ((x)>>(int)(y))
#define FIXNUM_MAX (LONG_MAX>>1)
#define FIXNUM_MIN RSHIFT((long)LONG_MIN,1)
#define POSFIXABLE(f) ((f) < FIXNUM_MAX+1)
#define NEGFIXABLE(f) ((f) >= FIXNUM_MIN)
#define FIXABLE(f) (POSFIXABLE(f) && NEGFIXABLE(f))

mrb_value mrb_flt2big(mrb_state *mrb, mrb_float d);
void mrb_num_zerodiv(mrb_state *mrb);
mrb_value mrb_fix2str(mrb_state *mrb, mrb_value x, int base);

#endif  /* MRUBY_NUMERIC_H */
