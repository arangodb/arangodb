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
  if (mrb_type(v) == MRB_TT_ARRAY) {
    return v;
  }
  else {
    return mrb_ary_new_from_values(mrb, &v, 1);
  }
}

static mrb_value
mrb_ary_size(mrb_state *mrb, mrb_value self)
{
  struct RArray *a = mrb_ary_ptr(self);

  return mrb_fixnum_value(a->len);
}

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
  mrb_define_method(mrb, a, "concat",          mrb_ary_concat_m,     ARGS_REQ(1)); /* 15.2.12.5.8  */
  mrb_define_method(mrb, a, "delete_at",       mrb_ary_delete_at,    ARGS_REQ(1)); /* 15.2.12.5.9  */
  mrb_define_method(mrb, a, "empty?",          mrb_ary_empty_p,      ARGS_NONE()); /* 15.2.12.5.12 */
  mrb_define_method(mrb, a, "first",           mrb_ary_first,        ARGS_ANY());  /* 15.2.12.5.13 */
  mrb_define_method(mrb, a, "index",           mrb_ary_index_m,      ARGS_REQ(1)); /* 15.2.12.5.14 */
  mrb_define_method(mrb, a, "initialize_copy", mrb_ary_replace_m,    ARGS_REQ(1)); /* 15.2.12.5.16 */
  mrb_define_method(mrb, a, "join",            mrb_ary_join_m,       ARGS_ANY());  /* 15.2.12.5.17 */
  mrb_define_method(mrb, a, "last",            mrb_ary_last,         ARGS_ANY());  /* 15.2.12.5.18 */
  mrb_define_method(mrb, a, "length",          mrb_ary_size,         ARGS_NONE()); /* 15.2.12.5.19 */
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
  mrb_define_method(mrb, a, "<=>",              mrb_ary_cmp,         ARGS_REQ(1)); /* 15.2.12.5.36 (x) */
}
