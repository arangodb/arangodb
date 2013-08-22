/*
** mruby/numeric.h - Numeric, Integer, Float, Fixnum class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_NUMERIC_H
#define MRUBY_NUMERIC_H

#if defined(__cplusplus)
extern "C" {
#endif

#define POSFIXABLE(f) ((f) <= MRB_INT_MAX)
#define NEGFIXABLE(f) ((f) >= MRB_INT_MIN)
#define FIXABLE(f) (POSFIXABLE(f) && NEGFIXABLE(f))

mrb_value mrb_flo_to_fixnum(mrb_state *mrb, mrb_value val);
mrb_value mrb_flo_to_str(mrb_state *mrb, mrb_value flo, int max_digit);

mrb_value mrb_fixnum_to_str(mrb_state *mrb, mrb_value x, int base);

mrb_value mrb_fixnum_plus(mrb_state *mrb, mrb_value x, mrb_value y);
mrb_value mrb_fixnum_minus(mrb_state *mrb, mrb_value x, mrb_value y);
mrb_value mrb_fixnum_mul(mrb_state *mrb, mrb_value x, mrb_value y);
mrb_value mrb_num_div(mrb_state *mrb, mrb_value x, mrb_value y);

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* MRUBY_NUMERIC_H */
