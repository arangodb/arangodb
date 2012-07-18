/*
** print.c - Kernel.#p
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/string.h"
#include <stdio.h>

mrb_value
printstr(mrb_state *mrb, mrb_value obj)
{
  struct RString *str;
  char *s;
  int len;

  if (mrb_type(obj) == MRB_TT_STRING) {
    str = mrb_str_ptr(obj);
    s = str->ptr;
    len = str->len;
    while (len--) {
      putc(*s, stdout);
      s++;
    }
  }
  return obj;
}

mrb_value
mrb_p(mrb_state *mrb, mrb_value obj)
{
  obj = mrb_funcall(mrb, obj, "inspect", 0);
  printstr(mrb, obj);
  putc('\n', stdout);
  return obj;
}

/* 15.3.1.2.9  */
/* 15.3.1.3.34 */
mrb_value
mrb_printstr(mrb_state *mrb, mrb_value self)
{
  mrb_value argv;

  mrb_get_args(mrb, "o", &argv);
  printstr(mrb, argv);

  return argv;
}

void
mrb_init_print(mrb_state *mrb)
{
  struct RClass *krn;

  krn = mrb->kernel_module;

  mrb_define_method(mrb, krn, "__printstr__", mrb_printstr, ARGS_REQ(1));
}


void
mrb_show_version(mrb_state *mrb)
{
  printf("mruby - Embeddable Ruby  Copyright (c) 2010-2012 mruby developers\n");
}

void
mrb_show_copyright(mrb_state *mrb)
{
  printf("mruby - Copyright (c) 2010-2012 mruby developers\n");
}
