/*
** minimain.c -
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/proc.h"

#if 0
#include "opcode.h"

mrb_code fib_iseq[256];

int fib_syms[4];

mrb_irep fib_irep = {
  1,
  MRB_IREP_NOFREE,
  2,
  5,
  fib_iseq,
  NULL,
  fib_syms,

  256, 0, 4,
};

mrb_code main_iseq[256];

int main_syms[2];

mrb_irep main_irep = {
  0,
  MRB_IREP_NOFREE,
  1,
  3,
  main_iseq,
  NULL,
  main_syms,

  256, 0, 2,
};

int
main(int argc, char **argv)
{
  mrb_state *mrb = mrb_open();
  int sirep = mrb->irep_len;
  int n;

  main_syms[0] = mrb_intern(mrb, "fib");
  main_syms[1] = mrb_intern(mrb, "p");
  n = 0;

  main_iseq[n++] = MKOP_AB(OP_LAMBDA, 1, 1);     /* r1 := lambda(1) */
  main_iseq[n++] = MKOP_AB(OP_METHOD, 1, 0);      /* defmethod(r1) */
  main_iseq[n++] = MKOP_AB(OP_MOVE, 1, 0);        /* r1 := r0 */
  main_iseq[n++] = MKOP_AB(OP_MOVE, 2, 0);        /* r2 := r0 */
  main_iseq[n++] = MKOP_AsBx(OP_LOADI, 3, 35);    /* r3 := 20 */
  main_iseq[n++] = MKOP_ABC(OP_SEND, 2, 0, 1);    /* r2 .fib r3 */
  main_iseq[n++] = MKOP_ABC(OP_SEND, 1, 1, 1);    /* r1 .p r2 */
  main_iseq[n++] = MKOP_ABC(OP_STOP, 1, 1, 2);    /* stop */
  main_irep.ilen = n;
  main_irep.idx  = sirep;

  fib_syms[0] = mrb_intern(mrb, "<");
  fib_syms[1] = mrb_intern(mrb, "-");
  fib_syms[2] = mrb_intern(mrb, "+");
  fib_syms[3] = mrb_intern(mrb, "fib");
  n = 0;

  fib_iseq[n++] = MKOP_AB(OP_MOVE, 2, 1);        /* r2 := r1 */
  fib_iseq[n++] = MKOP_AsBx(OP_LOADI, 3, 3);     /* r3 := 2 */
  fib_iseq[n++] = MKOP_ABC(OP_LT, 2, 0, 2);      /* r2 .< r3 */
  fib_iseq[n++] = MKOP_AsBx(OP_JMPNOT, 2, 2);    /* ifnot r2 :else */
  fib_iseq[n++] = MKOP_AsBx(OP_LOADI, 2, 1);     /* r6 := 1 */
  fib_iseq[n++] = MKOP_A(OP_RETURN, 2);          /* return r2 */
  fib_iseq[n++] = MKOP_AB(OP_MOVE, 3, 0);        /* r3 := r0  :else */
  fib_iseq[n++] = MKOP_AB(OP_MOVE, 4, 1);        /* r4 := r1 */
  fib_iseq[n++] = MKOP_ABC(OP_SUBI, 4, 1, 2);    /* r4 .- 2 */
  fib_iseq[n++] = MKOP_ABC(OP_SEND, 3, 3, 1);    /* r3 .fib r4 */
  fib_iseq[n++] = MKOP_AB(OP_MOVE, 4, 0);        /* r4 := r0 */
  fib_iseq[n++] = MKOP_AB(OP_MOVE, 5, 1);        /* r5 := r1 */
  fib_iseq[n++] = MKOP_ABC(OP_SUBI, 5, 1, 1);    /* r5 .- 1 */
  fib_iseq[n++] = MKOP_ABC(OP_SEND, 4, 3, 1);    /* r4 .fib :r5 */
  fib_iseq[n++] = MKOP_ABC(OP_ADD, 3, 2, 1);     /* r3 .+ r4 */
  fib_iseq[n++] = MKOP_A(OP_RETURN, 3);          /* return r3 */
  fib_irep.ilen = n;
  fib_irep.idx  = sirep+1;

  mrb_add_irep(mrb, sirep+2);
  mrb->irep[sirep  ] = &main_irep;
  mrb->irep[sirep+1] = &fib_irep;

  mrb_run(mrb, mrb_proc_new(mrb, &main_irep), mrb_nil_value());
}

#else
#include "compile.h"

int
main()
{
  mrb_state *mrb = mrb_open();
  int n;

  n = mrb_compile_string(mrb, "\
def fib(n)\n\
  if n<2\n\
    n\n\
  else\n\
    fib(n-2)+fib(n-1)\n\
  end\n\
end\n\
p(fib(30), \"\\n\")\n\
");
  mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_nil_value());

  return 0;
}

#endif
