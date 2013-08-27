#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/proc.h"

#define FIBER_STACK_INIT_SIZE 64
#define FIBER_CI_INIT_SIZE 8

/*
 *  call-seq:
 *     Fiber.new{...} -> obj
 *
 *  Creates a fiber, whose execution is suspend until it is explicitly
 *  resumed using <code>Fiber#resume</code> method.  
 *  The code running inside the fiber can give up control by calling
 *  <code>Fiber.yield</code> in which case it yields control back to caller
 *  (the caller of the <code>Fiber#resume</code>).
 *
 *  Upon yielding or termination the Fiber returns the value of the last
 *  executed expression
 *
 *  For instance:
 *
 *    fiber = Fiber.new do
 *      Fiber.yield 1
 *      2
 *    end
 *
 *    puts fiber.resume
 *    puts fiber.resume
 *    puts fiber.resume
 *
 *  <em>produces</em>
 *
 *    1
 *    2
 *    resuming dead fiber (RuntimeError)
 *
 *  The <code>Fiber#resume</code> method accepts an arbitrary number of
 *  parameters, if it is the first call to <code>resume</code> then they
 *  will be passed as block arguments. Otherwise they will be the return
 *  value of the call to <code>Fiber.yield</code>
 *
 *  Example:
 *
 *    fiber = Fiber.new do |first|
 *      second = Fiber.yield first + 2
 *    end
 *
 *    puts fiber.resume 10
 *    puts fiber.resume 14
 *    puts fiber.resume 18
 *
 *  <em>produces</em>
 *
 *    12
 *    14
 *    resuming dead fiber (RuntimeError)
 *
 */
static mrb_value
fiber_init(mrb_state *mrb, mrb_value self)
{
  static const struct mrb_context mrb_context_zero = { 0 };
  struct RFiber *f = (struct RFiber*)mrb_ptr(self);
  struct mrb_context *c;
  struct RProc *p;
  mrb_callinfo *ci;
  mrb_value blk;

  mrb_get_args(mrb, "&", &blk);
  
  if (mrb_nil_p(blk)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "tried to create Fiber object without a block");
  }
  p = mrb_proc_ptr(blk);
  if (MRB_PROC_CFUNC_P(p)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "tried to create Fiber from C defined method");
  }

  f->cxt = (struct mrb_context*)mrb_malloc(mrb, sizeof(struct mrb_context));
  *f->cxt = mrb_context_zero;
  c = f->cxt;

  /* initialize VM stack */
  c->stbase = (mrb_value *)mrb_calloc(mrb, FIBER_STACK_INIT_SIZE, sizeof(mrb_value));
  c->stend = c->stbase + FIBER_STACK_INIT_SIZE;
  c->stack = c->stbase;

  /* copy receiver from a block */
  c->stack[0] = mrb->c->stack[0];

  /* initialize callinfo stack */
  c->cibase = (mrb_callinfo *)mrb_calloc(mrb, FIBER_CI_INIT_SIZE, sizeof(mrb_callinfo));
  c->ciend = c->cibase + FIBER_CI_INIT_SIZE;
  c->ci = c->cibase;

  /* adjust return callinfo */
  ci = c->ci;
  ci->target_class = p->target_class;
  ci->proc = p;
  ci->pc = p->body.irep->iseq;
  ci->nregs = p->body.irep->nregs;
  ci[1] = ci[0];
  c->ci++;                      /* push dummy callinfo */

  c->fib = f;
  c->status = MRB_FIBER_CREATED;

  return self;
}

static struct mrb_context*
fiber_check(mrb_state *mrb, mrb_value fib)
{
  struct RFiber *f = (struct RFiber*)mrb_ptr(fib);

  if (!f->cxt) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "uninitialized Fiber");
  }
  return f->cxt;
}

static mrb_value
fiber_result(mrb_state *mrb, mrb_value *a, int len)
{
  if (len == 0) return mrb_nil_value();
  if (len == 1) return a[0];
  return mrb_ary_new_from_values(mrb, len, a);
}

/* mark return from context modifying method */
#define MARK_CONTEXT_MODIFY(c) (c)->ci->target_class = NULL

/*
 *  call-seq:
 *     fiber.resume(args, ...) -> obj
 *
 *  Resumes the fiber from the point at which the last <code>Fiber.yield</code>
 *  was called, or starts running it if it is the first call to
 *  <code>resume</code>. Arguments passed to resume will be the value of
 *  the <code>Fiber.yield</code> expression or will be passed as block
 *  parameters to the fiber's block if this is the first <code>resume</code>.
 *
 *  Alternatively, when resume is called it evaluates to the arguments passed
 *  to the next <code>Fiber.yield</code> statement inside the fiber's block
 *  or to the block value if it runs to completion without any
 *  <code>Fiber.yield</code>
 */
static mrb_value
fiber_resume(mrb_state *mrb, mrb_value self)
{
  struct mrb_context *c = fiber_check(mrb, self);
  mrb_value *a;
  int len;

  if (c->status == MRB_FIBER_RESUMED) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "double resume");
  }
  if (c->status == MRB_FIBER_TERMINATED) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "resuming dead fiber");
  }
  mrb_get_args(mrb, "*", &a, &len);
  mrb->c->status = MRB_FIBER_RESUMED;
  if (c->status == MRB_FIBER_CREATED) {
    mrb_value *b = c->stack+1;
    mrb_value *e = b + len;

    while (b<e) {
      *b++ = *a++;
    }
    c->cibase->argc = len;
    c->prev = mrb->c;
    if (c->prev->fib) 
      mrb_field_write_barrier(mrb, (struct RBasic*)c->fib, (struct RBasic*)c->prev->fib);
    mrb_write_barrier(mrb, (struct RBasic*)c->fib);
    c->status = MRB_FIBER_RUNNING;
    mrb->c = c;

    MARK_CONTEXT_MODIFY(c);
    return c->ci->proc->env->stack[0];
  }
  MARK_CONTEXT_MODIFY(c);
  c->prev = mrb->c;
  if (c->prev->fib) 
    mrb_field_write_barrier(mrb, (struct RBasic*)c->fib, (struct RBasic*)c->prev->fib);
  mrb_write_barrier(mrb, (struct RBasic*)c->fib);
  c->status = MRB_FIBER_RUNNING;
  mrb->c = c;
  return fiber_result(mrb, a, len);
}

/*
 *  call-seq:
 *     fiber.alive? -> true or false
 *
 *  Returns true if the fiber can still be resumed. After finishing
 *  execution of the fiber block this method will always return false.
 */
static mrb_value
fiber_alive_p(mrb_state *mrb, mrb_value self)
{
  struct mrb_context *c = fiber_check(mrb, self);
  return mrb_bool_value(c->status != MRB_FIBER_TERMINATED);
}

/*
 *  call-seq:
 *     Fiber.yield(args, ...) -> obj
 *
 *  Yields control back to the context that resumed the fiber, passing
 *  along any arguments that were passed to it. The fiber will resume
 *  processing at this point when <code>resume</code> is called next.
 *  Any arguments passed to the next <code>resume</code> will be the
 *  value that this <code>Fiber.yield</code> expression evaluates to.
 */
static mrb_value
fiber_yield(mrb_state *mrb, mrb_value self)
{
  struct mrb_context *c = mrb->c;
  mrb_value *a;
  int len;

  if (!c->prev) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "can't yield from root fiber");
  }
  mrb_get_args(mrb, "*", &a, &len);
  c->prev->status = MRB_FIBER_RUNNING;
  mrb->c = c->prev;
  c->prev = NULL;
  MARK_CONTEXT_MODIFY(mrb->c);
  return fiber_result(mrb, a, len);
}

void
mrb_mruby_fiber_gem_init(mrb_state* mrb)
{
  struct RClass *c;

  c = mrb_define_class(mrb, "Fiber", mrb->object_class);
  MRB_SET_INSTANCE_TT(c, MRB_TT_FIBER);

  mrb_define_method(mrb, c, "initialize", fiber_init,    MRB_ARGS_NONE());
  mrb_define_method(mrb, c, "resume",     fiber_resume,  MRB_ARGS_ANY());
  mrb_define_method(mrb, c, "alive?",     fiber_alive_p, MRB_ARGS_NONE());

  mrb_define_class_method(mrb, c, "yield", fiber_yield, MRB_ARGS_ANY());
}

void
mrb_mruby_fiber_gem_final(mrb_state* mrb)
{
}
