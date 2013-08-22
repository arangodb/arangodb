#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/array.h"
#include "mruby/string.h"

static mrb_value
mrb_proc_lambda(mrb_state *mrb, mrb_value self)
{
  struct RProc *p = mrb_proc_ptr(self);
  return mrb_bool_value(MRB_PROC_STRICT_P(p));
}

static mrb_value
mrb_proc_source_location(mrb_state *mrb, mrb_value self)
{
  struct RProc *p = mrb_proc_ptr(self);

  if (MRB_PROC_CFUNC_P(p)) {
    return mrb_nil_value();
  }
  else {
    mrb_irep *irep = p->body.irep;
    mrb_value filename = mrb_nil_value();
    mrb_value lines = mrb_nil_value();

    if (irep->filename) filename = mrb_str_new_cstr(mrb, irep->filename);
    if (irep->lines)    lines = mrb_fixnum_value(*irep->lines);

    return mrb_assoc_new(mrb, filename, lines);
  }
}

static mrb_value
mrb_proc_inspect(mrb_state *mrb, mrb_value self)
{
  struct RProc *p = mrb_proc_ptr(self);
  mrb_value str = mrb_str_new_cstr(mrb, "#<Proc:");
  mrb_str_concat(mrb, str, mrb_ptr_to_str(mrb, mrb_voidp(self)));

  if (!MRB_PROC_CFUNC_P(p)) {
    mrb_irep *irep = p->body.irep;
    mrb_str_cat_cstr(mrb, str, "@");   

    if (irep->filename) {
      mrb_str_cat_cstr(mrb, str, irep->filename);
    }
    else {
      mrb_str_cat_cstr(mrb, str, "-");
    }
    mrb_str_cat_cstr(mrb, str, ":");

    if (irep->lines) {
      mrb_str_append(mrb, str, mrb_fixnum_value(*irep->lines));
    }
    else {
      mrb_str_cat_cstr(mrb, str, "-");      
    }
  }

  if (MRB_PROC_STRICT_P(p)) {
    mrb_str_cat_cstr(mrb, str, " (lambda)");
  }

  mrb_str_cat_cstr(mrb, str, ">");
  return str;
}

static mrb_value
mrb_kernel_proc(mrb_state *mrb, mrb_value self)
{
  mrb_value blk;

  mrb_get_args(mrb, "&", &blk);
  if (mrb_nil_p(blk)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
  }

  return blk;
}

void
mrb_mruby_proc_ext_gem_init(mrb_state* mrb)
{
  struct RClass *p = mrb->proc_class;
  mrb_define_method(mrb, p, "lambda?",         mrb_proc_lambda,          MRB_ARGS_NONE());
  mrb_define_method(mrb, p, "source_location", mrb_proc_source_location, MRB_ARGS_NONE());
  mrb_define_method(mrb, p, "to_s",            mrb_proc_inspect,         MRB_ARGS_NONE());
  mrb_define_method(mrb, p, "inspect",         mrb_proc_inspect,         MRB_ARGS_NONE());

  mrb_define_class_method(mrb, mrb->kernel_module, "proc", mrb_kernel_proc, MRB_ARGS_NONE());
  mrb_define_method(mrb, mrb->kernel_module,       "proc", mrb_kernel_proc, MRB_ARGS_NONE());
}

void
mrb_mruby_proc_ext_gem_final(mrb_state* mrb)
{
}
