#include "mruby.h"
#include "mruby/range.h"

static mrb_bool
r_le(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_value r = mrb_funcall(mrb, a, "<=>", 1, b); /* compare result */
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  if (mrb_type(r) == MRB_TT_FIXNUM) {
    mrb_int c = mrb_fixnum(r);
    if (c == 0 || c == -1) return TRUE;
  }

  return FALSE;
}

static mrb_bool
r_lt(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_value r = mrb_funcall(mrb, a, "<=>", 1, b);
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  if (mrb_type(r) == MRB_TT_FIXNUM) {
    if (mrb_fixnum(r) == -1) return TRUE;
  }

  return FALSE;
}

/*
 *  call-seq:
 *     rng.cover?(obj)  ->  true or false
 *
 *  Returns <code>true</code> if +obj+ is between the begin and end of
 *  the range.
 *
 *  This tests <code>begin <= obj <= end</code> when #exclude_end? is +false+
 *  and <code>begin <= obj < end</code> when #exclude_end? is +true+.
 *
 *     ("a".."z").cover?("c")    #=> true
 *     ("a".."z").cover?("5")    #=> false
 *     ("a".."z").cover?("cc")   #=> true
 */
static mrb_value
mrb_range_cover(mrb_state *mrb, mrb_value range)
{
  mrb_value val;
  struct RRange *r = mrb_range_ptr(range);
  mrb_value beg, end;

  mrb_get_args(mrb, "o", &val);

  beg = r->edges->beg;
  end = r->edges->end;

  if (r_le(mrb, beg, val)) {
    if (r->excl) {
      if (r_lt(mrb, val, end))
        return mrb_true_value();
    }
    else {
      if (r_le(mrb, val, end))
        return mrb_true_value();
    }
  }

  return mrb_false_value();
}

/*
 *  call-seq:
 *     rng.first    -> obj
 *     rng.first(n) -> an_array
 *
 *  Returns the first object in the range, or an array of the first +n+
 *  elements.
 *
 *    (10..20).first     #=> 10
 *    (10..20).first(3)  #=> [10, 11, 12]
 */
static mrb_value
mrb_range_first(mrb_state *mrb, mrb_value range)
{
  mrb_value num;
  mrb_value array;
  struct RRange *r = mrb_range_ptr(range);

  if (mrb_get_args(mrb, "|o", &num) == 0) {
    return r->edges->beg;
  }

  array = mrb_funcall(mrb, range, "to_a", 0);
  return mrb_funcall(mrb, array, "first", 1, mrb_to_int(mrb, num));
}

/*
 *  call-seq:
 *     rng.last    -> obj
 *     rng.last(n) -> an_array
 *
 *  Returns the last object in the range,
 *  or an array of the last +n+ elements.
 *
 *  Note that with no arguments +last+ will return the object that defines
 *  the end of the range even if #exclude_end? is +true+.
 *
 *    (10..20).last      #=> 20
 *    (10...20).last     #=> 20
 *    (10..20).last(3)   #=> [18, 19, 20]
 *    (10...20).last(3)  #=> [17, 18, 19]
 */
static mrb_value
mrb_range_last(mrb_state *mrb, mrb_value range)
{
  mrb_value num;
  mrb_value array;
  struct RRange *r = mrb_range_ptr(range);

  if (mrb_get_args(mrb, "|o", &num) == 0) {
    return r->edges->end;
  }

  array = mrb_funcall(mrb, range, "to_a", 0);
  return mrb_funcall(mrb, array, "last", 1, mrb_to_int(mrb, num));
}

void
mrb_mruby_range_ext_gem_init(mrb_state* mrb)
{
  struct RClass * s = mrb_class_get(mrb, "Range");

  mrb_define_method(mrb, s, "cover?", mrb_range_cover, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, s, "first",  mrb_range_first, MRB_ARGS_OPT(1));
  mrb_define_method(mrb, s, "last",   mrb_range_last,  MRB_ARGS_OPT(1));
}

void
mrb_mruby_range_ext_gem_final(mrb_state* mrb)
{
}
