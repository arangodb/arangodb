/*
** range.c - Range class
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/class.h"
#include "mruby/range.h"
#include "mruby/string.h"

#define RANGE_CLASS (mrb_class_get(mrb, "Range"))

static void
range_check(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_value ans;
  enum mrb_vtype ta;
  enum mrb_vtype tb;

  ta = mrb_type(a);
  tb = mrb_type(b);
  if ((ta == MRB_TT_FIXNUM || ta == MRB_TT_FLOAT) &&
      (tb == MRB_TT_FIXNUM || tb == MRB_TT_FLOAT)) {
    return;
  }

  ans =  mrb_funcall(mrb, a, "<=>", 1, b);
  if (mrb_nil_p(ans)) {
    /* can not be compared */
    mrb_raise(mrb, E_ARGUMENT_ERROR, "bad value for range");
  }
}

mrb_value
mrb_range_new(mrb_state *mrb, mrb_value beg, mrb_value end, int excl)
{
  struct RRange *r;

  r = (struct RRange*)mrb_obj_alloc(mrb, MRB_TT_RANGE, RANGE_CLASS);
  range_check(mrb, beg, end);
  r->edges = (mrb_range_edges *)mrb_malloc(mrb, sizeof(mrb_range_edges));
  r->edges->beg = beg;
  r->edges->end = end;
  r->excl = excl;
  return mrb_range_value(r);
}

/*
 *  call-seq:
 *     rng.first    => obj
 *     rng.begin    => obj
 *
 *  Returns the first object in <i>rng</i>.
 */
mrb_value
mrb_range_beg(mrb_state *mrb, mrb_value range)
{
  struct RRange *r = mrb_range_ptr(range);

  return r->edges->beg;
}

/*
 *  call-seq:
 *     rng.end    => obj
 *     rng.last   => obj
 *
 *  Returns the object that defines the end of <i>rng</i>.
 *
 *     (1..10).end    #=> 10
 *     (1...10).end   #=> 10
 */

mrb_value
mrb_range_end(mrb_state *mrb, mrb_value range)
{
  struct RRange *r = mrb_range_ptr(range);

  return r->edges->end;
}

/*
 *  call-seq:
 *     range.exclude_end?    => true or false
 *
 *  Returns <code>true</code> if <i>range</i> excludes its end value.
 */
mrb_value
mrb_range_excl(mrb_state *mrb, mrb_value range)
{
  struct RRange *r = mrb_range_ptr(range);

  return mrb_bool_value(r->excl);
}

static void
range_init(mrb_state *mrb, mrb_value range, mrb_value beg, mrb_value end, int exclude_end)
{
  struct RRange *r = mrb_range_ptr(range);

  range_check(mrb, beg, end);
  r->excl = exclude_end;
  if (!r->edges) {
    r->edges = (mrb_range_edges *)mrb_malloc(mrb, sizeof(mrb_range_edges));
  }
  r->edges->beg = beg;
  r->edges->end = end;
}
/*
 *  call-seq:
 *     Range.new(start, end, exclusive=false)    => range
 *
 *  Constructs a range using the given <i>start</i> and <i>end</i>. If the third
 *  parameter is omitted or is <code>false</code>, the <i>range</i> will include
 *  the end object; otherwise, it will be excluded.
 */

mrb_value
mrb_range_initialize(mrb_state *mrb, mrb_value range)
{
  mrb_value beg, end;
  mrb_bool exclusive;
  int n;

  n = mrb_get_args(mrb, "oo|b", &beg, &end, &exclusive);
  if (n != 3) {
    exclusive = 0;
  }
  /* Ranges are immutable, so that they should be initialized only once. */
  range_init(mrb, range, beg, end, exclusive);
  return range;
}
/*
 *  call-seq:
 *     range == obj    => true or false
 *
 *  Returns <code>true</code> only if
 *  1) <i>obj</i> is a Range,
 *  2) <i>obj</i> has equivalent beginning and end items (by comparing them with <code>==</code>),
 *  3) <i>obj</i> has the same #exclude_end? setting as <i>rng</t>.
 *
 *    (0..2) == (0..2)            #=> true
 *    (0..2) == Range.new(0,2)    #=> true
 *    (0..2) == (0...2)           #=> false
 *
 */

mrb_value
mrb_range_eq(mrb_state *mrb, mrb_value range)
{
  struct RRange *rr;
  struct RRange *ro;
  mrb_value obj;

  mrb_get_args(mrb, "o", &obj);

  if (mrb_obj_equal(mrb, range, obj)) return mrb_true_value();
  if (!mrb_obj_is_instance_of(mrb, obj, mrb_obj_class(mrb, range))) { /* same class? */
    return mrb_false_value();
  }

  rr = mrb_range_ptr(range);
  ro = mrb_range_ptr(obj);
  if (!mrb_bool(mrb_funcall(mrb, rr->edges->beg, "==", 1, ro->edges->beg)) ||
      !mrb_bool(mrb_funcall(mrb, rr->edges->end, "==", 1, ro->edges->end)) ||
      rr->excl != ro->excl) {
    return mrb_false_value();
  }
  return mrb_true_value();
}

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
r_gt(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_value r = mrb_funcall(mrb, a, "<=>", 1, b);
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  if (mrb_type(r) == MRB_TT_FIXNUM) {
    if (mrb_fixnum(r) == 1) return TRUE;
  }

  return FALSE;
}

static mrb_bool
r_ge(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_value r = mrb_funcall(mrb, a, "<=>", 1, b); /* compare result */
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  if (mrb_type(r) == MRB_TT_FIXNUM) {
    mrb_int c = mrb_fixnum(r);
    if (c == 0 || c == 1) return TRUE;
  }

  return FALSE;
}

/*
 *  call-seq:
 *     range === obj       =>  true or false
 *     range.member?(val)  =>  true or false
 *     range.include?(val) =>  true or false
 *
 */
mrb_value
mrb_range_include(mrb_state *mrb, mrb_value range)
{
  mrb_value val;
  struct RRange *r = mrb_range_ptr(range);
  mrb_value beg, end;
  mrb_bool include_p;

  mrb_get_args(mrb, "o", &val);

  beg = r->edges->beg;
  end = r->edges->end;
  include_p = r_le(mrb, beg, val) && /* beg <= val */
              ((r->excl && r_gt(mrb, end, val)) || /* end >  val */
              (r_ge(mrb, end, val))); /* end >= val */

  return mrb_bool_value(include_p);
}

/*
 *  call-seq:
 *     rng.each {| i | block } => rng
 *
 *  Iterates over the elements <i>rng</i>, passing each in turn to the
 *  block. You can only iterate if the start object of the range
 *  supports the +succ+ method (which means that you can't iterate over
 *  ranges of +Float+ objects).
 *
 *     (10..15).each do |n|
 *        print n, ' '
 *     end
 *
 *  <em>produces:</em>
 *
 *     10 11 12 13 14 15
 */

mrb_value
mrb_range_each(mrb_state *mrb, mrb_value range)
{
    return range;
}

mrb_int
mrb_range_beg_len(mrb_state *mrb, mrb_value range, mrb_int *begp, mrb_int *lenp, mrb_int len)
{
  mrb_int beg, end, b, e;
  struct RRange *r = mrb_range_ptr(range);

  if (mrb_type(range) != MRB_TT_RANGE) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected Range.");
  }

  beg = b = mrb_fixnum(r->edges->beg);
  end = e = mrb_fixnum(r->edges->end);

  if (beg < 0) {
    beg += len;
    if (beg < 0) return FALSE;
  }

  if (beg > len) return FALSE;
  if (end > len) end = len;

  if (end < 0) end += len;
  if (!r->excl && end < len) end++;  /* include end point */
  len = end - beg;
  if (len < 0) len = 0;

  *begp = beg;
  *lenp = len;
  return TRUE;
}

/* 15.2.14.4.12(x) */
/*
 * call-seq:
 *   rng.to_s   -> string
 *
 * Convert this range object to a printable form.
 */

static mrb_value
range_to_s(mrb_state *mrb, mrb_value range)
{
  mrb_value str, str2;
  struct RRange *r = mrb_range_ptr(range);

  str  = mrb_obj_as_string(mrb, r->edges->beg);
  str2 = mrb_obj_as_string(mrb, r->edges->end);
  str  = mrb_str_dup(mrb, str);
  mrb_str_cat(mrb, str, "...", r->excl ? 3 : 2);
  mrb_str_append(mrb, str, str2);

  return str;
}

/* 15.2.14.4.13(x) */
/*
 * call-seq:
 *   rng.inspect  -> string
 *
 * Convert this range object to a printable form (using
 * <code>inspect</code> to convert the start and end
 * objects).
 */

static mrb_value
range_inspect(mrb_state *mrb, mrb_value range)
{
  mrb_value str, str2;
  struct RRange *r = mrb_range_ptr(range);

  str  = mrb_inspect(mrb, r->edges->beg);
  str2 = mrb_inspect(mrb, r->edges->end);
  str  = mrb_str_dup(mrb, str);
  mrb_str_cat(mrb, str, "...", r->excl ? 3 : 2);
  mrb_str_append(mrb, str, str2);

  return str;
}

/* 15.2.14.4.14(x) */
/*
 *  call-seq:
 *     rng.eql?(obj)    -> true or false
 *
 *  Returns <code>true</code> only if <i>obj</i> is a Range, has equivalent
 *  beginning and end items (by comparing them with #eql?), and has the same
 *  #exclude_end? setting as <i>rng</i>.
 *
 *    (0..2).eql?(0..2)            #=> true
 *    (0..2).eql?(Range.new(0,2))  #=> true
 *    (0..2).eql?(0...2)           #=> false
 *
 */

static mrb_value
range_eql(mrb_state *mrb, mrb_value range)
{
  mrb_value obj;
  struct RRange *r, *o;

  mrb_get_args(mrb, "o", &obj);

  if (mrb_obj_equal(mrb, range, obj)) return mrb_true_value();
  if (!mrb_obj_is_kind_of(mrb, obj, RANGE_CLASS)) {
    return mrb_false_value();
  }
  if (mrb_type(obj) != MRB_TT_RANGE) return mrb_false_value();

  r = mrb_range_ptr(range);
  o = mrb_range_ptr(obj);
  if (!mrb_eql(mrb, r->edges->beg, o->edges->beg) ||
      !mrb_eql(mrb, r->edges->end, o->edges->end) ||
      (r->excl != o->excl)) {
    return mrb_false_value();
  }
  return mrb_true_value();
}

/* 15.2.14.4.15(x) */
mrb_value
range_initialize_copy(mrb_state *mrb, mrb_value copy)
{
  mrb_value src;
  struct RRange *r;

  mrb_get_args(mrb, "o", &src);

  if (mrb_obj_equal(mrb, copy, src)) return copy;
  if (!mrb_obj_is_instance_of(mrb, src, mrb_obj_class(mrb, copy))) {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }

  r = mrb_range_ptr(src);
  range_init(mrb, copy, r->edges->beg, r->edges->end, r->excl);

  return copy;
}

void
mrb_init_range(mrb_state *mrb)
{
  struct RClass *r;

  r = mrb_define_class(mrb, "Range", mrb->object_class);
  MRB_SET_INSTANCE_TT(r, MRB_TT_RANGE);

  mrb_include_module(mrb, r, mrb_class_get(mrb, "Enumerable"));

  mrb_define_method(mrb, r, "begin",           mrb_range_beg,         MRB_ARGS_NONE()); /* 15.2.14.4.3  */
  mrb_define_method(mrb, r, "end",             mrb_range_end,         MRB_ARGS_NONE()); /* 15.2.14.4.5  */
  mrb_define_method(mrb, r, "==",              mrb_range_eq,          MRB_ARGS_REQ(1)); /* 15.2.14.4.1  */
  mrb_define_method(mrb, r, "===",             mrb_range_include,     MRB_ARGS_REQ(1)); /* 15.2.14.4.2  */
  mrb_define_method(mrb, r, "each",            mrb_range_each,        MRB_ARGS_NONE()); /* 15.2.14.4.4  */
  mrb_define_method(mrb, r, "exclude_end?",    mrb_range_excl,        MRB_ARGS_NONE()); /* 15.2.14.4.6  */
  mrb_define_method(mrb, r, "first",           mrb_range_beg,         MRB_ARGS_NONE()); /* 15.2.14.4.7  */
  mrb_define_method(mrb, r, "include?",        mrb_range_include,     MRB_ARGS_REQ(1)); /* 15.2.14.4.8  */
  mrb_define_method(mrb, r, "initialize",      mrb_range_initialize,  MRB_ARGS_ANY());  /* 15.2.14.4.9  */
  mrb_define_method(mrb, r, "last",            mrb_range_end,         MRB_ARGS_NONE()); /* 15.2.14.4.10 */
  mrb_define_method(mrb, r, "member?",         mrb_range_include,     MRB_ARGS_REQ(1)); /* 15.2.14.4.11 */

  mrb_define_method(mrb, r, "to_s",            range_to_s,            MRB_ARGS_NONE()); /* 15.2.14.4.12(x) */
  mrb_define_method(mrb, r, "inspect",         range_inspect,         MRB_ARGS_NONE()); /* 15.2.14.4.13(x) */
  mrb_define_method(mrb, r, "eql?",            range_eql,             MRB_ARGS_REQ(1)); /* 15.2.14.4.14(x) */
  mrb_define_method(mrb, r, "initialize_copy", range_initialize_copy, MRB_ARGS_REQ(1)); /* 15.2.14.4.15(x) */
}
