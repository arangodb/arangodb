/*
** array.c - Array class
** 
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/array.h"
#include <string.h>
#include "mruby/string.h"
#include "mruby/class.h"

#ifdef INCLUDE_REGEXP
  #define mrb_usascii_str_new2 mrb_usascii_str_new_cstr
#else
  #define mrb_usascii_str_new2 mrb_str_new_cstr
  #define mrb_usascii_str_new  mrb_str_new
#endif
mrb_value mrb_exec_recursive_paired(mrb_state *mrb, mrb_value (*func) (mrb_state *, mrb_value, mrb_value, int),
                                   mrb_value obj, mrb_value paired_obj, void* arg);

//#define ARY_DEFAULT_LEN  16
#define ARY_DEFAULT_LEN   4
#define ARY_SHRINK_RATIO  5 /* must be larger than 2 */
#ifdef LONG_MAX
#  define ARY_MAX_SIZE (LONG_MAX / sizeof(mrb_value))
#endif

static inline mrb_value
ary_elt(mrb_value ary, long offset)
{
  if (RARRAY_LEN(ary) == 0) return mrb_nil_value();
  if (offset < 0 || RARRAY_LEN(ary) <= offset) {
    return mrb_nil_value();
  }
  return RARRAY_PTR(ary)[offset];
}

mrb_value
mrb_ary_new_capa(mrb_state *mrb, size_t capa)
{
  struct RArray *a;
  size_t blen;

#ifdef LONG_MAX
  if (capa > ARY_MAX_SIZE) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "ary size too big");
  }
#endif
  if (capa < ARY_DEFAULT_LEN) {
    capa = ARY_DEFAULT_LEN;
  }
  blen = capa * sizeof(mrb_value) ;
  if (blen < capa) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "ary size too big");
  }

  a = mrb_obj_alloc(mrb, MRB_TT_ARRAY, mrb->array_class);
  a->buf = mrb_malloc(mrb, blen);
  memset(a->buf, 0, blen);
  a->capa = capa;
  a->len = 0;

  return mrb_obj_value(a);
}

mrb_value
mrb_ary_new(mrb_state *mrb)
{
  return mrb_ary_new_capa(mrb, 0);
}

mrb_value
mrb_ary_new_from_values(mrb_state *mrb, mrb_value *vals, size_t size)
{
  mrb_value ary;
  struct RArray *a;

  ary = mrb_ary_new_capa(mrb, size);
  a = mrb_ary_ptr(ary);
  memcpy(a->buf, vals, sizeof(mrb_value)*size);
  a->len = size;

  return ary;
}

mrb_value
mrb_assoc_new(mrb_state *mrb, mrb_value car, mrb_value cdr)
{
  mrb_value arv[2];
  arv[0] = car;
  arv[1] = cdr;
  return mrb_ary_new_from_values(mrb, arv, 2);
}

void
ary_fill_with_nil(mrb_value *buf, size_t size)
{
  mrb_value nil = mrb_nil_value();

  while((int)(size--)) {
    *buf++ = nil;
  }
}

void
mrb_ary_expand_capa(mrb_state *mrb, struct RArray *a, size_t len)
{
  size_t capa = a->capa;

#ifdef LONG_MAX
  if (len > ARY_MAX_SIZE) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
#endif

  while(capa < len) {
    if (capa == 0) {
      capa = ARY_DEFAULT_LEN;
    }
    else {
      capa *= 2;
    }
  }

#ifdef LONG_MAX
  if (capa > ARY_MAX_SIZE) capa = ARY_MAX_SIZE; /* len <= capa <= ARY_MAX_SIZE */
#endif

  if (capa > a->capa) {
    a->capa = capa;
    a->buf = mrb_realloc(mrb, a->buf, sizeof(mrb_value)*capa);
  }
}

void
mrb_ary_shrink_capa(mrb_state *mrb, struct RArray *a)
{
  size_t capa = a->capa;

  if (capa < ARY_DEFAULT_LEN * 2) return;
  if (capa <= a->len * ARY_SHRINK_RATIO) return;

  do {
    capa /= 2;
    if (capa < ARY_DEFAULT_LEN) {
      capa = ARY_DEFAULT_LEN;
      break;
    }
  } while(capa > a->len * ARY_SHRINK_RATIO);

  if (capa > a->len && capa < a->capa) {
    a->capa = capa;
    a->buf = mrb_realloc(mrb, a->buf, sizeof(mrb_value)*capa);
  }
}

mrb_value
mrb_ary_s_create(mrb_state *mrb, mrb_value self)
{
  mrb_value *vals;
  int len;

  mrb_get_args(mrb, "*", &vals, &len);
  return mrb_ary_new_from_values(mrb, vals, (size_t)len);
}

void
mrb_ary_concat(mrb_state *mrb, mrb_value self, mrb_value other)
{
  struct RArray *a1 = mrb_ary_ptr(self);
  struct RArray *a2 = mrb_ary_ptr(other);
  size_t len = a1->len + a2->len;

  if (a1->capa < len) mrb_ary_expand_capa(mrb, a1, len);
  memcpy(a1->buf+a1->len, a2->buf, sizeof(mrb_value)*a2->len);
  mrb_write_barrier(mrb, (struct RBasic*)a1);
  a1->len = len;
}

mrb_value
mrb_ary_concat_m(mrb_state *mrb, mrb_value self)
{
  mrb_value other;

  mrb_get_args(mrb, "o", &other);
  if (mrb_type(other) != MRB_TT_ARRAY) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "expected Array");
  }
  mrb_ary_concat(mrb, self, other);
  return self;
}

mrb_value
mrb_ary_plus(mrb_state *mrb, mrb_value self)
{
  struct RArray *a1 = mrb_ary_ptr(self);
  struct RArray *a2;
  mrb_value other;
  mrb_value ary;

  mrb_get_args(mrb, "o", &other);
  if (mrb_type(other) != MRB_TT_ARRAY) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "expected Array");
  }

  ary = mrb_ary_new_capa(mrb, a1->len + RARRAY_LEN(other));
  a2 = mrb_ary_ptr(ary);
  memcpy(a2->buf, a1->buf, sizeof(mrb_value)*a1->len);
  memcpy(a2->buf + a1->len, RARRAY_PTR(other), sizeof(mrb_value)*RARRAY_LEN(other));
  a2->len = a1->len + RARRAY_LEN(other);

  return ary;
}

static mrb_value
recursive_cmp(mrb_state *mrb, mrb_value ary1, mrb_value ary2, int recur)
{
  long i, len;

  if (recur) return mrb_undef_value(); /* Subtle! */
  len = RARRAY_LEN(ary1);
  if (len > RARRAY_LEN(ary2)) {
    len = RARRAY_LEN(ary2);
  }

  for (i=0; i<len; i++) {
    mrb_value r = mrb_funcall(mrb, ary_elt(ary1, i), "<=>", 1, ary_elt(ary2, i));
    if (mrb_type(r) != MRB_TT_FIXNUM || mrb_fixnum(r) != 0) return r;
  }

  return mrb_undef_value();
}

/*
 *  call-seq:
 *     ary <=> other_ary   ->  -1, 0, +1 or nil
 *
 *  Comparison---Returns an integer (-1, 0, or +1)
 *  if this array is less than, equal to, or greater than <i>other_ary</i>.
 *  Each object in each array is compared (using <=>). If any value isn't
 *  equal, then that inequality is the return value. If all the
 *  values found are equal, then the return is based on a
 *  comparison of the array lengths.  Thus, two arrays are
 *  ``equal'' according to <code>Array#<=></code> if and only if they have
 *  the same length and the value of each element is equal to the
 *  value of the corresponding element in the other array.
 *
 *     [ "a", "a", "c" ]    <=> [ "a", "b", "c" ]   #=> -1
 *     [ 1, 2, 3, 4, 5, 6 ] <=> [ 1, 2 ]            #=> +1
 *
 */
mrb_value
mrb_ary_cmp(mrb_state *mrb, mrb_value ary1)
{
  mrb_value ary2;
  struct RArray *a1, *a2;
  mrb_value r;
  long len;

  mrb_get_args(mrb, "o", &ary2);
  if (mrb_type(ary2) != MRB_TT_ARRAY) return mrb_nil_value();
  a1 = RARRAY(ary1); a2 = RARRAY(ary2);
  if (a1->len == a2->len && a1->buf == a2->buf) return mrb_fixnum_value(0);
  r = mrb_exec_recursive_paired(mrb, recursive_cmp, ary1, ary2, &ary2);
  if (mrb_type(r) != MRB_TT_UNDEF) return r;
  len = a1->len - a2->len;
  return mrb_fixnum_value((len == 0)? 0: (len > 0)? 1: -1);
}

void
mrb_ary_replace(mrb_state *mrb, struct RArray *a, mrb_value *argv, size_t len)
{
  if (a->capa < len) mrb_ary_expand_capa(mrb, a, len);
  memcpy(a->buf, argv, sizeof(mrb_value)*len);
  mrb_write_barrier(mrb, (struct RBasic*)a);
  a->len = len;
}

mrb_value
mrb_ary_replace_m(mrb_state *mrb, mrb_value self)
{
  mrb_value other;

  mrb_get_args(mrb, "o", &other);
  mrb_ary_replace(mrb, mrb_ary_ptr(self), RARRAY_PTR(other), RARRAY_LEN(other));

  return self;
}

mrb_value
mrb_ary_times(mrb_state *mrb, mrb_value self)
{
  struct RArray *a1 = mrb_ary_ptr(self);
  struct RArray *a2;
  mrb_value ary;
  mrb_value *buf;
  mrb_int times;
  //size_t len;

  mrb_get_args(mrb, "i", &times);
  if (times < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "negative argument");
  }
  if (times == 0) return mrb_ary_new(mrb);

  ary = mrb_ary_new_capa(mrb, a1->len * times);
  a2 = mrb_ary_ptr(ary);
  buf = a2->buf;
  while(times--) {
    memcpy(buf, a1->buf, sizeof(mrb_value)*(a1->len));
    buf += a1->len;
    a2->len += a1->len;
  }

  return ary;
}

static void
ary_reverse(struct RArray *a)
{
  mrb_value *p1, *p2;

  p1 = a->buf;
  p2 = a->buf + a->len - 1;

  while(p1 < p2) {
    mrb_value tmp = *p1;
    *p1++ = *p2;
    *p2-- = tmp;
  }
}

mrb_value
mrb_ary_reverse_bang(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);

  if (a->len > 1) {
    ary_reverse(a);
  }
  return self;
}

mrb_value
mrb_ary_reverse(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);
  mrb_value ary;

  ary = mrb_ary_new_capa(mrb, a->len);
  if (a->len > 0) {
    mrb_ary_replace(mrb, mrb_ary_ptr(ary), a->buf, a->len);
    ary_reverse(mrb_ary_ptr(ary));
  }
  return ary;
}

mrb_value
mrb_ary_new4(mrb_state *mrb, long n, const mrb_value *elts)
{
  mrb_value ary;

  ary = mrb_ary_new_capa(mrb, n);//mrb_ary_new2(n);
  if (n > 0 && elts) {
    memcpy(RARRAY_PTR(ary), elts, sizeof(mrb_value)*n);
    RARRAY_LEN(ary) = n; //ARY_SET_LEN(ary, n);
  }

  return ary;
}

mrb_value
mrb_ary_new_elts(mrb_state *mrb, long n, const mrb_value *elts)
{
  return mrb_ary_new4(mrb, n, elts);
}

void
mrb_ary_push(mrb_state *mrb, mrb_value ary, mrb_value elem) /* mrb_ary_push */
{
  struct RArray *a = mrb_ary_ptr(ary);

  if (a->len == a->capa) mrb_ary_expand_capa(mrb, a, a->len + 1);
  a->buf[a->len++] = elem;
  mrb_write_barrier(mrb, (struct RBasic*)a);
}

mrb_value
mrb_ary_pop(mrb_state *mrb, mrb_value ary)
{
  struct RArray *a = mrb_ary_ptr(ary);

  if (a->len == 0) return mrb_nil_value();

  return a->buf[--a->len];
}

mrb_value
mrb_ary_push_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int len;

  mrb_get_args(mrb, "*", &argv, &len);
  while(len--) {
    mrb_ary_push(mrb, self, *argv++);
  }

  return self;
}

mrb_value
mrb_ary_pop_m(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);

  return ((a->len == 0)? mrb_nil_value(): mrb_ary_pop(mrb, self));
}

mrb_value
mrb_ary_shift(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);
  mrb_value *buf = a->buf;
  size_t size = a->len;
  mrb_value val;

  if (size == 0) return mrb_nil_value();

  val = *buf;
  while((int)(--size)) {
    *buf = *(buf+1);
    ++buf;
  }
  --a->len;

  return val;
}

/* self = [1,2,3]
   item = 0
   self.unshift item
   p self #=> [0, 1, 2, 3] */
mrb_value
mrb_ary_unshift(mrb_state *mrb, mrb_value self, mrb_value item)
{
  struct RArray *a = mrb_ary_ptr(self);

  if (a->capa < a->len + 1) mrb_ary_expand_capa(mrb, a, a->len + 1);
  memmove(a->buf + 1, a->buf, sizeof(mrb_value)*a->len);
  memcpy(a->buf, &item, sizeof(mrb_value));
  a->len += 1;
  mrb_write_barrier(mrb, (struct RBasic*)a);

  return self;
}

mrb_value
mrb_ary_unshift_m(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);
  mrb_value *vals;
  int len;

  mrb_get_args(mrb, "*", &vals, &len);
  if (len == 0) return self;
  if (a->capa < a->len + len) mrb_ary_expand_capa(mrb, a, a->len + len);
  memmove(a->buf + len, a->buf, sizeof(mrb_value)*a->len);
  memcpy(a->buf, vals, sizeof(mrb_value)*len);
  a->len += len;
  mrb_write_barrier(mrb, (struct RBasic*)a);

  return self;
}

mrb_value
mrb_ary_ref(mrb_state *mrb, mrb_value ary, mrb_int n)
{
  struct RArray *a = mrb_ary_ptr(ary);

  /* range check */
  if (n < 0) n += a->len;
  if (n < 0 || a->len <= (size_t)n) return mrb_nil_value();

  return a->buf[n];
}

void
mrb_ary_set(mrb_state *mrb, mrb_value ary, mrb_int n, mrb_value val) /* rb_ary_store */
{
  struct RArray *a = mrb_ary_ptr(ary);

  /* range check */
  if (n < 0) n += a->len;
  if (n < 0) {
    mrb_raise(mrb, E_INDEX_ERROR, "index %ld out of array", n - a->len);
  }
  if (a->len <= (size_t)n) {
    if (a->capa <= (size_t)n) mrb_ary_expand_capa(mrb, a, n + 1);
    ary_fill_with_nil(a->buf + a->len, n + 1 - a->len);
    a->len = n + 1;
  }

  a->buf[n] = val;
  mrb_write_barrier(mrb, (struct RBasic*)a);
}

mrb_value
mrb_ary_splice(mrb_state *mrb, mrb_value ary, mrb_int head, mrb_int len, mrb_value rpl)
{
  struct RArray *a = mrb_ary_ptr(ary);
  mrb_int tail;
  size_t size;
  mrb_value *argv;
  int i, argc;

  /* range check */
  if (head < 0) head += a->len;
  if (head < 0) {
    mrb_raise(mrb, E_INDEX_ERROR, "index is out of array");
  }
  tail = head + len;

  /* size check */
  if (mrb_type(rpl) == MRB_TT_ARRAY) {
    argc = RARRAY_LEN(rpl);
    argv = RARRAY_PTR(rpl);
  }
  else {
    argc = 1;
    argv = &rpl;
  }
  size = head + argc;

  if (tail < a->len) size += a->len - tail;

  if (size > a->capa) mrb_ary_expand_capa(mrb, a, size);

  if (head > a->len) {
    ary_fill_with_nil(a->buf + a->len, (size_t)(head - a->len));
  }
  else if (head < a->len) {
    memmove(a->buf + head + argc, a->buf + tail, sizeof(mrb_value)*(a->len - tail));
  }

  for(i = 0; i < argc; i++) {
    *(a->buf + head + i) = *(argv + i);
  }

  a->len = size;

  return ary;
}

int
mrb_ary_alen(mrb_state *mrb, mrb_value ary)
{
  return RARRAY_LEN(ary);
}

mrb_value
mrb_ary_aget(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);
  mrb_int index, len;
  mrb_value *argv;
  int size;

  mrb_get_args(mrb, "i*", &index, &argv, &size);
  switch(size) {
  case 0:
    return mrb_ary_ref(mrb, self, index);

  case 1:
    if (mrb_type(argv[0]) != MRB_TT_FIXNUM) {
      mrb_raise(mrb, E_TYPE_ERROR, "expected Fixnum");
    }
    len = mrb_fixnum(argv[0]);
    if (index < 0) index += a->len;
    if (index < 0 || a->len < (size_t)index) return mrb_nil_value();
    if ((len = mrb_fixnum(argv[0])) < 0) return mrb_nil_value();
    if (a->len == (size_t)index) return mrb_ary_new(mrb);
    if ((size_t)len > a->len - index) len = a->len - index;
    return mrb_ary_new_from_values(mrb, a->buf + index, len);

  default:
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }

  return mrb_nil_value(); /* dummy to avoid warning : not reach here */
}

mrb_value
mrb_ary_aset(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  switch(argc) {
  case 2:
    if (FIXNUM_P(argv[0])) {
      mrb_ary_set(mrb, self, mrb_fixnum(argv[0]), argv[1]);
    }
    else {
      /* Should we support Range object for 1st arg ? */
      mrb_raise(mrb, E_TYPE_ERROR, "expected Fixnum for 1st argument");
    }
    break;

  case 3:
    mrb_ary_splice(mrb, self, mrb_fixnum(argv[0]), mrb_fixnum(argv[1]), argv[2]);
    break;

  default:
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }

  return self;
}

mrb_value
mrb_ary_delete_at(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);
  mrb_int   index;
  mrb_value val;
  mrb_value *buf;
  size_t len;

  mrb_get_args(mrb, "i", &index);
  if (index < 0) index += a->len;
  if (index < 0 || a->len <= (size_t)index) return mrb_nil_value();

  val = a->buf[index];

  buf = a->buf + index;
  len = a->len - index;
  while((int)(--len)) {
    *buf = *(buf+1);
    ++buf;
  }
  --a->len;

  mrb_ary_shrink_capa(mrb, a);

  return val;
}

mrb_value
mrb_ary_first(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);
  //mrb_value ary;
  size_t size;
  mrb_value *vals;
  int len;

  mrb_get_args(mrb, "*", &vals, &len);
  if (len > 1) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }

  if (len == 0) return (a->len > 0)? a->buf[0]: mrb_nil_value();

  /* len == 1 */
  size = mrb_fixnum(*vals);
  if (size > a->len) size = a->len;
  return mrb_ary_new_from_values(mrb, a->buf, size);
}

mrb_value
mrb_ary_last(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);
  //mrb_value ary;
  size_t size;
  mrb_value *vals;
  int len;

  mrb_get_args(mrb, "*", &vals, &len);
  if (len > 1) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }

  if (len == 0) return (a->len > 0)? a->buf[a->len - 1]: mrb_nil_value();

  /* len == 1 */
  size = mrb_fixnum(*vals);
  if (size > a->len) size = a->len;
  return mrb_ary_new_from_values(mrb, a->buf + a->len - size, size);
}

mrb_value
mrb_ary_index_m(mrb_state *mrb, mrb_value self)
{
  mrb_value obj;
  long i;

  mrb_get_args(mrb, "o", &obj);
  for (i = 0; i < RARRAY_LEN(self); i++) {
    if (mrb_equal(mrb, RARRAY_PTR(self)[i], obj)) {
      return mrb_fixnum_value(i);
    }
  }
  return mrb_nil_value();
}

mrb_value
mrb_ary_rindex_m(mrb_state *mrb, mrb_value self)
{
  mrb_value obj;
  long i;

  mrb_get_args(mrb, "o", &obj);
  for (i = RARRAY_LEN(self) - 1; i >= 0; i--) {
    if (mrb_equal(mrb, RARRAY_PTR(self)[i], obj)) {
      return mrb_fixnum_value(i);
    }
  }
  return mrb_nil_value();
}

mrb_value
mrb_ary_splat(mrb_state *mrb, mrb_value v)
{
  return v;
}

static mrb_value
mrb_ary_size(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);

  return mrb_fixnum_value(a->len);
}

#if 0 /* --> implement with ruby code */
mrb_value
mrb_ary_each(mrb_state *mrb, mrb_value self)
{
  long i;

  for (i = 0; i < RARRAY_LEN(self); i++) {
    mrb_yield(RARRAY_PTR(self)[i]);
  }

  return self;
}
#endif

#if 0 /* --> implement with ruby code */
mrb_value
mrb_ary_each_index(mrb_state *mrb, mrb_value self)
{
  long i;

  for (i = 0; i < RARRAY_LEN(self); i++) {
    mrb_yield(mrb_fixnum_value(i));
  }

  return self;
}
#endif

#if 0 /* --> implement with ruby code */
mrb_value
mrb_ary_collect_bang(mrb_state *mrb, mrb_value self)
{
  long i;

  for (i = 0; i < RARRAY_LEN(self); i++) {
    RARRAY_PTR(self)[i] = mrb_yield(RARRAY_PTR(self)[i]);
  }

  return self;
}
#endif

mrb_value
mrb_ary_clear(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);

  a->len = 0;
  mrb_ary_shrink_capa(mrb, a);

  return self;
}

mrb_value
mrb_ary_empty_p(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);

  return ((a->len == 0)? mrb_true_value(): mrb_false_value());
}

mrb_value
mrb_check_array_type(mrb_state *mrb, mrb_value ary)
{
    return mrb_check_convert_type(mrb, ary, MRB_TT_ARRAY, "Array", "to_ary");
}

mrb_value
mrb_ary_entry(mrb_value ary, long offset)
{
  if (offset < 0) {
    offset += RARRAY_LEN(ary);
  }
  return ary_elt(ary, offset);
}

void
mrb_mem_clear(mrb_value *mem, long size)
{
  while (size--) {
    *mem++ = mrb_nil_value();
  }
}

mrb_value
mrb_ary_tmp_new(mrb_state *mrb, long capa)
{
    return mrb_ary_new_capa(mrb, capa);//ary_new(0, capa);
}

#if 0
/*
 *  call-seq:
 *     ary.sort!                   -> ary
 *     ary.sort! {| a,b | block }  -> ary
 *
 *  Sorts +self+. Comparisons for
 *  the sort will be done using the <code><=></code> operator or using
 *  an optional code block. The block implements a comparison between
 *  <i>a</i> and <i>b</i>, returning -1, 0, or +1. See also
 *  <code>Enumerable#sort_by</code>.
 *
 *     a = [ "d", "a", "e", "c", "b" ]
 *     a.sort                    #=> ["a", "b", "c", "d", "e"]
 *     a.sort {|x,y| y <=> x }   #=> ["e", "d", "c", "b", "a"]
 */

mrb_value
mrb_ary_sort_bang(mrb_value ary)
{
#if 0
  mrb_ary_modify(ary);
  //assert(!ARY_SHARED_P(ary));
  if (RARRAY_LEN(ary) > 1) {
    mrb_value tmp = ary_make_substitution(ary); /* only ary refers tmp */
    struct ary_sort_data data;

    RBASIC(tmp)->klass = 0;
    data.ary = tmp;
    data.opt_methods = 0;
    data.opt_inited = 0;
    ruby_qsort(RARRAY_PTR(tmp), RARRAY_LEN(tmp), sizeof(VALUE),
             mrb_block_given_p()?sort_1:sort_2, &data);

      if (ARY_EMBED_P(tmp)) {
          assert(ARY_EMBED_P(tmp));
          if (ARY_SHARED_P(ary)) { /* ary might be destructively operated in the given block */
              mrb_ary_unshare(ary);
          }
          FL_SET_EMBED(ary);
          MEMCPY(RARRAY_PTR(ary), ARY_EMBED_PTR(tmp), VALUE, ARY_EMBED_LEN(tmp));
          ARY_SET_LEN(ary, ARY_EMBED_LEN(tmp));
      }
      else {
          assert(!ARY_EMBED_P(tmp));
          if (ARY_HEAP_PTR(ary) == ARY_HEAP_PTR(tmp)) {
              assert(!ARY_EMBED_P(ary));
              FL_UNSET_SHARED(ary);
              ARY_SET_CAPA(ary, ARY_CAPA(tmp));
          }
          else {
              assert(!ARY_SHARED_P(tmp));
              if (ARY_EMBED_P(ary)) {
                  FL_UNSET_EMBED(ary);
              }
              else if (ARY_SHARED_P(ary)) {
                  /* ary might be destructively operated in the given block */
                  mrb_ary_unshare(ary);
              }
              else {
                  xfree(ARY_HEAP_PTR(ary));
              }
              ARY_SET_PTR(ary, RARRAY_PTR(tmp));
              ARY_SET_HEAP_LEN(ary, RARRAY_LEN(tmp));
              ARY_SET_CAPA(ary, ARY_CAPA(tmp));
          }
          /* tmp was lost ownership for the ptr */
          FL_UNSET(tmp, FL_FREEZE);
          FL_SET_EMBED(tmp);
          ARY_SET_EMBED_LEN(tmp, 0);
          FL_SET(tmp, FL_FREEZE);
    }
    /* tmp will be GC'ed. */
    RBASIC(tmp)->c = mrb->array_class;
  }
#endif
  return ary;
}
#endif

mrb_value
mrb_ary_dup(mrb_state *mrb, mrb_value self)
{
  struct RArray *a1 = mrb_ary_ptr(self);
  struct RArray *a2;
  mrb_value ary;
  mrb_value *buf;
  mrb_int times;
  //size_t len;

  ary = mrb_ary_new_capa(mrb, a1->len);
  a2 = mrb_ary_ptr(ary);
  buf = a2->buf;
  while(times--) {
    memcpy(buf, a1->buf, sizeof(mrb_value)*a1->len);
    buf += a1->len;
  }
  a2->len = a1->len;

  return ary;
}

#if 0
/*
 *  call-seq:
 *     ary.sort                   -> new_ary
 *     ary.sort {| a,b | block }  -> new_ary
 *
 *  Returns a new array created by sorting +self+. Comparisons for
 *  the sort will be done using the <code><=></code> operator or using
 *  an optional code block. The block implements a comparison between
 *  <i>a</i> and <i>b</i>, returning -1, 0, or +1. See also
 *  <code>Enumerable#sort_by</code>.
 *
 *     a = [ "d", "a", "e", "c", "b" ]
 *     a.sort                    #=> ["a", "b", "c", "d", "e"]
 *     a.sort {|x,y| y <=> x }   #=> ["e", "d", "c", "b", "a"]
 */

mrb_value
mrb_ary_sort(mrb_state *mrb, mrb_value ary)
{
  ary = mrb_ary_dup(mrb, ary);
  mrb_ary_sort_bang(ary);
  return ary;
}
#endif

static mrb_value
inspect_ary(mrb_state *mrb, mrb_value ary, mrb_value list)
{
  long i;
  mrb_value s, arystr;
  char *head = "[";
  char *sep = ", ";
  char *tail = "]";

  /* check recursive */
  for(i=0; i<RARRAY_LEN(list); i++) {
    if (mrb_obj_equal(mrb, ary, RARRAY_PTR(list)[i])) {
      return mrb_str_new2(mrb, "[...]");
    }
  }

  mrb_ary_push(mrb, list, ary);

  arystr = mrb_str_buf_new(mrb, 64);
  mrb_str_buf_cat(mrb, arystr, head, strlen(head));

  for(i=0; i<RARRAY_LEN(ary); i++) {
    int ai = mrb_gc_arena_save(mrb);

    if (i > 0) {
      mrb_str_buf_cat(mrb, arystr, sep, strlen(sep));
    }
    if (mrb_type(RARRAY_PTR(ary)[i]) == MRB_TT_ARRAY) {
      s = inspect_ary(mrb, RARRAY_PTR(ary)[i], list);
    } else {
      s = mrb_inspect(mrb, RARRAY_PTR(ary)[i]);
    }
    //mrb_str_buf_append(mrb, arystr, s);
    mrb_str_buf_cat(mrb, arystr, RSTRING_PTR(s), RSTRING_LEN(s));
    mrb_gc_arena_restore(mrb, ai);
  }

  mrb_str_buf_cat(mrb, arystr, tail, strlen(tail));
  mrb_ary_pop(mrb, list);

  return arystr;
}

#if 0
static mrb_value
inspect_ary_r(mrb_state *mrb, mrb_value ary, mrb_value dummy, int recur)
{
  long i;
  mrb_value s, arystr;

  arystr = mrb_str_buf_new(mrb, 128);
  mrb_str_buf_cat(mrb, arystr, "[", strlen("[")); /* for capa */
  //arystr = mrb_str_new_cstr(mrb, "[");//mrb_str_buf_new2("[");
  for (i=0; i<RARRAY_LEN(ary); i++) {
    s = mrb_inspect(mrb, RARRAY_PTR(ary)[i]);//mrb_inspect(RARRAY_PTR(ary)[i]);
    if (i > 0) mrb_str_buf_cat(mrb, arystr, ", ", strlen(", "));//mrb_str_buf_cat2(str, ", ");
    mrb_str_buf_append(mrb, arystr, s);
  }
  mrb_str_buf_cat(mrb, arystr, "]", strlen("]"));// mrb_str_buf_cat2(str, "]");

  return arystr;
}
#endif

/* 15.2.12.5.31 (x) */
/*
 *  call-seq:
 *     ary.to_s -> string
 *     ary.inspect  -> string
 *
 *  Creates a string representation of +self+.
 */

static mrb_value
mrb_ary_inspect(mrb_state *mrb, mrb_value ary)
{
  if (RARRAY_LEN(ary) == 0) return mrb_str_new2(mrb, "[]");
  #if 0 /* THREAD */
    return mrb_exec_recursive(inspect_ary_r, ary, 0);
  #else
    return inspect_ary(mrb, ary, mrb_ary_new(mrb));
  #endif
}

static mrb_value
join_ary(mrb_state *mrb, mrb_value ary, mrb_value sep, mrb_value list)
{
  long i;
  mrb_value result, val, tmp;

  /* check recursive */
  for(i=0; i<RARRAY_LEN(list); i++) {
    if (mrb_obj_equal(mrb, ary, RARRAY_PTR(list)[i])) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "recursive array join");
    }
  }

  mrb_ary_push(mrb, list, ary);

  result = mrb_str_buf_new(mrb, 64);

  for(i=0; i<RARRAY_LEN(ary); i++) {
    if (i > 0 && !mrb_nil_p(sep)) {
      //mrb_str_buf_append(mrb, result, sep); // segv (encoding error?)
      mrb_str_buf_cat(mrb, result, RSTRING_PTR(sep), RSTRING_LEN(sep));
    }

    val = RARRAY_PTR(ary)[i];
    switch(mrb_type(val)) {
    case MRB_TT_ARRAY:
    ary_join:
      val = join_ary(mrb, val, sep, list);
      /* fall through */

    case MRB_TT_STRING:
    str_join:
      //mrb_str_buf_append(mrb, result, val);
      mrb_str_buf_cat(mrb, result, RSTRING_PTR(val), RSTRING_LEN(val));
      break;

    default:
      tmp = mrb_check_string_type(mrb, val);
      if (!mrb_nil_p(tmp)) {
        val = tmp;
        goto str_join;
      }
      tmp = mrb_check_convert_type(mrb, val, MRB_TT_ARRAY, "Array", "to_ary");
      if (!mrb_nil_p(tmp)) {
        val = tmp;
        goto ary_join;
      }
      val = mrb_obj_as_string(mrb, val);
      goto str_join;
    }
  }

  mrb_ary_pop(mrb, list);

  return result;
}

mrb_value
mrb_ary_join(mrb_state *mrb, mrb_value ary, mrb_value sep)
{
  sep = mrb_obj_as_string(mrb, sep);
  return join_ary(mrb, ary, sep, mrb_ary_new(mrb));
}

#if 0
static void ary_join_1(mrb_state *mrb, mrb_value obj, mrb_value ary, mrb_value sep, long i, mrb_value result, mrb_value first);

static mrb_value
recursive_join(mrb_state *mrb, mrb_value obj, mrb_value args, int recur)
{
  mrb_value ary = mrb_ary_ref(mrb, args, 0);
  mrb_value sep = mrb_ary_ref(mrb, args, 1);
  mrb_value result = mrb_ary_ref(mrb, args, 2);
  mrb_value first = mrb_ary_ref(mrb, args, 3);

  if (recur) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "recursive array join");
  }
  else {
    ary_join_1(mrb, obj, ary, sep, 0, result, first);
  }
  return mrb_nil_value();
}

static void
ary_join_0(mrb_state *mrb, mrb_value ary, mrb_value sep, long max, mrb_value result)
{
  long i;
  mrb_value val;

  for (i=0; i<max; i++) {
    val = RARRAY_PTR(ary)[i];
    if (i > 0 && !mrb_nil_p(sep))
        mrb_str_buf_append(mrb, result, sep);
    mrb_str_buf_append(mrb, result, val);
  }
}

static void
ary_join_1(mrb_state *mrb, mrb_value obj, mrb_value ary, mrb_value sep, long i, mrb_value result, mrb_value first)
{
  mrb_value val, tmp;

  for (; i<RARRAY_LEN(ary); i++) {
    if (i > 0 && !mrb_nil_p(sep)) {
      mrb_str_buf_append(mrb, result, sep);
    }

    val = RARRAY_PTR(ary)[i];
    switch (mrb_type(val)) {
      case MRB_TT_STRING:
    str_join:
        mrb_str_buf_append(mrb, result, val);
        break;
      case MRB_TT_ARRAY:
        obj = val;
    ary_join:
        if (mrb_obj_equal(mrb, val, ary)) {
          mrb_raise(mrb, E_ARGUMENT_ERROR, "recursive array join");
        }
        else {
          //struct recursive_join_arg args;
          mrb_value args = mrb_ary_new(mrb);

          mrb_ary_set(mrb, args, 0, val);
          mrb_ary_set(mrb, args, 1, sep);
          mrb_ary_set(mrb, args, 2, result);
          mrb_ary_set(mrb, args, 3, first);

          mrb_exec_recursive(mrb, recursive_join, obj, &args);
        }
        break;
      default:
        tmp = mrb_check_string_type(mrb, val);
        if (!mrb_nil_p(tmp)) {
          val = tmp;
          goto str_join;
        }
        tmp = mrb_check_convert_type(mrb, val, MRB_TT_ARRAY, "Array", "to_ary");
        if (!mrb_nil_p(tmp)) {
          obj = val;
          val = tmp;
          goto ary_join;
        }
        val = mrb_obj_as_string(mrb, val);
        if (mrb_test(first)) {
#ifdef INCLUDE_REGEXP /* include "encoding.h" */
          mrb_enc_copy(mrb, result, val);
#endif
          first = mrb_false_value();
        }
        goto str_join;
    }
  }
}

mrb_value
mrb_ary_join(mrb_state *mrb, mrb_value ary, mrb_value sep)
{
  long len = 1, i;
  mrb_value val, tmp, result;

  if (RARRAY_LEN(ary) == 0) return mrb_str_new2(mrb, "");

  if (!mrb_nil_p(sep)) {
    //StringValue(sep);
    mrb_string_value(mrb, &sep);
    len += RSTRING_LEN(sep) * (RARRAY_LEN(ary) - 1);
  }

  for (i=0; i<RARRAY_LEN(ary); i++) {
    val = RARRAY_PTR(ary)[i];
    tmp = mrb_check_string_type(mrb, val);

    if (mrb_nil_p(tmp) || (!mrb_obj_equal(mrb, tmp, val))) {
      mrb_value first;

      result = mrb_str_buf_new(mrb, len + (RARRAY_LEN(ary)-i)*10);
      first = (i == 0)? mrb_true_value(): mrb_false_value();
mrb_realloc(mrb, RSTRING(result)->buf, ++(RSTRING(result)->capa));
      ary_join_0(mrb, ary, sep, i, result);
mrb_realloc(mrb, RSTRING(result)->buf, ++(RSTRING(result)->capa));
      ary_join_1(mrb, ary, ary, sep, i, result, first);
mrb_realloc(mrb, RSTRING(result)->buf, ++(RSTRING(result)->capa));
      return result;
    }

    len += RSTRING_LEN(tmp);
  }

  result = mrb_str_buf_new(mrb, len);
  ary_join_0(mrb, ary, sep, RARRAY_LEN(ary), result);

  return result;
}
#endif

/*
 *  call-seq:
 *     ary.join(sep=nil)    -> str
 *
 *  Returns a string created by converting each element of the array to
 *  a string, separated by <i>sep</i>.
 *
 *     [ "a", "b", "c" ].join        #=> "abc"
 *     [ "a", "b", "c" ].join("-")   #=> "a-b-c"
 */

static mrb_value
mrb_ary_join_m(mrb_state *mrb, mrb_value ary)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  switch(argc) {
  case 0:
    return mrb_ary_join(mrb, ary, mrb_nil_value());

  case 1:
    return mrb_ary_join(mrb, ary, argv[0]);

  default:
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }

  return mrb_nil_value(); /* dummy */
}

static mrb_value
recursive_equal(mrb_state *mrb, mrb_value ary1, mrb_value ary2, int recur)
{
  long i;

  if (recur) return mrb_true_value(); /* Subtle! */
  for (i=0; i<RARRAY_LEN(ary1); i++) {
    if (!mrb_equal(mrb, ary_elt(ary1, i), ary_elt(ary2, i)))
        return mrb_false_value();
  }
  return mrb_true_value();
}

/* 15.2.12.5.33 (x) */
/*
 *  call-seq:
 *     ary == other_ary   ->   bool
 *
 *  Equality---Two arrays are equal if they contain the same number
 *  of elements and if each element is equal to (according to
 *  Object.==) the corresponding element in the other array.
 *
 *     [ "a", "c" ]    == [ "a", "c", 7 ]     #=> false
 *     [ "a", "c", 7 ] == [ "a", "c", 7 ]     #=> true
 *     [ "a", "c", 7 ] == [ "a", "d", "f" ]   #=> false
 *
 */

static mrb_value
mrb_ary_equal(mrb_state *mrb, mrb_value ary1)
{
  mrb_value ary2;

  mrb_get_args(mrb, "o", &ary2);
  if (mrb_obj_equal(mrb, ary1,ary2)) return mrb_true_value();
  if (mrb_type(ary2) != MRB_TT_ARRAY) {
    if (!mrb_respond_to(mrb, ary2, mrb_intern(mrb, "to_ary"))) {
        return mrb_false_value();
    }
    if (mrb_equal(mrb, ary2, ary1)){
      return mrb_true_value();
    }
    else {
      return mrb_false_value();
    }
  }
  if (RARRAY_LEN(ary1) != RARRAY_LEN(ary2)) return mrb_false_value();
  return mrb_exec_recursive_paired(mrb, recursive_equal, ary1, ary2, &ary2);
}

static mrb_value
recursive_eql(mrb_state *mrb, mrb_value ary1, mrb_value ary2, int recur)
{
  long i;

  if (recur) return mrb_true_value(); /* Subtle! */
  for (i=0; i<RARRAY_LEN(ary1); i++) {
    if (!mrb_eql(mrb, ary_elt(ary1, i), ary_elt(ary2, i)))
      return mrb_false_value();
  }
  return mrb_true_value();
}

/* 15.2.12.5.34 (x) */
/*
 *  call-seq:
 *     ary.eql?(other)  -> true or false
 *
 *  Returns <code>true</code> if +self+ and _other_ are the same object,
 *  or are both arrays with the same content.
 */

static mrb_value
mrb_ary_eql(mrb_state *mrb, mrb_value ary1)
{
  mrb_value ary2;

  mrb_get_args(mrb, "o", &ary2);
  if (mrb_obj_equal(mrb, ary1,ary2)) return mrb_true_value();
  if (mrb_type(ary2) != MRB_TT_ARRAY) return mrb_false_value();
  if (RARRAY_LEN(ary1) != RARRAY_LEN(ary2)) return mrb_false_value();
  return mrb_exec_recursive_paired(mrb, recursive_eql, ary1, ary2, &ary2);
}

#if 0
static mrb_value
recursive_hash(mrb_value ary, mrb_value dummy, int recur)
{
  long i;
  st_index_t h;
  mrb_value n;

  h = mrb_hash_start(RARRAY_LEN(ary));
  if (recur) {
    h = mrb_hash_uint(h, NUM2LONG(mrb_hash(mrb_cArray)));
  }
  else {
    for (i=0; i<RARRAY_LEN(ary); i++) {
        n = mrb_hash(RARRAY_PTR(ary)[i]);
        h = mrb_hash_uint(h, NUM2LONG(n));
    }
  }
  h = mrb_hash_end(h);
  return LONG2FIX(h);
}

/* 15.2.12.5.35 (x) */
/*
 *  call-seq:
 *     ary.hash   -> fixnum
 *
 *  Compute a hash-code for this array. Two arrays with the same content
 *  will have the same hash code (and will compare using <code>eql?</code>).
 */

static mrb_value
mrb_ary_hash(mrb_state *mrb, mrb_value ary)
{
  return mrb_exec_recursive_outer(mrb, recursive_hash, ary, mrb_fixnum_value(0));
}
#endif

void
mrb_init_array(mrb_state *mrb)
{
  struct RClass *a;

  a = mrb->array_class = mrb_define_class(mrb, "Array", mrb->object_class);
  MRB_SET_INSTANCE_TT(a, MRB_TT_ARRAY);
  mrb_include_module(mrb, a, mrb_class_get(mrb, "Enumerable"));

  mrb_define_class_method(mrb, a, "[]",        mrb_ary_s_create,     ARGS_ANY());  /* 15.2.12.4.1 */

  mrb_define_method(mrb, a, "*",               mrb_ary_times,        ARGS_REQ(1)); /* 15.2.12.5.1  */
  mrb_define_method(mrb, a, "+",               mrb_ary_plus,         ARGS_REQ(1)); /* 15.2.12.5.2  */
  mrb_define_method(mrb, a, "<<",              mrb_ary_push_m,       ARGS_REQ(1)); /* 15.2.12.5.3  */
  mrb_define_method(mrb, a, "[]",              mrb_ary_aget,         ARGS_ANY());  /* 15.2.12.5.4  */
  mrb_define_method(mrb, a, "[]=",             mrb_ary_aset,         ARGS_ANY());  /* 15.2.12.5.5  */
  mrb_define_method(mrb, a, "clear",           mrb_ary_clear,        ARGS_NONE()); /* 15.2.12.5.6  */
#if 0 /* --> implement with ruby code */
  mrb_define_method(mrb, a, "collect!",        mrb_ary_collect_bang, ARGS_NONE()); /* 15.2.12.5.7  */
#endif
  mrb_define_method(mrb, a, "concat",          mrb_ary_concat_m,     ARGS_REQ(1)); /* 15.2.12.5.8  */
  mrb_define_method(mrb, a, "delete_at",       mrb_ary_delete_at,    ARGS_REQ(1)); /* 15.2.12.5.9  */
#if 0 /* --> implement with ruby code */
  mrb_define_method(mrb, a, "each",            mrb_ary_each,         ARGS_NONE()); /* 15.2.12.5.10 */
#endif
#if 0 /* --> implement with ruby code */
  mrb_define_method(mrb, a, "each_index",      mrb_ary_each_index,   ARGS_NONE()); /* 15.2.12.5.11 */
#endif
  mrb_define_method(mrb, a, "empty?",          mrb_ary_empty_p,      ARGS_NONE()); /* 15.2.12.5.12 */
  mrb_define_method(mrb, a, "first",           mrb_ary_first,        ARGS_ANY());  /* 15.2.12.5.13 */
  mrb_define_method(mrb, a, "index",           mrb_ary_index_m,      ARGS_REQ(1)); /* 15.2.12.5.14 */
#if 0 /* --> implement with ruby code */
  mrb_define_method(mrb, a, "initialize",      mrb_ary_initialize,   ARGS_ANY());  /* 15.2.12.5.15 */
#endif
  mrb_define_method(mrb, a, "initialize_copy", mrb_ary_replace_m,    ARGS_REQ(1)); /* 15.2.12.5.16 */
  mrb_define_method(mrb, a, "join",            mrb_ary_join_m,       ARGS_ANY());  /* 15.2.12.5.17 */
  mrb_define_method(mrb, a, "last",            mrb_ary_last,         ARGS_ANY());  /* 15.2.12.5.18 */
  mrb_define_method(mrb, a, "length",          mrb_ary_size,         ARGS_NONE()); /* 15.2.12.5.19 */
#if 0 /* --> implement with ruby code */
  mrb_define_method(mrb, a, "map!",            mrb_ary_collect_bang, ARGS_NONE()); /* 15.2.12.5.20 */
#endif
  mrb_define_method(mrb, a, "pop",             mrb_ary_pop_m,        ARGS_NONE()); /* 15.2.12.5.21 */
  mrb_define_method(mrb, a, "push",            mrb_ary_push_m,       ARGS_ANY());  /* 15.2.12.5.22 */
  mrb_define_method(mrb, a, "replace",         mrb_ary_replace_m,    ARGS_REQ(1)); /* 15.2.12.5.23 */
  mrb_define_method(mrb, a, "reverse",         mrb_ary_reverse,      ARGS_NONE()); /* 15.2.12.5.24 */
  mrb_define_method(mrb, a, "reverse!",        mrb_ary_reverse_bang, ARGS_NONE()); /* 15.2.12.5.25 */
  mrb_define_method(mrb, a, "rindex",          mrb_ary_rindex_m,     ARGS_REQ(1)); /* 15.2.12.5.26 */
  mrb_define_method(mrb, a, "shift",           mrb_ary_shift,        ARGS_NONE()); /* 15.2.12.5.27 */
  mrb_define_method(mrb, a, "size",            mrb_ary_size,         ARGS_NONE()); /* 15.2.12.5.28 */
  mrb_define_method(mrb, a, "slice",           mrb_ary_aget,         ARGS_ANY());  /* 15.2.12.5.29 */
  mrb_define_method(mrb, a, "unshift",         mrb_ary_unshift_m,    ARGS_ANY());  /* 15.2.12.5.30 */

  mrb_define_method(mrb, a, "inspect",         mrb_ary_inspect,      ARGS_NONE()); /* 15.2.12.5.31 (x) */
  mrb_define_alias(mrb,   a, "to_s", "inspect");                                   /* 15.2.12.5.32 (x) */
  mrb_define_method(mrb, a, "==",              mrb_ary_equal,        ARGS_REQ(1)); /* 15.2.12.5.33 (x) */
  mrb_define_method(mrb, a, "eql?",            mrb_ary_eql,          ARGS_REQ(1)); /* 15.2.12.5.34 (x) */
  //mrb_define_method(mrb, a, "hash",            mrb_ary_hash,         ARGS_NONE()); /* 15.2.12.5.35 (x) */
  mrb_define_method(mrb, a, "<=>",              mrb_ary_cmp,         ARGS_REQ(1)); /* 15.2.12.5.36 (x) */
}
