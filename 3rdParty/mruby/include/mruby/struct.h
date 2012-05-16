/*
** struct.h - Struct class
**
** See Copyright Notice in mruby.h
*/

#ifndef MSTRUCT_H
#define MSTRUCT_H

struct RStruct {
    struct RBasic basic;
    long len;
    mrb_value *ptr;
};
#define RSTRUCT(st)     ((struct RStruct*)((st).value.p))
#define RSTRUCT_LEN(st) ((int)(RSTRUCT(st)->len))
#define RSTRUCT_PTR(st) (RSTRUCT(st)->ptr)

mrb_value mrb_yield_values(int n, ...);
mrb_value mrb_mod_module_eval(mrb_state *mrb, int argc, mrb_value *argv, mrb_value mod);

#endif //MSTRUCT_H
