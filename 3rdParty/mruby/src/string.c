/*
** string.c - String class
**
** See Copyright Notice in mruby.h
*/

#include <ctype.h>
#ifndef SIZE_MAX
 /* Some versions of VC++
  * has SIZE_MAX in stdint.h
  */
# include <limits.h>
#endif
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/range.h"
#include "mruby/string.h"
#include "re.h"

const char mrb_digitmap[] = "0123456789abcdefghijklmnopqrstuvwxyz";

typedef struct mrb_shared_string {
  mrb_bool nofree;
  int refcnt;
  char *ptr;
  mrb_int len;
} mrb_shared_string;

#define MRB_STR_SHARED    1
#define MRB_STR_NOFREE    2

static mrb_value str_replace(mrb_state *mrb, struct RString *s1, struct RString *s2);
static mrb_value mrb_str_subseq(mrb_state *mrb, mrb_value str, mrb_int beg, mrb_int len);

#define RESIZE_CAPA(s,capacity) do {\
      s->ptr = (char *)mrb_realloc(mrb, s->ptr, (capacity)+1);\
      s->aux.capa = capacity;\
} while(0)

static void
str_decref(mrb_state *mrb, mrb_shared_string *shared)
{
  shared->refcnt--;
  if (shared->refcnt == 0) {
    if (!shared->nofree) {
      mrb_free(mrb, shared->ptr);
    }
    mrb_free(mrb, shared);
  }
}

void
mrb_str_modify(mrb_state *mrb, struct RString *s)
{
  if (s->flags & MRB_STR_SHARED) {
    mrb_shared_string *shared = s->aux.shared;

    if (shared->refcnt == 1 && s->ptr == shared->ptr) {
      s->ptr = shared->ptr;
      s->aux.capa = shared->len;
      mrb_free(mrb, shared);
    }
    else {
      char *ptr, *p;
      mrb_int len;

      p = s->ptr;
      len = s->len;
      ptr = (char *)mrb_malloc(mrb, (size_t)len + 1);
      if (p) {
        memcpy(ptr, p, len);
      }
      ptr[len] =  '\0';
      s->ptr = ptr;
      s->aux.capa = len;
      str_decref(mrb, shared);
    }
    s->flags &= ~MRB_STR_SHARED;
    return;
  }
  if (s->flags & MRB_STR_NOFREE) {
    char *p = s->ptr;

    s->ptr = (char *)mrb_malloc(mrb, (size_t)s->len+1);
    if (p) {
      memcpy(s->ptr, p, s->len);
    }
    s->ptr[s->len] = '\0';
    s->aux.capa = s->len;
    return;
  }
}

mrb_value
mrb_str_resize(mrb_state *mrb, mrb_value str, mrb_int len)
{
  int slen;
  struct RString *s = mrb_str_ptr(str);

  mrb_str_modify(mrb, s);
  slen = s->len;
  if (len != slen) {
    if (slen < len || slen - len > 256) {
      RESIZE_CAPA(s, len);
    }
    s->len = len;
    s->ptr[len] = '\0';   /* sentinel */
  }
  return str;
}

static inline void
str_mod_check(mrb_state *mrb, mrb_value str, char *p, mrb_int len)
{
  struct RString *s = mrb_str_ptr(str);

  if (s->ptr != p || s->len != len) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "string modified");
  }
}

#define mrb_obj_alloc_string(mrb) ((struct RString*)mrb_obj_alloc((mrb), MRB_TT_STRING, (mrb)->string_class))

/* char offset to byte offset */
int
mrb_str_offset(mrb_state *mrb, mrb_value str, int pos)
{
  return pos;
}

static struct RString*
str_new(mrb_state *mrb, const char *p, mrb_int len)
{
  struct RString *s;

  s = mrb_obj_alloc_string(mrb);
  s->len = len;
  s->aux.capa = len;
  s->ptr = (char *)mrb_malloc(mrb, (size_t)len+1);
  if (p) {
    memcpy(s->ptr, p, len);
  }
  s->ptr[len] = '\0';
  return s;
}

void
str_with_class(mrb_state *mrb, struct RString *s, mrb_value obj)
{
  s->c = mrb_str_ptr(obj)->c;
}

static mrb_value
mrb_str_new_empty(mrb_state *mrb, mrb_value str)
{
  struct RString *s = str_new(mrb, 0, 0);

  str_with_class(mrb, s, str);
  return mrb_obj_value(s);
}

#ifndef MRB_STR_BUF_MIN_SIZE
# define MRB_STR_BUF_MIN_SIZE 128
#endif

mrb_value
mrb_str_buf_new(mrb_state *mrb, mrb_int capa)
{
  struct RString *s;

  s = mrb_obj_alloc_string(mrb);

  if (capa < MRB_STR_BUF_MIN_SIZE) {
    capa = MRB_STR_BUF_MIN_SIZE;
  }
  s->len = 0;
  s->aux.capa = capa;
  s->ptr = (char *)mrb_malloc(mrb, capa+1);
  s->ptr[0] = '\0';

  return mrb_obj_value(s);
}

static void
str_buf_cat(mrb_state *mrb, struct RString *s, const char *ptr, size_t len)
{
  mrb_int capa;
  mrb_int total;
  ptrdiff_t off = -1;

  mrb_str_modify(mrb, s);
  if (ptr >= s->ptr && ptr <= s->ptr + s->len) {
      off = ptr - s->ptr;
  }
  if (len == 0) return;
  capa = s->aux.capa;
  if (s->len >= MRB_INT_MAX - (mrb_int)len) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "string sizes too big");
  }
  total = s->len+len;
  if (capa <= total) {
    while (total > capa) {
        if (capa + 1 >= MRB_INT_MAX / 2) {
          capa = (total + 4095) / 4096;
          break;
        }
        capa = (capa + 1) * 2;
    }
    RESIZE_CAPA(s, capa);
  }
  if (off != -1) {
      ptr = s->ptr + off;
  }
  memcpy(s->ptr + s->len, ptr, len);
  s->len = total;
  s->ptr[total] = '\0';   /* sentinel */
}

mrb_value
mrb_str_buf_cat(mrb_state *mrb, mrb_value str, const char *ptr, size_t len)
{
  if (len == 0) return str;
  str_buf_cat(mrb, mrb_str_ptr(str), ptr, len);
  return str;
}

mrb_value
mrb_str_new(mrb_state *mrb, const char *p, size_t len)
{
  struct RString *s;

  s = str_new(mrb, p, len);
  return mrb_obj_value(s);
}

/*
 *  call-seq: (Caution! NULL string)
 *     String.new(str="")   => new_str
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */

mrb_value
mrb_str_new_cstr(mrb_state *mrb, const char *p)
{
  struct RString *s;
  size_t len;

  if (p) {
    len = strlen(p);
    if ((mrb_int)len < 0) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "argument too big");
    }
  }
  else {
    len = 0;
  }

  s = str_new(mrb, p, len);

  return mrb_obj_value(s);
}

mrb_value
mrb_str_new_static(mrb_state *mrb, const char *p, size_t len)
{
  struct RString *s;

  s = mrb_obj_alloc_string(mrb);
  s->len = len;
  s->aux.capa = 0;             /* nofree */
  s->ptr = (char *)p;
  s->flags = MRB_STR_NOFREE;
  return mrb_obj_value(s);
}

void
mrb_gc_free_str(mrb_state *mrb, struct RString *str)
{
  if (str->flags & MRB_STR_SHARED)
    str_decref(mrb, str->aux.shared);
  else if ((str->flags & MRB_STR_NOFREE) == 0)
    mrb_free(mrb, str->ptr);
}

char *
mrb_str_to_cstr(mrb_state *mrb, mrb_value str0)
{
  struct RString *s;

  if (!mrb_string_p(str0)) {
      mrb_raise(mrb, E_TYPE_ERROR, "expected String");
  }

  s = str_new(mrb, RSTRING_PTR(str0), RSTRING_LEN(str0));
  if ((strlen(s->ptr) ^ s->len) != 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "string contains null byte");
  }
  return s->ptr;
}

static void
str_make_shared(mrb_state *mrb, struct RString *s)
{
  if (!(s->flags & MRB_STR_SHARED)) {
    mrb_shared_string *shared = (mrb_shared_string *)mrb_malloc(mrb, sizeof(mrb_shared_string));

    shared->refcnt = 1;
    if (s->flags & MRB_STR_NOFREE) {
      shared->nofree = TRUE;
      shared->ptr = s->ptr;
      s->flags &= ~MRB_STR_NOFREE;
    }
    else {
      shared->nofree = FALSE;
      if (s->aux.capa > s->len) {
        s->ptr = shared->ptr = (char *)mrb_realloc(mrb, s->ptr, s->len+1);
      }
      else {
        shared->ptr = s->ptr;
      }
    }
    shared->len = s->len;
    s->aux.shared = shared;
    s->flags |= MRB_STR_SHARED;
  }
}

/*
 *  call-seq: (Caution! string literal)
 *     String.new(str="")   => new_str
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */

mrb_value
mrb_str_literal(mrb_state *mrb, mrb_value str)
{
  struct RString *s, *orig;
  mrb_shared_string *shared;

  s = mrb_obj_alloc_string(mrb);
  orig = mrb_str_ptr(str);
  if (!(orig->flags & MRB_STR_SHARED)) {
    str_make_shared(mrb, orig);
  }
  shared = orig->aux.shared;
  shared->refcnt++;
  s->ptr = shared->ptr;
  s->len = shared->len;
  s->aux.shared = shared;
  s->flags |= MRB_STR_SHARED;

  return mrb_obj_value(s);
}

/*
 *  call-seq:
 *     char* str = String("abcd"), len=strlen("abcd")
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
const char*
mrb_str_body(mrb_value str, int *len_p)
{
  struct RString *s = mrb_str_ptr(str);

  *len_p = s->len;
  return s->ptr;
}

/*
 *  call-seq: (Caution! String("abcd") change)
 *     String("abcdefg") = String("abcd") + String("efg")
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
void
mrb_str_concat(mrb_state *mrb, mrb_value self, mrb_value other)
{
  struct RString *s1 = mrb_str_ptr(self), *s2;
  mrb_int len;

  mrb_str_modify(mrb, s1);
  if (!mrb_string_p(other)) {
    other = mrb_str_to_str(mrb, other);
  }
  s2 = mrb_str_ptr(other);
  len = s1->len + s2->len;

  if (s1->aux.capa < len) {
    s1->aux.capa = len;
    s1->ptr = (char *)mrb_realloc(mrb, s1->ptr, len+1);
  }
  memcpy(s1->ptr+s1->len, s2->ptr, s2->len);
  s1->len = len;
  s1->ptr[len] =  '\0';
}

/*
 *  call-seq: (Caution! String("abcd") remain)
 *     String("abcdefg") = String("abcd") + String("efg")
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
mrb_value
mrb_str_plus(mrb_state *mrb, mrb_value a, mrb_value b)
{
  struct RString *s = mrb_str_ptr(a);
  struct RString *s2 = mrb_str_ptr(b);
  struct RString *t;

  t = str_new(mrb, 0, s->len + s2->len);
  memcpy(t->ptr, s->ptr, s->len);
  memcpy(t->ptr + s->len, s2->ptr, s2->len);

  return mrb_obj_value(t);
}

/* 15.2.10.5.2  */

/*
 *  call-seq: (Caution! String("abcd") remain) for stack_argument
 *     String("abcdefg") = String("abcd") + String("efg")
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
static mrb_value
mrb_str_plus_m(mrb_state *mrb, mrb_value self)
{
  mrb_value str;

  mrb_get_args(mrb, "S", &str);
  return mrb_str_plus(mrb, self, str);
}

/*
 *  call-seq:
 *     len = strlen(String("abcd"))
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
static mrb_value
mrb_str_bytesize(mrb_state *mrb, mrb_value self)
{
  struct RString *s = mrb_str_ptr(self);
  return mrb_fixnum_value(s->len);
}

/* 15.2.10.5.26 */
/* 15.2.10.5.33 */
/*
 *  call-seq:
 *     len = strlen(String("abcd"))
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
mrb_value
mrb_str_size(mrb_state *mrb, mrb_value self)
{
  struct RString *s = mrb_str_ptr(self);
  return mrb_fixnum_value(s->len);
}

/* 15.2.10.5.1  */
/*
 *  call-seq:
 *     str * integer   => new_str
 *
 *  Copy---Returns a new <code>String</code> containing <i>integer</i> copies of
 *  the receiver.
 *
 *     "Ho! " * 3   #=> "Ho! Ho! Ho! "
 */
static mrb_value
mrb_str_times(mrb_state *mrb, mrb_value self)
{
  mrb_int n,len,times;
  struct RString *str2;
  char *p;

  mrb_get_args(mrb, "i", &times);
  if (times < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "negative argument");
  }
  if (times && MRB_INT_MAX / times < RSTRING_LEN(self)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "argument too big");
  }

  len = RSTRING_LEN(self)*times;
  str2 = str_new(mrb, 0, len);
  str_with_class(mrb, str2, self);
  p = str2->ptr;
  if (len > 0) {
    n = RSTRING_LEN(self);
    memcpy(p, RSTRING_PTR(self), n);
    while (n <= len/2) {
      memcpy(p + n, p, n);
      n *= 2;
    }
    memcpy(p + n, p, len-n);
  }
  p[str2->len] = '\0';

  return mrb_obj_value(str2);
}
/* -------------------------------------------------------------- */

#define lesser(a,b) (((a)>(b))?(b):(a))

/* ---------------------------*/
/*
 *  call-seq:
 *     mrb_value str1 <=> mrb_value str2   => int
 *                     >  1
 *                     =  0
 *                     <  -1
 */
int
mrb_str_cmp(mrb_state *mrb, mrb_value str1, mrb_value str2)
{
  mrb_int len;
  mrb_int retval;
  struct RString *s1 = mrb_str_ptr(str1);
  struct RString *s2 = mrb_str_ptr(str2);

  len = lesser(s1->len, s2->len);
  retval = memcmp(s1->ptr, s2->ptr, len);
  if (retval == 0) {
    if (s1->len == s2->len) return 0;
    if (s1->len > s2->len)  return 1;
    return -1;
  }
  if (retval > 0) return 1;
  return -1;
}

/* 15.2.10.5.3  */

/*
 *  call-seq:
 *     str <=> other_str   => -1, 0, +1
 *
 *  Comparison---Returns -1 if <i>other_str</i> is less than, 0 if
 *  <i>other_str</i> is equal to, and +1 if <i>other_str</i> is greater than
 *  <i>str</i>. If the strings are of different lengths, and the strings are
 *  equal when compared up to the shortest length, then the longer string is
 *  considered greater than the shorter one. If the variable <code>$=</code> is
 *  <code>false</code>, the comparison is based on comparing the binary values
 *  of each character in the string. In older versions of Ruby, setting
 *  <code>$=</code> allowed case-insensitive comparisons; this is now deprecated
 *  in favor of using <code>String#casecmp</code>.
 *
 *  <code><=></code> is the basis for the methods <code><</code>,
 *  <code><=</code>, <code>></code>, <code>>=</code>, and <code>between?</code>,
 *  included from module <code>Comparable</code>.  The method
 *  <code>String#==</code> does not use <code>Comparable#==</code>.
 *
 *     "abcdef" <=> "abcde"     #=> 1
 *     "abcdef" <=> "abcdef"    #=> 0
 *     "abcdef" <=> "abcdefg"   #=> -1
 *     "abcdef" <=> "ABCDEF"    #=> 1
 */
static mrb_value
mrb_str_cmp_m(mrb_state *mrb, mrb_value str1)
{
  mrb_value str2;
  mrb_int result;

  mrb_get_args(mrb, "o", &str2);
  if (!mrb_string_p(str2)) {
    if (!mrb_respond_to(mrb, str2, mrb_intern2(mrb, "to_s", 4))) {
      return mrb_nil_value();
    }
    else if (!mrb_respond_to(mrb, str2, mrb_intern2(mrb, "<=>", 3))) {
      return mrb_nil_value();
    }
    else {
      mrb_value tmp = mrb_funcall(mrb, str2, "<=>", 1, str1);

      if (mrb_nil_p(tmp)) return mrb_nil_value();
      if (!mrb_fixnum(tmp)) {
        return mrb_funcall(mrb, mrb_fixnum_value(0), "-", 1, tmp);
      }
      result = -mrb_fixnum(tmp);
    }
  }
  else {
    result = mrb_str_cmp(mrb, str1, str2);
  }
  return mrb_fixnum_value(result);
}

static mrb_bool
str_eql(mrb_state *mrb, const mrb_value str1, const mrb_value str2)
{
  const mrb_int len = RSTRING_LEN(str1);

  if (len != RSTRING_LEN(str2)) return FALSE;
  if (memcmp(RSTRING_PTR(str1), RSTRING_PTR(str2), (size_t)len) == 0)
    return TRUE;
  return FALSE;
}

mrb_bool
mrb_str_equal(mrb_state *mrb, mrb_value str1, mrb_value str2)
{
  if (mrb_obj_equal(mrb, str1, str2)) return TRUE;
  if (!mrb_string_p(str2)) {
    if (mrb_nil_p(str2)) return FALSE;
    if (!mrb_respond_to(mrb, str2, mrb_intern2(mrb, "to_str", 6))) {
      return FALSE;
    }
    str2 = mrb_funcall(mrb, str2, "to_str", 0);
    return mrb_equal(mrb, str2, str1);
  }
  return str_eql(mrb, str1, str2);
}

/* 15.2.10.5.4  */
/*
 *  call-seq:
 *     str == obj   => true or false
 *
 *  Equality---
 *  If <i>obj</i> is not a <code>String</code>, returns <code>false</code>.
 *  Otherwise, returns <code>false</code> or <code>true</code>
 *
 *   caution:if <i>str</i> <code><=></code> <i>obj</i> returns zero.
 */
static mrb_value
mrb_str_equal_m(mrb_state *mrb, mrb_value str1)
{
  mrb_value str2;
  mrb_bool equal_p;

  mrb_get_args(mrb, "o", &str2);
  equal_p = mrb_str_equal(mrb, str1, str2);

  return mrb_bool_value(equal_p);
}
/* ---------------------------------- */
mrb_value
mrb_str_to_str(mrb_state *mrb, mrb_value str)
{
  mrb_value s;

  if (!mrb_string_p(str)) {
    s = mrb_check_convert_type(mrb, str, MRB_TT_STRING, "String", "to_str");
    if (mrb_nil_p(s)) {
      s = mrb_convert_type(mrb, str, MRB_TT_STRING, "String", "to_s");
    }
    return s;
  }
  return str;
}

char *
mrb_string_value_ptr(mrb_state *mrb, mrb_value ptr)
{
    mrb_value str = mrb_str_to_str(mrb, ptr);
    return RSTRING_PTR(str);
}

static mrb_value
noregexp(mrb_state *mrb, mrb_value self)
{
  mrb_raise(mrb, E_NOTIMP_ERROR, "Regexp class not implemented");
  return mrb_nil_value();
}

static void
regexp_check(mrb_state *mrb, mrb_value obj)
{
  if (!memcmp(mrb_obj_classname(mrb, obj), REGEXP_CLASS, sizeof(REGEXP_CLASS) - 1)) {
    noregexp(mrb, obj);
  }
}

static inline mrb_int
mrb_memsearch_qs(const unsigned char *xs, mrb_int m, const unsigned char *ys, mrb_int n)
{
  const unsigned char *x = xs, *xe = xs + m;
  const unsigned char *y = ys;
  int i, qstable[256];

  /* Preprocessing */
  for (i = 0; i < 256; ++i)
    qstable[i] = m + 1;
  for (; x < xe; ++x)
    qstable[*x] = xe - x;
 /* Searching */
  for (; y + m <= ys + n; y += *(qstable + y[m])) {
    if (*xs == *y && memcmp(xs, y, m) == 0)
        return y - ys;
  }
  return -1;
}

static mrb_int
mrb_memsearch(const void *x0, mrb_int m, const void *y0, mrb_int n)
{
  const unsigned char *x = (const unsigned char *)x0, *y = (const unsigned char *)y0;

  if (m > n) return -1;
  else if (m == n) {
    return memcmp(x0, y0, m) == 0 ? 0 : -1;
  }
  else if (m < 1) {
    return 0;
  }
 else if (m == 1) {
    const unsigned char *ys = y, *ye = ys + n;
    for (; y < ye; ++y) {
      if (*x == *y)
        return y - ys;
    }
    return -1;
  }
  return mrb_memsearch_qs((const unsigned char *)x0, m, (const unsigned char *)y0, n);
}

static mrb_int
mrb_str_index(mrb_state *mrb, mrb_value str, mrb_value sub, mrb_int offset)
{
  mrb_int pos;
  char *s, *sptr;
  mrb_int len, slen;

  len = RSTRING_LEN(str);
  slen = RSTRING_LEN(sub);
  if (offset < 0) {
    offset += len;
    if (offset < 0) return -1;
  }
  if (len - offset < slen) return -1;
  s = RSTRING_PTR(str);
  if (offset) {
    s += offset;
  }
  if (slen == 0) return offset;
  /* need proceed one character at a time */
  sptr = RSTRING_PTR(sub);
  slen = RSTRING_LEN(sub);
  len = RSTRING_LEN(str) - offset;
  pos = mrb_memsearch(sptr, slen, s, len);
  if (pos < 0) return pos;
  return pos + offset;
}

mrb_value
mrb_str_dup(mrb_state *mrb, mrb_value str)
{
  /* should return shared string */
  struct RString *s = mrb_str_ptr(str);

  return mrb_str_new(mrb, s->ptr, s->len);
}

static mrb_value
mrb_str_aref(mrb_state *mrb, mrb_value str, mrb_value indx)
{
  mrb_int idx;

  regexp_check(mrb, indx);
  switch (mrb_type(indx)) {
    case MRB_TT_FIXNUM:
      idx = mrb_fixnum(indx);

num_index:
      str = mrb_str_substr(mrb, str, idx, 1);
      if (!mrb_nil_p(str) && RSTRING_LEN(str) == 0) return mrb_nil_value();
      return str;

    case MRB_TT_STRING:
      if (mrb_str_index(mrb, str, indx, 0) != -1)
        return mrb_str_dup(mrb, indx);
      return mrb_nil_value();

    case MRB_TT_RANGE:
      /* check if indx is Range */
      {
        mrb_int beg, len;
        mrb_value tmp;

        len = RSTRING_LEN(str);
        if (mrb_range_beg_len(mrb, indx, &beg, &len, len)) {
          tmp = mrb_str_subseq(mrb, str, beg, len);
          return tmp;
        }
        else {
          return mrb_nil_value();
        }
      }
    default:
      idx = mrb_fixnum(indx);
      goto num_index;
    }
    return mrb_nil_value();    /* not reached */
}

/* 15.2.10.5.6  */
/* 15.2.10.5.34 */
/*
 *  call-seq:
 *     str[fixnum]                 => fixnum or nil
 *     str[fixnum, fixnum]         => new_str or nil
 *     str[range]                  => new_str or nil
 *     str[regexp]                 => new_str or nil
 *     str[regexp, fixnum]         => new_str or nil
 *     str[other_str]              => new_str or nil
 *     str.slice(fixnum)           => fixnum or nil
 *     str.slice(fixnum, fixnum)   => new_str or nil
 *     str.slice(range)            => new_str or nil
 *     str.slice(regexp)           => new_str or nil
 *     str.slice(regexp, fixnum)   => new_str or nil
 *     str.slice(other_str)        => new_str or nil
 *
 *  Element Reference---If passed a single <code>Fixnum</code>, returns the code
 *  of the character at that position. If passed two <code>Fixnum</code>
 *  objects, returns a substring starting at the offset given by the first, and
 *  a length given by the second. If given a range, a substring containing
 *  characters at offsets given by the range is returned. In all three cases, if
 *  an offset is negative, it is counted from the end of <i>str</i>. Returns
 *  <code>nil</code> if the initial offset falls outside the string, the length
 *  is negative, or the beginning of the range is greater than the end.
 *
 *  If a <code>Regexp</code> is supplied, the matching portion of <i>str</i> is
 *  returned. If a numeric parameter follows the regular expression, that
 *  component of the <code>MatchData</code> is returned instead. If a
 *  <code>String</code> is given, that string is returned if it occurs in
 *  <i>str</i>. In both cases, <code>nil</code> is returned if there is no
 *  match.
 *
 *     a = "hello there"
 *     a[1]                   #=> 101(1.8.7) "e"(1.9.2)
 *     a[1,3]                 #=> "ell"
 *     a[1..3]                #=> "ell"
 *     a[-3,2]                #=> "er"
 *     a[-4..-2]              #=> "her"
 *     a[12..-1]              #=> nil
 *     a[-2..-4]              #=> ""
 *     a[/[aeiou](.)\1/]      #=> "ell"
 *     a[/[aeiou](.)\1/, 0]   #=> "ell"
 *     a[/[aeiou](.)\1/, 1]   #=> "l"
 *     a[/[aeiou](.)\1/, 2]   #=> nil
 *     a["lo"]                #=> "lo"
 *     a["bye"]               #=> nil
 */
static mrb_value
mrb_str_aref_m(mrb_state *mrb, mrb_value str)
{
  mrb_value a1, a2;
  int argc;

  argc = mrb_get_args(mrb, "o|o", &a1, &a2);
  if (argc == 2) {
    regexp_check(mrb, a1);
    return mrb_str_substr(mrb, str, mrb_fixnum(a1), mrb_fixnum(a2));
  }
  if (argc != 1) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for 1)", mrb_fixnum_value(argc));
  }
  return mrb_str_aref(mrb, str, a1);
}

/* 15.2.10.5.8  */
/*
 *  call-seq:
 *     str.capitalize!   => str or nil
 *
 *  Modifies <i>str</i> by converting the first character to uppercase and the
 *  remainder to lowercase. Returns <code>nil</code> if no changes are made.
 *
 *     a = "hello"
 *     a.capitalize!   #=> "Hello"
 *     a               #=> "Hello"
 *     a.capitalize!   #=> nil
 */
static mrb_value
mrb_str_capitalize_bang(mrb_state *mrb, mrb_value str)
{
  char *p, *pend;
  int modify = 0;
  struct RString *s = mrb_str_ptr(str);

  mrb_str_modify(mrb, s);
  if (s->len == 0 || !s->ptr) return mrb_nil_value();
  p = s->ptr; pend = s->ptr + s->len;
  if (ISLOWER(*p)) {
    *p = TOUPPER(*p);
    modify = 1;
  }
  while (++p < pend) {
    if (ISUPPER(*p)) {
      *p = TOLOWER(*p);
      modify = 1;
    }
  }
  if (modify) return str;
  return mrb_nil_value();
}

/* 15.2.10.5.7  */
/*
 *  call-seq:
 *     str.capitalize   => new_str
 *
 *  Returns a copy of <i>str</i> with the first character converted to uppercase
 *  and the remainder to lowercase.
 *
 *     "hello".capitalize    #=> "Hello"
 *     "HELLO".capitalize    #=> "Hello"
 *     "123ABC".capitalize   #=> "123abc"
 */
static mrb_value
mrb_str_capitalize(mrb_state *mrb, mrb_value self)
{
  mrb_value str;

  str = mrb_str_dup(mrb, self);
  mrb_str_capitalize_bang(mrb, str);
  return str;
}

/* 15.2.10.5.10  */
/*
 *  call-seq:
 *     str.chomp!(separator=$/)   => str or nil
 *
 *  Modifies <i>str</i> in place as described for <code>String#chomp</code>,
 *  returning <i>str</i>, or <code>nil</code> if no modifications were made.
 */
static mrb_value
mrb_str_chomp_bang(mrb_state *mrb, mrb_value str)
{
  mrb_value rs;
  mrb_int newline;
  char *p, *pp;
  mrb_int rslen;
  mrb_int len;
  struct RString *s = mrb_str_ptr(str);

  mrb_str_modify(mrb, s);
  len = s->len;
  if (mrb_get_args(mrb, "|S", &rs) == 0) {
    if (len == 0) return mrb_nil_value();
  smart_chomp:
    if (s->ptr[len-1] == '\n') {
      s->len--;
      if (s->len > 0 &&
          s->ptr[s->len-1] == '\r') {
        s->len--;
      }
    }
    else if (s->ptr[len-1] == '\r') {
      s->len--;
    }
    else {
      return mrb_nil_value();
    }
    s->ptr[s->len] = '\0';
    return str;
  }

  if (len == 0 || mrb_nil_p(rs)) return mrb_nil_value();
  p = s->ptr;
  rslen = RSTRING_LEN(rs);
  if (rslen == 0) {
    while (len>0 && p[len-1] == '\n') {
      len--;
      if (len>0 && p[len-1] == '\r')
        len--;
    }
    if (len < s->len) {
      s->len = len;
      p[len] = '\0';
      return str;
    }
    return mrb_nil_value();
  }
  if (rslen > len) return mrb_nil_value();
  newline = RSTRING_PTR(rs)[rslen-1];
  if (rslen == 1 && newline == '\n')
    newline = RSTRING_PTR(rs)[rslen-1];
  if (rslen == 1 && newline == '\n')
    goto smart_chomp;

  pp = p + len - rslen;
  if (p[len-1] == newline &&
     (rslen <= 1 ||
     memcmp(RSTRING_PTR(rs), pp, rslen) == 0)) {
    s->len = len - rslen;
    p[s->len] = '\0';
    return str;
  }
  return mrb_nil_value();
}

/* 15.2.10.5.9  */
/*
 *  call-seq:
 *     str.chomp(separator=$/)   => new_str
 *
 *  Returns a new <code>String</code> with the given record separator removed
 *  from the end of <i>str</i> (if present). If <code>$/</code> has not been
 *  changed from the default Ruby record separator, then <code>chomp</code> also
 *  removes carriage return characters (that is it will remove <code>\n</code>,
 *  <code>\r</code>, and <code>\r\n</code>).
 *
 *     "hello".chomp            #=> "hello"
 *     "hello\n".chomp          #=> "hello"
 *     "hello\r\n".chomp        #=> "hello"
 *     "hello\n\r".chomp        #=> "hello\n"
 *     "hello\r".chomp          #=> "hello"
 *     "hello \n there".chomp   #=> "hello \n there"
 *     "hello".chomp("llo")     #=> "he"
 */
static mrb_value
mrb_str_chomp(mrb_state *mrb, mrb_value self)
{
  mrb_value str;

  str = mrb_str_dup(mrb, self);
  mrb_str_chomp_bang(mrb, str);
  return str;
}

/* 15.2.10.5.12 */
/*
 *  call-seq:
 *     str.chop!   => str or nil
 *
 *  Processes <i>str</i> as for <code>String#chop</code>, returning <i>str</i>,
 *  or <code>nil</code> if <i>str</i> is the empty string.  See also
 *  <code>String#chomp!</code>.
 */
static mrb_value
mrb_str_chop_bang(mrb_state *mrb, mrb_value str)
{
  struct RString *s = mrb_str_ptr(str);

  mrb_str_modify(mrb, s);
  if (s->len > 0) {
    int len;
    len = s->len - 1;
    if (s->ptr[len] == '\n') {
      if (len > 0 &&
          s->ptr[len-1] == '\r') {
        len--;
      }
    }
    s->len = len;
    s->ptr[len] = '\0';
    return str;
  }
  return mrb_nil_value();
}

/* 15.2.10.5.11 */
/*
 *  call-seq:
 *     str.chop   => new_str
 *
 *  Returns a new <code>String</code> with the last character removed.  If the
 *  string ends with <code>\r\n</code>, both characters are removed. Applying
 *  <code>chop</code> to an empty string returns an empty
 *  string. <code>String#chomp</code> is often a safer alternative, as it leaves
 *  the string unchanged if it doesn't end in a record separator.
 *
 *     "string\r\n".chop   #=> "string"
 *     "string\n\r".chop   #=> "string\n"
 *     "string\n".chop     #=> "string"
 *     "string".chop       #=> "strin"
 *     "x".chop            #=> ""
 */
static mrb_value
mrb_str_chop(mrb_state *mrb, mrb_value self)
{
  mrb_value str;
  str = mrb_str_dup(mrb, self);
  mrb_str_chop_bang(mrb, str);
  return str;
}

/* 15.2.10.5.14 */
/*
 *  call-seq:
 *     str.downcase!   => str or nil
 *
 *  Downcases the contents of <i>str</i>, returning <code>nil</code> if no
 *  changes were made.
 */
static mrb_value
mrb_str_downcase_bang(mrb_state *mrb, mrb_value str)
{
  char *p, *pend;
  int modify = 0;
  struct RString *s = mrb_str_ptr(str);

  mrb_str_modify(mrb, s);
  p = s->ptr;
  pend = s->ptr + s->len;
  while (p < pend) {
    if (ISUPPER(*p)) {
      *p = TOLOWER(*p);
      modify = 1;
    }
    p++;
  }

  if (modify) return str;
  return mrb_nil_value();
}

/* 15.2.10.5.13 */
/*
 *  call-seq:
 *     str.downcase   => new_str
 *
 *  Returns a copy of <i>str</i> with all uppercase letters replaced with their
 *  lowercase counterparts. The operation is locale insensitive---only
 *  characters ``A'' to ``Z'' are affected.
 *
 *     "hEllO".downcase   #=> "hello"
 */
static mrb_value
mrb_str_downcase(mrb_state *mrb, mrb_value self)
{
  mrb_value str;

  str = mrb_str_dup(mrb, self);
  mrb_str_downcase_bang(mrb, str);
  return str;
}

/* 15.2.10.5.16 */
/*
 *  call-seq:
 *     str.empty?   => true or false
 *
 *  Returns <code>true</code> if <i>str</i> has a length of zero.
 *
 *     "hello".empty?   #=> false
 *     "".empty?        #=> true
 */
static mrb_value
mrb_str_empty_p(mrb_state *mrb, mrb_value self)
{
  struct RString *s = mrb_str_ptr(self);

  return mrb_bool_value(s->len == 0);
}

/* 15.2.10.5.17 */
/*
 * call-seq:
 *   str.eql?(other)   => true or false
 *
 * Two strings are equal if the have the same length and content.
 */
static mrb_value
mrb_str_eql(mrb_state *mrb, mrb_value self)
{
  mrb_value str2;
  mrb_bool eql_p;

  mrb_get_args(mrb, "o", &str2);
  eql_p = (mrb_type(str2) == MRB_TT_STRING) && str_eql(mrb, self, str2);

  return mrb_bool_value(eql_p);
}

static mrb_value
mrb_str_subseq(mrb_state *mrb, mrb_value str, mrb_int beg, mrb_int len)
{
  struct RString *orig, *s;
  mrb_shared_string *shared;

  orig = mrb_str_ptr(str);
  str_make_shared(mrb, orig);
  shared = orig->aux.shared;
  s = mrb_obj_alloc_string(mrb);
  s->ptr = orig->ptr + beg;
  s->len = len;
  s->aux.shared = shared;
  s->flags |= MRB_STR_SHARED;
  shared->refcnt++;

  return mrb_obj_value(s);
}

mrb_value
mrb_str_substr(mrb_state *mrb, mrb_value str, mrb_int beg, mrb_int len)
{
  mrb_value str2;

  if (len < 0) return mrb_nil_value();
  if (!RSTRING_LEN(str)) {
    len = 0;
  }
  if (beg > RSTRING_LEN(str)) return mrb_nil_value();
  if (beg < 0) {
    beg += RSTRING_LEN(str);
    if (beg < 0) return mrb_nil_value();
  }
  if (beg + len > RSTRING_LEN(str))
    len = RSTRING_LEN(str) - beg;
  if (len <= 0) {
    len = 0;
  }
  str2 = mrb_str_subseq(mrb, str, beg, len);

  return str2;
}

mrb_value
mrb_str_buf_append(mrb_state *mrb, mrb_value str, mrb_value str2)
{
  mrb_str_cat(mrb, str, RSTRING_PTR(str2), RSTRING_LEN(str2));
  return str;
}

mrb_int
mrb_str_hash(mrb_state *mrb, mrb_value str)
{
  /* 1-8-7 */
  struct RString *s = mrb_str_ptr(str);
  mrb_int len = s->len;
  char *p = s->ptr;
  mrb_int key = 0;

  while (len--) {
    key = key*65599 + *p;
    p++;
  }
  key = key + (key>>5);
  return key;
}

/* 15.2.10.5.20 */
/*
 * call-seq:
 *    str.hash   => fixnum
 *
 * Return a hash based on the string's length and content.
 */
static mrb_value
mrb_str_hash_m(mrb_state *mrb, mrb_value self)
{
  mrb_int key = mrb_str_hash(mrb, self);
  return mrb_fixnum_value(key);
}

/* 15.2.10.5.21 */
/*
 *  call-seq:
 *     str.include? other_str   => true or false
 *     str.include? fixnum      => true or false
 *
 *  Returns <code>true</code> if <i>str</i> contains the given string or
 *  character.
 *
 *     "hello".include? "lo"   #=> true
 *     "hello".include? "ol"   #=> false
 *     "hello".include? ?h     #=> true
 */
static mrb_value
mrb_str_include(mrb_state *mrb, mrb_value self)
{
  mrb_int i;
  mrb_value str2;
  mrb_bool include_p;

  mrb_get_args(mrb, "o", &str2);
  if (mrb_type(str2) == MRB_TT_FIXNUM) {
    include_p = (memchr(RSTRING_PTR(self), mrb_fixnum(str2), RSTRING_LEN(self)) != NULL);
  }
  else {
    str2 = mrb_str_to_str(mrb, str2);
    i = mrb_str_index(mrb, self, str2, 0);

    include_p = (i != -1);
  }

  return mrb_bool_value(include_p);
}

/* 15.2.10.5.22 */
/*
 *  call-seq:
 *     str.index(substring [, offset])   => fixnum or nil
 *     str.index(fixnum [, offset])      => fixnum or nil
 *     str.index(regexp [, offset])      => fixnum or nil
 *
 *  Returns the index of the first occurrence of the given
 *  <i>substring</i>,
 *  character (<i>fixnum</i>), or pattern (<i>regexp</i>) in <i>str</i>.
 *  Returns
 *  <code>nil</code> if not found.
 *  If the second parameter is present, it
 *  specifies the position in the string to begin the search.
 *
 *     "hello".index('e')             #=> 1
 *     "hello".index('lo')            #=> 3
 *     "hello".index('a')             #=> nil
 *     "hello".index(101)             #=> 1(101=0x65='e')
 *     "hello".index(/[aeiou]/, -3)   #=> 4
 */
static mrb_value
mrb_str_index_m(mrb_state *mrb, mrb_value str)
{
  mrb_value *argv;
  int argc;

  mrb_value sub;
  mrb_int pos;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 2) {
    pos = mrb_fixnum(argv[1]);
    sub = argv[0];
  }
  else {
    pos = 0;
    if (argc > 0)
      sub = argv[0];
    else
      sub = mrb_nil_value();

  }
  regexp_check(mrb, sub);
  if (pos < 0) {
    pos += RSTRING_LEN(str);
    if (pos < 0) {
      return mrb_nil_value();
    }
  }

  switch (mrb_type(sub)) {
    case MRB_TT_FIXNUM: {
      int c = mrb_fixnum(sub);
      mrb_int len = RSTRING_LEN(str);
      unsigned char *p = (unsigned char*)RSTRING_PTR(str);

      for (;pos<len;pos++) {
        if (p[pos] == c) return mrb_fixnum_value(pos);
      }
      return mrb_nil_value();
    }

    default: {
      mrb_value tmp;

      tmp = mrb_check_string_type(mrb, sub);
      if (mrb_nil_p(tmp)) {
        mrb_raisef(mrb, E_TYPE_ERROR, "type mismatch: %S given", sub);
      }
      sub = tmp;
    }
    /* fall through */
    case MRB_TT_STRING:
      pos = mrb_str_index(mrb, str, sub, pos);
      break;
  }

  if (pos == -1) return mrb_nil_value();
  return mrb_fixnum_value(pos);
}

#define STR_REPLACE_SHARED_MIN 10

static mrb_value
str_replace(mrb_state *mrb, struct RString *s1, struct RString *s2)
{
  if (s2->flags & MRB_STR_SHARED) {
  L_SHARE:
    if (s1->flags & MRB_STR_SHARED){
      str_decref(mrb, s1->aux.shared);
    }
    else {
      mrb_free(mrb, s1->ptr);
    }
    s1->ptr = s2->ptr;
    s1->len = s2->len;
    s1->aux.shared = s2->aux.shared;
    s1->flags |= MRB_STR_SHARED;
    s1->aux.shared->refcnt++;
  }
  else if (s2->len > STR_REPLACE_SHARED_MIN) {
    str_make_shared(mrb, s2);
    goto L_SHARE;
  }
  else {
    if (s1->flags & MRB_STR_SHARED) {
      str_decref(mrb, s1->aux.shared);
      s1->flags &= ~MRB_STR_SHARED;
      s1->ptr = (char *)mrb_malloc(mrb, s2->len+1);
    }
    else {
      s1->ptr = (char *)mrb_realloc(mrb, s1->ptr, s2->len+1);
    }
    memcpy(s1->ptr, s2->ptr, s2->len);
    s1->ptr[s2->len] = 0;
    s1->len = s2->len;
    s1->aux.capa = s2->len;
  }
  return mrb_obj_value(s1);
}

/* 15.2.10.5.24 */
/* 15.2.10.5.28 */
/*
 *  call-seq:
 *     str.replace(other_str)   => str
 *
 *     s = "hello"         #=> "hello"
 *     s.replace "world"   #=> "world"
 */
static mrb_value
mrb_str_replace(mrb_state *mrb, mrb_value str)
{
  mrb_value str2;

  mrb_get_args(mrb, "S", &str2);
  return str_replace(mrb, mrb_str_ptr(str), mrb_str_ptr(str2));
}

/* 15.2.10.5.23 */
/*
 *  call-seq:
 *     String.new(str="")   => new_str
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
static mrb_value
mrb_str_init(mrb_state *mrb, mrb_value self)
{
  mrb_value str2;

  if (mrb_get_args(mrb, "|S", &str2) == 1) {
    str_replace(mrb, mrb_str_ptr(self), mrb_str_ptr(str2));
  }
  return self;
}

/* 15.2.10.5.25 */
/* 15.2.10.5.41 */
/*
 *  call-seq:
 *     str.intern   => symbol
 *     str.to_sym   => symbol
 *
 *  Returns the <code>Symbol</code> corresponding to <i>str</i>, creating the
 *  symbol if it did not previously exist. See <code>Symbol#id2name</code>.
 *
 *     "Koala".intern         #=> :Koala
 *     s = 'cat'.to_sym       #=> :cat
 *     s == :cat              #=> true
 *     s = '@cat'.to_sym      #=> :@cat
 *     s == :@cat             #=> true
 *
 *  This can also be used to create symbols that cannot be represented using the
 *  <code>:xxx</code> notation.
 *
 *     'cat and dog'.to_sym   #=> :"cat and dog"
 */
mrb_value
mrb_str_intern(mrb_state *mrb, mrb_value self)
{
  mrb_sym id;

  id = mrb_intern_str(mrb, self);
  return mrb_symbol_value(id);

}
/* ---------------------------------- */
mrb_value
mrb_obj_as_string(mrb_state *mrb, mrb_value obj)
{
  mrb_value str;

  if (mrb_string_p(obj)) {
    return obj;
  }
  str = mrb_funcall(mrb, obj, "to_s", 0);
  if (!mrb_string_p(str))
    return mrb_any_to_s(mrb, obj);
  return str;
}

mrb_value
mrb_ptr_to_str(mrb_state *mrb, void *p)
{
  struct RString *p_str;
  char *p1;
  char *p2;
  intptr_t n = (intptr_t)p;

  p_str = str_new(mrb, NULL, 2 + sizeof(uintptr_t) * CHAR_BIT / 4);
  p1 = p_str->ptr;
  *p1++ = '0';
  *p1++ = 'x';
  p2 = p1;

  do {
    *p2++ = mrb_digitmap[n % 16];
    n /= 16;
  } while (n > 0);
  *p2 = '\0';
  p_str->len = (mrb_int)(p2 - p_str->ptr);

  while (p1 < p2) {
    const char  c = *p1;
    *p1++ = *--p2;
    *p2 = c;
  }

  return mrb_obj_value(p_str);
}

mrb_value
mrb_check_string_type(mrb_state *mrb, mrb_value str)
{
  return mrb_check_convert_type(mrb, str, MRB_TT_STRING, "String", "to_str");
}

/* ---------------------------------- */
/* 15.2.10.5.29 */
/*
 *  call-seq:
 *     str.reverse   => new_str
 *
 *  Returns a new string with the characters from <i>str</i> in reverse order.
 *
 *     "stressed".reverse   #=> "desserts"
 */
static mrb_value
mrb_str_reverse(mrb_state *mrb, mrb_value str)
{
  struct RString *s2;
  char *s, *e, *p;

  if (RSTRING(str)->len <= 1) return mrb_str_dup(mrb, str);

  s2 = str_new(mrb, 0, RSTRING(str)->len);
  str_with_class(mrb, s2, str);
  s = RSTRING_PTR(str); e = RSTRING_END(str) - 1;
  p = s2->ptr;

  while (e >= s) {
    *p++ = *e--;
  }
  return mrb_obj_value(s2);
}

/* 15.2.10.5.30 */
/*
 *  call-seq:
 *     str.reverse!   => str
 *
 *  Reverses <i>str</i> in place.
 */
static mrb_value
mrb_str_reverse_bang(mrb_state *mrb, mrb_value str)
{
  struct RString *s = mrb_str_ptr(str);
  char *p, *e;
  char c;

  mrb_str_modify(mrb, s);
  if (s->len > 1) {
    p = s->ptr;
    e = p + s->len - 1;
    while (p < e) {
      c = *p;
      *p++ = *e;
      *e-- = c;
    }
  }
  return str;
}

/*
 *  call-seq:
 *     str.rindex(substring [, fixnum])   => fixnum or nil
 *     str.rindex(fixnum [, fixnum])   => fixnum or nil
 *     str.rindex(regexp [, fixnum])   => fixnum or nil
 *
 *  Returns the index of the last occurrence of the given <i>substring</i>,
 *  character (<i>fixnum</i>), or pattern (<i>regexp</i>) in <i>str</i>. Returns
 *  <code>nil</code> if not found. If the second parameter is present, it
 *  specifies the position in the string to end the search---characters beyond
 *  this point will not be considered.
 *
 *     "hello".rindex('e')             #=> 1
 *     "hello".rindex('l')             #=> 3
 *     "hello".rindex('a')             #=> nil
 *     "hello".rindex(101)             #=> 1
 *     "hello".rindex(/[aeiou]/, -2)   #=> 1
 */
static mrb_int
mrb_str_rindex(mrb_state *mrb, mrb_value str, mrb_value sub, mrb_int pos)
{
  char *s, *sbeg, *t;
  struct RString *ps = mrb_str_ptr(str);
  struct RString *psub = mrb_str_ptr(sub);
  mrb_int len = psub->len;

  /* substring longer than string */
  if (ps->len < len) return -1;
  if (ps->len - pos < len) {
    pos = ps->len - len;
  }
  sbeg = ps->ptr;
  s = ps->ptr + pos;
  t = psub->ptr;
  if (len) {
    while (sbeg <= s) {
      if (memcmp(s, t, len) == 0) {
        return s - ps->ptr;
      }
      s--;
    }
    return -1;
  }
  else {
    return pos;
  }
}

/* 15.2.10.5.31 */
/*
 *  call-seq:
 *     str.rindex(substring [, fixnum])   => fixnum or nil
 *     str.rindex(fixnum [, fixnum])   => fixnum or nil
 *     str.rindex(regexp [, fixnum])   => fixnum or nil
 *
 *  Returns the index of the last occurrence of the given <i>substring</i>,
 *  character (<i>fixnum</i>), or pattern (<i>regexp</i>) in <i>str</i>. Returns
 *  <code>nil</code> if not found. If the second parameter is present, it
 *  specifies the position in the string to end the search---characters beyond
 *  this point will not be considered.
 *
 *     "hello".rindex('e')             #=> 1
 *     "hello".rindex('l')             #=> 3
 *     "hello".rindex('a')             #=> nil
 *     "hello".rindex(101)             #=> 1
 *     "hello".rindex(/[aeiou]/, -2)   #=> 1
 */
static mrb_value
mrb_str_rindex_m(mrb_state *mrb, mrb_value str)
{
  mrb_value *argv;
  int argc;
  mrb_value sub;
  mrb_value vpos;
  int pos, len = RSTRING_LEN(str);

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 2) {
    sub = argv[0];
    vpos = argv[1];
    pos = mrb_fixnum(vpos);
    if (pos < 0) {
      pos += len;
      if (pos < 0) {
        regexp_check(mrb, sub);
        return mrb_nil_value();
      }
    }
    if (pos > len) pos = len;
  }
  else {
    pos = len;
    if (argc > 0)
      sub = argv[0];
    else
      sub = mrb_nil_value();
  }
  regexp_check(mrb, sub);

  switch (mrb_type(sub)) {
    case MRB_TT_FIXNUM: {
      int c = mrb_fixnum(sub);
      mrb_int len = RSTRING_LEN(str);
      unsigned char *p = (unsigned char*)RSTRING_PTR(str);

      for (pos=len;pos>=0;pos--) {
        if (p[pos] == c) return mrb_fixnum_value(pos);
      }
      return mrb_nil_value();
    }

    default: {
      mrb_value tmp;

      tmp = mrb_check_string_type(mrb, sub);
      if (mrb_nil_p(tmp)) {
        mrb_raisef(mrb, E_TYPE_ERROR, "type mismatch: %S given", sub);
      }
      sub = tmp;
    }
     /* fall through */
    case MRB_TT_STRING:
      pos = mrb_str_rindex(mrb, str, sub, pos);
      if (pos >= 0) return mrb_fixnum_value(pos);
      break;

  } /* end of switch (TYPE(sub)) */
  return mrb_nil_value();
}

static const char isspacetable[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define ascii_isspace(c) isspacetable[(unsigned char)(c)]

/* 15.2.10.5.35 */

/*
 *  call-seq:
 *     str.split(pattern=$;, [limit])   => anArray
 *
 *  Divides <i>str</i> into substrings based on a delimiter, returning an array
 *  of these substrings.
 *
 *  If <i>pattern</i> is a <code>String</code>, then its contents are used as
 *  the delimiter when splitting <i>str</i>. If <i>pattern</i> is a single
 *  space, <i>str</i> is split on whitespace, with leading whitespace and runs
 *  of contiguous whitespace characters ignored.
 *
 *  If <i>pattern</i> is a <code>Regexp</code>, <i>str</i> is divided where the
 *  pattern matches. Whenever the pattern matches a zero-length string,
 *  <i>str</i> is split into individual characters.
 *
 *  If <i>pattern</i> is omitted, the value of <code>$;</code> is used.  If
 *  <code>$;</code> is <code>nil</code> (which is the default), <i>str</i> is
 *  split on whitespace as if ` ' were specified.
 *
 *  If the <i>limit</i> parameter is omitted, trailing null fields are
 *  suppressed. If <i>limit</i> is a positive number, at most that number of
 *  fields will be returned (if <i>limit</i> is <code>1</code>, the entire
 *  string is returned as the only entry in an array). If negative, there is no
 *  limit to the number of fields returned, and trailing null fields are not
 *  suppressed.
 *
 *     " now's  the time".split        #=> ["now's", "the", "time"]
 *     " now's  the time".split(' ')   #=> ["now's", "the", "time"]
 *     " now's  the time".split(/ /)   #=> ["", "now's", "", "the", "time"]
 *     "1, 2.34,56, 7".split(%r{,\s*}) #=> ["1", "2.34", "56", "7"]
 *     "hello".split(//)               #=> ["h", "e", "l", "l", "o"]
 *     "hello".split(//, 3)            #=> ["h", "e", "llo"]
 *     "hi mom".split(%r{\s*})         #=> ["h", "i", "m", "o", "m"]
 *
 *     "mellow yellow".split("ello")   #=> ["m", "w y", "w"]
 *     "1,2,,3,4,,".split(',')         #=> ["1", "2", "", "3", "4"]
 *     "1,2,,3,4,,".split(',', 4)      #=> ["1", "2", "", "3,4,,"]
 *     "1,2,,3,4,,".split(',', -4)     #=> ["1", "2", "", "3", "4", "", ""]
 */

static mrb_value
mrb_str_split_m(mrb_state *mrb, mrb_value str)
{
  int argc;
  mrb_value spat = mrb_nil_value();
  enum {awk, string, regexp} split_type = string;
  long i = 0, lim_p;
  mrb_int beg;
  mrb_int end;
  mrb_int lim = 0;
  mrb_value result, tmp;

  argc = mrb_get_args(mrb, "|oi", &spat, &lim);
  lim_p = (lim > 0 && argc == 2);
  if (argc == 2) {
    if (lim == 1) {
      if (RSTRING_LEN(str) == 0)
        return mrb_ary_new_capa(mrb, 0);
      return mrb_ary_new_from_values(mrb, 1, &str);
    }
    i = 1;
  }

  if (argc == 0 || mrb_nil_p(spat)) {
    split_type = awk;
  }
  else {
    if (mrb_string_p(spat)) {
      split_type = string;
      if (RSTRING_LEN(spat) == 1 && RSTRING_PTR(spat)[0] == ' '){
          split_type = awk;
      }
    }
    else {
      noregexp(mrb, str);
    }
  }

  result = mrb_ary_new(mrb);
  beg = 0;
  if (split_type == awk) {
    char *ptr = RSTRING_PTR(str);
    char *eptr = RSTRING_END(str);
    char *bptr = ptr;
    int skip = 1;
    unsigned int c;

    end = beg;
    while (ptr < eptr) {
      int ai = mrb_gc_arena_save(mrb);
      c = (unsigned char)*ptr++;
      if (skip) {
        if (ascii_isspace(c)) {
          beg = ptr - bptr;
        }
        else {
          end = ptr - bptr;
          skip = 0;
          if (lim_p && lim <= i) break;
        }
      }
      else if (ascii_isspace(c)) {
        mrb_ary_push(mrb, result, mrb_str_subseq(mrb, str, beg, end-beg));
        mrb_gc_arena_restore(mrb, ai);
        skip = 1;
        beg = ptr - bptr;
        if (lim_p) ++i;
      }
      else {
        end = ptr - bptr;
      }
    }
  }
  else if (split_type == string) {
    char *ptr = RSTRING_PTR(str);
    char *temp = ptr;
    char *eptr = RSTRING_END(str);
    mrb_int slen = RSTRING_LEN(spat);

    if (slen == 0) {
      int ai = mrb_gc_arena_save(mrb);
      while (ptr < eptr) {
        mrb_ary_push(mrb, result, mrb_str_subseq(mrb, str, ptr-temp, 1));
        mrb_gc_arena_restore(mrb, ai);
        ptr++;
        if (lim_p && lim <= ++i) break;
      }
    }
    else {
      char *sptr = RSTRING_PTR(spat);
      int ai = mrb_gc_arena_save(mrb);

      while (ptr < eptr &&
        (end = mrb_memsearch(sptr, slen, ptr, eptr - ptr)) >= 0) {
        mrb_ary_push(mrb, result, mrb_str_subseq(mrb, str, ptr - temp, end));
        mrb_gc_arena_restore(mrb, ai);
        ptr += end + slen;
        if (lim_p && lim <= ++i) break;
      }
    }
    beg = ptr - temp;
  }
  else {
    noregexp(mrb, str);
  }
  if (RSTRING_LEN(str) > 0 && (lim_p || RSTRING_LEN(str) > beg || lim < 0)) {
    if (RSTRING_LEN(str) == beg) {
      tmp = mrb_str_new_empty(mrb, str);
    }
    else {
      tmp = mrb_str_subseq(mrb, str, beg, RSTRING_LEN(str)-beg);
    }
    mrb_ary_push(mrb, result, tmp);
  }
  if (!lim_p && lim == 0) {
    mrb_int len;
    while ((len = RARRAY_LEN(result)) > 0 &&
           (tmp = RARRAY_PTR(result)[len-1], RSTRING_LEN(tmp) == 0))
      mrb_ary_pop(mrb, result);
  }

  return result;
}

mrb_value
mrb_cstr_to_inum(mrb_state *mrb, const char *str, int base, int badcheck)
{
  char *end;
  char sign = 1;
  int c;
  unsigned long n;
  mrb_int val;

#undef ISDIGIT
#define ISDIGIT(c) ('0' <= (c) && (c) <= '9')
#define conv_digit(c) \
    (!ISASCII(c) ? -1 : \
     isdigit(c) ? ((c) - '0') : \
     islower(c) ? ((c) - 'a' + 10) : \
     isupper(c) ? ((c) - 'A' + 10) : \
     -1)

  if (!str) {
    if (badcheck) goto bad;
    return mrb_fixnum_value(0);
  }
  while (ISSPACE(*str)) str++;

  if (str[0] == '+') {
    str++;
  }
  else if (str[0] == '-') {
    str++;
    sign = 0;
  }
  if (str[0] == '+' || str[0] == '-') {
    if (badcheck) goto bad;
    return mrb_fixnum_value(0);
  }
  if (base <= 0) {
    if (str[0] == '0') {
      switch (str[1]) {
        case 'x': case 'X':
          base = 16;
          break;
        case 'b': case 'B':
          base = 2;
          break;
        case 'o': case 'O':
          base = 8;
          break;
        case 'd': case 'D':
          base = 10;
          break;
        default:
          base = 8;
      }
    }
    else if (base < -1) {
      base = -base;
    }
    else {
      base = 10;
    }
  }
  switch (base) {
    case 2:
      if (str[0] == '0' && (str[1] == 'b'||str[1] == 'B')) {
        str += 2;
      }
      break;
    case 3:
      break;
    case 8:
      if (str[0] == '0' && (str[1] == 'o'||str[1] == 'O')) {
        str += 2;
      }
    case 4: case 5: case 6: case 7:
      break;
    case 10:
      if (str[0] == '0' && (str[1] == 'd'||str[1] == 'D')) {
        str += 2;
      }
    case 9: case 11: case 12: case 13: case 14: case 15:
      break;
    case 16:
      if (str[0] == '0' && (str[1] == 'x'||str[1] == 'X')) {
        str += 2;
      }
      break;
    default:
      if (base < 2 || 36 < base) {
        mrb_raisef(mrb, E_ARGUMENT_ERROR, "illegal radix %S", mrb_fixnum_value(base));
      }
      break;
  } /* end of switch (base) { */
  if (*str == '0') {    /* squeeze preceeding 0s */
    int us = 0;
    while ((c = *++str) == '0' || c == '_') {
      if (c == '_') {
        if (++us >= 2)
          break;
      }
      else
        us = 0;
    }
    if (!(c = *str) || ISSPACE(c)) --str;
  }
  c = *str;
  c = conv_digit(c);
  if (c < 0 || c >= base) {
    if (badcheck) goto bad;
    return mrb_fixnum_value(0);
  }

  n = strtoul((char*)str, &end, base);
  if (n > MRB_INT_MAX) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "string (%S) too big for integer", mrb_str_new_cstr(mrb, str));
  }
  val = n;
  if (badcheck) {
    if (end == str) goto bad; /* no number */
    while (*end && ISSPACE(*end)) end++;
    if (*end) goto bad;        /* trailing garbage */
  }

  return mrb_fixnum_value(sign ? val : -val);
bad:
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid string for number(%S)", mrb_str_new_cstr(mrb, str));
  /* not reached */
  return mrb_fixnum_value(0);
}

char *
mrb_string_value_cstr(mrb_state *mrb, mrb_value *ptr)
{
  struct RString *ps = mrb_str_ptr(*ptr);
  char *s = ps->ptr;

  if (!s || ps->len != strlen(s)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "string contains null byte");
  }
  return s;
}

mrb_value
mrb_str_to_inum(mrb_state *mrb, mrb_value str, int base, int badcheck)
{
  char *s;
  int len;

  str = mrb_str_to_str(mrb, str);
  if (badcheck) {
    s = mrb_string_value_cstr(mrb, &str);
  }
  else {
    s = RSTRING_PTR(str);
  }
  if (s) {
    len = RSTRING_LEN(str);
    if (s[len]) {    /* no sentinel somehow */
      struct RString *temp_str = str_new(mrb, s, len);
      s = temp_str->ptr;
    }
  }
  return mrb_cstr_to_inum(mrb, s, base, badcheck);
}

/* 15.2.10.5.38 */
/*
 *  call-seq:
 *     str.to_i(base=10)   => integer
 *
 *  Returns the result of interpreting leading characters in <i>str</i> as an
 *  integer base <i>base</i> (between 2 and 36). Extraneous characters past the
 *  end of a valid number are ignored. If there is not a valid number at the
 *  start of <i>str</i>, <code>0</code> is returned. This method never raises an
 *  exception.
 *
 *     "12345".to_i             #=> 12345
 *     "99 red balloons".to_i   #=> 99
 *     "0a".to_i                #=> 0
 *     "0a".to_i(16)            #=> 10
 *     "hello".to_i             #=> 0
 *     "1100101".to_i(2)        #=> 101
 *     "1100101".to_i(8)        #=> 294977
 *     "1100101".to_i(10)       #=> 1100101
 *     "1100101".to_i(16)       #=> 17826049
 */
static mrb_value
mrb_str_to_i(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;
  int base;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 0)
    base = 10;
  else
    base = mrb_fixnum(argv[0]);

  if (base < 0) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "illegal radix %S", mrb_fixnum_value(base));
  }
  return mrb_str_to_inum(mrb, self, base, 0/*Qfalse*/);
}

double
mrb_cstr_to_dbl(mrb_state *mrb, const char * p, int badcheck)
{
  char *end;
  double d;
#if !defined(DBL_DIG)
# define DBL_DIG 16
#endif

  enum {max_width = 20};
#define OutOfRange() (((w = end - p) > max_width) ? \
      (w = max_width, ellipsis = "...") : \
      (w = (int)(end - p), ellipsis = ""))

  if (!p) return 0.0;
  while (ISSPACE(*p)) p++;

  if (!badcheck && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    return 0.0;
  }
  d = strtod(p, &end);
  if (p == end) {
    if (badcheck) {
bad:
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid string for float(%S)", mrb_str_new_cstr(mrb, p));
      /* not reached */
    }
    return d;
  }
  if (*end) {
    char buf[DBL_DIG * 4 + 10];
    char *n = buf;
    char *e = buf + sizeof(buf) - 1;
    char prev = 0;

    while (p < end && n < e) prev = *n++ = *p++;
    while (*p) {
      if (*p == '_') {
        /* remove underscores between digits */
        if (badcheck) {
          if (n == buf || !ISDIGIT(prev)) goto bad;
          ++p;
          if (!ISDIGIT(*p)) goto bad;
        }
        else {
          while (*++p == '_');
          continue;
        }
      }
      prev = *p++;
      if (n < e) *n++ = prev;
    }
    *n = '\0';
    p = buf;

    if (!badcheck && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
      return 0.0;
    }

    d = strtod(p, &end);
    if (badcheck) {
      if (!end || p == end) goto bad;
      while (*end && ISSPACE(*end)) end++;
      if (*end) goto bad;
    }
  }
  return d;
}

double
mrb_str_to_dbl(mrb_state *mrb, mrb_value str, int badcheck)
{
  char *s;
  int len;

  str = mrb_str_to_str(mrb, str);
  s = RSTRING_PTR(str);
  len = RSTRING_LEN(str);
  if (s) {
    if (badcheck && memchr(s, '\0', len)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "string for Float contains null byte");
    }
    if (s[len]) {    /* no sentinel somehow */
      struct RString *temp_str = str_new(mrb, s, len);
      s = temp_str->ptr;
    }
  }
  return mrb_cstr_to_dbl(mrb, s, badcheck);
}

/* 15.2.10.5.39 */
/*
 *  call-seq:
 *     str.to_f   => float
 *
 *  Returns the result of interpreting leading characters in <i>str</i> as a
 *  floating point number. Extraneous characters past the end of a valid number
 *  are ignored. If there is not a valid number at the start of <i>str</i>,
 *  <code>0.0</code> is returned. This method never raises an exception.
 *
 *     "123.45e1".to_f        #=> 1234.5
 *     "45.67 degrees".to_f   #=> 45.67
 *     "thx1138".to_f         #=> 0.0
 */
static mrb_value
mrb_str_to_f(mrb_state *mrb, mrb_value self)
{
  return mrb_float_value(mrb, mrb_str_to_dbl(mrb, self, 0/*Qfalse*/));
}

/* 15.2.10.5.40 */
/*
 *  call-seq:
 *     str.to_s     => str
 *     str.to_str   => str
 *
 *  Returns the receiver.
 */
static mrb_value
mrb_str_to_s(mrb_state *mrb, mrb_value self)
{
  if (mrb_obj_class(mrb, self) != mrb->string_class) {
    return mrb_str_dup(mrb, self);
  }
  return self;
}

/* 15.2.10.5.43 */
/*
 *  call-seq:
 *     str.upcase!   => str or nil
 *
 *  Upcases the contents of <i>str</i>, returning <code>nil</code> if no changes
 *  were made.
 */
static mrb_value
mrb_str_upcase_bang(mrb_state *mrb, mrb_value str)
{
  struct RString *s = mrb_str_ptr(str);
  char *p, *pend;
  int modify = 0;

  mrb_str_modify(mrb, s);
  p = RSTRING_PTR(str);
  pend = RSTRING_END(str);
  while (p < pend) {
    if (ISLOWER(*p)) {
      *p = TOUPPER(*p);
      modify = 1;
    }
    p++;
  }

  if (modify) return str;
  return mrb_nil_value();
}

/* 15.2.10.5.42 */
/*
 *  call-seq:
 *     str.upcase   => new_str
 *
 *  Returns a copy of <i>str</i> with all lowercase letters replaced with their
 *  uppercase counterparts. The operation is locale insensitive---only
 *  characters ``a'' to ``z'' are affected.
 *
 *     "hEllO".upcase   #=> "HELLO"
 */
static mrb_value
mrb_str_upcase(mrb_state *mrb, mrb_value self)
{
  mrb_value str;

  str = mrb_str_dup(mrb, self);
  mrb_str_upcase_bang(mrb, str);
  return str;
}

/*
 *  call-seq:
 *     str.dump   -> new_str
 *
 *  Produces a version of <i>str</i> with all nonprinting characters replaced by
 *  <code>\nnn</code> notation and all special characters escaped.
 */
mrb_value
mrb_str_dump(mrb_state *mrb, mrb_value str)
{
  mrb_int len;
  const char *p, *pend;
  char *q;
  struct RString *result;

  len = 2;                  /* "" */
  p = RSTRING_PTR(str); pend = p + RSTRING_LEN(str);
  while (p < pend) {
    unsigned char c = *p++;
    switch (c) {
      case '"':  case '\\':
      case '\n': case '\r':
      case '\t': case '\f':
      case '\013': case '\010': case '\007': case '\033':
        len += 2;
        break;

      case '#':
        len += IS_EVSTR(p, pend) ? 2 : 1;
        break;

      default:
        if (ISPRINT(c)) {
          len++;
        }
        else {
          len += 4;                /* \NNN */
        }
        break;
    }
  }

  result = str_new(mrb, 0, len);
  str_with_class(mrb, result, str);
  p = RSTRING_PTR(str); pend = p + RSTRING_LEN(str);
  q = result->ptr;

  *q++ = '"';
  while (p < pend) {
    unsigned char c = *p++;

    switch (c) {
      case '"':
      case '\\':
        *q++ = '\\';
        *q++ = c;
        break;

      case '\n':
        *q++ = '\\';
        *q++ = 'n';
        break;

      case '\r':
        *q++ = '\\';
        *q++ = 'r';
        break;

      case '\t':
        *q++ = '\\';
        *q++ = 't';
        break;

      case '\f':
        *q++ = '\\';
        *q++ = 'f';
        break;

      case '\013':
        *q++ = '\\';
        *q++ = 'v';
        break;

      case '\010':
        *q++ = '\\';
        *q++ = 'b';
        break;

      case '\007':
        *q++ = '\\';
        *q++ = 'a';
        break;

      case '\033':
        *q++ = '\\';
        *q++ = 'e';
        break;

      case '#':
        if (IS_EVSTR(p, pend)) *q++ = '\\';
        *q++ = '#';
        break;

      default:
        if (ISPRINT(c)) {
          *q++ = c;
        }
        else {
          *q++ = '\\';
          q[2] = '0' + c % 8; c /= 8;
          q[1] = '0' + c % 8; c /= 8;
          q[0] = '0' + c % 8;
          q += 3;
        }
    }
  }
  *q++ = '"';
  return mrb_obj_value(result);
}

mrb_value
mrb_str_cat(mrb_state *mrb, mrb_value str, const char *ptr, size_t len)
{
  if ((mrb_int)len < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "negative string size (or size too big)");
  }
  str_buf_cat(mrb, mrb_str_ptr(str), ptr, len);
  return str;
}

mrb_value
mrb_str_cat_cstr(mrb_state *mrb, mrb_value str, const char *ptr)
{
  return mrb_str_cat(mrb, str, ptr, strlen(ptr));
}

mrb_value
mrb_str_append(mrb_state *mrb, mrb_value str, mrb_value str2)
{
  str2 = mrb_str_to_str(mrb, str2);
  return mrb_str_buf_append(mrb, str, str2);
}

#define CHAR_ESC_LEN 13 /* sizeof(\x{ hex of 32bit unsigned int } \0) */

/*
 * call-seq:
 *   str.inspect   -> string
 *
 * Returns a printable version of _str_, surrounded by quote marks,
 * with special characters escaped.
 *
 *    str = "hello"
 *    str[3] = "\b"
 *    str.inspect       #=> "\"hel\\bo\""
 */
mrb_value
mrb_str_inspect(mrb_state *mrb, mrb_value str)
{
    const char *p, *pend;
    char buf[CHAR_ESC_LEN + 1];
    mrb_value result = mrb_str_new(mrb, "\"", 1);

    p = RSTRING_PTR(str); pend = RSTRING_END(str);
    for (;p < pend; p++) {
      unsigned int c, cc;

      c = *p;
      if (c == '"'|| c == '\\' || (c == '#' && IS_EVSTR(p, pend))) {
          buf[0] = '\\'; buf[1] = c;
          mrb_str_buf_cat(mrb, result, buf, 2);
          continue;
      }
      if (ISPRINT(c)) {
          buf[0] = c;
          mrb_str_buf_cat(mrb, result, buf, 1);
          continue;
      }
      switch (c) {
        case '\n': cc = 'n'; break;
        case '\r': cc = 'r'; break;
        case '\t': cc = 't'; break;
        case '\f': cc = 'f'; break;
        case '\013': cc = 'v'; break;
        case '\010': cc = 'b'; break;
        case '\007': cc = 'a'; break;
        case 033: cc = 'e'; break;
        default: cc = 0; break;
      }
      if (cc) {
          buf[0] = '\\';
          buf[1] = (char)cc;
          mrb_str_buf_cat(mrb, result, buf, 2);
          continue;
      }
      else {
        buf[0] = '\\';
        buf[3] = '0' + c % 8; c /= 8;
        buf[2] = '0' + c % 8; c /= 8;
        buf[1] = '0' + c % 8;
        mrb_str_buf_cat(mrb, result, buf, 4);
        continue;
      }
    }
    mrb_str_buf_cat(mrb, result, "\"", 1);

    return result;
}

/*
 * call-seq:
 *   str.bytes   -> array of fixnums
 *
 * Returns an array of bytes in _str_.
 *
 *    str = "hello"
 *    str.bytes       #=> [104, 101, 108, 108, 111]
 */
static mrb_value
mrb_str_bytes(mrb_state *mrb, mrb_value str)
{
  struct RString *s = mrb_str_ptr(str);
  mrb_value a = mrb_ary_new_capa(mrb, s->len);
  unsigned char *p = (unsigned char *)(s->ptr), *pend = p + s->len;

  while (p < pend) {
    mrb_ary_push(mrb, a, mrb_fixnum_value(p[0]));
    p++;
  }
  return a;
}

/* ---------------------------*/
void
mrb_init_string(mrb_state *mrb)
{
  struct RClass *s;

  s = mrb->string_class = mrb_define_class(mrb, "String", mrb->object_class);
  MRB_SET_INSTANCE_TT(s, MRB_TT_STRING);
  mrb_include_module(mrb, s, mrb_class_get(mrb, "Comparable"));


  mrb_define_method(mrb, s, "bytesize",        mrb_str_bytesize,        MRB_ARGS_NONE());

  mrb_define_method(mrb, s, "<=>",             mrb_str_cmp_m,           MRB_ARGS_REQ(1)); /* 15.2.10.5.1  */
  mrb_define_method(mrb, s, "==",              mrb_str_equal_m,         MRB_ARGS_REQ(1)); /* 15.2.10.5.2  */
  mrb_define_method(mrb, s, "+",               mrb_str_plus_m,          MRB_ARGS_REQ(1)); /* 15.2.10.5.4  */
  mrb_define_method(mrb, s, "*",               mrb_str_times,           MRB_ARGS_REQ(1)); /* 15.2.10.5.5  */
  mrb_define_method(mrb, s, "[]",              mrb_str_aref_m,          MRB_ARGS_ANY());  /* 15.2.10.5.6  */
  mrb_define_method(mrb, s, "capitalize",      mrb_str_capitalize,      MRB_ARGS_NONE()); /* 15.2.10.5.7  */
  mrb_define_method(mrb, s, "capitalize!",     mrb_str_capitalize_bang, MRB_ARGS_REQ(1)); /* 15.2.10.5.8  */
  mrb_define_method(mrb, s, "chomp",           mrb_str_chomp,           MRB_ARGS_ANY());  /* 15.2.10.5.9  */
  mrb_define_method(mrb, s, "chomp!",          mrb_str_chomp_bang,      MRB_ARGS_ANY());  /* 15.2.10.5.10 */
  mrb_define_method(mrb, s, "chop",            mrb_str_chop,            MRB_ARGS_REQ(1)); /* 15.2.10.5.11 */
  mrb_define_method(mrb, s, "chop!",           mrb_str_chop_bang,       MRB_ARGS_REQ(1)); /* 15.2.10.5.12 */
  mrb_define_method(mrb, s, "downcase",        mrb_str_downcase,        MRB_ARGS_NONE()); /* 15.2.10.5.13 */
  mrb_define_method(mrb, s, "downcase!",       mrb_str_downcase_bang,   MRB_ARGS_NONE()); /* 15.2.10.5.14 */
  mrb_define_method(mrb, s, "empty?",          mrb_str_empty_p,         MRB_ARGS_NONE()); /* 15.2.10.5.16 */
  mrb_define_method(mrb, s, "eql?",            mrb_str_eql,             MRB_ARGS_REQ(1)); /* 15.2.10.5.17 */

  mrb_define_method(mrb, s, "hash",            mrb_str_hash_m,          MRB_ARGS_REQ(1)); /* 15.2.10.5.20 */
  mrb_define_method(mrb, s, "include?",        mrb_str_include,         MRB_ARGS_REQ(1)); /* 15.2.10.5.21 */
  mrb_define_method(mrb, s, "index",           mrb_str_index_m,         MRB_ARGS_ANY());  /* 15.2.10.5.22 */
  mrb_define_method(mrb, s, "initialize",      mrb_str_init,            MRB_ARGS_REQ(1)); /* 15.2.10.5.23 */
  mrb_define_method(mrb, s, "initialize_copy", mrb_str_replace,         MRB_ARGS_REQ(1)); /* 15.2.10.5.24 */
  mrb_define_method(mrb, s, "intern",          mrb_str_intern,          MRB_ARGS_NONE()); /* 15.2.10.5.25 */
  mrb_define_method(mrb, s, "length",          mrb_str_size,            MRB_ARGS_NONE()); /* 15.2.10.5.26 */
  mrb_define_method(mrb, s, "replace",         mrb_str_replace,         MRB_ARGS_REQ(1)); /* 15.2.10.5.28 */
  mrb_define_method(mrb, s, "reverse",         mrb_str_reverse,         MRB_ARGS_NONE()); /* 15.2.10.5.29 */
  mrb_define_method(mrb, s, "reverse!",        mrb_str_reverse_bang,    MRB_ARGS_NONE()); /* 15.2.10.5.30 */
  mrb_define_method(mrb, s, "rindex",          mrb_str_rindex_m,        MRB_ARGS_ANY());  /* 15.2.10.5.31 */
  mrb_define_method(mrb, s, "size",            mrb_str_size,            MRB_ARGS_NONE()); /* 15.2.10.5.33 */
  mrb_define_method(mrb, s, "slice",           mrb_str_aref_m,          MRB_ARGS_ANY());  /* 15.2.10.5.34 */
  mrb_define_method(mrb, s, "split",           mrb_str_split_m,         MRB_ARGS_ANY());  /* 15.2.10.5.35 */

  mrb_define_method(mrb, s, "to_f",            mrb_str_to_f,            MRB_ARGS_NONE()); /* 15.2.10.5.38 */
  mrb_define_method(mrb, s, "to_i",            mrb_str_to_i,            MRB_ARGS_ANY());  /* 15.2.10.5.39 */
  mrb_define_method(mrb, s, "to_s",            mrb_str_to_s,            MRB_ARGS_NONE()); /* 15.2.10.5.40 */
  mrb_define_method(mrb, s, "to_str",          mrb_str_to_s,            MRB_ARGS_NONE());
  mrb_define_method(mrb, s, "to_sym",          mrb_str_intern,          MRB_ARGS_NONE()); /* 15.2.10.5.41 */
  mrb_define_method(mrb, s, "upcase",          mrb_str_upcase,          MRB_ARGS_REQ(1)); /* 15.2.10.5.42 */
  mrb_define_method(mrb, s, "upcase!",         mrb_str_upcase_bang,     MRB_ARGS_REQ(1)); /* 15.2.10.5.43 */
  mrb_define_method(mrb, s, "inspect",         mrb_str_inspect,         MRB_ARGS_NONE()); /* 15.2.10.5.46(x) */
  mrb_define_method(mrb, s, "bytes",           mrb_str_bytes,           MRB_ARGS_NONE());
}
