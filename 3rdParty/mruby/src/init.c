/*
** init.c - initialize mruby core
**
** See Copyright Notice in mruby.h
*/

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
void mrb_init_exception(mrb_state*);
void mrb_init_time(mrb_state*);
void mrb_init_io(mrb_state*);
void mrb_init_file(mrb_state*);
void mrb_init_thread(mrb_state*);
void mrb_init_struct(mrb_state*);
void mrb_init_gc(mrb_state*);
void mrb_init_print(mrb_state*);
void mrb_init_mrblib(mrb_state*);
void mrb_init_math(mrb_state*);

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
  mrb_init_array(mrb);
  mrb_init_hash(mrb);
  mrb_init_numeric(mrb);
  mrb_init_range(mrb);
  mrb_init_struct(mrb);
  mrb_init_gc(mrb);
#ifdef INCLUDE_REGEXP
  mrb_init_regexp(mrb);
#endif
  mrb_init_exception(mrb);
  mrb_init_print(mrb);
  mrb_init_time(mrb);
  mrb_init_math(mrb);

  mrb_init_mrblib(mrb);

  mrb_gc_arena_restore(mrb, 0);
}
