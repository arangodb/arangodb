/*
** variable.h - mruby variables
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_VARIABLE_H
#define MRUBY_VARIABLE_H

typedef struct global_variable {
    int   counter;
    mrb_value *data;
    mrb_value (*getter)();
    void  (*setter)();
    //void  (*marker)();
    //int block_trace;
    //struct trace_var *trace;
} global_variable;
struct global_entry {
    global_variable *var;
    mrb_sym id;
};

mrb_value mrb_vm_special_get(mrb_state*, mrb_sym);
void mrb_vm_special_set(mrb_state*, mrb_sym, mrb_value);
mrb_value mrb_vm_iv_get(mrb_state*, mrb_sym);
void mrb_vm_iv_set(mrb_state*, mrb_sym, mrb_value);
mrb_value mrb_vm_cv_get(mrb_state*, mrb_sym);
void mrb_vm_cv_set(mrb_state*, mrb_sym, mrb_value);
mrb_value mrb_vm_const_get(mrb_state*, mrb_sym);
void mrb_vm_const_set(mrb_state*, mrb_sym, mrb_value);
mrb_value mrb_const_get(mrb_state*, mrb_value, mrb_sym);
void mrb_const_set(mrb_state*, mrb_value, mrb_sym, mrb_value);
int mrb_const_defined(mrb_state*, mrb_value, mrb_sym);

mrb_value mrb_obj_iv_get(mrb_state*, struct RObject*, mrb_sym);
void mrb_obj_iv_set(mrb_state*, struct RObject*, mrb_sym, mrb_value);
const char * mrb_class2name(mrb_state *mrb, struct RClass* klass);
mrb_value mrb_iv_get(mrb_state *mrb, mrb_value obj, mrb_sym sym);
void mrb_iv_set(mrb_state *mrb, mrb_value obj, mrb_sym sym, mrb_value v); /* mrb_iv_set */
void mrb_copy_generic_ivar(mrb_value clone, mrb_value obj);
int mrb_const_defined_at(mrb_state *mrb, struct RClass *klass, mrb_sym id);
mrb_value mrb_f_global_variables(mrb_state *mrb, mrb_value self);
mrb_value mrb_gv_get(mrb_state *mrb, mrb_sym sym);
void mrb_gv_set(mrb_state *mrb, mrb_sym sym, mrb_value val);

#endif  /* MRUBY_VARIABLE_H */
