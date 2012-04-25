/*
** compar.c - Comparable module
** 
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/string.h"
#include "mruby/numeric.h"

void
mrb_cmperr(mrb_state *mrb, mrb_value x, mrb_value y)
{
  const char *classname;

  if (SPECIAL_CONST_P(y)) {
    y = mrb_inspect(mrb, y);
    //classname = StringValuePtr(y);
    classname = mrb_string_value_ptr(mrb, y);
  }
  else {
    classname = mrb_obj_classname(mrb, y);
  }
  mrb_raise(mrb, E_ARGUMENT_ERROR, "comparison of %s with %s failed",
	    mrb_obj_classname(mrb, x), classname);
}

int
mrb_cmpint(mrb_state *mrb, mrb_value val, mrb_value a, mrb_value b)
{
  if (mrb_nil_p(val)) {
    mrb_cmperr(mrb, a, b);
  }
  if (FIXNUM_P(val)) {
      long l = mrb_fixnum(val);
      if (l > 0) return 1;
      if (l < 0) return -1;
      return 0;
  }
  if (mrb_test(mrb_funcall(mrb, val, ">", 1, mrb_fixnum_value(0)))) return 1;
  if (mrb_test(mrb_funcall(mrb, val, "<", 1, mrb_fixnum_value(0)))) return -1;
  return 0;
}

static mrb_value
cmp_equal(mrb_state *mrb, mrb_value x)
{
  mrb_value y, c;

  /* *** TEMPORAL IMPLEMENT *** */

  mrb_get_args(mrb, "o", &y);
  if (mrb_obj_equal(mrb, x, y)) return mrb_true_value();

  c = mrb_funcall(mrb, x, "<=>", 1, y);

  if (mrb_cmpint(mrb, c, x, y) == 0) return mrb_true_value();
  return mrb_false_value();
}

#include <stdio.h>
static mrb_value
cmp_gt(mrb_state *mrb, mrb_value x, mrb_value y)
{
  mrb_value c;

  c = mrb_funcall(mrb, x, "<=>", 1, y);

  if (mrb_cmpint(mrb, c, x, y) > 0) return mrb_true_value();
  return mrb_false_value();
}

static mrb_value
cmp_gt_m(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  return cmp_gt(mrb, x, y);
}

static mrb_value
cmp_ge_m(mrb_state *mrb, mrb_value x)
{
  mrb_value y, c;

  mrb_get_args(mrb, "o", &y);
  c = mrb_funcall(mrb, x, "<=>", 1, y);

  if (mrb_cmpint(mrb, c, x, y) >= 0) return mrb_true_value();
  return mrb_false_value();
}

static mrb_value
cmp_lt(mrb_state *mrb, mrb_value x, mrb_value y)
{
  mrb_value c;

  c = mrb_funcall(mrb, x, "<=>", 1, y);

  if (mrb_cmpint(mrb, c, x, y) < 0) return mrb_true_value();
  return mrb_false_value();
}

static mrb_value
cmp_lt_m(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  return cmp_lt(mrb, x, y);
}

static mrb_value
cmp_le_m(mrb_state *mrb, mrb_value x)
{
  mrb_value y, c;

  mrb_get_args(mrb, "o", &y);
  c = mrb_funcall(mrb, x, "<=>", 1, y);

  if (mrb_cmpint(mrb, c, x, y) <= 0) return mrb_true_value();
  return mrb_false_value();
}

static mrb_value
cmp_between(mrb_state *mrb, mrb_value x)
{
  mrb_value min, max;

  mrb_get_args(mrb, "oo", &min, &max);

  if (mrb_test(cmp_lt(mrb, x, min))) return mrb_false_value();
  if (mrb_test(cmp_gt(mrb, x, max))) return mrb_false_value();
  return mrb_true_value();
}

void
mrb_init_comparable(mrb_state *mrb)
{
  struct RClass *comp;

  comp = mrb_define_module(mrb, "Comparable");
  mrb_define_method(mrb, comp, "<",  cmp_lt_m, ARGS_REQ(1)); /* 15.3.3.2.1 */
  mrb_define_method(mrb, comp, "<=", cmp_le_m, ARGS_REQ(1)); /* 15.3.3.2.2 */
  mrb_define_method(mrb, comp, "==", cmp_equal, ARGS_REQ(1)); /* 15.3.3.2.3 */
  mrb_define_method(mrb, comp, ">",  cmp_gt_m, ARGS_REQ(1)); /* 15.3.3.2.4 */
  mrb_define_method(mrb, comp, ">=", cmp_ge_m, ARGS_REQ(1)); /* 15.3.3.2.5 */
  mrb_define_method(mrb, comp, "between?",  cmp_between, ARGS_REQ(2)); /* 15.3.3.2.6 */
}
