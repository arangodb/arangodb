/*
** string.h - String class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_STRING_H
#define MRUBY_STRING_H

#ifdef INCLUDE_ENCODING
#include "encoding.h"
#endif

#ifndef RB_GC_GUARD
#define RB_GC_GUARD(v) v
#endif

#define IS_EVSTR(p,e) ((p) < (e) && (*(p) == '$' || *(p) == '@' || *(p) == '{'))

#define mrb_str_new4 mrb_str_new_frozen

#define STR_BUF_MIN_SIZE 128
//#define RSTRING_EMBED_LEN_MAX STR_BUF_MIN_SIZE

extern const char ruby_digitmap[];

struct RString {
  MRUBY_OBJECT_HEADER;
  int len;
  union {
    int capa;
    struct RString *shared;
  } aux;
  char *buf;
};

extern struct SCOPE {
    struct RBasic super;
    mrb_sym *local_tbl;
    mrb_value *local_vars;
    int flags;
} *ruby_scope;

struct RVarmap {
    struct RBasic super;
    mrb_sym id;
    mrb_value val;
    struct RVarmap *next;
};
extern struct RVarmap *ruby_dyna_vars;

#define mrb_str_ptr(s)    ((struct RString*)((s).value.p))
#define RSTRING(s)        ((struct RString*)((s).value.p))
#define RSTRING_PTR(s)    (RSTRING(s)->buf)
#define RSTRING_LEN(s)    (RSTRING(s)->len)
#define RSTRING_CAPA(s)   (RSTRING(s)->aux.capa)
#define RSTRING_SHARED(s) (RSTRING(s)->aux.shared)
#define RSTRING_END(s)    (RSTRING(s)->buf + RSTRING(s)->len)

#define MRB_STR_SHARED      256
#define MRB_STR_SHARED_P(s) (FL_ALL(s, MRB_STR_SHARED))
#define MRB_STR_NOCAPA      (MRB_STR_SHARED)
#define MRB_STR_NOCAPA_P(s) (FL_ANY(s, MRB_STR_NOCAPA))
#define MRB_STR_UNSET_NOCAPA(s) do {\
  FL_UNSET(s, MRB_STR_NOCAPA);\
} while (0)

mrb_value mrb_str_literal(mrb_state*, mrb_value);
void mrb_str_concat(mrb_state*, mrb_value, mrb_value);
mrb_value mrb_obj_to_str(mrb_state*, mrb_value);
mrb_value mrb_str_plus(mrb_state*, mrb_value, mrb_value);
mrb_value mrb_obj_as_string(mrb_state *mrb, mrb_value obj);
mrb_value mrb_str_resize(mrb_state *mrb, mrb_value str, size_t len); /* mrb_str_resize */
mrb_value mrb_string_value(mrb_state *mrb, mrb_value *ptr); /* StringValue */
mrb_value mrb_str_substr(mrb_state *mrb, mrb_value str, mrb_int beg, int len);
mrb_value mrb_check_string_type(mrb_state *mrb, mrb_value str);
mrb_value mrb_str_buf_new(mrb_state *mrb, size_t capa);
mrb_value mrb_str_buf_cat(mrb_state *mrb, mrb_value str, const char *ptr, size_t len);
mrb_value str_buf_cat(mrb_state *mrb, mrb_value str, const char *ptr, size_t len);

char *mrb_string_value_cstr(mrb_state *mrb, mrb_value *ptr);
char *mrb_string_value_ptr(mrb_state *mrb, mrb_value ptr);
mrb_value mrb_str_subseq(mrb_state *mrb, mrb_value str, long beg, long len);
size_t mrb_str_sublen(mrb_state *mrb, mrb_value str, long pos);
mrb_value mrb_str_size(mrb_state *mrb, mrb_value self);
long mrb_str_offset(mrb_state *mrb, mrb_value str, long pos);
mrb_value mrb_str_new2(mrb_state *mrb, const char *p);
mrb_value mrb_str_dup(mrb_state *mrb, mrb_value str); /* mrb_str_dup */
mrb_value mrb_str_new_frozen(mrb_state *mrb, mrb_value orig);
mrb_value mrb_lastline_get(mrb_state *mrb);
mrb_value mrb_usascii_str_new(mrb_state *mrb, const char *ptr, long len);
void mrb_lastline_set(mrb_value val);
mrb_value mrb_str_buf_cat_ascii(mrb_state *mrb, mrb_value str, const char *ptr);
void mrb_str_modify(mrb_state *mrb, mrb_value str);
void mrb_str_set_len(mrb_state *mrb, mrb_value str, long len);
mrb_value mrb_str_intern(mrb_state *mrb, mrb_value self);
void mrb_str_shared_replace(mrb_state *mrb, mrb_value str, mrb_value str2);
mrb_value mrb_str_cat2(mrb_state *mrb, mrb_value str, const char *ptr);
mrb_value mrb_str_catf(mrb_state *mrb, mrb_value str, const char *format, ...);
mrb_value mrb_str_to_inum(mrb_state *mrb, mrb_value str, int base, int badcheck);
double mrb_str_to_dbl(mrb_state *mrb, mrb_value str, int badcheck);
mrb_value mrb_str_to_str(mrb_state *mrb, mrb_value str);
mrb_value mrb_locale_str_new(mrb_state *mrb, const char *ptr, long len);
mrb_value mrb_filesystem_str_new_cstr(mrb_state *mrb, const char *ptr);
mrb_int mrb_str_hash(mrb_state *mrb, mrb_value str);
int mrb_str_hash_cmp(mrb_state *mrb, mrb_value str1, mrb_value str2);
mrb_value str_new3(mrb_state *mrb, struct RClass* klass, mrb_value str);
mrb_value mrb_str_buf_append(mrb_state *mrb, mrb_value str, mrb_value str2);
void mrb_str_setter(mrb_state *mrb, mrb_value val, mrb_sym id, mrb_value *var);
int mrb_str_is_ascii_only_p(mrb_state *mrb, mrb_value str);
mrb_value mrb_str_inspect(mrb_state *mrb, mrb_value str);
int mrb_str_equal(mrb_state *mrb, mrb_value str1, mrb_value str2);
mrb_value str_new4(mrb_state *mrb, mrb_value str);
mrb_value * mrb_svar(mrb_int cnt);
mrb_value mrb_str_drop_bytes(mrb_state *mrb, mrb_value str, long len);
mrb_value mrb_str_dump(mrb_state *mrb, mrb_value str);
mrb_value mrb_str_cat(mrb_state *mrb, mrb_value str, const char *ptr, long len);
mrb_value mrb_str_append(mrb_state *mrb, mrb_value str, mrb_value str2);
size_t mrb_str_capacity(mrb_value str);

#ifdef INCLUDE_ENCODING
int sym_printable(mrb_state *mrb, const char *s, const char *send, mrb_encoding *enc);
mrb_value mrb_str_conv_enc(mrb_state *mrb, mrb_value str, mrb_encoding *from, mrb_encoding *to);
mrb_value mrb_str_conv_enc_opts(mrb_state *mrb, mrb_value str, mrb_encoding *from, mrb_encoding *to, int ecflags, mrb_value ecopts);
mrb_value mrb_enc_str_new(mrb_state *mrb, const char *ptr, long len, mrb_encoding *enc);
#else
int mrb_symname_p(const char *name);
#endif

mrb_value mrb_tainted_str_new(mrb_state *mrb, const char *ptr, long len);
int mrb_str_cmp(mrb_state *mrb, mrb_value str1, mrb_value str2);

#endif  /* MRUBY_STRING_H */
