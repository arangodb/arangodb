#include <limits.h>
#include "mruby.h"
#include "mruby/numeric.h"

static mrb_value
mrb_int_chr(mrb_state *mrb, mrb_value x)
{
  mrb_int chr;
  char c;

  chr = mrb_fixnum(x);
  if (chr >= (1 << CHAR_BIT)) {
    mrb_raisef(mrb, E_RANGE_ERROR, "%S out of char range", x);
  }
  c = (char)chr;

  return mrb_str_new(mrb, &c, 1);
}

void
mrb_mruby_numeric_ext_gem_init(mrb_state* mrb)
{
  struct RClass *i = mrb_class_get(mrb, "Integer");

  mrb_define_method(mrb, i, "chr", mrb_int_chr, MRB_ARGS_NONE());
}

void
mrb_mruby_numeric_ext_gem_final(mrb_state* mrb)
{
}
