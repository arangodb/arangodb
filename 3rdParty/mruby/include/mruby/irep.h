/*
** irep.h - mrb_irep structure
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_IREP_H
#define MRUBY_IREP_H

typedef struct mrb_irep {
  int idx;

  int flags;
  int nlocals;
  int nregs;

  mrb_code *iseq;
  mrb_value *pool;
  mrb_sym *syms;

  int ilen, plen, slen;
} mrb_irep;

#define MRB_IREP_NOFREE 3
#define MRB_ISEQ_NOFREE 1

void mrb_add_irep(mrb_state *mrb, int n);

#endif  /* MRUBY_IREP_H */
