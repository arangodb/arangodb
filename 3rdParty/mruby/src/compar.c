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

void
mrb_init_comparable(mrb_state *mrb)
{
  mrb_define_module(mrb, "Comparable");
}
