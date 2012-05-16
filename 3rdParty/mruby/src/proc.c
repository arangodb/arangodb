/*
** proc.c - Proc class
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "opcode.h"

struct RProc *
mrb_proc_new(mrb_state *mrb, mrb_irep *irep)
{
  struct RProc *p;

  p = mrb_obj_alloc(mrb, MRB_TT_PROC, mrb->proc_class);
  p->body.irep = irep;
  p->target_class = (mrb->ci) ? mrb->ci->target_class : 0;
  p->env = 0;

  return p;
}

struct RProc *
mrb_closure_new(mrb_state *mrb, mrb_irep *irep)
{
  struct RProc *p = mrb_proc_new(mrb, irep);
  struct REnv *e;

  if (!mrb->ci->env) {
    e = mrb_obj_alloc(mrb, MRB_TT_ENV, (struct RClass *) mrb->ci->proc->env);
    e->flags= (unsigned int)mrb->ci->proc->body.irep->nlocals;
    e->mid = mrb->ci->mid;
    e->cioff = mrb->ci - mrb->cibase;
    e->stack = mrb->stack;
    mrb->ci->env = e;
  }
  else {
    e = mrb->ci->env;
  }
  p->env = e;
  return p;
}

struct RProc *
mrb_proc_new_cfunc(mrb_state *mrb, mrb_func_t func)
{
  struct RProc *p;

  p = mrb_obj_alloc(mrb, MRB_TT_PROC, mrb->proc_class);
  p->body.func = func;
  p->flags |= MRB_PROC_CFUNC;

  return p;
}

static mrb_value
mrb_proc_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value blk;

  mrb_get_args(mrb, "&", &blk);
  if (!mrb_nil_p(blk)) {
    *mrb_proc_ptr(self) = *mrb_proc_ptr(blk);
  }
  else {
    /* Calling Proc.new without a block is not implemented yet */
    mrb_raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
  }

  return self;
}

int
mrb_proc_cfunc_p(struct RProc *p)
{
  return MRB_PROC_CFUNC_P(p);
}

mrb_value
mrb_proc_call_cfunc(mrb_state *mrb, struct RProc *p, mrb_value self)
{
  return (p->body.func)(mrb, self);
}

mrb_code*
mrb_proc_iseq(mrb_state *mrb, struct RProc *p)
{
  return p->body.irep->iseq;
}

void
mrb_init_proc(mrb_state *mrb)
{
  struct RProc *m;
  mrb_code *call_iseq = mrb_malloc(mrb, sizeof(mrb_code));
  mrb_irep *call_irep = mrb_calloc(mrb, sizeof(mrb_irep), 1);

  if ( call_iseq == NULL || call_irep == NULL )
    return;

  *call_iseq = MKOP_A(OP_CALL, 0);
  call_irep->idx = -1;
  call_irep->flags = MRB_IREP_NOFREE;
  call_irep->iseq = call_iseq;
  call_irep->ilen = 1;

  mrb->proc_class = mrb_define_class(mrb, "Proc", mrb->object_class);

  mrb_define_method(mrb, mrb->proc_class, "initialize", mrb_proc_initialize, ARGS_NONE());

  m = mrb_proc_new(mrb, call_irep);
  mrb_define_method_raw(mrb, mrb->proc_class, mrb_intern(mrb, "call"), m);
  mrb_define_method_raw(mrb, mrb->proc_class, mrb_intern(mrb, "[]"), m);
}
