/*
** array.h - Array class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_ARRAY_H
#define MRUBY_ARRAY_H

struct RArray {
  MRUBY_OBJECT_HEADER;
  size_t len;
  size_t capa;
  mrb_value *buf;
};

#define mrb_ary_ptr(v)    ((struct RArray*)((v).value.p))
#define mrb_ary_value(p)  mrb_obj_value((void*)(p))
#define RARRAY(v)  ((struct RArray*)((v).value.p))

#define RARRAY_LEN(a) (RARRAY(a)->len)
#define RARRAY_PTR(a) (RARRAY(a)->buf)

mrb_value mrb_ary_new_capa(mrb_state*, size_t);
mrb_value mrb_ary_new(mrb_state *mrb);
mrb_value mrb_ary_new_elts(mrb_state *mrb, long n, const mrb_value *elts);
void mrb_ary_concat(mrb_state*, mrb_value, mrb_value);
mrb_value mrb_ary_splat(mrb_state*, mrb_value);
void mrb_ary_push(mrb_state*, mrb_value, mrb_value);
mrb_value mrb_ary_pop(mrb_state *mrb, mrb_value ary);
mrb_value mrb_ary_new_from_values(mrb_state *mrb, mrb_value *vals, size_t size);
mrb_value mrb_ary_aget(mrb_state *mrb, mrb_value self);
mrb_value mrb_ary_ref(mrb_state *mrb, mrb_value ary, mrb_int n);
void mrb_ary_set(mrb_state *mrb, mrb_value ary, mrb_int n, mrb_value val);
int mrb_ary_len(mrb_state *mrb, mrb_value ary);
mrb_value mrb_ary_replace_m(mrb_state *mrb, mrb_value self);
void mrb_ary_replace(mrb_state *mrb, struct RArray *a, mrb_value *argv, size_t len);
mrb_value mrb_check_array_type(mrb_state *mrb, mrb_value self);
mrb_value mrb_ary_unshift(mrb_state *mrb, mrb_value self, mrb_value item);
mrb_value mrb_ary_new4(mrb_state *mrb, long n, const mrb_value *elts);
mrb_value mrb_assoc_new(mrb_state *mrb, mrb_value car, mrb_value cdr);
mrb_value mrb_ary_entry(mrb_value ary, long offset);
void mrb_mem_clear(mrb_value *mem, long size);
mrb_value mrb_ary_tmp_new(mrb_state *mrb, long capa);
mrb_value mrb_ary_sort(mrb_state *mrb, mrb_value ary);
mrb_value mrb_ary_shift(mrb_state *mrb, mrb_value self);

#endif  /* MRUBY_ARRAY_H */
