#include <stdlib.h>
#include "mruby.h"

static mrb_value
f_exit(mrb_state *mrb, mrb_value self)
{
  mrb_int i = EXIT_SUCCESS;

  mrb_get_args(mrb, "|i", &i);
  exit(i);
  /* not reached */
  return mrb_nil_value();
}

void
mrb_mruby_exit_gem_init(mrb_state* mrb)
{
  mrb_define_method(mrb, mrb->kernel_module, "exit", f_exit, MRB_ARGS_REQ(1));
}

void
mrb_mruby_exit_gem_final(mrb_state* mrb)
{
}
