#include "mruby.h"

void mrb_init_class(mrb_state*);
void mrb_init_symtbl(mrb_state*);
void mrb_init_symbols(mrb_state*);
void mrb_init_object(mrb_state*);
void mrb_init_kernel(mrb_state*);
void mrb_init_enumerable(mrb_state*);
void mrb_init_comparable(mrb_state*);
void mrb_init_array(mrb_state*);
void mrb_init_hash(mrb_state*);
void mrb_init_numeric(mrb_state*);
void mrb_init_proc(mrb_state*);
void mrb_init_range(mrb_state*);
void mrb_init_string(mrb_state*);
void mrb_init_regexp(mrb_state*);
void mrb_init_encoding(mrb_state*);
void mrb_init_exception(mrb_state*);
void mrb_init_time(mrb_state *);
void mrb_init_io(mrb_state *);
void mrb_init_file(mrb_state *);
void mrb_init_thread(mrb_state *);
void mrb_init_struct(mrb_state *);
void mrb_init_gc(mrb_state *);
void Init_var_tables(mrb_state *mrb);
void Init_version(mrb_state *mrb);
void mrb_init_print(mrb_state *mrb);
void mrb_init_mrblib(mrb_state *mrb);

#define MANDEL
#ifdef MANDEL
#include <stdio.h>
#include <math.h>
static mrb_value
mpow(mrb_state *mrb, mrb_value obj)
{
  mrb_float x, y;

  mrb_get_args(mrb, "ff", &x, &y);
  x = pow(x, y);

  return mrb_float_value(x);
}

static mrb_value
msqrt(mrb_state *mrb, mrb_value obj)
{
  mrb_float x;

  mrb_get_args(mrb, "f", &x);
  x = sqrt(x);

  return mrb_float_value(x);
}

static mrb_value
mputc(mrb_state *mrb, mrb_value obj)
{
  int x;

  mrb_get_args(mrb, "i", &x);
  putc(x, stdout);

  return mrb_nil_value();
}
#endif

void
mrb_init_core(mrb_state *mrb)
{
  mrb_init_symtbl(mrb);

  mrb_init_class(mrb);
  mrb_init_object(mrb);
  mrb_init_kernel(mrb);
  mrb_init_comparable(mrb);
  mrb_init_enumerable(mrb);

  mrb_init_symbols(mrb);
  mrb_init_proc(mrb);
  mrb_init_string(mrb);
  Init_version(mrb); /* after init_string */
  mrb_init_array(mrb);
  mrb_init_hash(mrb);
  mrb_init_numeric(mrb);
  mrb_init_range(mrb);
  mrb_init_struct(mrb);
  mrb_init_gc(mrb);
#ifdef INCLUDE_REGEXP
  mrb_init_regexp(mrb);
  mrb_init_encoding(mrb);
#endif
  mrb_init_exception(mrb);
  mrb_init_print(mrb);

#ifdef MANDEL
  mrb_define_method(mrb, mrb->kernel_module, "pow", mpow, ARGS_REQ(2));
  mrb_define_method(mrb, mrb->kernel_module, "sqrt", msqrt, ARGS_REQ(1));
  mrb_define_method(mrb, mrb->kernel_module, "putc", mputc, ARGS_REQ(1));
#endif

  mrb_init_mrblib(mrb);

  mrb_gc_arena_restore(mrb, 0);
}
