/*
** backtrace.c -
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/variable.h"
#include "mruby/proc.h"

void
mrb_print_backtrace(mrb_state *mrb)
{
#ifdef ENABLE_STDIO
  mrb_callinfo *ci;
  mrb_int ciidx;
  const char *filename, *method, *sep;
  int i, line;

  fputs("trace:\n", stderr);
  ciidx = mrb_fixnum(mrb_obj_iv_get(mrb, mrb->exc, mrb_intern2(mrb, "ciidx", 5)));
  if (ciidx >= mrb->c->ciend - mrb->c->cibase)
    ciidx = 10; /* ciidx is broken... */

  for (i = ciidx; i >= 0; i--) {
    ci = &mrb->c->cibase[i];
    filename = "(unknown)";
    line = -1;

    if (MRB_PROC_CFUNC_P(ci->proc)) {
      continue;
    }
    else {
      mrb_irep *irep = ci->proc->body.irep;
      if (irep->filename != NULL)
        filename = irep->filename;
      if (irep->lines != NULL) {
        mrb_code *pc;

        if (i+1 <= ciidx) {
          pc = mrb->c->cibase[i+1].pc;
        }
        else {
          pc = (mrb_code*)mrb_voidp(mrb_obj_iv_get(mrb, mrb->exc, mrb_intern2(mrb, "lastpc", 6)));
        }
        if (irep->iseq <= pc && pc < irep->iseq + irep->ilen) {
          line = irep->lines[pc - irep->iseq - 1];
        }
      }
    }
    if (line == -1) continue;
    if (ci->target_class == ci->proc->target_class)
      sep = ".";
    else
      sep = "#";

    method = mrb_sym2name(mrb, ci->mid);
    if (method) {
      const char *cn = mrb_class_name(mrb, ci->proc->target_class);

      if (cn) {
        fprintf(stderr, "\t[%d] %s:%d:in %s%s%s\n", i, filename, line, cn, sep, method);
      }
      else {
        fprintf(stderr, "\t[%d] %s:%d:in %s\n", i, filename, line, method);
      }
    }
    else {
      fprintf(stderr, "\t[%d] %s:%d\n", i, filename, line);
    }
  }
#endif
}
