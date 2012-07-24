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

  p = (struct RProc*)mrb_obj_alloc(mrb, MRB_TT_PROC, mrb->proc_class);
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
    e = (struct REnv*)mrb_obj_alloc(mrb, MRB_TT_ENV, (struct RClass*)mrb->ci->proc->env);
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

  p = (struct RProc*)mrb_obj_alloc(mrb, MRB_TT_PROC, mrb->proc_class);
  p->body.func = func;
  p->flags |= MRB_PROC_CFUNC;

  return p;
}

static inline void
proc_copy(struct RProc *a, struct RProc *b)
{
  a->flags = b->flags;
  a->body = b->body;
  a->target_class = b->target_class;
  a->env = b->env;
}

static mrb_value
mrb_proc_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value blk;

  mrb_get_args(mrb, "&", &blk);
  if (mrb_nil_p(blk)) {
    /* Calling Proc.new without a block is not implemented yet */
    mrb_raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
  }
  else {
    proc_copy(mrb_proc_ptr(self), mrb_proc_ptr(blk));
  }
  return self;
}

static mrb_value
mrb_proc_init_copy(mrb_state *mrb, mrb_value self)
{
  mrb_value proc;

  mrb_get_args(mrb, "o", &proc);
  if (mrb_type(proc) != MRB_TT_PROC) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "not a proc");
  }
  proc_copy(mrb_proc_ptr(self), mrb_proc_ptr(proc));
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

/* 15.3.1.2.6  */
/* 15.3.1.3.27 */
/*
 * call-seq:
 *   lambda { |...| block }  -> a_proc
 *
 * Equivalent to <code>Proc.new</code>, except the resulting Proc objects
 * check the number of parameters passed when called.
 */
static mrb_value
proc_lambda(mrb_state *mrb, mrb_value self)
{
  mrb_value blk;
  struct RProc *p;

  mrb_get_args(mrb, "&", &blk);
  if (mrb_nil_p(blk)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
  }
  p = mrb_proc_ptr(blk);
  if (!MRB_PROC_STRICT_P(p)) {
    struct RProc *p2 = (struct RProc*)mrb_obj_alloc(mrb, MRB_TT_PROC, p->c);
    proc_copy(p2, p);
    p2->flags |= MRB_PROC_STRICT;
    return mrb_obj_value(p2);
  }
  return self;
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
  MRB_SET_INSTANCE_TT(mrb->proc_class, MRB_TT_PROC);

  mrb_define_method(mrb, mrb->proc_class, "initialize", mrb_proc_initialize, ARGS_NONE());
  mrb_define_method(mrb, mrb->proc_class, "initialize_copy", mrb_proc_init_copy, ARGS_REQ(1));

  m = mrb_proc_new(mrb, call_irep);
  mrb_define_method_raw(mrb, mrb->proc_class, mrb_intern(mrb, "call"), m);
  mrb_define_method_raw(mrb, mrb->proc_class, mrb_intern(mrb, "[]"), m);

  mrb_define_class_method(mrb, mrb->kernel_module, "lambda", proc_lambda, ARGS_NONE());    /* 15.3.1.2.6  */
  mrb_define_method(mrb, mrb->kernel_module,       "lambda", proc_lambda, ARGS_NONE());    /* 15.3.1.3.27 */
}
