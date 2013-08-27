#include <mruby.h>
#include <mruby/gc.h>
#include <mruby/hash.h>
#include <mruby/value.h>

struct os_count_struct {
  size_t total;
  size_t freed;
  size_t counts[MRB_TT_MAXDEFINE+1];
};

void
os_count_object_type(mrb_state *mrb, struct RBasic* obj, void *data)
{
  struct os_count_struct* obj_count;
  obj_count = (struct os_count_struct*)(data);

  if (is_dead(mrb, obj)) {
    obj_count->freed++;
  } else {
    obj_count->counts[obj->tt]++;
    obj_count->total++;
  }
}

/*
 *  call-seq:
 *     ObjectSpace.count_objects([result_hash]) -> hash
 *
 *  Counts objects for each type.
 *
 *  It returns a hash, such as:
 *  {
 *    :TOTAL=>10000,
 *    :FREE=>3011,
 *    :MRB_TT_OBJECT=>6,
 *    :MRB_TT_CLASS=>404,
 *    # ...
 *  }
 *
 *  If the optional argument +result_hash+ is given,
 *  it is overwritten and returned. This is intended to avoid probe effect.
 *
 */

mrb_value
os_count_objects(mrb_state *mrb, mrb_value self)
{
  struct os_count_struct obj_count;
  size_t i;
  mrb_value hash;

  if (mrb_get_args(mrb, "|H", &hash) == 0) {
    hash = mrb_hash_new(mrb);
  }

  if (!mrb_test(mrb_hash_empty_p(mrb, hash))) {
    mrb_hash_clear(mrb, hash);
  }

  for (i = 0; i <= MRB_TT_MAXDEFINE; i++) {
    obj_count.counts[i] = 0;
  }
  obj_count.total = 0;
  obj_count.freed = 0;

  mrb_objspace_each_objects(mrb, os_count_object_type, &obj_count);

  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, "TOTAL")), mrb_fixnum_value(obj_count.total));
  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, "FREE")), mrb_fixnum_value(obj_count.freed));

  for (i = 0; i < MRB_TT_MAXDEFINE; i++) {
    mrb_value type;
    switch (i) {
#define COUNT_TYPE(t) case (t): type = mrb_symbol_value(mrb_intern_cstr(mrb, #t)); break;
      COUNT_TYPE(MRB_TT_FALSE);
      COUNT_TYPE(MRB_TT_FREE);
      COUNT_TYPE(MRB_TT_TRUE);
      COUNT_TYPE(MRB_TT_FIXNUM);
      COUNT_TYPE(MRB_TT_SYMBOL);
      COUNT_TYPE(MRB_TT_UNDEF);
      COUNT_TYPE(MRB_TT_FLOAT);
      COUNT_TYPE(MRB_TT_VOIDP);
      COUNT_TYPE(MRB_TT_OBJECT);
      COUNT_TYPE(MRB_TT_CLASS);
      COUNT_TYPE(MRB_TT_MODULE);
      COUNT_TYPE(MRB_TT_ICLASS);
      COUNT_TYPE(MRB_TT_SCLASS);
      COUNT_TYPE(MRB_TT_PROC);
      COUNT_TYPE(MRB_TT_ARRAY);
      COUNT_TYPE(MRB_TT_HASH);
      COUNT_TYPE(MRB_TT_STRING);
      COUNT_TYPE(MRB_TT_RANGE);
      COUNT_TYPE(MRB_TT_EXCEPTION);
      COUNT_TYPE(MRB_TT_FILE);
      COUNT_TYPE(MRB_TT_ENV);
      COUNT_TYPE(MRB_TT_DATA);
#undef COUNT_TYPE
    default:
      type = mrb_fixnum_value(i); break;
    }
    if (obj_count.counts[i])
      mrb_hash_set(mrb, hash, type, mrb_fixnum_value(obj_count.counts[i]));
  }

  return hash;
}

void
mrb_mruby_objectspace_gem_init(mrb_state* mrb) {
  struct RClass *os = mrb_define_module(mrb, "ObjectSpace");
  mrb_define_class_method(mrb, os, "count_objects", os_count_objects, MRB_ARGS_ANY());
}

void
mrb_mruby_objectspace_gem_final(mrb_state* mrb) {
}
