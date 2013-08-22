#include "mruby.h"
#include "mruby/compile.h"

static mrb_value
f_eval(mrb_state *mrb, mrb_value self)
{
  char *s;
  int len;

  mrb_get_args(mrb, "s", &s, &len);
  return mrb_load_nstring(mrb, s, len);
}

void
mrb_mruby_eval_gem_init(mrb_state* mrb)
{
  mrb_define_class_method(mrb, mrb->kernel_module, "eval", f_eval, MRB_ARGS_REQ(1));
}

void
mrb_mruby_eval_gem_final(mrb_state* mrb)
{
}
