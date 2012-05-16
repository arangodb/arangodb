/*
** string.c - String class
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"

#include <stdarg.h>
#include <string.h>
#include "mruby/string.h"
#include "mruby/numeric.h"
#include "mruby/range.h"
#include <ctype.h>
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/variable.h"
#include "mruby/hash.h"
#include <stdio.h>
#include "re.h"
#ifdef INCLUDE_REGEXP
#include "regex.h"
#include "st.h"
#endif //INCLUDE_REGEXP

#define mrb_usascii_str_new2 mrb_usascii_str_new_cstr

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

const char ruby_digitmap[] = "0123456789abcdefghijklmnopqrstuvwxyz";

#ifdef INCLUDE_REGEXP
static mrb_value get_pat(mrb_state *mrb, mrb_value pat, mrb_int quote);
#endif //INCLUDE_REGEXP
#ifdef INCLUDE_ENCODING
static void mrb_enc_cr_str_copy_for_substr(mrb_state *mrb, mrb_value dest, mrb_value src);
#else
#define mrb_enc_cr_str_copy_for_substr(mrb, dest, src)
#endif //INCLUDE_ENCODING
static mrb_value str_replace(mrb_state *mrb, mrb_value str, mrb_value str2);
#ifdef INCLUDE_ENCODING
static long str_strlen(mrb_state *mrb, mrb_value str, mrb_encoding *enc);
#endif //INCLUDE_ENCODING
int mrb_block_given_p();
#ifdef INCLUDE_ENCODING
#define is_ascii_string(mrb, str) (mrb_enc_str_coderange(mrb, str) == ENC_CODERANGE_7BIT)
#define is_broken_string(mrb, str) (mrb_enc_str_coderange(mrb, str) == ENC_CODERANGE_BROKEN)
#define STR_ENC_GET(mrb, str) mrb_enc_from_index(mrb, ENCODING_GET(mrb, str))
#endif //INCLUDE_ENCODING

void
mrb_str_set_len(mrb_state *mrb, mrb_value str, long len)
{
  mrb_str_modify(mrb, str);
  RSTRING_LEN(str) = len;
  RSTRING_PTR(str)[len] = '\0';
}

#define RESIZE_CAPA(str,capacity) do {\
      RSTRING(str)->buf = mrb_realloc(mrb, RSTRING(str)->buf, (capacity)+1);\
      if (!MRB_STR_NOCAPA_P(str))\
        RSTRING_CAPA(str) = capacity;\
} while (0)

#define STR_SET_LEN(str, n) do { \
    RSTRING(str)->len = (n);\
} while (0)

#define STR_DEC_LEN(str) do {\
    RSTRING(str)->len--;\
} while (0)

#ifdef INCLUDE_ENCODING
static mrb_value mrb_enc_cr_str_buf_cat(mrb_state *mrb, mrb_value str, const char *ptr, long len,
                                       int ptr_encindex, int ptr_cr, int *ptr_cr_ret);
#endif //INCLUDE_ENCODING

#ifdef INCLUDE_ENCODING
mrb_value
mrb_usascii_str_new_cstr(mrb_state *mrb, const char *ptr)
{
  mrb_value str = mrb_str_new_cstr(mrb, ptr);//mrb_str_new2(ptr);
  ENCODING_CODERANGE_SET(mrb, str, mrb_usascii_encindex(), ENC_CODERANGE_7BIT);
  return str;
}

mrb_value
mrb_external_str_new_with_enc(mrb_state *mrb, const char *ptr, long len, mrb_encoding *eenc)
{
  mrb_value str;

  str = mrb_str_new(mrb, ptr, len);
  if (eenc == mrb_usascii_encoding(mrb) &&
    mrb_enc_str_coderange(mrb, str) != ENC_CODERANGE_7BIT) {
    mrb_enc_associate(mrb, str, mrb_ascii8bit_encoding(mrb));
    return str;
  }
  mrb_enc_associate(mrb, str, eenc);
  return mrb_str_conv_enc(mrb, str, eenc, mrb_default_internal_encoding(mrb));
}

mrb_value
mrb_locale_str_new(mrb_state *mrb, const char *ptr, long len)
{
    return mrb_external_str_new_with_enc(mrb, ptr, len, mrb_locale_encoding(mrb));
}

mrb_value
mrb_str_buf_cat_ascii(mrb_state *mrb, mrb_value str, const char *ptr)
{
  /* ptr must reference NUL terminated ASCII string. */
  int encindex = ENCODING_GET(mrb, str);
  mrb_encoding *enc = mrb_enc_from_index(mrb, encindex);
  if (mrb_enc_asciicompat(mrb, enc)) {
    return mrb_enc_cr_str_buf_cat(mrb, str, ptr, strlen(ptr),
          encindex, ENC_CODERANGE_7BIT, 0);
  }
  else {
    //char *buf = ALLOCA_N(char, mrb_enc_mbmaxlen(enc));
    char *buf = mrb_malloc(mrb, mrb_enc_mbmaxlen(enc));
    while (*ptr) {
      unsigned int c = (unsigned char)*ptr;
      int len = mrb_enc_codelen(mrb, c, enc);
      mrb_enc_mbcput(c, buf, enc);
      mrb_enc_cr_str_buf_cat(mrb, str, buf, len,
            encindex, ENC_CODERANGE_VALID, 0);
      ptr++;
    }
    return str;
  }
}

mrb_value
mrb_filesystem_str_new_cstr(mrb_state *mrb, const char *ptr)
{
    return mrb_external_str_new_with_enc(mrb, ptr, strlen(ptr), mrb_filesystem_encoding(mrb));
}
#endif //INCLUDE_ENCODING

mrb_value
mrb_str_resize(mrb_state *mrb, mrb_value str, size_t len)
{
  size_t slen;

  mrb_str_modify(mrb, str);
  slen = RSTRING_LEN(str);
  if (len != slen) {
    if (slen < len || slen -len > 1024) {
      RSTRING_PTR(str) = mrb_realloc(mrb, RSTRING_PTR(str), len+1);
    }
    if (!MRB_STR_NOCAPA_P(str)) {
      RSTRING(str)->aux.capa = len;
    }
    RSTRING(str)->len = len;
    RSTRING(str)->buf[len] = '\0';    /* sentinel */
  }
  return str;
}

#ifdef INCLUDE_ENCODING
mrb_value
mrb_usascii_str_new(mrb_state *mrb, const char *ptr, long len)
{
    mrb_value str = mrb_str_new(mrb, ptr, len);
    ENCODING_CODERANGE_SET(mrb, str, mrb_usascii_encindex(), ENC_CODERANGE_7BIT);
    return str;
}
#endif //INCLUDE_ENCODING

static inline void
str_mod_check(mrb_state *mrb, mrb_value str, char *p, mrb_int len)
{
  struct RString *s = mrb_str_ptr(str);

  if (s->buf != p || s->len != len) {
    mrb_raise(mrb, mrb->eRuntimeError_class, "string modified");
  }
}

#ifdef INCLUDE_ENCODING
static inline int
single_byte_optimizable(mrb_state *mrb, mrb_value str)
{
    mrb_encoding *enc;
    /* Conservative.  It may be ENC_CODERANGE_UNKNOWN. */
    if (ENC_CODERANGE(str) == ENC_CODERANGE_7BIT)
        return 1;

    enc = STR_ENC_GET(mrb, str);
    if (mrb_enc_mbmaxlen(enc) == 1)
        return 1;

    /* Conservative.  Possibly single byte.
     * "\xa1" in Shift_JIS for example. */
    return 0;
}

static inline const char *
search_nonascii(const char *p, const char *e)
{
  while (p < e) {
    if (!ISASCII(*p))
      return p;
    p++;
  }
  return NULL;
}
#endif //INCLUDE_ENCODING

static inline void
str_modifiable(mrb_value str)
{
  ;
}

static inline int
str_independent(mrb_value str)
{
  str_modifiable(str);
  if (!MRB_STR_SHARED_P(str)) return 1;
  return 0;
}

#ifdef INCLUDE_ENCODING
static inline void
str_enc_copy(mrb_state *mrb, mrb_value str1, mrb_value str2)
{
  unsigned int tmp;
  tmp = ENCODING_GET_INLINED(str2);
  mrb_enc_set_index(mrb, str1, ENCODING_GET(mrb, str2));
}

static inline long
enc_strlen(const char *p, const char *e, mrb_encoding *enc, int cr)
{
  long c;
  const char *q;

  if (mrb_enc_mbmaxlen(enc) == mrb_enc_mbminlen(enc)) {
    return (e - p + mrb_enc_mbminlen(enc) - 1) / mrb_enc_mbminlen(enc);
  }
  else if (mrb_enc_asciicompat(mrb, enc)) {
    c = 0;
    if (cr == ENC_CODERANGE_7BIT || cr == ENC_CODERANGE_VALID) {
      while (p < e) {
        if (ISASCII(*p)) {
          q = search_nonascii(p, e);
          if (!q)
            return c + (e - p);
          c += q - p;
          p = q;
        }
        p += mrb_enc_fast_mbclen(p, e, enc);
        c++;
      }
    }
    else {
      while (p < e) {
        if (ISASCII(*p)) {
          q = search_nonascii(p, e);
          if (!q)
            return c + (e - p);
          c += q - p;
          p = q;
        }
        p += mrb_enc_mbclen(p, e, enc);
        c++;
      }
    }
    return c;
  }

  for (c=0; p<e; c++) {
    p += mrb_enc_mbclen(p, e, enc);
  }
  return c;
}

size_t
mrb_str_capacity(mrb_value str)
{
  if (MRB_STR_NOCAPA_P(str)) {
    return RSTRING_LEN(str);
  }
  else {
    return RSTRING_CAPA(str);
  }
}
#endif //INCLUDE_ENCODING

static inline mrb_value
str_alloc(mrb_state *mrb)
{
  struct RString* s;

  s = mrb_obj_alloc(mrb, MRB_TT_STRING, mrb->string_class);
  //NEWOBJ(str, struct RString);
  //OBJSETUP(str, klass, T_STRING);

  s->buf = 0;
  s->len = 0;
  s->aux.capa = 0;

  return mrb_obj_value(s);
}

#ifdef INCLUDE_ENCODING
long
mrb_enc_strlen(const char *p, const char *e, mrb_encoding *enc)
{
    return enc_strlen(p, e, enc, ENC_CODERANGE_UNKNOWN);
}
#endif //INCLUDE_ENCODING

static void
str_make_independent(mrb_state *mrb, mrb_value str)
{
  char *ptr;
  long len = RSTRING_LEN(str);

  ptr = mrb_malloc(mrb, sizeof(char)*(len+1));
  if (RSTRING_PTR(str)) {
    memcpy(ptr, RSTRING_PTR(str), len);
  }
  ptr[len] = 0;
  RSTRING(str)->buf = ptr;
  RSTRING(str)->len = len;
  RSTRING(str)->aux.capa = len;
  MRB_STR_UNSET_NOCAPA(str);
}

#ifdef INCLUDE_ENCODING
static int
coderange_scan(const char *p, long len, mrb_encoding *enc)
{
    const char *e = p + len;

    if (mrb_enc_to_index(enc) == 0) {
        /* enc is ASCII-8BIT.  ASCII-8BIT string never be broken. */
        p = search_nonascii(p, e);
        return p ? ENC_CODERANGE_VALID : ENC_CODERANGE_7BIT;
    }

    if (mrb_enc_asciicompat(mrb, enc)) {
        p = search_nonascii(p, e);
        if (!p) {
            return ENC_CODERANGE_7BIT;
        }
        while (p < e) {
            int ret = mrb_enc_precise_mbclen(p, e, enc);
            if (!MBCLEN_CHARFOUND_P(ret)) {
                return ENC_CODERANGE_BROKEN;
            }
            p += MBCLEN_CHARFOUND_LEN(ret);
            if (p < e) {
                p = search_nonascii(p, e);
                if (!p) {
                    return ENC_CODERANGE_VALID;
                }
            }
        }
        if (e < p) {
            return ENC_CODERANGE_BROKEN;
        }
        return ENC_CODERANGE_VALID;
    }

    while (p < e) {
        int ret = mrb_enc_precise_mbclen(p, e, enc);

        if (!MBCLEN_CHARFOUND_P(ret)) {
            return ENC_CODERANGE_BROKEN;
        }
        p += MBCLEN_CHARFOUND_LEN(ret);
    }
    if (e < p) {
        return ENC_CODERANGE_BROKEN;
    }
    return ENC_CODERANGE_VALID;
}

int
mrb_enc_str_coderange(mrb_state *mrb, mrb_value str)
{
    int cr = ENC_CODERANGE(str);

    if (cr == ENC_CODERANGE_UNKNOWN) {
      mrb_encoding *enc = STR_ENC_GET(mrb, str);
        cr = coderange_scan(RSTRING_PTR(str), RSTRING_LEN(str), enc);
        ENC_CODERANGE_SET(str, cr);
    }
    return cr;
}

char*
mrb_enc_nth(mrb_state *mrb, const char *p, const char *e, long nth, mrb_encoding *enc)
{
    if (mrb_enc_mbmaxlen(enc) == 1) {
        p += nth;
    }
    else if (mrb_enc_mbmaxlen(enc) == mrb_enc_mbminlen(enc)) {
        p += nth * mrb_enc_mbmaxlen(enc);
    }
    else if (mrb_enc_asciicompat(mrb, enc)) {
        const char *p2, *e2;
        int n;

        while (p < e && 0 < nth) {
            e2 = p + nth;
            if (e < e2)
                return (char *)e;
            if (ISASCII(*p)) {
                p2 = search_nonascii(p, e2);
                if (!p2)
                    return (char *)e2;
                nth -= p2 - p;
                p = p2;
            }
            n = mrb_enc_mbclen(p, e, enc);
            p += n;
            nth--;
        }
        if (nth != 0)
            return (char *)e;
        return (char *)p;
    }
    else {
        while (p<e && nth--) {
            p += mrb_enc_mbclen(p, e, enc);
        }
    }
    if (p > e) p = e;
    return (char*)p;
}

static char*
str_nth(mrb_state *mrb, const char *p, const char *e, long nth, mrb_encoding *enc, int singlebyte)
{
  if (singlebyte)
    p += nth;
  else {
    p = mrb_enc_nth(mrb, p, e, nth, enc);
  }
  if (!p) return 0;
  if (p > e) p = e;
  return (char *)p;
}

/* char offset to byte offset */
static long
str_offset(mrb_state *mrb, const char *p, const char *e, long nth, mrb_encoding *enc, int singlebyte)
{
    const char *pp = str_nth(mrb, p, e, nth, enc, singlebyte);
    if (!pp) return e - p;
    return pp - p;
}

long
mrb_str_offset(mrb_state *mrb, mrb_value str, long pos)
{
    return str_offset(mrb, RSTRING_PTR(str), RSTRING_END(str), pos,
             STR_ENC_GET(mrb, str), single_byte_optimizable(mrb, str));
}

static void
mrb_enc_cr_str_exact_copy(mrb_state *mrb, mrb_value dest, mrb_value src)
{
    str_enc_copy(mrb, dest, src);
    ENC_CODERANGE_SET(dest, ENC_CODERANGE(src));
}
#else
#define mrb_enc_cr_str_exact_copy(mrb, dest, src)
#endif //INCLUDE_ENCODING

mrb_value
str_new4(mrb_state *mrb, mrb_value str)
{
  mrb_value str2;

  str2 = mrb_obj_value(mrb_obj_alloc(mrb, MRB_TT_STRING, mrb->string_class));
  RSTRING(str2)->len = RSTRING_LEN(str);
  RSTRING(str2)->buf = RSTRING_PTR(str);

  if (MRB_STR_SHARED_P(str)) {
    struct RString *shared = RSTRING_SHARED(str);
    FL_SET(str2, MRB_STR_SHARED);
    RSTRING_SHARED(str2) = shared;
  }
  else {
    FL_SET(str, MRB_STR_SHARED);
    RSTRING_SHARED(str) = mrb_str_ptr(str2);
  }
  mrb_enc_cr_str_exact_copy(mrb, str2, str);
  return str2;
}

static mrb_value
str_new(mrb_state *mrb, enum mrb_vtype ttype, const char *p, size_t len)
{
  mrb_value str;

  //str = str_alloc(mrb);
  str = mrb_str_buf_new(mrb, len);
#ifdef INCLUDE_ENCODING
  if (len == 0) {
    ENC_CODERANGE_SET(str, ENC_CODERANGE_7BIT);
  }
#endif //INCLUDE_ENCODING
  if (p) {
    memcpy(RSTRING_PTR(str), p, len);
  }
  STR_SET_LEN(str, len);
  RSTRING_PTR(str)[len] = '\0';
  return str;
}

mrb_value
mrb_str_new_with_class(mrb_state *mrb, mrb_value obj, const char *ptr, long len)
{
    return str_new(mrb, mrb_type(obj), ptr, len);
}

#define mrb_str_new5 mrb_str_new_with_class

static mrb_value
str_new_empty(mrb_state *mrb, mrb_value str)
{
    mrb_value v = mrb_str_new5(mrb, str, 0, 0);
    return v;
}

mrb_value
mrb_str_buf_new(mrb_state *mrb, size_t capa)
{
  struct RString *s;

  s = mrb_obj_alloc(mrb, MRB_TT_STRING, mrb->string_class);

  if (capa < STR_BUF_MIN_SIZE) {
    capa = STR_BUF_MIN_SIZE;
  }
  s->len = 0;
  s->aux.capa = capa;
  s->buf = mrb_malloc(mrb, capa+1);
  s->buf[0] = '\0';

  return mrb_obj_value(s);
}

mrb_value
str_buf_cat(mrb_state *mrb, mrb_value str, const char *ptr, size_t len)
{
  long capa, total, off = -1;

  if (ptr >= RSTRING_PTR(str) && ptr <= RSTRING_END(str)) {
      off = ptr - RSTRING_PTR(str);
  }
  mrb_str_modify(mrb, str);
  if (len == 0) return mrb_fixnum_value(0);
  capa = RSTRING_CAPA(str);
  if (RSTRING_LEN(str) >= LONG_MAX - len) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "string sizes too big");
  }
  total = RSTRING_LEN(str)+len;
  if (capa <= total) {
    while (total > capa) {
        if (capa + 1 >= LONG_MAX / 2) {
          capa = (total + 4095) / 4096;
          break;
        }
        capa = (capa + 1) * 2;
    }
    RESIZE_CAPA(str, capa);
  }
  if (off != -1) {
      ptr = RSTRING_PTR(str) + off;
  }
  memcpy(RSTRING_PTR(str) + RSTRING_LEN(str), ptr, len);
  STR_SET_LEN(str, total);
  RSTRING_PTR(str)[total] = '\0'; /* sentinel */

  return str;
}

mrb_value
mrb_str_buf_cat(mrb_state *mrb, mrb_value str, const char *ptr, size_t len)
{
  if (len == 0) return str;
  return str_buf_cat(mrb, str, ptr, len);
}

/*
 *  call-seq:
 *     String.new(str="")   => new_str
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */

mrb_value
mrb_str_new(mrb_state *mrb, const char *p, size_t len)
{
  struct RString *s;

  if (len == 0) {
    return mrb_str_buf_new(mrb, len);
  }
  s = mrb_obj_alloc(mrb, MRB_TT_STRING, mrb->string_class);
  s->buf = mrb_malloc(mrb, len+1);
  if (p) {
    memcpy(s->buf, p, len);
  }
  s->len = len;
  s->aux.capa = len;
  s->buf[len] ='\0';
  return mrb_obj_value(s);
}

/* ptr==0 is error */
mrb_value
mrb_str_new2(mrb_state *mrb, const char *ptr)
{
  if (!ptr) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "NULL pointer given");
  }
#ifdef INCLUDE_ENCODING
  return mrb_usascii_str_new2(mrb, ptr);
#else
  return mrb_str_new(mrb, ptr, strlen(ptr));
#endif //INCLUDE_ENCODING
}

#ifdef INCLUDE_ENCODING
mrb_value
mrb_enc_str_new(mrb_state *mrb, const char *ptr, long len, mrb_encoding *enc)
{
    mrb_value str = mrb_str_new(mrb, ptr, len);
    mrb_enc_associate(mrb, str, enc);
    return str;
}
#endif //INCLUDE_ENCODING

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
  size_t len = strlen(p);

  s = mrb_obj_alloc(mrb, MRB_TT_STRING, mrb->string_class);
  s->buf = mrb_malloc(mrb, len+1);
  memcpy(s->buf, p, len);
  s->buf[len] = 0;
  s->len = len;
  s->aux.capa = len;

  return mrb_obj_value(s);
}

/*
 *  call-seq: (Caution! string literal)
 *     String.new(str="")   => new_str
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */

mrb_value
mrb_str_literal(mrb_state *mrb, mrb_value lit)
{
  struct RString *s = mrb_str_ptr(lit);

  return mrb_str_new(mrb, s->buf, s->len);
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
  return s->buf;
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
  size_t len;

  if (mrb_type(other) != MRB_TT_STRING) {
    other = mrb_str_to_str(mrb, other);
  }
  s2 = mrb_str_ptr(other);
  len = s1->len + s2->len;

  if (s1->aux.capa < len) {
    s1->aux.capa = len;
    s1->buf = mrb_realloc(mrb, s1->buf, len+1);
  }
  memcpy(s1->buf+s1->len, s2->buf, s2->len);
  s1->len = len;
  s1->buf[len] = 0;
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
  mrb_value r;

  r = mrb_str_new(mrb, 0, s->len + s2->len);
  t = mrb_str_ptr(r);
  memcpy(t->buf, s->buf, s->len);
  memcpy(t->buf + s->len, s2->buf, s2->len);

  return r;
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
  mrb_value str3;
  mrb_value str2;
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;
#endif //INCLUDE_ENCODING

  //mrb_get_args(mrb, "s", &p, &len);
  mrb_get_args(mrb, "o", &str2);

  mrb_string_value(mrb, &str2);
#ifdef INCLUDE_ENCODING
  enc = mrb_enc_check(mrb, self, str2);
#endif //INCLUDE_ENCODING
  str3 = mrb_str_new(mrb, 0, RSTRING_LEN(self)+RSTRING_LEN(str2));
  memcpy(RSTRING_PTR(str3), RSTRING_PTR(self), RSTRING_LEN(self));
  memcpy(RSTRING_PTR(str3) + RSTRING_LEN(self),
             RSTRING_PTR(str2), RSTRING_LEN(str2));
  RSTRING_PTR(str3)[RSTRING_LEN(str3)] = '\0';
#ifdef INCLUDE_ENCODING
  ENCODING_CODERANGE_SET(mrb, str3, mrb_enc_to_index(enc),
     ENC_CODERANGE_AND(ENC_CODERANGE(self), ENC_CODERANGE(str2)));
#endif //INCLUDE_ENCODING

  return str3;
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
#ifdef INCLUDE_ENCODING
    long len;

    len = str_strlen(mrb, self, STR_ENC_GET(mrb, self));
    return mrb_fixnum_value(len);
#else
  return mrb_str_bytesize(mrb, self);
#endif //INCLUDE_ENCODING
}

void
mrb_str_modify(mrb_state *mrb, mrb_value str)
{
  if (!str_independent(str))
    str_make_independent(mrb, str);
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
  mrb_value str2;
  mrb_int n,len,times;
  char *ptr2;

  mrb_get_args(mrb, "i", &times);

  if (times < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "negative argument");
  }
  if (times && INT32_MAX/times < RSTRING_LEN(self)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "argument too big");
  }

  str2 = mrb_str_new5(mrb, self, 0, len = RSTRING_LEN(self)*times);
  ptr2 = RSTRING_PTR(str2);
  if (len > 0) {
    n = RSTRING_LEN(self);
    memcpy(ptr2, RSTRING_PTR(self), n);
    while (n <= len/2) {
      memcpy(ptr2 + n, ptr2, n);
      n *= 2;
    }
    memcpy(ptr2 + n, ptr2, len-n);
  }
  ptr2[RSTRING_LEN(str2)] = '\0';

  mrb_enc_cr_str_copy_for_substr(mrb, str2, self);

  return str2;
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
  retval = memcmp(s1->buf, s2->buf, len);
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
  if (mrb_type(str2) != MRB_TT_STRING) {
    if (!mrb_respond_to(mrb, str2, mrb_intern(mrb, "to_s"))) {
      return mrb_nil_value();
    }
    else if (!mrb_respond_to(mrb, str2, mrb_intern(mrb, "<=>"))) {
      return mrb_nil_value();
    }
    else
    {
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

#ifdef INCLUDE_ENCODING
int
mrb_str_comparable(mrb_state *mrb, mrb_value str1, mrb_value str2)
{
  int idx1, idx2;
  int rc1, rc2;

  if (RSTRING_LEN(str1) == 0) return TRUE;
  if (RSTRING_LEN(str2) == 0) return TRUE;
  idx1 = ENCODING_GET(mrb, str1);
  idx2 = ENCODING_GET(mrb, str2);
  if (idx1 == idx2) return TRUE;
  rc1 = mrb_enc_str_coderange(mrb, str1);
  rc2 = mrb_enc_str_coderange(mrb, str2);
  if (rc1 == ENC_CODERANGE_7BIT) {
    if (rc2 == ENC_CODERANGE_7BIT) return TRUE;
    if (mrb_enc_asciicompat(mrb, mrb_enc_from_index(mrb, idx2)))
        return TRUE;
  }
  if (rc2 == ENC_CODERANGE_7BIT) {
    if (mrb_enc_asciicompat(mrb, mrb_enc_from_index(mrb, idx1)))
       return TRUE;
  }
  return FALSE;
}

int
mrb_str_hash_cmp(mrb_state *mrb, mrb_value str1, mrb_value str2)
{
    long len;

    if (!mrb_str_comparable(mrb, str1, str2)) return 1;
    if (RSTRING_LEN(str1) == (len = RSTRING_LEN(str2)) &&
        memcmp(RSTRING_PTR(str1), RSTRING_PTR(str2), len) == 0) {
        return 0;
    }
    return 1;
}
#endif //INCLUDE_ENCODING

static int
str_eql(mrb_state *mrb, const mrb_value str1, const mrb_value str2)
{
  const long len = RSTRING_LEN(str1);

  if (len != RSTRING_LEN(str2)) return FALSE;
#ifdef INCLUDE_ENCODING
  if (!mrb_str_comparable(mrb, str1, str2)) return FALSE;
#endif //INCLUDE_ENCODING
  if (memcmp(RSTRING_PTR(str1), RSTRING_PTR(str2), len) == 0)
    return TRUE;
  return FALSE;
}

int
mrb_str_equal(mrb_state *mrb, mrb_value str1, mrb_value str2)
{
  if (mrb_obj_equal(mrb, str1, str2)) return TRUE;
  if (mrb_type(str2) != MRB_TT_STRING) {
    if (mrb_nil_p(str2)) return FALSE;
    if (!mrb_respond_to(mrb, str2, mrb_intern(mrb, "to_str"))) {
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

  mrb_get_args(mrb, "o", &str2);
  if (mrb_str_equal(mrb, str1, str2))
    return mrb_true_value();
  return mrb_false_value();
}
/* ---------------------------------- */
mrb_value
mrb_str_to_str(mrb_state *mrb, mrb_value str)
{
  mrb_value s;

  if (mrb_type(str) != MRB_TT_STRING) {
    s = mrb_check_convert_type(mrb, str, MRB_TT_STRING, "String", "to_str");
    if (mrb_nil_p(s)) {
      s = mrb_convert_type(mrb, str, MRB_TT_STRING, "String", "to_s");
    }
    return s;
  }
  return str;
}

mrb_value
mrb_string_value(mrb_state *mrb, mrb_value *ptr)
{
  struct RString *ps;
  mrb_value s = *ptr;
  if (mrb_type(s) != MRB_TT_STRING) {
    s = mrb_str_to_str(mrb, s);
    *ptr = s;
  }
  ps = mrb_str_ptr(s);
  return s;
}

char *
mrb_string_value_ptr(mrb_state *mrb, mrb_value ptr)
{
    mrb_value str = mrb_string_value(mrb, &ptr);
    return RSTRING_PTR(str);
}
/* 15.2.10.5.5  */

/*
 *  call-seq:
 *     str =~ obj   -> fixnum or nil
 *
 *  Match---If <i>obj</i> is a <code>Regexp</code>, use it as a pattern to match
 *  against <i>str</i>,and returns the position the match starts, or
 *  <code>nil</code> if there is no match. Otherwise, invokes
 *  <i>obj.=~</i>, passing <i>str</i> as an argument. The default
 *  <code>=~</code> in <code>Object</code> returns <code>nil</code>.
 *
 *     "cat o' 9 tails" =~ /\d/   #=> 7
 *     "cat o' 9 tails" =~ 9      #=> nil
 */

static mrb_value
mrb_str_match(mrb_state *mrb, mrb_value self/* x */)
{
  mrb_value y;
  mrb_get_args(mrb, "o", &y);
  switch (mrb_type(y)) {
    case MRB_TT_STRING:
      mrb_raise(mrb, E_TYPE_ERROR, "type mismatch: String given");
    case MRB_TT_REGEX:
#ifdef INCLUDE_REGEXP
      return mrb_reg_match_str(mrb, y, self);
#else
      mrb_raise(mrb, E_TYPE_ERROR, "Regexp Class not supported");
#endif //INCLUDE_REGEXP
    default:
      if (mrb_respond_to(mrb, y, mrb_intern(mrb, "=~"))) {
        return mrb_funcall(mrb, y, "=~", 1, self);
      }
      else {
        return mrb_nil_value();
      }
  }
}
/* ---------------------------------- */
mrb_value
mrb_str_substr(mrb_state *mrb, mrb_value str, mrb_int beg, int len)
{
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc = STR_ENC_GET(mrb, str);
#endif //INCLUDE_ENCODING
  mrb_value str2;
#ifdef INCLUDE_ENCODING
  char *p, *s = RSTRING_PTR(str), *e = s + RSTRING_LEN(str);
#else
  char *p, *s = RSTRING_PTR(str);
#endif //INCLUDE_ENCODING

  if (len < 0) return mrb_nil_value();
  if (!RSTRING_LEN(str)) {
    len = 0;
  }
#ifdef INCLUDE_ENCODING
  if (single_byte_optimizable(mrb, str)) {
#endif //INCLUDE_ENCODING
    if (beg > RSTRING_LEN(str)) return mrb_nil_value();
    if (beg < 0) {
      beg += RSTRING_LEN(str);
      if (beg < 0) return mrb_nil_value();
    }
    if (beg + len > RSTRING_LEN(str))
      len = RSTRING_LEN(str) - beg;
    if (len <= 0) {
      len = 0;
      p = 0;
    }
    else
      p = s + beg;
#ifdef INCLUDE_ENCODING
    goto sub;
  }
  if (beg < 0) {
    if (len > -beg) len = -beg;
    if (-beg * mrb_enc_mbmaxlen(enc) < RSTRING_LEN(str) / 8) {
        beg = -beg;
        while (beg-- > len && (e = mrb_enc_prev_char(s, e, e, enc)) != 0);
        p = e;
        if (!p) return mrb_nil_value();
        while (len-- > 0 && (p = mrb_enc_prev_char(s, p, e, enc)) != 0);
        if (!p) return mrb_nil_value();
        len = e - p;
        goto sub;
    }
    else {
        beg += str_strlen(mrb, str, enc);
        if (beg < 0) return mrb_nil_value();
    }
  }
  else if (beg > 0 && beg > str_strlen(mrb, str, enc)) {
    return mrb_nil_value();
  }
  if (len == 0) {
    p = 0;
  }
  else if (mrb_enc_mbmaxlen(enc) == mrb_enc_mbminlen(enc)) {
    int char_sz = mrb_enc_mbmaxlen(enc);

    p = s + beg * char_sz;
    if (p > e) {
      p = e;
      len = 0;
    }
    else if (len * char_sz > e - p)
      len = e - p;
    else
      len *= char_sz;
  }
  else if ((p = str_nth(mrb, s, e, beg, enc, 0)) == e) {
    len = 0;
  }
  else {
    len = str_offset(mrb, p, e, len, enc, 0);
  }
sub:
#endif //INCLUDE_ENCODING
  if (len > STR_BUF_MIN_SIZE && beg + len == RSTRING_LEN(str)) {
#ifdef INCLUDE_ENCODING
    str2 = mrb_str_new4(mrb, str);
    str2 = str_new3(mrb, mrb_obj_class(mrb, str2), str2);
#else
    str2 = mrb_str_new(mrb, s, RSTRING_LEN(str));
#endif //INCLUDE_ENCODING
    RSTRING(str2)->buf += RSTRING(str2)->len - len;
    RSTRING(str2)->len = len;
  }
  else {
    str2 = mrb_str_new5(mrb, str, p, len);
    mrb_enc_cr_str_copy_for_substr(mrb, str2, str);
  }

  return str2;
}

#ifdef INCLUDE_REGEXP
static mrb_value
mrb_str_subpat(mrb_state *mrb, mrb_value str, mrb_value re, mrb_int backref)
{
  if (mrb_reg_search(mrb, re, str, 0, 0) >= 0) {
    mrb_value match = mrb_backref_get(mrb);
    int nth = mrb_reg_backref_number(mrb, match, mrb_fixnum_value(backref));
    return mrb_reg_nth_match(mrb, nth, mrb_backref_get(mrb));
  }
  return mrb_nil_value();
}
#endif //INCLUDE_REGEXP

/* --- 1-8-7parse.c --> */

#ifdef INCLUDE_ENCODING
long
mrb_enc_strlen_cr(mrb_state *mrb, const char *p, const char *e, mrb_encoding *enc, int *cr)
{
  long c;
  const char *q;
  int ret;

  *cr = 0;
  if (mrb_enc_mbmaxlen(enc) == mrb_enc_mbminlen(enc)) {
    return (e - p + mrb_enc_mbminlen(enc) - 1) / mrb_enc_mbminlen(enc);
  }
  else if (mrb_enc_asciicompat(mrb, enc)) {
    c = 0;
    while (p < e) {
      if (ISASCII(*p)) {
      q = search_nonascii(p, e);
      if (!q) {
          if (!*cr) *cr = ENC_CODERANGE_7BIT;
          return c + (e - p);
      }
      c += q - p;
      p = q;
      }
      ret = mrb_enc_precise_mbclen(p, e, enc);
      if (MBCLEN_CHARFOUND_P(ret)) {
      *cr |= ENC_CODERANGE_VALID;
      p += MBCLEN_CHARFOUND_LEN(ret);
      }
      else {
      *cr = ENC_CODERANGE_BROKEN;
      p++;
      }
      c++;
  }
  if (!*cr) *cr = ENC_CODERANGE_7BIT;
    return c;
  }

  for (c=0; p<e; c++) {
    ret = mrb_enc_precise_mbclen(p, e, enc);
    if (MBCLEN_CHARFOUND_P(ret)) {
        *cr |= ENC_CODERANGE_VALID;
        p += MBCLEN_CHARFOUND_LEN(ret);
    }
    else {
        *cr = ENC_CODERANGE_BROKEN;
            if (p + mrb_enc_mbminlen(enc) <= e)
                p += mrb_enc_mbminlen(enc);
            else
                p = e;
    }
  }
  if (!*cr) *cr = ENC_CODERANGE_7BIT;
  return c;
}
#endif //INCLUDE_ENCODING

/* --- 1-8-7parse.c --< */

#ifndef INCLUDE_ENCODING
static inline long
mrb_memsearch_qs(const unsigned char *xs, long m, const unsigned char *ys, long n)
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

int
mrb_memsearch(const void *x0, int m, const void *y0, int n)
{
  const unsigned char *x = x0, *y = y0;

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
  return mrb_memsearch_qs(x0, m, y0, n);
}
#endif //INCLUDE_ENCODING

/* --- 1-8-7parse.c --< */
#ifdef INCLUDE_ENCODING
static long
str_strlen(mrb_state *mrb, mrb_value str, mrb_encoding *enc)
{
    const char *p, *e;
    long n;
    int cr;

    if (single_byte_optimizable(mrb, str)) return RSTRING_LEN(str);
    if (!enc) enc = STR_ENC_GET(mrb, str);
    p = RSTRING_PTR(str);
    e = RSTRING_END(str);
    cr = ENC_CODERANGE(str);
    n = mrb_enc_strlen_cr(mrb, p, e, enc, &cr);
    if (cr) {
        ENC_CODERANGE_SET(str, cr);
    }
    return n;
}
#endif //INCLUDE_ENCODING

static mrb_int
mrb_str_index(mrb_state *mrb, mrb_value str, mrb_value sub, mrb_int offset)
{
  mrb_int pos;
  char *s, *sptr, *e;
  int len, slen;
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;

  enc = mrb_enc_check(mrb, str, sub);
  if (is_broken_string(mrb, sub)) {
    return -1;
  }
  len = str_strlen(mrb, str, enc);
  slen = str_strlen(mrb, sub, enc);
#else
  len = RSTRING_LEN(str);
  slen = RSTRING_LEN(sub);
#endif //INCLUDE_ENCODING
  if (offset < 0) {
    offset += len;
    if (offset < 0) return -1;
  }
  if (len - offset < slen) return -1;
  s = RSTRING_PTR(str);
  e = s + RSTRING_LEN(str);
  if (offset) {
#ifdef INCLUDE_ENCODING
    offset = str_offset(mrb, s, RSTRING_END(str), offset, enc, single_byte_optimizable(mrb, str));
#endif //INCLUDE_ENCODING
    s += offset;
  }
  if (slen == 0) return offset;
  /* need proceed one character at a time */
  sptr = RSTRING_PTR(sub);
  slen = RSTRING_LEN(sub);
  len = RSTRING_LEN(str) - offset;
#ifdef INCLUDE_ENCODING
  for (;;) {
    char *t;
    pos = mrb_memsearch(mrb, sptr, slen, s, len, enc);
    if (pos < 0) return pos;
    t = mrb_enc_right_char_head(s, s+pos, e, enc);
    if (t == s + pos) break;
    if ((len -= t - s) <= 0) return -1;
    offset += t - s;
    s = t;
  }
#else
  pos = mrb_memsearch(sptr, slen, s+offset, len-offset);
  if (pos < 0) return pos;
#endif //INCLUDE_ENCODING
  return pos + offset;
}

mrb_value
mrb_str_dup(mrb_state *mrb, mrb_value str)
{
  struct RString *s = mrb_str_ptr(str);
  struct RString *dup;

  dup = mrb_obj_alloc(mrb, MRB_TT_STRING, mrb->string_class);
  dup->buf = mrb_malloc(mrb, s->len+1);
  if (s->buf) {
    memcpy(dup->buf, s->buf, s->len);
    dup->buf[s->len] = 0;
  }
  dup->len = s->len;
  dup->aux.capa = s->len;
  return mrb_obj_value(dup);
}

static mrb_value
mrb_str_aref(mrb_state *mrb, mrb_value str, mrb_value indx)
{
  long idx;

  switch (mrb_type(indx)) {
    case MRB_TT_FIXNUM:
      idx = mrb_fixnum(indx);

num_index:
      str = mrb_str_substr(mrb, str, idx, 1);
      if (!mrb_nil_p(str) && RSTRING_LEN(str) == 0) return mrb_nil_value();
      return str;

    case MRB_TT_REGEX:
#ifdef INCLUDE_REGEXP
      return mrb_str_subpat(mrb, str, indx, 0); //mrb_str_subpat(str, indx, INT2FIX(0));
#else
      mrb_raise(mrb, E_TYPE_ERROR, "Regexp Class not supported");
      return mrb_nil_value();
#endif //INCLUDE_REGEXP

    case MRB_TT_STRING:
      if (mrb_str_index(mrb, str, indx, 0) != -1)
        return mrb_str_dup(mrb, indx);
      return mrb_nil_value();

    default:
      /* check if indx is Range */
      {
        mrb_int beg, len;
        mrb_value tmp;

#ifdef INCLUDE_ENCODING
        len = str_strlen(mrb, str, STR_ENC_GET(mrb, str));
#else
        len = RSTRING_LEN(str);
#endif //INCLUDE_ENCODING
        switch (mrb_range_beg_len(mrb, indx, &beg, &len, len, 0)) {
          case 0/*FLASE*/:
            break;
          case 2/*OTHER*/:
            return mrb_nil_value();
          default:
            tmp = mrb_str_substr(mrb, str, beg, len);
            return tmp;
        }
      }
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
  int argc;
  mrb_value *argv;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 2) {
    if (mrb_type(argv[0]) == MRB_TT_REGEX) {
#ifdef INCLUDE_REGEXP
      return mrb_str_subpat(mrb, str, argv[0], mrb_fixnum(argv[1]));
#else
      mrb_raise(mrb, E_TYPE_ERROR, "Regexp Class not supported");
      return mrb_nil_value();
#endif //INCLUDE_REGEXP
    }
    return mrb_str_substr(mrb, str, mrb_fixnum(argv[0]), mrb_fixnum(argv[1]));
  }
  if (argc != 1) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%d for 1)", argc);
  }
  return mrb_str_aref(mrb, str, argv[0]);
}

#ifdef INCLUDE_ENCODING
/* As mrb_str_modify(), but don't clear coderange */
static void
str_modify_keep_cr(mrb_state *mrb, mrb_value str)
{
  if (!str_independent(str))
    str_make_independent(mrb, str);
  if (ENC_CODERANGE(str) == ENC_CODERANGE_BROKEN)
    /* Force re-scan later */
    ENC_CODERANGE_CLEAR(str);
}

static void
mrb_str_check_dummy_enc(mrb_state *mrb, mrb_encoding *enc)
{
  if (mrb_enc_dummy_p(enc)) {
    mrb_raise(mrb, E_ENCODING_ERROR, "incompatible encoding with this operation: %s",
      mrb_enc_name(enc));
  }
}
#else
#define str_modify_keep_cr(mrb, str) mrb_str_modify((mrb), (str))
#endif //INCLUDE_ENCODING

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
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;
#endif //INCLUDE_ENCODING
  char *s, *send;
  int modify = 0;
#ifdef INCLUDE_ENCODING
  unsigned int c;
  int n;
#endif //INCLUDE_ENCODING

  str_modify_keep_cr(mrb, str);
#ifdef INCLUDE_ENCODING
  enc = STR_ENC_GET(mrb, str);
  mrb_str_check_dummy_enc(mrb, enc);
#endif //INCLUDE_ENCODING
  if (RSTRING_LEN(str) == 0 || !RSTRING_PTR(str)) return mrb_nil_value();
  s = RSTRING_PTR(str); send = RSTRING_END(str);
#ifdef INCLUDE_ENCODING
  c = mrb_enc_codepoint_len(mrb, s, send, &n, enc);
  if (mrb_enc_islower(c, enc)) {
    mrb_enc_mbcput(mrb_enc_toupper(c, enc), s, enc);
    modify = 1;
  }
  s += n;
  while (s < send) {
    c = mrb_enc_codepoint_len(mrb, s, send, &n, enc);
    if (mrb_enc_isupper(c, enc)) {
      mrb_enc_mbcput(mrb_enc_tolower(c, enc), s, enc);
      modify = 1;
    }
    s += n;
  }
#else
  if (ISLOWER(*s)) {
    *s = toupper(*s);
    modify = 1;
  }
  while (++s < send) {
    if (ISUPPER(*s)) {
      *s = tolower(*s);
      modify = 1;
    }
  }
#endif //INCLUDE_ENCODING
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
  mrb_value *argv;
  int argc;
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;
#endif //INCLUDE_ENCODING
  mrb_value rs;
  mrb_int newline;
  char *p, *pp, *e;
  long len, rslen;

  str_modify_keep_cr(mrb, str);
  len = RSTRING_LEN(str);
  if (len == 0) return mrb_nil_value();
  p = RSTRING_PTR(str);
  e = p + len;
  //if (mrb_scan_args(argc, argv, "01", &rs) == 0) {
  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 0) {
    rs = mrb_str_new2(mrb, "\n");
smart_chomp:
#ifdef INCLUDE_ENCODING
    enc = mrb_enc_get(mrb, str);
    if (mrb_enc_mbminlen(enc) > 1) {
      pp = mrb_enc_left_char_head(p, e-mrb_enc_mbminlen(enc), e, enc);
      if (mrb_enc_is_newline(pp, e, enc)) {
          e = pp;
      }
      pp = e - mrb_enc_mbminlen(enc);
      if (pp >= p) {
        pp = mrb_enc_left_char_head(p, pp, e, enc);
        if (mrb_enc_ascget(mrb, pp, e, 0, enc) == '\r') {
          e = pp;
        }
      }
      if (e == RSTRING_END(str)) {
          return mrb_nil_value();
      }
      len = e - RSTRING_PTR(str);
      STR_SET_LEN(str, len);
    }
    else {
#endif //INCLUDE_ENCODING
      if (RSTRING_PTR(str)[len-1] == '\n') {
        STR_DEC_LEN(str);
        if (RSTRING_LEN(str) > 0 &&
              RSTRING_PTR(str)[RSTRING_LEN(str)-1] == '\r') {
          STR_DEC_LEN(str);
        }
      }
      else if (RSTRING_PTR(str)[len-1] == '\r') {
        STR_DEC_LEN(str);
      }
      else {
        return mrb_nil_value();
      }
#ifdef INCLUDE_ENCODING
    }
#endif //INCLUDE_ENCODING
    RSTRING_PTR(str)[RSTRING_LEN(str)] = '\0';
    return str;
  }
  rs = argv[0];
  if (mrb_nil_p(rs)) return mrb_nil_value();
  //StringValue(rs);
  mrb_string_value(mrb, &rs);
  rslen = RSTRING_LEN(rs);
  if (rslen == 0) {
    while (len>0 && p[len-1] == '\n') {
      len--;
      if (len>0 && p[len-1] == '\r')
        len--;
    }
    if (len < RSTRING_LEN(str)) {
      STR_SET_LEN(str, len);
      RSTRING_PTR(str)[len] = '\0';
      return str;
    }
    return mrb_nil_value();
  }
  if (rslen > len) return mrb_nil_value();
  newline = RSTRING_PTR(rs)[rslen-1];
  if (rslen == 1 && newline == '\n')
    goto smart_chomp;

#ifdef INCLUDE_ENCODING
  enc = mrb_enc_check(mrb, str, rs);
  if (is_broken_string(mrb, rs)) {
    return mrb_nil_value();
  }
  pp = e - rslen;
#else
  pp = p + len - rslen;
#endif //INCLUDE_ENCODING
  if (p[len-1] == newline &&
     (rslen <= 1 ||
     memcmp(RSTRING_PTR(rs), pp, rslen) == 0)) {
#ifdef INCLUDE_ENCODING
    if (mrb_enc_left_char_head(p, pp, e, enc) != pp)
        return mrb_nil_value();
    if (ENC_CODERANGE(str) != ENC_CODERANGE_7BIT) {
        ENC_CODERANGE_CLEAR(str);
    }
#endif //INCLUDE_ENCODING
    STR_SET_LEN(str, RSTRING_LEN(str) - rslen);
    RSTRING_PTR(str)[RSTRING_LEN(str)] = '\0';
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

#ifdef INCLUDE_ENCODING
static long
chopped_length(mrb_state *mrb, mrb_value str)
{
    mrb_encoding *enc = STR_ENC_GET(mrb, str);
    const char *p, *p2, *beg, *end;

    beg = RSTRING_PTR(str);
    end = beg + RSTRING_LEN(str);
    if (beg > end) return 0;
    p = mrb_enc_prev_char(beg, end, end, enc);
    if (!p) return 0;
    if (p > beg && mrb_enc_ascget(mrb, p, end, 0, enc) == '\n') {
    p2 = mrb_enc_prev_char(beg, p, end, enc);
    if (p2 && mrb_enc_ascget(mrb, p2, end, 0, enc) == '\r') p = p2;
    }
    return p - beg;
}
#endif //INCLUDE_ENCODING

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
  str_modify_keep_cr(mrb, str);
  if (RSTRING_LEN(str) > 0) {
#ifdef INCLUDE_ENCODING
    long len;
    len = chopped_length(mrb, str);
#else
    size_t len;
    len = RSTRING_LEN(str) - 1;
    if (RSTRING_PTR(str)[len] == '\n') {
      if (len > 0 &&
          RSTRING_PTR(str)[len-1] == '\r') {
        len--;
      }
    }
#endif //INCLUDE_ENCODING
    STR_SET_LEN(str, len);
    RSTRING_PTR(str)[len] = '\0';
#ifdef INCLUDE_ENCODING
    if (ENC_CODERANGE(str) != ENC_CODERANGE_7BIT) {
        ENC_CODERANGE_CLEAR(str);
    }
#endif //INCLUDE_ENCODING
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
#ifdef INCLUDE_ENCODING
  str = mrb_str_new5(mrb, self, RSTRING_PTR(self), chopped_length(mrb, self));
  mrb_enc_cr_str_copy_for_substr(mrb, str, self);
#else
  str = mrb_str_dup(mrb, self);
  mrb_str_chop_bang(mrb, str);
#endif //INCLUDE_ENCODING
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
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;
#endif //INCLUDE_ENCODING
  char *s, *send;
  int modify = 0;

  str_modify_keep_cr(mrb, str);
#ifdef INCLUDE_ENCODING
  enc = STR_ENC_GET(mrb, str);
  mrb_str_check_dummy_enc(mrb, enc);
#endif //INCLUDE_ENCODING
  s = RSTRING_PTR(str); send = RSTRING_END(str);
#ifdef INCLUDE_ENCODING
  if (single_byte_optimizable(mrb, str)) {
#endif //INCLUDE_ENCODING
    while (s < send) {
      unsigned int c = *(unsigned char*)s;

#ifdef INCLUDE_ENCODING
      if (mrb_enc_isascii(c, enc) && 'A' <= c && c <= 'Z') {
#else
      if ('A' <= c && c <= 'Z') {
#endif //INCLUDE_ENCODING
        *s = 'a' + (c - 'A');
        modify = 1;
      }
      s++;
    }
#ifdef INCLUDE_ENCODING
  }
  else {
    int ascompat = mrb_enc_asciicompat(mrb, enc);

    while (s < send) {
      unsigned int c;
      int n;

      if (ascompat && (c = *(unsigned char*)s) < 0x80) {
        if (mrb_enc_isascii(c, enc) && 'A' <= c && c <= 'Z') {
            *s = 'a' + (c - 'A');
            modify = 1;
        }
        s++;
      }
      else {
        c = mrb_enc_codepoint_len(mrb, s, send, &n, enc);
        if (mrb_enc_isupper(c, enc)) {
            /* assuming toupper returns codepoint with same size */
            mrb_enc_mbcput(mrb_enc_tolower(c, enc), s, enc);
            modify = 1;
        }
        s += n;
      }
    }
  }
#endif //INCLUDE_ENCODING
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

/* 15.2.10.5.15 */
/*
 *  call-seq:
 *     str.each(separator=$/) {|substr| block }        => str
 *     str.each_line(separator=$/) {|substr| block }   => str
 *
 *  Splits <i>str</i> using the supplied parameter as the record separator
 *  (<code>$/</code> by default), passing each substring in turn to the supplied
 *  block. If a zero-length record separator is supplied, the string is split
 *  into paragraphs delimited by multiple successive newlines.
 *
 *     print "Example one\n"
 *     "hello\nworld".each {|s| p s}
 *     print "Example two\n"
 *     "hello\nworld".each('l') {|s| p s}
 *     print "Example three\n"
 *     "hello\n\n\nworld".each('') {|s| p s}
 *
 *  <em>produces:</em>
 *
 *     Example one
 *     "hello\n"
 *     "world"
 *     Example two
 *     "hel"
 *     "l"
 *     "o\nworl"
 *     "d"
 *     Example three
 *     "hello\n\n\n"
 *     "world"
 */
static mrb_value
mrb_str_each_line(mrb_state *mrb, mrb_value str)
{
  mrb_value rs;
  int newline;
  struct RString *ps = mrb_str_ptr(str);
  char *p = ps->buf, *pend = p + ps->len, *s;
  char *ptr = p;
  long len = ps->len, rslen;
  mrb_value line;
  struct RString *prs;
  mrb_value *argv, b;
  int argc;

  //if (mrb_scan_args(argc, argv, "01", &rs) == 0) {
  mrb_get_args(mrb, "&*", &b, &argv, &argc);
  if (argc > 0) {
    rs = argv[0];
  } else {
    rs = mrb_str_new2(mrb, "\n");
  }
  /*RETURN_ENUMERATOR(str, argc, argv);*/
  if (mrb_nil_p(rs)) {
    mrb_yield(mrb, b, str);
    return str;
  }
  //StringValue(rs);
  mrb_string_value(mrb, &rs);
  prs = mrb_str_ptr(rs);
  rslen = prs->len;
  if (rslen == 0) {
    newline = '\n';
  }
  else {
    newline = prs->buf[rslen-1];
  }

  for (s = p, p += rslen; p < pend; p++) {
    if (rslen == 0 && *p == '\n') {
      if (*++p != '\n') continue;
      while (*p == '\n') p++;
    }
    if (ps->buf < p && p[-1] == newline &&
       (rslen <= 1 ||
        memcmp(prs->buf, p-rslen, rslen) == 0)) {
      line = mrb_str_new5(mrb, str, s, p - s);
      mrb_yield(mrb, b, line);
      str_mod_check(mrb, str, ptr, len);
      s = p;
    }
  }

  if (s != pend) {
    if (p > pend) p = pend;
    line = mrb_str_new5(mrb, str, s, p - s);
    mrb_yield(mrb, b, line);
  }

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
mrb_str_empty(mrb_state *mrb, mrb_value self)
{
  struct RString *s = mrb_str_ptr(self);

  if (s->len == 0)
    return mrb_true_value();
  return mrb_false_value();
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

  mrb_get_args(mrb, "o", &str2);
  if (mrb_type(str2) != MRB_TT_STRING)
    return mrb_false_value();
  if (str_eql(mrb, self, str2))
    return mrb_true_value();
  return mrb_false_value();
}

#ifdef INCLUDE_ENCODING
static void
mrb_enc_cr_str_copy_for_substr(mrb_state *mrb, mrb_value dest, mrb_value src)
{
  /* this function is designed for copying encoding and coderange
   * from src to new string "dest" which is made from the part of src.
   */
  str_enc_copy(mrb, dest, src);
  switch (ENC_CODERANGE(src)) {
    case ENC_CODERANGE_7BIT:
      ENC_CODERANGE_SET(dest, ENC_CODERANGE_7BIT);
      break;
    case ENC_CODERANGE_VALID:
      if (!mrb_enc_asciicompat(mrb, STR_ENC_GET(mrb, src)) ||
          search_nonascii(RSTRING_PTR(dest), RSTRING_END(dest)))
        ENC_CODERANGE_SET(dest, ENC_CODERANGE_VALID);
      else
        ENC_CODERANGE_SET(dest, ENC_CODERANGE_7BIT);
      break;
    default:
      if (RSTRING_LEN(dest) == 0) {
        if (!mrb_enc_asciicompat(mrb, STR_ENC_GET(mrb, src)))
          ENC_CODERANGE_SET(dest, ENC_CODERANGE_VALID);
        else
          ENC_CODERANGE_SET(dest, ENC_CODERANGE_7BIT);
      }
      break;
  }
}
#endif //INCLUDE_ENCODING

static mrb_value
str_replace_shared(mrb_state *mrb, mrb_value str2, mrb_value str)
{
  str = mrb_str_new_frozen(mrb, str);
  RSTRING(str2)->len = RSTRING_LEN(str);
  RSTRING(str2)->buf = RSTRING_PTR(str);
  RSTRING_SHARED(str2) = mrb_str_ptr(str);
  FL_SET(str2, MRB_STR_SHARED);
  mrb_enc_cr_str_exact_copy(mrb, str2, str);

  return str2;
}

static mrb_value
str_new_shared(mrb_state *mrb, struct RClass* klass, mrb_value str)
{
    return str_replace_shared(mrb, str_alloc(mrb), str);
}

mrb_value
str_new3(mrb_state *mrb, struct RClass* klass, mrb_value str)
{
    return str_new_shared(mrb, klass, str);
}

mrb_value
mrb_str_new_shared(mrb_state *mrb, mrb_value str)
{
    mrb_value str2 = str_new3(mrb, mrb_obj_class(mrb, str), str);

    return str2;
}

mrb_value
mrb_str_new_frozen(mrb_state *mrb, mrb_value orig)
{
  struct RClass* klass;
  mrb_value str;

  klass = mrb_obj_class(mrb, orig);

  if (MRB_STR_SHARED_P(orig) && RSTRING_SHARED(orig)) {
    long ofs;
    ofs = RSTRING_LEN(str) - RSTRING_SHARED(orig)->len;
#ifdef INCLUDE_ENCODING
    if ((ofs > 0) || (klass != RBASIC(str)->c) ||
        ENCODING_GET(mrb, str) != ENCODING_GET(mrb, orig)) {
#else
    if ((ofs > 0) || (klass != RBASIC(str)->c)) {
#endif //INCLUDE_ENCODING
      str = str_new3(mrb, klass, str);
      RSTRING_PTR(str) += ofs;
      RSTRING_LEN(str) -= ofs;
      mrb_enc_cr_str_exact_copy(mrb, str, orig);
    }
  }
  else {
    str = str_new4(mrb, orig);
  }
  return str;
}

mrb_value
mrb_str_drop_bytes(mrb_state *mrb, mrb_value str, long len)
{
  char *ptr = RSTRING_PTR(str);
  long olen = RSTRING_LEN(str), nlen;

  str_modifiable(str);
  if (len > olen) len = olen;
  nlen = olen - len;
  if (!MRB_STR_SHARED_P(str)) mrb_str_new4(mrb, str);
  ptr = RSTRING(str)->buf += len;
  RSTRING(str)->len = nlen;
  ptr[nlen] = 0;
  //ENC_CODERANGE_CLEAR(str);
  return str;
}

mrb_value
mrb_str_subseq(mrb_state *mrb, mrb_value str, long beg, long len)
{
  mrb_value str2;
  if (RSTRING_LEN(str) == beg + len &&
      STR_BUF_MIN_SIZE < len) {
      str2 = mrb_str_new_shared(mrb, mrb_str_new_frozen(mrb, str));
      mrb_str_drop_bytes(mrb, str2, beg);
  }
  else {
      str2 = mrb_str_new5(mrb, str, RSTRING_PTR(str)+beg, len);
  }
  mrb_enc_cr_str_copy_for_substr(mrb, str2, str);

  return str2;
}

#ifdef INCLUDE_ENCODING
int
mrb_enc_str_asciionly_p(mrb_state *mrb, mrb_value str)
{
    mrb_encoding *enc = STR_ENC_GET(mrb, str);

    if (!mrb_enc_asciicompat(mrb, enc))
        return 0/*FALSE*/;
    else if (mrb_enc_str_coderange(mrb, str) == ENC_CODERANGE_7BIT)
        return 1/*TRUE*/;
    return 0/*FALSE*/;
}

static mrb_value
mrb_enc_cr_str_buf_cat(mrb_state *mrb, mrb_value str, const char *ptr, long len,
    int ptr_encindex, int ptr_cr, int *ptr_cr_ret)
{
  int str_encindex = ENCODING_GET(mrb, str);
  int res_encindex;
  int str_cr, res_cr;
  int str_a8 = ENCODING_IS_ASCII8BIT(str);
  int ptr_a8 = ptr_encindex == 0;

  str_cr = ENC_CODERANGE(str);

  if (str_encindex == ptr_encindex) {
    if (str_cr == ENC_CODERANGE_UNKNOWN ||
        (ptr_a8 && str_cr != ENC_CODERANGE_7BIT)) {
        ptr_cr = ENC_CODERANGE_UNKNOWN;
    }
    else if (ptr_cr == ENC_CODERANGE_UNKNOWN) {
        ptr_cr = coderange_scan(ptr, len, mrb_enc_from_index(mrb, ptr_encindex));
    }
  }
  else {
    mrb_encoding *str_enc = mrb_enc_from_index(mrb, str_encindex);
    mrb_encoding *ptr_enc = mrb_enc_from_index(mrb, ptr_encindex);
    if (!mrb_enc_asciicompat(mrb, str_enc) || !mrb_enc_asciicompat(mrb, ptr_enc)) {
        if (len == 0)
            return str;
        if (RSTRING_LEN(str) == 0) {
            mrb_str_buf_cat(mrb, str, ptr, len);
            ENCODING_CODERANGE_SET(mrb, str, ptr_encindex, ptr_cr);
            return str;
        }
        goto incompatible;
    }
    if (ptr_cr == ENC_CODERANGE_UNKNOWN) {
      ptr_cr = coderange_scan(ptr, len, ptr_enc);
    }
    if (str_cr == ENC_CODERANGE_UNKNOWN) {
      if (str_a8 || ptr_cr != ENC_CODERANGE_7BIT) {
          str_cr = mrb_enc_str_coderange(mrb, str);
      }
    }
  }
  if (ptr_cr_ret)
    *ptr_cr_ret = ptr_cr;

  if (str_encindex != ptr_encindex &&
      str_cr != ENC_CODERANGE_7BIT &&
      ptr_cr != ENC_CODERANGE_7BIT) {
incompatible:
      mrb_raise(mrb, E_ENCODING_ERROR, "incompatible character encodings: %s and %s",
          mrb_enc_name(mrb_enc_from_index(mrb, str_encindex)),
          mrb_enc_name(mrb_enc_from_index(mrb, ptr_encindex)));
  }

  if (str_cr == ENC_CODERANGE_UNKNOWN) {
    res_encindex = str_encindex;
    res_cr = ENC_CODERANGE_UNKNOWN;
  }
  else if (str_cr == ENC_CODERANGE_7BIT) {
    if (ptr_cr == ENC_CODERANGE_7BIT) {
      res_encindex = !str_a8 ? str_encindex : ptr_encindex;
      res_cr = ENC_CODERANGE_7BIT;
    }
    else {
      res_encindex = ptr_encindex;
      res_cr = ptr_cr;
    }
  }
  else if (str_cr == ENC_CODERANGE_VALID) {
    res_encindex = str_encindex;
    if (ptr_cr == ENC_CODERANGE_7BIT || ptr_cr == ENC_CODERANGE_VALID)
      res_cr = str_cr;
    else
      res_cr = ptr_cr;
  }
  else { /* str_cr == ENC_CODERANGE_BROKEN */
    res_encindex = str_encindex;
    res_cr = str_cr;
    if (0 < len) res_cr = ENC_CODERANGE_UNKNOWN;
  }

  if (len < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "negative string size (or size too big)");
  }
  str_buf_cat(mrb, str, ptr, len);
  ENCODING_CODERANGE_SET(mrb, str, res_encindex, res_cr);
  return str;
}

mrb_value
mrb_enc_str_buf_cat(mrb_state *mrb, mrb_value str, const char *ptr, long len, mrb_encoding *ptr_enc)
{
  return mrb_enc_cr_str_buf_cat(mrb, str, ptr, len,
        mrb_enc_to_index(ptr_enc), ENC_CODERANGE_UNKNOWN, NULL);
}

mrb_value
mrb_str_buf_append(mrb_state *mrb, mrb_value str, mrb_value str2)
{
    int str2_cr;

    str2_cr = ENC_CODERANGE(str2);

    mrb_enc_cr_str_buf_cat(mrb, str, RSTRING_PTR(str2), RSTRING_LEN(str2),
        ENCODING_GET(mrb, str2), str2_cr, &str2_cr);

    ENC_CODERANGE_SET(str2, str2_cr);

    return str;
}
#else
mrb_value
mrb_str_buf_append(mrb_state *mrb, mrb_value str, mrb_value str2)
{
  mrb_str_cat(mrb, str, RSTRING_PTR(str2), RSTRING_LEN(str2));
  return str;
}
#endif //INCLUDE_ENCODING

static inline void
str_discard(mrb_state *mrb, mrb_value str)
{
  str_modifiable(str);
  if (!MRB_STR_SHARED_P(str)) {
    mrb_free(mrb, RSTRING_PTR(str));
    RSTRING(str)->buf = 0;
    RSTRING(str)->len = 0;
  }
}

void
mrb_str_shared_replace(mrb_state *mrb, mrb_value str, mrb_value str2)
{
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;
  int cr;
#endif //INCLUDE_ENCODING

  if (mrb_obj_equal(mrb, str, str2)) return;
#ifdef INCLUDE_ENCODING
  enc = STR_ENC_GET(mrb, str2);
  cr = ENC_CODERANGE(str2);
#endif //INCLUDE_ENCODING
  str_discard(mrb, str);
  MRB_STR_UNSET_NOCAPA(str);
  RSTRING_PTR(str) = RSTRING_PTR(str2);
  RSTRING_LEN(str) = RSTRING_LEN(str2);
  if (MRB_STR_NOCAPA_P(str2)) {
    FL_SET(str, RBASIC(str2)->flags & MRB_STR_NOCAPA);
    RSTRING_SHARED(str) = RSTRING_SHARED(str2);
  }
  else {
    RSTRING_CAPA(str) = RSTRING_CAPA(str2);
  }

  MRB_STR_UNSET_NOCAPA(str2); /* abandon str2 */
  RSTRING_PTR(str2)[0] = 0;
  RSTRING_LEN(str2) = 0;
  mrb_enc_associate(mrb, str, enc);
  ENC_CODERANGE_SET(str, cr);
}

#ifdef INCLUDE_REGEXP
static mrb_value
str_gsub(mrb_state *mrb, mrb_value str, mrb_int bang)
{
  mrb_value *argv;
  int argc;
  mrb_value pat, val, repl, match, dest = mrb_nil_value();
  struct re_registers *regs;
  mrb_int beg, n;
  mrb_int beg0, end0;
  mrb_int offset, blen, slen, len, last;
  int iter = 0;
  char *sp, *cp;
  mrb_encoding *str_enc;

  mrb_get_args(mrb, "*", &argv, &argc);
  switch (argc) {
    case 1:
      /*RETURN_ENUMERATOR(str, argc, argv);*/
      iter = 1;
      break;
    case 2:
      repl = argv[1];
      mrb_string_value(mrb, &repl);
      break;
    default:
      mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%d for 2)", argc);
  }

  pat = get_pat(mrb, argv[0], 1);
  beg = mrb_reg_search(mrb, pat, str, 0, 0);
  if (beg < 0) {
    if (bang) return mrb_nil_value();  /* no match, no substitution */
    return mrb_str_dup(mrb, str);
  }

  offset = 0;
  n = 0;
  blen = RSTRING_LEN(str) + 30;
  dest = mrb_str_buf_new(mrb, blen);
  sp = RSTRING_PTR(str);
  slen = RSTRING_LEN(str);
  cp = sp;
  str_enc = STR_ENC_GET(mrb, str);

  do {
    n++;
    match = mrb_backref_get(mrb);
    regs = RMATCH_REGS(match);
    beg0 = BEG(0);
    end0 = END(0);
      val = mrb_reg_regsub(mrb, repl, str, regs, pat);

    len = beg - offset;  /* copy pre-match substr */
    if (len) {
      mrb_enc_str_buf_cat(mrb, dest, cp, len, str_enc);
    }

    mrb_str_buf_append(mrb, dest, val);

    last = offset;
    offset = end0;
    if (beg0 == end0) {
      /*
       * Always consume at least one character of the input string
       * in order to prevent infinite loops.
       */
      if (RSTRING_LEN(str) <= end0) break;
      len = mrb_enc_fast_mbclen(RSTRING_PTR(str)+end0, RSTRING_END(str), str_enc);
      mrb_enc_str_buf_cat(mrb, dest, RSTRING_PTR(str)+end0, len, str_enc);
      offset = end0 + len;
    }
    cp = RSTRING_PTR(str) + offset;
    if (offset > RSTRING_LEN(str)) break;
    beg = mrb_reg_search(mrb, pat, str, offset, 0);
  } while (beg >= 0);
  if (RSTRING_LEN(str) > offset) {
    mrb_enc_str_buf_cat(mrb, dest, cp, RSTRING_LEN(str) - offset, str_enc);
  }
  mrb_reg_search(mrb, pat, str, last, 0);
  if (bang) {
    mrb_str_shared_replace(mrb, str, dest);
  }
  else {
    RBASIC(dest)->c = mrb_obj_class(mrb, str);
    str = dest;
  }

  return str;
}

/* 15.2.10.5.18 */
/*
 *  call-seq:
 *     str.gsub(pattern, replacement)       => new_str
 *     str.gsub(pattern) {|match| block }   => new_str
 *
 *  Returns a copy of <i>str</i> with <em>all</em> occurrences of <i>pattern</i>
 *  replaced with either <i>replacement</i> or the value of the block. The
 *  <i>pattern</i> will typically be a <code>Regexp</code>; if it is a
 *  <code>String</code> then no regular expression metacharacters will be
 *  interpreted (that is <code>/\d/</code> will match a digit, but
 *  <code>'\d'</code> will match a backslash followed by a 'd').
 *
 *  If a string is used as the replacement, special variables from the match
 *  (such as <code>$&</code> and <code>$1</code>) cannot be substituted into it,
 *  as substitution into the string occurs before the pattern match
 *  starts. However, the sequences <code>\1</code>, <code>\2</code>, and so on
 *  may be used to interpolate successive groups in the match.
 *
 *  In the block form, the current match string is passed in as a parameter, and
 *  variables such as <code>$1</code>, <code>$2</code>, <code>$`</code>,
 *  <code>$&</code>, and <code>$'</code> will be set appropriately. The value
 *  returned by the block will be substituted for the match on each call.
 *
 *  When neither a block nor a second argument is supplied, an
 *  <code>Enumerator</code> is returned.
 *
 *     "hello".gsub(/[aeiou]/, '*')                  #=> "h*ll*"
 *     "hello".gsub(/([aeiou])/, '<\1>')             #=> "h<e>ll<o>"
 *     "hello".gsub(/./) {|s| s.ord.to_s + ' '}      #=> "104 101 108 108 111 "
 *     "hello".gsub(/(?<foo>[aeiou])/, '{\k<foo>}')  #=> "h{e}ll{o}"
 *     'hello'.gsub(/[eo]/, 'e' => 3, 'o' => '*')    #=> "h3ll*"
 */
static mrb_value
mrb_str_gsub(mrb_state *mrb, mrb_value self)
{
  //return str_gsub(argc, argv, self, 0);
  return str_gsub(mrb, self, 0);
}

/* 15.2.10.5.19 */
/*
 *  call-seq:
 *     str.gsub!(pattern, replacement)        => str or nil
 *     str.gsub!(pattern) {|match| block }    => str or nil
 *
 *  Performs the substitutions of <code>String#gsub</code> in place, returning
 *  <i>str</i>, or <code>nil</code> if no substitutions were performed.
 */
static mrb_value
mrb_str_gsub_bang(mrb_state *mrb, mrb_value self)
{
  str_modify_keep_cr(mrb, self);
  //return str_gsub(argc, argv, self, 1);
  return str_gsub(mrb, self, 1);
}
#endif //INCLUDE_REGEXP

mrb_int
mrb_str_hash(mrb_state *mrb, mrb_value str)
{
  /* 1-8-7 */
  struct RString *s = mrb_str_ptr(str);
  long len = s->len;
  char *p = s->buf;
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
  mrb_get_args(mrb, "o", &str2);

  if (mrb_type(str2) == MRB_TT_FIXNUM) {
    if (memchr(RSTRING_PTR(self), mrb_fixnum(str2), RSTRING_LEN(self)))
      return mrb_true_value();
    return mrb_false_value();
  }
  //StringValue(arg);
  mrb_string_value(mrb, &str2);
  i = mrb_str_index(mrb, self, str2, 0);

  if (i == -1) return mrb_false_value();
  return mrb_true_value();
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
  //mrb_value initpos;
  mrb_int pos;

  //if (mrb_scan_args(argc, argv, "11", &sub, &initpos) == 2) {
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
  if (pos < 0) {
#ifdef INCLUDE_ENCODING
    pos += str_strlen(mrb, str, STR_ENC_GET(mrb, str));
#else
    pos += RSTRING_LEN(str);
#endif //INCLUDE_ENCODING
    if (pos < 0) {
      if (mrb_type(sub) == MRB_TT_REGEX) {
#ifdef INCLUDE_REGEXP
        mrb_backref_set(mrb, mrb_nil_value());
#else
        mrb_raise(mrb, E_TYPE_ERROR, "Regexp Class not supported");
#endif //INCLUDE_REGEXP
      }
      return mrb_nil_value();
    }
  }

  switch (mrb_type(sub)) {
    case MRB_TT_REGEX:
#ifdef INCLUDE_REGEXP
      if (pos > str_strlen(mrb, str, STR_ENC_GET(mrb, str)))
        return mrb_nil_value();
      pos = str_offset(mrb, RSTRING_PTR(str), RSTRING_END(str), pos,
             mrb_enc_check(mrb, str, sub), single_byte_optimizable(mrb, str));

      pos = mrb_reg_search(mrb, sub, str, pos, 0);
      pos = mrb_str_sublen(mrb, str, pos);
#else
      mrb_raise(mrb, E_TYPE_ERROR, "Regexp Class not supported");
#endif //INCLUDE_REGEXP
      break;

    case MRB_TT_FIXNUM: {
      int c = mrb_fixnum(sub);
      long len = RSTRING_LEN(str);
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
        mrb_raise(mrb, E_TYPE_ERROR, "type mismatch: %s given",
           mrb_obj_classname(mrb, sub));
      }
      sub = tmp;
    }
    /* fall through */
    case MRB_TT_STRING:
      pos = mrb_str_index(mrb, str, sub, pos);
#ifdef INCLUDE_ENCODING
      pos = mrb_str_sublen(mrb, str, pos);
#endif //INCLUDE_ENCODING
      break;
    }

    if (pos == -1) return mrb_nil_value();
    return mrb_fixnum_value(pos);
}

static mrb_value
str_replace(mrb_state *mrb, mrb_value str, mrb_value str2)
{
  long len;

  len = RSTRING_LEN(str2);
  if (MRB_STR_SHARED_P(str2)) {
    struct RString *shared = RSTRING_SHARED(str2);
    RSTRING_LEN(str) = len;
    RSTRING_PTR(str) = shared->buf;
    FL_SET(str, MRB_STR_SHARED);
    RSTRING_SHARED(str) = shared;
  }
  else {
    str_replace_shared(mrb, str, str2);
  }

  mrb_enc_cr_str_exact_copy(mrb, str, str2);
  return str;
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

  mrb_get_args(mrb, "o", &str2);
  str_modifiable(str);
  if (mrb_obj_equal(mrb, str, str2)) return str;

  //StringValue(str2);
  mrb_string_value(mrb, &str2);
  //str_discard(str);
  return str_replace(mrb, str, str2);
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
  //mrb_value orig;
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 1)
    mrb_str_replace(mrb, self);
  return self;
}

#ifdef INCLUDE_ENCODING
mrb_sym
mrb_intern3(mrb_state *mrb, const char *name, long len, mrb_encoding *enc)
{
  return mrb_intern(mrb, name);
}
#endif //INCLUDE_ENCODING

mrb_sym
mrb_intern_str(mrb_state *mrb, mrb_value str)
{
  mrb_sym id;
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;

  if (mrb_enc_str_coderange(mrb, str) == ENC_CODERANGE_7BIT) {
    enc = mrb_usascii_encoding(mrb);
  }
  else {
    enc = mrb_enc_get(mrb, str);
  }
  id = mrb_intern3(mrb, RSTRING_PTR(str), RSTRING_LEN(str), enc);
#else
  id = mrb_intern(mrb, RSTRING_PTR(str));
#endif //INCLUDE_ENCODING
  str = RB_GC_GUARD(str);
  return id;
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
  mrb_value str = RB_GC_GUARD(self);

  id = mrb_intern_str(mrb, str);
  return mrb_symbol_value(id);

}
/* ---------------------------------- */
mrb_value
mrb_obj_as_string(mrb_state *mrb, mrb_value obj)
{
  mrb_value str;

  if (mrb_type(obj) == MRB_TT_STRING) {
    return obj;
  }
  str = mrb_funcall(mrb, obj, "to_s", 0);
  if (mrb_type(str) != MRB_TT_STRING)
    return mrb_any_to_s(mrb, obj);
  return str;
}

mrb_value
mrb_check_string_type(mrb_state *mrb, mrb_value str)
{
  return mrb_check_convert_type(mrb, str, MRB_TT_STRING, "String", "to_str");
}

#ifdef INCLUDE_REGEXP
static mrb_value
get_pat(mrb_state *mrb, mrb_value pat, mrb_int quote)
{
  mrb_value val;

  switch (mrb_type(pat)) {
    case MRB_TT_REGEX:
      return pat;

    case MRB_TT_STRING:
      break;

    default:
      val = mrb_check_string_type(mrb, pat);
      if (mrb_nil_p(val)) {
        //Check_Type(pat, T_REGEXP);
        mrb_check_type(mrb, pat, MRB_TT_REGEX);
      }
      pat = val;
  }

  if (quote) {
    pat = mrb_reg_quote(mrb, pat);
  }

  return mrb_reg_regcomp(mrb, pat);
}
#endif //INCLUDE_REGEXP

/* 15.2.10.5.27 */
/*
 *  call-seq:
 *     str.match(pattern)   => matchdata or nil
 *
 *  Converts <i>pattern</i> to a <code>Regexp</code> (if it isn't already one),
 *  then invokes its <code>match</code> method on <i>str</i>.
 *
 *     'hello'.match('(.)\1')      #=> #<MatchData:0x401b3d30>
 *     'hello'.match('(.)\1')[0]   #=> "ll"
 *     'hello'.match(/(.)\1/)[0]   #=> "ll"
 *     'hello'.match('xx')         #=> nil
 */
#ifdef INCLUDE_REGEXP
static mrb_value
mrb_str_match_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;
  mrb_value re, result, b;
  mrb_get_args(mrb, "&*", &b, &argv, &argc);
  if (argc < 1)
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%d for 1..2)", argc);
  re = argv[0];
  argv[0] = self;
  //  result = mrb_funcall2(get_pat(re, 0), mrb_intern("match"), argc, argv);
  result = mrb_funcall(mrb, get_pat(mrb, re, 0), "match", 1, self);
  if (!mrb_nil_p(result) && mrb_block_given_p()) {
    return mrb_yield(mrb, b, result);
  }
  return result;
}
#endif //INCLUDE_REGEXP

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
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;
#endif //INCLUDE_ENCODING
  mrb_value rev;
  char *s, *e, *p;
#ifdef INCLUDE_ENCODING
  int single = 1;
#endif //INCLUDE_ENCODING

  if (RSTRING_LEN(str) <= 1) return mrb_str_dup(mrb, str);
#ifdef INCLUDE_ENCODING
  enc = STR_ENC_GET(mrb, str);
#endif //INCLUDE_ENCODING
  rev = mrb_str_new5(mrb, str, 0, RSTRING_LEN(str));
  s = RSTRING_PTR(str); e = RSTRING_END(str);
  p = RSTRING_END(rev);

  if (RSTRING_LEN(str) > 1) {
#ifdef INCLUDE_ENCODING
    if (single_byte_optimizable(mrb, str)) {
#endif //INCLUDE_ENCODING
      while (s < e) {
        *--p = *s++;
      }
#ifdef INCLUDE_ENCODING
    }
    else if (ENC_CODERANGE(str) == ENC_CODERANGE_VALID) {
      while (s < e) {
        int clen = mrb_enc_fast_mbclen(s, e, enc);

        if (clen > 1 || (*s & 0x80)) single = 0;
        p -= clen;
        memcpy(p, s, clen);
        s += clen;
      }
    }
    else {
      while (s < e) {
        int clen = mrb_enc_mbclen(s, e, enc);

        if (clen > 1 || (*s & 0x80)) single = 0;
        p -= clen;
        memcpy(p, s, clen);
        s += clen;
      }
    }
  }
  STR_SET_LEN(rev, RSTRING_LEN(str));
  if (ENC_CODERANGE(str) == ENC_CODERANGE_UNKNOWN) {
    if (single) {
      ENC_CODERANGE_SET(str, ENC_CODERANGE_7BIT);
    }
    else {
      ENC_CODERANGE_SET(str, ENC_CODERANGE_VALID);
    }
#endif //INCLUDE_ENCODING
  }
  mrb_enc_cr_str_copy_for_substr(mrb, rev, str);

  return rev;
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
#ifdef INCLUDE_ENCODING
  if (RSTRING_LEN(str) > 1) {
    if (single_byte_optimizable(mrb, str)) {
#endif //INCLUDE_ENCODING
        char *s, *e, c;
        str_modify_keep_cr(mrb, str);
        s = RSTRING_PTR(str);
        e = RSTRING_END(str) - 1;
        while (s < e) {
        c = *s;
        *s++ = *e;
        *e-- = c;
        }
#ifdef INCLUDE_ENCODING
    }
    else {
        mrb_str_shared_replace(mrb, str, mrb_str_reverse(mrb, str));
    }
  }
  else {
    str_modify_keep_cr(mrb, str);
  }
#endif //INCLUDE_ENCODING
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
  long len = psub->len;

  /* substring longer than string */
  if (ps->len < len) return -1;
  if (ps->len - pos < len) {
    pos = ps->len - len;
  }
  sbeg = ps->buf;
  s = ps->buf + pos;
  t = psub->buf;
  if (len) {
    while (sbeg <= s) {
      if (memcmp(s, t, len) == 0) {
        return s - ps->buf;
      }
      s--;
    }
    return -1;
  }
  else {
    return pos;
  }
}

#ifdef INCLUDE_ENCODING
/* byte offset to char offset */
size_t
mrb_str_sublen(mrb_state *mrb, mrb_value str, long pos)
{
  if (single_byte_optimizable(mrb, str) || pos < 0)
    return pos;
  else {
    char *p = RSTRING_PTR(str);
    return enc_strlen(p, p + pos, STR_ENC_GET(mrb, str), ENC_CODERANGE(str));
  }
}
#endif //INCLUDE_ENCODING

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
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc = STR_ENC_GET(mrb, str);
  int pos, len = str_strlen(mrb, str, enc);
#else
  int pos, len = RSTRING_LEN(str);
#endif //INCLUDE_ENCODING

  //if (mrb_scan_args(argc, argv, "11", &sub, &vpos) == 2) {
  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 2) {
    sub = argv[0];
    vpos = argv[1];
    pos = mrb_fixnum(vpos);
    if (pos < 0) {
      pos += len;
      if (pos < 0) {
        if (mrb_type(sub) == MRB_TT_REGEX) {
#ifdef INCLUDE_REGEXP
          mrb_backref_set(mrb, mrb_nil_value());
#else
          mrb_raise(mrb, E_TYPE_ERROR, "Regexp Class not supported");
#endif //INCLUDE_REGEXP
        }
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

  switch (mrb_type(sub)) {
    case MRB_TT_REGEX:
#ifdef INCLUDE_REGEXP
      pos = str_offset(mrb, RSTRING_PTR(str), RSTRING_END(str), pos,
         STR_ENC_GET(mrb, str), single_byte_optimizable(mrb, str));

      if (!RREGEXP(sub)->ptr || RREGEXP_SRC_LEN(sub)) {
        pos = mrb_reg_search(mrb, sub, str, pos, 1);
        pos = mrb_str_sublen(mrb, str, pos);
      }
      if (pos >= 0) return mrb_fixnum_value(pos);
#else
      mrb_raise(mrb, E_TYPE_ERROR, "Regexp Class not supported");
#endif //INCLUDE_REGEXP
      break;

    case MRB_TT_FIXNUM: {
      int c = mrb_fixnum(sub);
      long len = RSTRING_LEN(str);
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
        mrb_raise(mrb, E_TYPE_ERROR, "type mismatch: %s given",
                 mrb_obj_classname(mrb, sub));
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

#ifdef INCLUDE_REGEXP
static mrb_value
scan_once(mrb_state *mrb, mrb_value str, mrb_value pat, mrb_int *start)
{
  mrb_value result, match;
  struct re_registers *regs;
  long i;
  struct RString *ps = mrb_str_ptr(str);
  struct RMatch *pmatch;

  if (mrb_reg_search(mrb, pat, str, *start, 0) >= 0) {
    match = mrb_backref_get(mrb);
    //regs = RMATCH(match)->regs;
    pmatch = mrb_match_ptr(match);
    regs = &pmatch->rmatch->regs;
    if (regs->beg[0] == regs->end[0]) {
      mrb_encoding *enc = STR_ENC_GET(mrb, str);
      /*
       * Always consume at least one character of the input string
       */
      if (ps->len > regs->end[0])
        *start = regs->end[0] + mrb_enc_fast_mbclen(RSTRING_PTR(str)+regs->end[0],RSTRING_END(str), enc);
      else
        *start = regs->end[0] + 1;
    }
    else {
      *start = regs->end[0];
    }
    if (regs->num_regs == 1) {
      return mrb_reg_nth_match(mrb, 0, match);
    }
    result = mrb_ary_new_capa(mrb, regs->num_regs);//mrb_ary_new2(regs->num_regs);
    for (i=1; i < regs->num_regs; i++) {
      mrb_ary_push(mrb, result, mrb_reg_nth_match(mrb, i, match));
    }

    return result;
  }
  return mrb_nil_value();
}
#endif //INCLUDE_REGEXP

/* 15.2.10.5.32 */
/*
 *  call-seq:
 *     str.scan(pattern)                         => array
 *     str.scan(pattern) {|match, ...| block }   => str
 *
 *  Both forms iterate through <i>str</i>, matching the pattern (which may be a
 *  <code>Regexp</code> or a <code>String</code>). For each match, a result is
 *  generated and either added to the result array or passed to the block. If
 *  the pattern contains no groups, each individual result consists of the
 *  matched string, <code>$&</code>.  If the pattern contains groups, each
 *  individual result is itself an array containing one entry per group.
 *
 *     a = "cruel world"
 *     a.scan(/\w+/)        #=> ["cruel", "world"]
 *     a.scan(/.../)        #=> ["cru", "el ", "wor"]
 *     a.scan(/(...)/)      #=> [["cru"], ["el "], ["wor"]]
 *     a.scan(/(..)(..)/)   #=> [["cr", "ue"], ["l ", "wo"]]
 *
 *  And the block form:
 *
 *     a.scan(/\w+/) {|w| print "<<#{w}>> " }
 *     print "\n"
 *     a.scan(/(.)(.)/) {|x,y| print y, x }
 *     print "\n"
 *
 *  <em>produces:</em>
 *
 *     <<cruel>> <<world>>
 *     rceu lowlr
 */
#ifdef INCLUDE_REGEXP
static mrb_value
mrb_str_scan(mrb_state *mrb, mrb_value str)
{
  mrb_value result;
  mrb_value pat, b;
  mrb_int start = 0;
  mrb_value match = mrb_nil_value();
  struct RString *ps = mrb_str_ptr(str);
  char *p = ps->buf;
  long len = ps->len;

  mrb_get_args(mrb, "&o", &b, &pat);
  pat = get_pat(mrb, pat, 1);
  if (!mrb_block_given_p()) {
    mrb_value ary = mrb_ary_new(mrb);

    while (!mrb_nil_p(result = scan_once(mrb, str, pat, &start))) {
      match = mrb_backref_get(mrb);
      mrb_ary_push(mrb, ary, result);
    }
    mrb_backref_set(mrb, match);
    return ary;
  }

  while (!mrb_nil_p(result = scan_once(mrb, str, pat, &start))) {
    match = mrb_backref_get(mrb);
    mrb_yield(mrb, b, result);
    str_mod_check(mrb, str, p, len);
    mrb_backref_set(mrb, match);  /* restore $~ value */
  }
  mrb_backref_set(mrb, match);
  return str;
}
#endif //INCLUDE_REGEXP

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

//static mrb_value
//mrb_str_split_m(int argc, mrb_value *argv, mrb_value str)
static mrb_value
mrb_str_split_m(mrb_state *mrb, mrb_value str)
{
  mrb_value *argv;
  int argc;
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;
#endif //INCLUDE_ENCODING
  mrb_value spat;
  mrb_value limit;
  enum {awk, string, regexp} split_type;
  long beg, end, i = 0;
  int lim = 0;
  mrb_value result, tmp;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc > 0)
    spat  = argv[0];
  if (argc > 1)
    limit = argv[1];
  else
    limit = mrb_nil_value();

  if (argc == 2) {
    lim = mrb_fixnum(limit);
    if (lim <= 0) limit = mrb_nil_value();
    else if (lim == 1) {
      if (RSTRING_LEN(str) == 0)
        return mrb_ary_new_capa(mrb, 0);
      return mrb_ary_new_from_values(mrb, &str, 1);
    }
    i = 1;
  }

#ifdef INCLUDE_ENCODING
  enc = STR_ENC_GET(mrb, str);
#endif //INCLUDE_ENCODING
  //if (mrb_nil_p(spat)) {
  if (argc == 0) {
//    spat = mrb_nil_value();
//    goto fs_set;
    split_type = awk;
  }
  else {
//fs_set:
    if (mrb_type(spat) == MRB_TT_STRING) {
#ifdef INCLUDE_REGEXP
      mrb_encoding *enc2 = STR_ENC_GET(mrb, spat);
#endif //INCLUDE_REGEXP
      split_type = string;
#ifdef INCLUDE_REGEXP
      if (RSTRING_LEN(spat) == 0) {
        /* Special case - split into chars */
        spat = mrb_reg_regcomp(mrb, spat);
        split_type = regexp;
      }
      else if (mrb_enc_asciicompat(mrb, enc2) == 1) {
#endif //INCLUDE_REGEXP
        if (RSTRING_LEN(spat) == 1 && RSTRING_PTR(spat)[0] == ' '){
            split_type = awk;
        }
#ifdef INCLUDE_REGEXP
      }
      else {
        int l;
        if (mrb_enc_ascget(mrb, RSTRING_PTR(spat), RSTRING_END(spat), &l, enc2) == ' ' &&
            RSTRING_LEN(spat) == l) {
            split_type = awk;
        }
      }
#endif //INCLUDE_REGEXP
    }
    else {
#ifdef INCLUDE_REGEXP
      spat = get_pat(mrb, spat, 1);
      split_type = regexp;
#else
      mrb_raise(mrb, E_TYPE_ERROR, "Regexp Class not supported");
#endif //INCLUDE_REGEXP
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
#ifdef INCLUDE_ENCODING
    if (is_ascii_string(mrb, str)) {
#endif //INCLUDE_ENCODING
        while (ptr < eptr) {
        c = (unsigned char)*ptr++;
        if (skip) {
            if (ascii_isspace(c)) {
            beg = ptr - bptr;
            }
            else {
            end = ptr - bptr;
            skip = 0;
            if (!mrb_nil_p(limit) && lim <= i) break;
            }
        }
        else if (ascii_isspace(c)) {
            mrb_ary_push(mrb, result, mrb_str_subseq(mrb, str, beg, end-beg));
            skip = 1;
            beg = ptr - bptr;
            if (!mrb_nil_p(limit)) ++i;
        }
        else {
            end = ptr - bptr;
        }
        }
#ifdef INCLUDE_ENCODING
    }
    else {
      while (ptr < eptr) {
        int n;

        c = mrb_enc_codepoint_len(mrb, ptr, eptr, &n, enc);
        ptr += n;
        if (skip) {
            if (mrb_isspace(c)) {
            beg = ptr - bptr;
            }
            else {
            end = ptr - bptr;
            skip = 0;
            if (!mrb_nil_p(limit) && lim <= i) break;
            }
        }
        else if (mrb_isspace(c)) {
            mrb_ary_push(mrb, result, mrb_str_subseq(mrb, str, beg, end-beg));
            skip = 1;
            beg = ptr - bptr;
            if (!mrb_nil_p(limit)) ++i;
        }
        else {
            end = ptr - bptr;
        }
      }
    }
  }
  else if (split_type == string) {
    char *ptr = RSTRING_PTR(str);
    char *temp = ptr;
    char *eptr = RSTRING_END(str);
    char *sptr = RSTRING_PTR(spat);
    long slen = RSTRING_LEN(spat);

    if (is_broken_string(mrb, str)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid byte sequence in %s", mrb_enc_name(STR_ENC_GET(mrb, str)));
    }
    if (is_broken_string(mrb, spat)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid byte sequence in %s", mrb_enc_name(STR_ENC_GET(mrb, spat)));
    }
    enc = mrb_enc_check(mrb, str, spat);
    while (ptr < eptr &&
           (end = mrb_memsearch(mrb, sptr, slen, ptr, eptr - ptr, enc)) >= 0) {
      /* Check we are at the start of a char */
      char *t = mrb_enc_right_char_head(ptr, ptr + end, eptr, enc);
      if (t != ptr + end) {
        ptr = t;
        continue;
      }
      mrb_ary_push(mrb, result, mrb_str_subseq(mrb, str, ptr - temp, end));
      ptr += end + slen;
      if (!mrb_nil_p(limit) && lim <= ++i) break;
    }
    beg = ptr - temp;
#endif //INCLUDE_ENCODING
  }
  else {
#ifdef INCLUDE_REGEXP
    char *ptr = RSTRING_PTR(str);
    long len = RSTRING_LEN(str);
    long start = beg;
    long idx;
    int last_null = 0;
    struct re_registers *regs;

    while ((end = mrb_reg_search(mrb, spat, str, start, 0)) >= 0) {
      regs = RMATCH_REGS(mrb_backref_get(mrb));
      if (start == end && BEG(0) == END(0)) {
        if (!ptr) {
          mrb_ary_push(mrb, result, str_new_empty(mrb, str));
          break;
        }
        else if (last_null == 1) {
          mrb_ary_push(mrb, result, mrb_str_subseq(mrb, str, beg,
                              mrb_enc_fast_mbclen(ptr+beg,
                                     ptr+len,
                                     enc)));
          beg = start;
        }
        else {
          if (ptr+start == ptr+len)
              start++;
          else
              start += mrb_enc_fast_mbclen(ptr+start,ptr+len,enc);
          last_null = 1;
          continue;
        }
      }
      else {
        mrb_ary_push(mrb, result, mrb_str_subseq(mrb, str, beg, end-beg));
        beg = start = END(0);
      }
      last_null = 0;

      for (idx=1; idx < regs->num_regs; idx++) {
        if (BEG(idx) == -1) continue;
        if (BEG(idx) == END(idx))
            tmp = str_new_empty(mrb, str);
        else
            tmp = mrb_str_subseq(mrb, str, BEG(idx), END(idx)-BEG(idx));
        mrb_ary_push(mrb, result, tmp);
      }
      if (!mrb_nil_p(limit) && lim <= ++i) break;
    }
#else
    mrb_raise(mrb, E_TYPE_ERROR, "Regexp Class not supported");
#endif //INCLUDE_REGEXP
  }
  if (RSTRING_LEN(str) > 0 && (!mrb_nil_p(limit) || RSTRING_LEN(str) > beg || lim < 0)) {
    if (RSTRING_LEN(str) == beg)
        tmp = str_new_empty(mrb, str);
    else
        tmp = mrb_str_subseq(mrb, str, beg, RSTRING_LEN(str)-beg);
    mrb_ary_push(mrb, result, tmp);
  }
  if (mrb_nil_p(limit) && lim == 0) {
    long len;
    while ((len = RARRAY_LEN(result)) > 0 &&
           (tmp = RARRAY_PTR(result)[len-1], RSTRING_LEN(tmp) == 0))
      mrb_ary_pop(mrb, result);
  }

  return result;
}


int
mrb_block_given_p()
{
  /*if (ruby_frame->iter == ITER_CUR && ruby_block)
    return 1;*//*Qtrue*/
  return 0/*Qfalse*/;
}

/* 15.2.10.5.37 */
/*
 *  call-seq:
 *     str.sub!(pattern, replacement)          => str or nil
 *     str.sub!(pattern) {|match| block }      => str or nil
 *
 *  Performs the substitutions of <code>String#sub</code> in place,
 *  returning <i>str</i>, or <code>nil</code> if no substitutions were
 *  performed.
 */
#ifdef INCLUDE_REGEXP
static mrb_value
mrb_str_sub_bang(mrb_state *mrb, mrb_value str)
{
  mrb_value *argv;
  int argc;
  mrb_value pat, repl;
  int iter = 0;
  long plen;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 1 && mrb_block_given_p()) {
    iter = 1;
  }
  else if (argc == 2) {
    repl = argv[1];
    //StringValue(repl);
    mrb_string_value(mrb, &repl);
  }
  else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%d for 2)", argc);
  }

  pat = get_pat(mrb, argv[0], 1);
  str_modifiable(str);
  if (mrb_reg_search(mrb, pat, str, 0, 0) >= 0) {
    mrb_encoding *enc;
    int cr = ENC_CODERANGE(str);
    mrb_value match = mrb_backref_get(mrb);
    struct re_registers *regs = RMATCH_REGS(match);
    long beg0 = BEG(0);
    long end0 = END(0);
    char *p, *rp;
    long len, rlen;

    repl = mrb_reg_regsub(mrb, repl, str, regs, pat);
    enc = mrb_enc_compatible(mrb, str, repl);
    if (!enc) {
      mrb_encoding *str_enc = STR_ENC_GET(mrb, str);
      p = RSTRING_PTR(str); len = RSTRING_LEN(str);
      if (coderange_scan(p, beg0, str_enc) != ENC_CODERANGE_7BIT ||
          coderange_scan(p+end0, len-end0, str_enc) != ENC_CODERANGE_7BIT) {
        mrb_raise(mrb, E_ENCODING_ERROR, "incompatible character encodings: %s and %s",
         mrb_enc_name(str_enc),
         mrb_enc_name(STR_ENC_GET(mrb, repl)));
      }
      enc = STR_ENC_GET(mrb, repl);
    }
    mrb_str_modify(mrb, str);
    mrb_enc_associate(mrb, str, enc);
    if (ENC_CODERANGE_UNKNOWN < cr && cr < ENC_CODERANGE_BROKEN) {
      int cr2 = ENC_CODERANGE(repl);
      if (cr2 == ENC_CODERANGE_BROKEN ||
          (cr == ENC_CODERANGE_VALID && cr2 == ENC_CODERANGE_7BIT))
        cr = ENC_CODERANGE_UNKNOWN;
      else
        cr = cr2;
    }
    plen = end0 - beg0;
    rp = RSTRING_PTR(repl); rlen = RSTRING_LEN(repl);
    len = RSTRING_LEN(str);
    if (rlen > plen) {
      RESIZE_CAPA(str, len + rlen - plen);
    }
    p = RSTRING_PTR(str);
    if (rlen != plen) {
      memmove(p + beg0 + rlen, p + beg0 + plen, len - beg0 - plen);
    }
    memcpy(p + beg0, rp, rlen);
    len += rlen - plen;
    STR_SET_LEN(str, len);
    RSTRING_PTR(str)[len] = '\0';
    ENC_CODERANGE_SET(str, cr);

    return str;
  }
  return mrb_nil_value();
}
#endif //INCLUDE_REGEXP

/* 15.2.10.5.36 */

/*
 *  call-seq:
 *     str.sub(pattern, replacement)         -> new_str
 *     str.sub(pattern, hash)                -> new_str
 *     str.sub(pattern) {|match| block }     -> new_str
 *
 *  Returns a copy of <i>str</i> with the <em>first</em> occurrence of
 *  <i>pattern</i> substituted for the second argument. The <i>pattern</i> is
 *  typically a <code>Regexp</code>; if given as a <code>String</code>, any
 *  regular expression metacharacters it contains will be interpreted
 *  literally, e.g. <code>'\\\d'</code> will match a backlash followed by 'd',
 *  instead of a digit.
 *
 *  If <i>replacement</i> is a <code>String</code> it will be substituted for
 *  the matched text. It may contain back-references to the pattern's capture
 *  groups of the form <code>\\\d</code>, where <i>d</i> is a group number, or
 *  <code>\\\k<n></code>, where <i>n</i> is a group name. If it is a
 *  double-quoted string, both back-references must be preceded by an
 *  additional backslash. However, within <i>replacement</i> the special match
 *  variables, such as <code>&$</code>, will not refer to the current match.
 *
 *  If the second argument is a <code>Hash</code>, and the matched text is one
 *  of its keys, the corresponding value is the replacement string.
 *
 *  In the block form, the current match string is passed in as a parameter,
 *  and variables such as <code>$1</code>, <code>$2</code>, <code>$`</code>,
 *  <code>$&</code>, and <code>$'</code> will be set appropriately. The value
 *  returned by the block will be substituted for the match on each call.
 *
 *     "hello".sub(/[aeiou]/, '*')                  #=> "h*llo"
 *     "hello".sub(/([aeiou])/, '<\1>')             #=> "h<e>llo"
 *     "hello".sub(/./) {|s| s.ord.to_s + ' ' }     #=> "104 ello"
 *     "hello".sub(/(?<foo>[aeiou])/, '*\k<foo>*')  #=> "h*e*llo"
 *     'Is SHELL your preferred shell?'.sub(/[[:upper:]]{2,}/, ENV)
 *      #=> "Is /bin/bash your preferred shell?"
 */

#ifdef INCLUDE_REGEXP
static mrb_value
mrb_str_sub(mrb_state *mrb, mrb_value self)
{
  mrb_value str = mrb_str_dup(mrb, self);

  mrb_str_sub_bang(mrb, str);
  return str;
}
#endif //INCLUDE_REGEXP

mrb_value
mrb_cstr_to_inum(mrb_state *mrb, const char *str, int base, int badcheck)
{
  #define BDIGIT unsigned int
  #define BDIGIT_DBL unsigned long

//  const char *s = str;
  char *end;
  char sign = 1;
//  char nondigit = 0;
  int c;
//  BDIGIT_DBL num;
  long len;
//  long blen = 1;
//  long i;
//  mrb_value z;
//  BDIGIT *zds;
  unsigned long val;

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
      len = 1;
      if (str[0] == '0' && (str[1] == 'b'||str[1] == 'B')) {
        str += 2;
      }
      break;
    case 3:
      len = 2;
      break;
    case 8:
      if (str[0] == '0' && (str[1] == 'o'||str[1] == 'O')) {
        str += 2;
      }
    case 4: case 5: case 6: case 7:
      len = 3;
      break;
    case 10:
      if (str[0] == '0' && (str[1] == 'd'||str[1] == 'D')) {
        str += 2;
      }
    case 9: case 11: case 12: case 13: case 14: case 15:
      len = 4;
      break;
    case 16:
      len = 4;
      if (str[0] == '0' && (str[1] == 'x'||str[1] == 'X')) {
        str += 2;
      }
      break;
    default:
      if (base < 2 || 36 < base) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "illegal radix %d", base);
      }
      if (base <= 32) {
        len = 5;
      }
      else {
        len = 6;
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
  len *= strlen(str)*sizeof(char);

    val = strtoul((char*)str, &end, base);

    if (badcheck) {
      if (end == str) goto bad; /* no number */
      while (*end && ISSPACE(*end)) end++;
      if (*end) goto bad;        /* trailing garbage */
    }

      if (sign) return mrb_fixnum_value(val);
      else {
        long result = -(long)val;
        return mrb_fixnum_value(result);
      }
bad:
      printf("Integer");
  return mrb_fixnum_value(0);
}
char *
mrb_string_value_cstr(mrb_state *mrb, mrb_value *ptr)
{
  struct RString *ps = mrb_str_ptr(*ptr);
  char *s = ps->buf;

  if (!s || ps->len != strlen(s)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "string contains null byte");
  }
  return s;
}

mrb_value
mrb_str_to_inum(mrb_state *mrb, mrb_value str, int base, int badcheck)
{
  char *s;
  size_t len;

  //StringValue(str);
  mrb_string_value(mrb, &str);
  if (badcheck) {
    //s = StringValueCStr(str);
    s = mrb_string_value_cstr(mrb, &str);
  }
  else {
    s = RSTRING_PTR(str);
  }
  if (s) {
    len = RSTRING_LEN(str);
    if (s[len]) {    /* no sentinel somehow */
      //char *p = ALLOCA_N(char, len+1);
      char *p = mrb_malloc(mrb, len+1);

      //MEMCPY(p, s, char, len);
      memcpy(p, s, sizeof(char)*len);
      p[len] = '\0';
      s = p;
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
  //mrb_value b;
  int base;

  //mrb_scan_args(argc, *argv, "01", &b);
  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 0)
    base = 10;
  else
    base = mrb_fixnum(argv[0]);

  if (base < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "illegal radix %d", base);
  }
  return mrb_str_to_inum(mrb, self, base, 0/*Qfalse*/);
}

double
mrb_cstr_to_dbl(mrb_state *mrb, const char * p, int badcheck)
{
  const char *q;
  char *end;
  double d;
//  const char *ellipsis = "";
//  int w;
#define DBL_DIG 16
  enum {max_width = 20};
#define OutOfRange() (((w = end - p) > max_width) ? \
      (w = max_width, ellipsis = "...") : \
      (w = (int)(end - p), ellipsis = ""))

  if (!p) return 0.0;
  q = p;
  while (ISSPACE(*p)) p++;

  if (!badcheck && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    return 0.0;
  }
  d = strtod(p, &end);
  if (p == end) {
    if (badcheck) {
bad:
      //mrb_invalid_str(q, "Float()");
      printf("Float()\n");
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
  size_t len;

  //StringValue(str);
  mrb_string_value(mrb, &str);
  s = RSTRING_PTR(str);
  len = RSTRING_LEN(str);
  if (s) {
    if (badcheck && memchr(s, '\0', len)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "string for Float contains null byte");
    }
    if (s[len]) {    /* no sentinel somehow */
      char *p = mrb_malloc(mrb, len+1);

      memcpy(p, s, sizeof(char)*len);
      p[len] = '\0';
      s = p;
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
  //return mrb_float_new(mrb_str_to_dbl(self, 0/*Qfalse*/));
  return mrb_float_value(mrb_str_to_dbl(mrb, self, 0/*Qfalse*/));
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
#ifdef INCLUDE_ENCODING
  mrb_encoding *enc;
#endif //INCLUDE_ENCODING
  char *s, *send;
  int modify = 0;
#ifdef INCLUDE_ENCODING
  int n;

  str_modify_keep_cr(mrb, str);
  enc = STR_ENC_GET(mrb, str);
  mrb_str_check_dummy_enc(mrb, enc);
  s = RSTRING_PTR(str); send = RSTRING_END(str);
  if (single_byte_optimizable(mrb, str)) {
    while (s < send) {
      unsigned int c = *(unsigned char*)s;

      if (mrb_enc_isascii(c, enc) && 'a' <= c && c <= 'z') {
        *s = 'A' + (c - 'a');
        modify = 1;
      }
      s++;
    }
  }
  else {
    int ascompat = mrb_enc_asciicompat(mrb, enc);

    while (s < send) {
      unsigned int c;

      if (ascompat && (c = *(unsigned char*)s) < 0x80) {
        if (mrb_enc_isascii(c, enc) && 'a' <= c && c <= 'z') {
            *s = 'A' + (c - 'a');
            modify = 1;
        }
        s++;
      }
      else {
        c = mrb_enc_codepoint_len(mrb, s, send, &n, enc);
        if (mrb_enc_islower(c, enc)) {
            /* assuming toupper returns codepoint with same size */
            mrb_enc_mbcput(mrb_enc_toupper(c, enc), s, enc);
            modify = 1;
        }
        s += n;
      }
    }
  }
#else
  mrb_str_modify(mrb, str);
  s = RSTRING_PTR(str); send = RSTRING_END(str);
  while (s < send) {
    unsigned int c = *(unsigned char*)s;

    if ('a' <= c && c <= 'z') {
      *s = 'A' + (c - 'a');
      modify = 1;
    }
    s++;
  }
#endif //INCLUDE_ENCODING
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

/* 15.2.10.5.xx */
/*
 *  call-seq:
 *     str.force_encoding(encoding)   -> str
 *
 *  Changes the encoding to +encoding+ and returns self.
 */
#ifdef INCLUDE_ENCODING
static mrb_value
mrb_str_force_encoding(mrb_state *mrb, mrb_value self)
{
  mrb_value enc;
  mrb_get_args(mrb, "o", &enc);
  str_modifiable(self);
  mrb_enc_associate(mrb, self, mrb_to_encoding(mrb, enc));
  ENC_CODERANGE_CLEAR(self);
  return self;
}

long
mrb_str_coderange_scan_restartable(const char *s, const char *e, mrb_encoding *enc, int *cr)
{
    const char *p = s;

    if (*cr == ENC_CODERANGE_BROKEN)
        return e - s;

    if (mrb_enc_to_index(enc) == 0) {
        /* enc is ASCII-8BIT.  ASCII-8BIT string never be broken. */
        p = search_nonascii(p, e);
        *cr = (!p && *cr != ENC_CODERANGE_VALID) ? ENC_CODERANGE_7BIT : ENC_CODERANGE_VALID;
        return e - s;
    }
    else if (mrb_enc_asciicompat(mrb, enc)) {
        p = search_nonascii(p, e);
        if (!p) {
            if (*cr != ENC_CODERANGE_VALID) *cr = ENC_CODERANGE_7BIT;
            return e - s;
        }
        while (p < e) {
            int ret = mrb_enc_precise_mbclen(p, e, enc);
            if (!MBCLEN_CHARFOUND_P(ret)) {
                *cr = MBCLEN_INVALID_P(ret) ? ENC_CODERANGE_BROKEN: ENC_CODERANGE_UNKNOWN;
                return p - s;
            }
            p += MBCLEN_CHARFOUND_LEN(ret);
            if (p < e) {
                p = search_nonascii(p, e);
                if (!p) {
                    *cr = ENC_CODERANGE_VALID;
                    return e - s;
                }
            }
        }
        *cr = e < p ? ENC_CODERANGE_BROKEN: ENC_CODERANGE_VALID;
        return p - s;
    }
    else {
        while (p < e) {
            int ret = mrb_enc_precise_mbclen(p, e, enc);
            if (!MBCLEN_CHARFOUND_P(ret)) {
                *cr = MBCLEN_INVALID_P(ret) ? ENC_CODERANGE_BROKEN: ENC_CODERANGE_UNKNOWN;
                return p - s;
            }
            p += MBCLEN_CHARFOUND_LEN(ret);
        }
        *cr = e < p ? ENC_CODERANGE_BROKEN: ENC_CODERANGE_VALID;
        return p - s;
    }
}

mrb_value
mrb_str_conv_enc_opts(mrb_state *mrb, mrb_value str, mrb_encoding *from, mrb_encoding *to, int ecflags, mrb_value ecopts)
{
    mrb_econv_t *ec;
    mrb_econv_result_t ret;
    long len;
    mrb_value newstr;
    const unsigned char *sp;
    unsigned char *dp;

    if (!to) return str;
    if (from == to) return str;
    if ((mrb_enc_asciicompat(mrb, to) && ENC_CODERANGE(str) == ENC_CODERANGE_7BIT) ||
        to == mrb_ascii8bit_encoding(mrb)) {
        if (STR_ENC_GET(mrb, str) != to) {
            str = mrb_str_dup(mrb, str);
            mrb_enc_associate(mrb, str, to);
        }
        return str;
    }

    len = RSTRING_LEN(str);
    newstr = mrb_str_new(mrb, 0, len);

  retry:
    ec = mrb_econv_open_opts(mrb, from->name, to->name, ecflags, ecopts);
    if (!ec) return str;

    sp = (unsigned char*)RSTRING_PTR(str);
    dp = (unsigned char*)RSTRING_PTR(newstr);
    ret = mrb_econv_convert(mrb, ec, &sp, (unsigned char*)RSTRING_END(str),
                           &dp, (unsigned char*)RSTRING_END(newstr), 0);
    mrb_econv_close(ec);
    switch (ret) {
      case econv_destination_buffer_full:
        /* destination buffer short */
        len = len < 2 ? 2 : len * 2;
        mrb_str_resize(mrb, newstr, len);
        goto retry;

      case econv_finished:
        len = dp - (unsigned char*)RSTRING_PTR(newstr);
        mrb_str_set_len(mrb, newstr, len);
        mrb_enc_associate(mrb, newstr, to);
        return newstr;

      default:
        /* some error, return original */
        return str;
    }
}

mrb_value
mrb_str_conv_enc(mrb_state *mrb, mrb_value str, mrb_encoding *from, mrb_encoding *to)
{
    return mrb_str_conv_enc_opts(mrb, str, from, to, 0, mrb_nil_value());
}
#endif //INCLUDE_ENCODING

#ifndef INCLUDE_ENCODING
#undef SIGN_EXTEND_CHAR
#if __STDC__
# define SIGN_EXTEND_CHAR(c) ((signed char)(c))
#else  /* not __STDC__ */
/* As in Harbison and Steele.  */
# define SIGN_EXTEND_CHAR(c) ((((unsigned char)(c)) ^ 128) - 128)
#endif
#define is_identchar(c) (SIGN_EXTEND_CHAR(c)!=-1&&(ISALNUM(c) || (c) == '_'))

static int
is_special_global_name(m)
    const char *m;
{
    switch (*m) {
      case '~': case '*': case '$': case '?': case '!': case '@':
      case '/': case '\\': case ';': case ',': case '.': case '=':
      case ':': case '<': case '>': case '\"':
      case '&': case '`': case '\'': case '+':
      case '0':
        ++m;
        break;
      case '-':
        ++m;
        if (is_identchar(*m)) m += 1;
        break;
      default:
        if (!ISDIGIT(*m)) return 0;
        do ++m; while (ISDIGIT(*m));
    }
    return !*m;
}

int
mrb_symname_p(const char *name)
{
    const char *m = name;
    int localid = FALSE;

    if (!m) return FALSE;
    switch (*m) {
      case '\0':
        return FALSE;

      case '$':
        if (is_special_global_name(++m)) return TRUE;
        goto id;

      case '@':
        if (*++m == '@') ++m;
        goto id;

      case '<':
        switch (*++m) {
          case '<': ++m; break;
          case '=': if (*++m == '>') ++m; break;
          default: break;
        }
        break;

      case '>':
        switch (*++m) {
          case '>': case '=': ++m; break;
        }
        break;

      case '=':
        switch (*++m) {
          case '~': ++m; break;
          case '=': if (*++m == '=') ++m; break;
          default: return FALSE;
        }
        break;

      case '*':
        if (*++m == '*') ++m;
        break;

      case '+': case '-':
        if (*++m == '@') ++m;
        break;

      case '|': case '^': case '&': case '/': case '%': case '~': case '`':
        ++m;
        break;

      case '[':
        if (*++m != ']') return FALSE;
        if (*++m == '=') ++m;
        break;

      default:
        localid = !ISUPPER(*m);
id:
        if (*m != '_' && !ISALPHA(*m)) return FALSE;
        while (is_identchar(*m)) m += 1;
        if (localid) {
            switch (*m) {
              case '!': case '?': case '=': ++m;
            }
        }
        break;
    }
    return *m ? FALSE : TRUE;
}
#endif //INCLUDE_ENCODING

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
#ifdef INCLUDE_ENCODING
    mrb_encoding *enc = mrb_enc_get(mrb, str);
#endif //INCLUDE_ENCODING
    long len;
    const char *p, *pend;
    char *q, *qend;
    mrb_value result;
#ifdef INCLUDE_ENCODING
    int u8 = (enc == mrb_utf8_encoding(mrb));
#endif //INCLUDE_ENCODING

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
#ifdef INCLUDE_ENCODING
            if (u8) {      /* \u{NN} */
                int n = mrb_enc_precise_mbclen(p-1, pend, enc);
                if (MBCLEN_CHARFOUND_P(n-1)) {
                  unsigned int cc = mrb_enc_mbc_to_codepoint(p-1, pend, enc);
                  while (cc >>= 4) len++;
                  len += 5;
                  p += MBCLEN_CHARFOUND_LEN(n)-1;
                  break;
                }
            }
#endif //INCLUDE_ENCODING
            len += 4;                /* \xNN */
          }
          break;
      }
    }
#ifdef INCLUDE_ENCODING
    if (!mrb_enc_asciicompat(mrb, enc)) {
      len += 19;            /* ".force_encoding('')" */
      len += strlen(enc->name);
    }
#endif //INCLUDE_ENCODING

    result = mrb_str_new5(mrb, str, 0, len);
    p = RSTRING_PTR(str); pend = p + RSTRING_LEN(str);
    q = RSTRING_PTR(result); qend = q + len + 1;

    *q++ = '"';
    while (p < pend) {
      unsigned char c = *p++;

      if (c == '"' || c == '\\') {
          *q++ = '\\';
          *q++ = c;
      }
      else if (c == '#') {
          if (IS_EVSTR(p, pend)) *q++ = '\\';
          *q++ = '#';
      }
      else if (c == '\n') {
          *q++ = '\\';
          *q++ = 'n';
      }
      else if (c == '\r') {
          *q++ = '\\';
          *q++ = 'r';
      }
      else if (c == '\t') {
          *q++ = '\\';
          *q++ = 't';
      }
      else if (c == '\f') {
          *q++ = '\\';
          *q++ = 'f';
      }
      else if (c == '\013') {
          *q++ = '\\';
          *q++ = 'v';
      }
      else if (c == '\010') {
          *q++ = '\\';
          *q++ = 'b';
      }
      else if (c == '\007') {
          *q++ = '\\';
          *q++ = 'a';
      }
      else if (c == '\033') {
          *q++ = '\\';
          *q++ = 'e';
      }
      else if (ISPRINT(c)) {
          *q++ = c;
      }
      else {
          *q++ = '\\';
#ifdef INCLUDE_ENCODING
          if (u8) {
            int n = mrb_enc_precise_mbclen(p-1, pend, enc) - 1;
            if (MBCLEN_CHARFOUND_P(n)) {
                int cc = mrb_enc_mbc_to_codepoint(p-1, pend, enc);
                p += n;
                snprintf(q, qend-q, "u{%x}", cc);
                q += strlen(q);
                continue;
            }
          }
          snprintf(q, qend-q, "x%02X", c);
#else
          sprintf(q, "%03o", c&0xff);
#endif //INCLUDE_ENCODING
          q += 3;
      }
    }
    *q++ = '"';
#ifdef INCLUDE_ENCODING
    *q = '\0';
    if (!mrb_enc_asciicompat(mrb, enc)) {
      snprintf(q, qend-q, ".force_encoding(\"%s\")", enc->name);
      enc = mrb_ascii8bit_encoding(mrb);
    }
    /* result from dump is ASCII */
    mrb_enc_associate(mrb, result, enc);
    ENC_CODERANGE_SET(result, ENC_CODERANGE_7BIT);
#endif //INCLUDE_ENCODING
    return result;
}

mrb_value
mrb_str_cat(mrb_state *mrb, mrb_value str, const char *ptr, long len)
{
    if (len < 0) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "negative string size (or size too big)");
    }
    if (0/*STR_ASSOC_P(str)*/) {
      mrb_str_modify(mrb, str);
      //if (STR_EMBED_P(str)) str_make_independent(mrb, str);
      mrb_realloc(mrb, RSTRING(str)->buf, RSTRING(str)->len+len+1);
      memcpy(RSTRING(str)->buf + RSTRING(str)->len, ptr, len);
      RSTRING(str)->len += len;
      RSTRING(str)->buf[RSTRING(str)->len] = '\0'; /* sentinel */
      return str;
    }

    return str_buf_cat(mrb, str, ptr, len);
}

mrb_value
mrb_str_cat2(mrb_state *mrb, mrb_value str, const char *ptr)
{
    return mrb_str_cat(mrb, str, ptr, strlen(ptr));
}

mrb_value
mrb_str_vcatf(mrb_state *mrb, mrb_value str, const char *fmt, va_list ap)
{
    //mrb_printf_buffer f;
    //mrb_value klass;

    //StringValue(str);
    mrb_string_value(mrb, &str);
    mrb_str_modify(mrb, str);
    mrb_str_resize(mrb, str, (char *)RSTRING_END(str) - RSTRING_PTR(str));

    return str;
}

mrb_value
mrb_str_catf(mrb_state *mrb, mrb_value str, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    str = mrb_str_vcatf(mrb, str, format, ap);
    va_end(ap);

    return str;
}

void
mrb_lastline_set(mrb_value val)
{
  //vm_svar_set(0, val);
}

mrb_value
mrb_str_append(mrb_state *mrb, mrb_value str, mrb_value str2)
{
  mrb_string_value(mrb, &str2);
  return mrb_str_buf_append(mrb, str, str2);
}

void
mrb_str_setter(mrb_state *mrb, mrb_value val, mrb_sym id, mrb_value *var)
{
  if (!mrb_nil_p(val) && (mrb_type(val) != MRB_TT_STRING)) {
    mrb_raise(mrb, E_TYPE_ERROR, "value of %s must be String", mrb_sym2name(mrb, id));
  }
  *var = val;
}

#ifdef INCLUDE_ENCODING
/*
 *  call-seq:
 *     str.ascii_only?  -> true or false
 *
 *  Returns true for a string which has only ASCII characters.
 *
 *    "abc".force_encoding("UTF-8").ascii_only?          #=> true
 *    "abc\u{6666}".force_encoding("UTF-8").ascii_only?  #=> false
 */

int
mrb_str_is_ascii_only_p(mrb_state *mrb, mrb_value str)
{
    int cr = mrb_enc_str_coderange(mrb, str);

    return cr == ENC_CODERANGE_7BIT ? TRUE : FALSE;
}

#endif //INCLUDE_ENCODING

#define CHAR_ESC_LEN 13 /* sizeof(\x{ hex of 32bit unsigned int } \0) */
int
mrb_str_buf_cat_escaped_char(mrb_state *mrb, mrb_value result, unsigned int c, int unicode_p)
{
  char buf[CHAR_ESC_LEN + 1];
  int l;

  if (sizeof(c) > 4) {
    c &= 0xffffffff;
  }
  if (unicode_p) {
    if (c < 0x7F && ISPRINT(c)) {
        snprintf(buf, CHAR_ESC_LEN, "%c", c);
    }
    else if (c < 0x10000) {
        snprintf(buf, CHAR_ESC_LEN, "\\u%04X", c);
    }
    else {
        snprintf(buf, CHAR_ESC_LEN, "\\u{%X}", c);
    }
  }
  else {
    if (c < 0x100) {
        snprintf(buf, CHAR_ESC_LEN, "\\x%02X", c);
    }
    else {
        snprintf(buf, CHAR_ESC_LEN, "\\x{%X}", c);
    }
  }
  l = (int)strlen(buf);      /* CHAR_ESC_LEN cannot exceed INT_MAX */
  mrb_str_buf_cat(mrb, result, buf, l);
  return l;
}

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
#ifdef INCLUDE_ENCODING
    mrb_encoding *enc = STR_ENC_GET(mrb, str);
#endif //INCLUDE_ENCODING
    const char *p, *pend, *prev;
    char buf[CHAR_ESC_LEN + 1];
#ifdef INCLUDE_ENCODING
    mrb_value result = mrb_str_buf_new(mrb, 0);
    mrb_encoding *resenc = mrb_default_internal_encoding(mrb);
    int unicode_p = mrb_enc_unicode_p(enc);
    int asciicompat = mrb_enc_asciicompat(mrb, enc);

    if (resenc == NULL) resenc = mrb_default_external_encoding(mrb);
    if (!mrb_enc_asciicompat(mrb, resenc)) resenc = mrb_usascii_encoding(mrb);
    mrb_enc_associate(mrb, result, resenc);
    mrb_str_buf_cat(mrb, result, "\"", strlen("\"")); //str_buf_cat2(result, "\"");
#else
    mrb_value result = mrb_str_new_cstr(mrb, "\"");//mrb_str_buf_new2("\"");
#endif //INCLUDE_ENCODING

    p = RSTRING_PTR(str); pend = RSTRING_END(str);
    prev = p;
    while (p < pend) {
      unsigned int c, cc;
      int n;

#ifdef INCLUDE_ENCODING
        n = mrb_enc_precise_mbclen(p, pend, enc);
        if (!MBCLEN_CHARFOUND_P(n)) {
          if (p > prev) mrb_str_buf_cat(mrb, result, prev, p - prev);
            n = mrb_enc_mbminlen(enc);
            if (pend < p + n)
                n = (int)(pend - p);
            while (n--) {
                snprintf(buf, CHAR_ESC_LEN, "\\x%02X", *p & 0377);
                mrb_str_buf_cat(mrb, result, buf, strlen(buf));
                prev = ++p;
            }
          continue;
      }
        n = MBCLEN_CHARFOUND_LEN(n);
      c = mrb_enc_mbc_to_codepoint(p, pend, enc);
      p += n;
      if (c == '"'|| c == '\\' ||
          (c == '#' &&
             p < pend &&
             MBCLEN_CHARFOUND_P(mrb_enc_precise_mbclen(p,pend,enc)) &&
             (cc = mrb_enc_codepoint(mrb, p, pend, enc),
              (cc == '$' || cc == '@' || cc == '{')))) {
          if (p - n > prev) mrb_str_buf_cat(mrb, result, prev, p - n - prev);
          mrb_str_buf_cat(mrb, result, "\\", strlen("\\")); //str_buf_cat2(result, "\\");
          if (asciicompat || enc == resenc) {
            prev = p - n;
            continue;
          }
      }
#else
      c = *p++;
      n = 1;
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
#endif //INCLUDE_ENCODING
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
          if (p - n > prev) mrb_str_buf_cat(mrb, result, prev, p - n - prev);
          buf[0] = '\\';
          buf[1] = (char)cc;
          mrb_str_buf_cat(mrb, result, buf, 2);
          prev = p;
          continue;
      }
#ifdef INCLUDE_ENCODING
      if ((enc == resenc && mrb_enc_isprint(c, enc)) ||
          (asciicompat && mrb_enc_isascii(c, enc) && ISPRINT(c))) {
          continue;
      }
#endif //INCLUDE_ENCODING
      else {
          if (p - n > prev) mrb_str_buf_cat(mrb, result, prev, p - n - prev);
#ifdef INCLUDE_ENCODING
          mrb_str_buf_cat_escaped_char(mrb, result, c, unicode_p);
#else
          sprintf(buf, "\\%03o", c & 0377);
          mrb_str_buf_cat(mrb, result, buf, strlen(buf));
#endif //INCLUDE_ENCODING
          prev = p;
          continue;
      }
    }
    if (p > prev) mrb_str_buf_cat(mrb, result, prev, p - prev);
    mrb_str_buf_cat(mrb, result, "\"", strlen("\"")); //str_buf_cat2(result, "\"");

    return result;
}

#ifdef INCLUDE_ENCODING
int
sym_printable(mrb_state *mrb, const char *s, const char *send, mrb_encoding *enc)
{
    while (s < send) {
      int n;
      int c = mrb_enc_codepoint_len(mrb, s, send, &n, enc);

      if (!mrb_enc_isprint(c, enc)) return FALSE;
      s += n;
    }
    return TRUE;
}
#endif //INCLUDE_ENCODING

/* ---------------------------*/
void
mrb_init_string(mrb_state *mrb)
{
  struct RClass *s;

  s = mrb->string_class = mrb_define_class(mrb, "String", mrb->object_class);
  MRB_SET_INSTANCE_TT(s, MRB_TT_STRING);
  mrb_include_module(mrb, s, mrb_class_get(mrb, "Comparable"));

  mrb_define_method(mrb, s, "+",               mrb_str_plus_m,          ARGS_REQ(1));              /* 15.2.10.5.2  */
  mrb_define_method(mrb, s, "bytesize",        mrb_str_bytesize,        ARGS_NONE());
  mrb_define_method(mrb, s, "size",            mrb_str_size,            ARGS_NONE());              /* 15.2.10.5.33 */
  mrb_define_method(mrb, s, "length",          mrb_str_size,            ARGS_NONE());              /* 15.2.10.5.26 */
  mrb_define_method(mrb, s, "*",               mrb_str_times,           ARGS_REQ(1));              /* 15.2.10.5.1  */
  mrb_define_method(mrb, s, "<=>",             mrb_str_cmp_m,           ARGS_REQ(1));              /* 15.2.10.5.3  */
  mrb_define_method(mrb, s, "==",              mrb_str_equal_m,         ARGS_REQ(1));              /* 15.2.10.5.4  */
  mrb_define_method(mrb, s, "=~",              mrb_str_match,           ARGS_REQ(1));              /* 15.2.10.5.5  */
  mrb_define_method(mrb, s, "[]",              mrb_str_aref_m,          ARGS_ANY());               /* 15.2.10.5.6  */
  mrb_define_method(mrb, s, "capitalize",      mrb_str_capitalize,      ARGS_NONE());              /* 15.2.10.5.7  */
  mrb_define_method(mrb, s, "capitalize!",     mrb_str_capitalize_bang, ARGS_REQ(1));              /* 15.2.10.5.8  */
  mrb_define_method(mrb, s, "chomp",           mrb_str_chomp,           ARGS_ANY());               /* 15.2.10.5.9  */
  mrb_define_method(mrb, s, "chomp!",          mrb_str_chomp_bang,      ARGS_ANY());               /* 15.2.10.5.10 */
  mrb_define_method(mrb, s, "chop",            mrb_str_chop,            ARGS_REQ(1));              /* 15.2.10.5.11 */
  mrb_define_method(mrb, s, "chop!",           mrb_str_chop_bang,       ARGS_REQ(1));              /* 15.2.10.5.12 */
  mrb_define_method(mrb, s, "downcase",        mrb_str_downcase,        ARGS_NONE());              /* 15.2.10.5.13 */
  mrb_define_method(mrb, s, "downcase!",       mrb_str_downcase_bang,   ARGS_NONE());              /* 15.2.10.5.14 */
  mrb_define_method(mrb, s, "each_line",       mrb_str_each_line,       ARGS_REQ(1));              /* 15.2.10.5.15 */
  mrb_define_method(mrb, s, "empty?",          mrb_str_empty,           ARGS_NONE());              /* 15.2.10.5.16 */
  mrb_define_method(mrb, s, "eql?",            mrb_str_eql,             ARGS_REQ(1));              /* 15.2.10.5.17 */
#ifdef INCLUDE_REGEXP
  mrb_define_method(mrb, s, "gsub",            mrb_str_gsub,            ARGS_REQ(1));              /* 15.2.10.5.18 */
  mrb_define_method(mrb, s, "gsub!",           mrb_str_gsub_bang,       ARGS_REQ(1));              /* 15.2.10.5.19 */
#endif
  mrb_define_method(mrb, s, "hash",            mrb_str_hash_m,          ARGS_REQ(1));              /* 15.2.10.5.20 */
  mrb_define_method(mrb, s, "include?",        mrb_str_include,         ARGS_REQ(1));              /* 15.2.10.5.21 */
  mrb_define_method(mrb, s, "index",           mrb_str_index_m,         ARGS_ANY());               /* 15.2.10.5.22 */
  mrb_define_method(mrb, s, "initialize",      mrb_str_init,            ARGS_REQ(1));              /* 15.2.10.5.23 */
  mrb_define_method(mrb, s, "initialize_copy", mrb_str_replace,         ARGS_REQ(1));              /* 15.2.10.5.24 */
  mrb_define_method(mrb, s, "intern",          mrb_str_intern,          ARGS_NONE());              /* 15.2.10.5.25 */
#ifdef INCLUDE_REGEXP
  mrb_define_method(mrb, s, "match",           mrb_str_match_m,         ARGS_REQ(1));              /* 15.2.10.5.27 */
#endif
  mrb_define_method(mrb, s, "replace",         mrb_str_replace,         ARGS_REQ(1));              /* 15.2.10.5.28 */
  mrb_define_method(mrb, s, "reverse",         mrb_str_reverse,         ARGS_NONE());              /* 15.2.10.5.29 */
  mrb_define_method(mrb, s, "reverse!",        mrb_str_reverse_bang,    ARGS_NONE());              /* 15.2.10.5.30 */
  mrb_define_method(mrb, s, "rindex",          mrb_str_rindex_m,        ARGS_ANY());               /* 15.2.10.5.31 */
#ifdef INCLUDE_REGEXP
  mrb_define_method(mrb, s, "scan",            mrb_str_scan,            ARGS_REQ(1));              /* 15.2.10.5.32 */
#endif
  mrb_define_method(mrb, s, "slice",           mrb_str_aref_m,          ARGS_ANY());               /* 15.2.10.5.34 */
  mrb_define_method(mrb, s, "split",           mrb_str_split_m,         ARGS_ANY());               /* 15.2.10.5.35 */
#ifdef INCLUDE_REGEXP
  mrb_define_method(mrb, s, "sub",             mrb_str_sub,             ARGS_REQ(1));              /* 15.2.10.5.36 */
  mrb_define_method(mrb, s, "sub!",            mrb_str_sub_bang,        ARGS_REQ(1));              /* 15.2.10.5.37 */
#endif
  mrb_define_method(mrb, s, "to_i",            mrb_str_to_i,            ARGS_ANY());               /* 15.2.10.5.38 */
  mrb_define_method(mrb, s, "to_f",            mrb_str_to_f,            ARGS_NONE());              /* 15.2.10.5.39 */
  mrb_define_method(mrb, s, "to_s",            mrb_str_to_s,            ARGS_NONE());              /* 15.2.10.5.40 */
  mrb_define_method(mrb, s, "to_str",          mrb_str_to_s,            ARGS_NONE());              /* 15.2.10.5.40 */
  mrb_define_method(mrb, s, "to_sym",          mrb_str_intern,          ARGS_NONE());              /* 15.2.10.5.41 */
  mrb_define_method(mrb, s, "upcase",          mrb_str_upcase,          ARGS_REQ(1));              /* 15.2.10.5.42 */
  mrb_define_method(mrb, s, "upcase!",         mrb_str_upcase_bang,     ARGS_REQ(1));              /* 15.2.10.5.43 */
#ifdef INCLUDE_ENCODING
  mrb_define_method(mrb, s, "encoding",        mrb_obj_encoding,        ARGS_NONE());              /* 15.2.10.5.44(x) */
  mrb_define_method(mrb, s, "force_encoding",  mrb_str_force_encoding,  ARGS_REQ(1));              /* 15.2.10.5.45(x) */
#endif
  mrb_define_method(mrb, s, "inspect",         mrb_str_inspect,         ARGS_NONE());              /* 15.2.10.5.46(x) */
}
