/**********************************************************************

  encoding.c -

  $Author: naruse $
  created at: Thu May 24 17:23:27 JST 2007

  Copyright (C) 2007 Yukihiro Matsumoto

**********************************************************************/

#include "mruby.h"
#ifdef INCLUDE_ENCODING
#include <ctype.h>
#ifndef NO_LOCALE_CHARMAP
#ifdef __CYGWIN__
#include <windows.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#endif

#define USE_UPPER_CASE_TABLE

#include <ctype.h>
#include <stdio.h>
#include "regenc.h"
#include "regint.h"
#include "encoding.h"
#include "st.h"
#include <string.h>
#include "mruby/numeric.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include "variable.h"
#include "mruby/hash.h"

#define pprintf printf
#define mrb_warning printf
#define mrb_bug printf
#ifndef INT_MAX
#define INT_MAX  2147483647
#endif
#define mrb_isascii(c) ((unsigned long)(c) < 128)
#define OBJ_FREEZE(a)
static mrb_sym id_encoding;
//mrb_value mrb_cEncoding;
static mrb_value mrb_encoding_list;

struct mrb_encoding_entry {
    const char *name;
    mrb_encoding *enc;
    mrb_encoding *base;
};

static struct {
    struct mrb_encoding_entry *list;
    int count;
    int size;
    st_table *names;
} enc_table;

void mrb_enc_init(mrb_state *mrb);

enum {
    ENCINDEX_ASCII,
    ENCINDEX_UTF_8,
    ENCINDEX_US_ASCII,
    ENCINDEX_BUILTIN_MAX
};
#define ENCODING_COUNT ENCINDEX_BUILTIN_MAX
#define ENCODING_NAMELEN_MAX 63
#define valid_encoding_name_p(name) ((name) && strlen(name) <= ENCODING_NAMELEN_MAX)
#define STRCASECMP(s1, s2) (st_strcasecmp(s1, s2))

//#define BUILTIN_TYPE(x) (int)(((struct RBasic*)(x))->flags & T_MASK)
#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

#ifndef OTHER
#define OTHER 2
#endif

#define mrb_usascii_str_new2 mrb_usascii_str_new_cstr

static size_t
enc_memsize(mrb_state *mrb, const void *p)
{
    return 0;
}

static const struct mrb_data_type encoding_data_type = {
  "encoding", 0,
};
#define is_data_encoding(obj) (DATA_TYPE(obj) == &encoding_data_type)

//    RUBY_IMMEDIATE_MASK = 0x03,
//#define IMMEDIATE_MASK RUBY_IMMEDIATE_MASK
//#define IMMEDIATE_P(x) ((VALUE)(x) & IMMEDIATE_MASK)
//#define SPECIAL_CONST_P(x) (IMMEDIATE_P(x) || !RTEST(x))

static mrb_value
enc_new(mrb_state *mrb, mrb_encoding *encoding)
{
    return mrb_obj_value(Data_Wrap_Struct(mrb, mrb->encode_class, &encoding_data_type, encoding));
}

#define enc_autoload_p(enc) (!mrb_enc_mbmaxlen(enc))

#define UNSPECIFIED_ENCODING INT_MAX


static mrb_value
mrb_enc_from_encoding_index(mrb_state *mrb, int idx)
{
  mrb_value list, enc;

  if (mrb_nil_p(list = mrb_encoding_list)) {
    mrb_bug("mrb_enc_from_encoding_index(%d): no mrb_encoding_list", idx);
  }
  enc = mrb_ary_ref(mrb, list, idx);//mrb_ary_entry(list, idx);
  if (mrb_nil_p(enc)) {
    mrb_bug("mrb_enc_from_encoding_index(%d): not created yet", idx);
  }
  return enc;
}

mrb_value
mrb_enc_from_encoding(mrb_state *mrb, mrb_encoding *encoding)
{
    int idx;
    if (!encoding) return mrb_nil_value();
    idx = ENC_TO_ENCINDEX(encoding);
    return mrb_enc_from_encoding_index(mrb, idx);
}

static int enc_autoload(mrb_state *mrb, mrb_encoding *enc);
static int
check_encoding(mrb_state *mrb, mrb_encoding *enc)
{
    int index = mrb_enc_to_index(enc);
    if (mrb_enc_from_index(mrb, index) != enc)
    return -1;
    if (enc_autoload_p(enc)) {
    index = enc_autoload(mrb, enc);
    }
    return index;
}

static int
enc_check_encoding(mrb_state *mrb, mrb_value obj)
{
  if (SPECIAL_CONST_P(obj) || !is_data_encoding(obj)) {
    return -1;
  }
  return check_encoding(mrb, RDATA(obj)->data);
}

static int
must_encoding(mrb_state *mrb, mrb_value enc)
{
  int index = enc_check_encoding(mrb, enc);
  if (index < 0) {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument type %s (expected Encoding)",
         mrb_obj_classname(mrb, enc));
  }
  return index;
}

int
mrb_to_encoding_index(mrb_state *mrb, mrb_value enc)
{
    int idx;

    idx = enc_check_encoding(mrb, enc);
    if (idx >= 0) {
    return idx;
    }
    else if (mrb_nil_p(enc = mrb_check_string_type(mrb, enc))) {
    return -1;
    }
    if (!mrb_enc_asciicompat(mrb, mrb_enc_get(mrb, enc))) {
    return -1;
    }
    //return mrb_enc_find_index(StringValueCStr(enc));
    return mrb_enc_find_index(mrb, mrb_string_value_cstr(mrb, &enc));

}

static mrb_encoding *
to_encoding(mrb_state *mrb, mrb_value enc)
{
  int idx;

  //StringValue(enc);
  mrb_string_value(mrb, &enc);

  if (!mrb_enc_asciicompat(mrb, mrb_enc_get(mrb, enc))) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid name encoding (non ASCII)");
  }
  //idx = mrb_enc_find_index(StringValueCStr(enc));
  idx = mrb_enc_find_index(mrb, mrb_string_value_cstr(mrb, &enc));
  if (idx < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown encoding name - %s", RSTRING_PTR(enc));
  }
  return mrb_enc_from_index(mrb, idx);
}

mrb_encoding *
mrb_to_encoding(mrb_state *mrb, mrb_value enc)
{
    if (enc_check_encoding(mrb, enc) >= 0) return RDATA(enc)->data;
    return to_encoding(mrb, enc);
}

static int
enc_table_expand(int newsize)
{
    struct mrb_encoding_entry *ent;
    int count = newsize;

    if (enc_table.size >= newsize) return newsize;
    newsize = (newsize + 7) / 8 * 8;
    ent = realloc(enc_table.list, sizeof(*enc_table.list) * newsize);
    if (!ent) return -1;
    memset(ent + enc_table.size, 0, sizeof(*ent)*(newsize - enc_table.size));
    enc_table.list = ent;
    enc_table.size = newsize;
    return count;
}

static int
enc_register_at(mrb_state *mrb, int index, const char *name, mrb_encoding *encoding)
{
    struct mrb_encoding_entry *ent = &enc_table.list[index];
    mrb_value list;
    mrb_value ref_ary;

    if (!valid_encoding_name_p(name)) return -1;
    if (!ent->name) {
    ent->name = name = strdup(name);
    }
    else if (STRCASECMP(name, ent->name)) {
    return -1;
    }
    if (!ent->enc) {
    ent->enc = xmalloc(sizeof(mrb_encoding));
    }
    if (encoding) {
    *ent->enc = *encoding;
    }
    else {
    memset(ent->enc, 0, sizeof(*ent->enc));
    }
    encoding = ent->enc;
    encoding->name = name;
    encoding->ruby_encoding_index = index;
    st_insert(enc_table.names, (st_data_t)name, (st_data_t)index);
    list = mrb_encoding_list;
    //if (list && mrb_nil_p((mrb_ary_ref(mrb, list, index)))) {
    if (list.tt) {
      ref_ary = mrb_ary_ref(mrb, list, index);
      if mrb_nil_p(ref_ary) {
        /* initialize encoding data */
        mrb_ary_set(mrb, list, index, enc_new(mrb, encoding));//rb_ary_store(list, index, enc_new(encoding));
      }
    }
    return index;
}


static int
enc_register(mrb_state *mrb, const char *name, mrb_encoding *encoding)
{
    int index = enc_table.count;

    if ((index = enc_table_expand(index + 1)) < 0) return -1;
    enc_table.count = index;
    return enc_register_at(mrb, index - 1, name, encoding);
}

static void set_encoding_const(mrb_state *mrb, const char *, mrb_encoding *);
int mrb_enc_registered(const char *name);

static void
enc_check_duplication(mrb_state *mrb, const char *name)
{
  if (mrb_enc_registered(name) >= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "encoding %s is already registered", name);
  }
}
static mrb_encoding*
set_base_encoding(int index, mrb_encoding *base)
{
    mrb_encoding *enc = enc_table.list[index].enc;

    enc_table.list[index].base = base;
    if (mrb_enc_dummy_p(base)) ENC_SET_DUMMY(enc);
    return enc;
}

int
mrb_enc_replicate(mrb_state *mrb, const char *name, mrb_encoding *encoding)
{
    int idx;

    enc_check_duplication(mrb, name);
    idx = enc_register(mrb, name, encoding);
    set_base_encoding(idx, encoding);
    set_encoding_const(mrb, name, mrb_enc_from_index(mrb, idx));
    return idx;
}

/* 15.2.40.2.17 */
/*
 * call-seq:
 *   enc.replicate(name) -> encoding
 *
 * Returns a replicated encoding of _enc_ whose name is _name_.
 * The new encoding should have the same byte structure of _enc_.
 * If _name_ is used by another encoding, raise ArgumentError.
 *
 */
static mrb_value
enc_replicate(mrb_state *mrb, mrb_value encoding)
{
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  return mrb_enc_from_encoding_index(mrb,
    //mrb_enc_replicate(mrb, StringValueCStr(name),
    mrb_enc_replicate(mrb, mrb_string_value_cstr(mrb, &name),
             mrb_to_encoding(mrb, encoding)));
}
static int
enc_replicate_with_index(mrb_state *mrb, const char *name, mrb_encoding *origenc, int idx)
{
    if (idx < 0) {
    idx = enc_register(mrb, name, origenc);
    }
    else {
    idx = enc_register_at(mrb, idx, name, origenc);
    }
    if (idx >= 0) {
    set_base_encoding(idx, origenc);
    set_encoding_const(mrb, name, mrb_enc_from_index(mrb, idx));
    }
    return idx;
}
int
mrb_encdb_replicate(mrb_state *mrb, const char *name, const char *orig)
{
    int origidx = mrb_enc_registered(orig);
    int idx = mrb_enc_registered(name);

    if (origidx < 0) {
    origidx = enc_register(mrb, orig, 0);
    }
    return enc_replicate_with_index(mrb, name, mrb_enc_from_index(mrb, origidx), idx);
}
int
mrb_define_dummy_encoding(mrb_state *mrb, const char *name)
{
    int index = mrb_enc_replicate(mrb, name, mrb_ascii8bit_encoding(mrb));
    mrb_encoding *enc = enc_table.list[index].enc;

    ENC_SET_DUMMY(enc);
    return index;
}

int
mrb_encdb_dummy(mrb_state *mrb, const char *name)
{
    int index = enc_replicate_with_index(mrb, name, mrb_ascii8bit_encoding(mrb),
                     mrb_enc_registered(name));
    mrb_encoding *enc = enc_table.list[index].enc;

    ENC_SET_DUMMY(enc);
    return index;
}

/* 15.2.40.2.13 */
/*
 * call-seq:
 *   enc.dummy? -> true or false
 *
 * Returns true for dummy encodings.
 * A dummy encoding is an encoding for which character handling is not properly
 * implemented.
 * It is used for stateful encodings.
 *
 *   Encoding::ISO_2022_JP.dummy?       #=> true
 *   Encoding::UTF_8.dummy?             #=> false
 *
 */
static mrb_value
enc_dummy_p(mrb_state *mrb, mrb_value enc)
{
    return ENC_DUMMY_P(enc_table.list[must_encoding(mrb, enc)].enc) ? mrb_true_value() : mrb_false_value();
}

/* 15.2.40.2.12 */
/*
 * call-seq:
 *   enc.ascii_compatible? -> true or false
 *
 * Returns whether ASCII-compatible or not.
 *
 *   Encoding::UTF_8.ascii_compatible?     #=> true
 *   Encoding::UTF_16BE.ascii_compatible?  #=> false
 *
 */
static mrb_value
enc_ascii_compatible_p(mrb_state *mrb, mrb_value enc)
{
    return mrb_enc_asciicompat(mrb, enc_table.list[must_encoding(mrb, enc)].enc) ? mrb_true_value() : mrb_false_value();
}

static const char *
enc_alias_internal(const char *alias, int idx)
{
    alias = strdup(alias);
    st_insert(enc_table.names, (st_data_t)alias, (st_data_t)idx);
    return alias;
}

/*
 * Returns 1 when the encoding is Unicode series other than UTF-7 else 0.
 */
int
mrb_enc_unicode_p(mrb_encoding *enc)
{
    const char *name = mrb_enc_name(enc);
    return name[0] == 'U' && name[1] == 'T' && name[2] == 'F' && name[4] != '7';
}

extern mrb_encoding OnigEncodingUTF_8;
extern mrb_encoding OnigEncodingUS_ASCII;

void
mrb_enc_init(mrb_state *mrb)
{
  enc_table_expand(ENCODING_COUNT + 1);
  if (!enc_table.names) {
    enc_table.names = st_init_strcasetable();
  }
#define ENC_REGISTER(enc) enc_register_at(mrb, ENCINDEX_##enc, mrb_enc_name(&OnigEncoding##enc), &OnigEncoding##enc)
  ENC_REGISTER(ASCII);
  ENC_REGISTER(UTF_8);
  ENC_REGISTER(US_ASCII);
#undef ENC_REGISTER
  enc_table.count = ENCINDEX_BUILTIN_MAX;
}

mrb_encoding *
mrb_enc_from_index(mrb_state *mrb, int index)
{
    if (!enc_table.list) {
      mrb_enc_init(mrb);
    }
    if (index < 0 || enc_table.count <= index) {
      return 0;
    }
    return enc_table.list[index].enc;
}

int
mrb_enc_registered(const char *name)
{
    st_data_t idx = 0;

    if (!name) return -1;
    if (!enc_table.list) return -1;
    if (st_lookup(enc_table.names, (st_data_t)name, &idx)) {
    return (int)idx;
    }
    return -1;
}

mrb_value
mrb_require_safe(mrb_value fname, int safe)
{
    mrb_value result = mrb_nil_value();
    return result;
}
static int
load_encoding(const char *name)
{
    mrb_value enclib;// = mrb_sprintf("enc/%s.so", name);
    //mrb_value verbose;// = ruby_verbose;
    //mrb_value debug;// = ruby_debug;
    //mrb_value loaded;
    char *s = RSTRING_PTR(enclib) + 4, *e = RSTRING_END(enclib) - 3;
    int idx;

    while (s < e) {
    if (!ISALNUM(*s)) *s = '_';
    else if (ISUPPER(*s)) *s = TOLOWER(*s);
    ++s;
    }
    OBJ_FREEZE(enclib);
    //ruby_verbose = mrb_false_value();
    //ruby_debug = mrb_false_value();
    //loaded = mrb_protect(require_enc, enclib, 0);
    //ruby_verbose = verbose;
    //ruby_debug = debug;
    //rb_set_errinfo(mrb_nil_value());
    //if (mrb_nil_p(loaded)) return -1;
    if ((idx = mrb_enc_registered(name)) < 0) return -1;
    if (enc_autoload_p(enc_table.list[idx].enc)) return -1;
    return idx;
}

static int
enc_autoload(mrb_state *mrb, mrb_encoding *enc)
{
    int i;
    mrb_encoding *base = enc_table.list[ENC_TO_ENCINDEX(enc)].base;

    if (base) {
    i = 0;
    do {
        if (i >= enc_table.count) return -1;
    } while (enc_table.list[i].enc != base && (++i, 1));
    if (enc_autoload_p(base)) {
        if (enc_autoload(mrb, base) < 0) return -1;
    }
    i = ENC_TO_ENCINDEX(enc);
    enc_register_at(mrb, i, mrb_enc_name(enc), base);
    }
    else {
    i = load_encoding(mrb_enc_name(enc));
    }
    return i;
}

int
mrb_enc_find_index(mrb_state *mrb, const char *name)
{
  int i = mrb_enc_registered(name);
  mrb_encoding *enc;

  if (i < 0) {
    i = load_encoding(name);
  }
  else if (!(enc = mrb_enc_from_index(mrb, i))) {
    if (i != UNSPECIFIED_ENCODING) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "encoding %s is not registered", name);
    }
  }
  else if (enc_autoload_p(enc)) {
    if (enc_autoload(mrb, enc) < 0) {
      //mrb_warn("failed to load encoding (%s); use ASCII-8BIT instead",
      printf("failed to load encoding (%s); use ASCII-8BIT instead",
            name);
      return 0;
    }
  }
  return i;
}

mrb_encoding *
mrb_enc_find(mrb_state *mrb, const char *name)
{
    int idx = mrb_enc_find_index(mrb, name);
    if (idx < 0) idx = 0;
    return mrb_enc_from_index(mrb, idx);
}

static inline int
enc_capable(mrb_value obj)
{
  if (SPECIAL_CONST_P(obj)) return (mrb_type(obj) == MRB_TT_SYMBOL);
  switch (mrb_type(obj)/*BUILTIN_TYPE(obj)*/) {
    case MRB_TT_STRING:
    case MRB_TT_REGEX:
    case MRB_TT_FILE:
      return TRUE;
    case MRB_TT_DATA:
      if (is_data_encoding(obj)) return TRUE;
    default:
      return FALSE;
  }
}

mrb_sym
mrb_id_encoding(mrb_state *mrb)
{
    //CONST_ID(id_encoding, "encoding");
    id_encoding = mrb_intern(mrb, "encoding");
    return id_encoding;
}

int
mrb_enc_get_index(mrb_state *mrb, mrb_value obj)
{
  int i = -1;
  mrb_value tmp;
  struct RString *ps;

  if (SPECIAL_CONST_P(obj)) {
    if (mrb_type(obj) != MRB_TT_SYMBOL) return -1;
    //obj = mrb_id2str(SYM2ID(obj));
    obj = mrb_str_new_cstr(mrb, mrb_sym2name(mrb, SYM2ID(obj)));
  }
  switch (mrb_type(obj)/*BUILTIN_TYPE(obj)*/) {
    as_default:
    default:
    case MRB_TT_STRING:
    case MRB_TT_REGEX:
      i = (int)ENCODING_GET_INLINED(obj);
      ps = mrb_str_ptr(obj);
      if (i == ENCODING_INLINE_MAX) {
        mrb_value iv;

        //iv = rb_ivar_get(obj, mrb_id_encoding(mrb));
        iv = mrb_iv_get(mrb, obj, mrb_id_encoding(mrb));
        i = mrb_fixnum(iv);
      }
      break;

    case MRB_TT_FILE:
      tmp = mrb_funcall(mrb, obj, "internal_encoding", 0, 0);
      if (mrb_nil_p(tmp)) obj = mrb_funcall(mrb, obj, "external_encoding", 0, 0);
      else obj = tmp;
      if (mrb_nil_p(obj)) break;
    case MRB_TT_DATA:
      if (is_data_encoding(obj)) {
        i = enc_check_encoding(mrb, obj);
      }
      else {
        goto as_default;
      }
      break;
  }
  return i;
}

void
mrb_enc_set_index(mrb_state *mrb, mrb_value obj, int idx)
{
  if (idx < ENCODING_INLINE_MAX) {
    ENCODING_SET_INLINED(obj, idx);
    return;
  }
  ENCODING_SET_INLINED(obj, ENCODING_INLINE_MAX);
  //mrb_ivar_set(obj, mrb_id_encoding(mrb), INT2NUM(idx));
  mrb_iv_set(mrb, obj, mrb_id_encoding(mrb), mrb_fixnum_value(idx));
  return;
}

mrb_value
mrb_enc_associate_index(mrb_state *mrb, mrb_value obj, int idx)
{
/*    enc_check_capable(obj);*/
  if (mrb_enc_get_index(mrb, obj) == idx)
    return obj;
  if (SPECIAL_CONST_P(obj)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "cannot set encoding");
  }
  if (!ENC_CODERANGE_ASCIIONLY(obj) ||
    !mrb_enc_asciicompat(mrb, mrb_enc_from_index(mrb, idx))) {
    ENC_CODERANGE_CLEAR(obj);
  }
  mrb_enc_set_index(mrb, obj, idx);
  return obj;
}

mrb_value
mrb_enc_associate(mrb_state *mrb, mrb_value obj, mrb_encoding *enc)
{
    return mrb_enc_associate_index(mrb, obj, mrb_enc_to_index(enc));
}

mrb_encoding*
mrb_enc_get(mrb_state *mrb, mrb_value obj)
{
    return mrb_enc_from_index(mrb, mrb_enc_get_index(mrb, obj));
}

mrb_encoding*
mrb_enc_check(mrb_state *mrb, mrb_value str1, mrb_value str2)
{
  mrb_encoding *enc = mrb_enc_compatible(mrb, str1, str2);
  if (!enc)
    mrb_raise(mrb, E_ENCODING_ERROR, "incompatible character encodings: %s and %s",
         mrb_enc_name(mrb_enc_get(mrb, str1)),
         mrb_enc_name(mrb_enc_get(mrb, str2)));
   return enc;
}

mrb_encoding*
mrb_enc_compatible(mrb_state *mrb, mrb_value str1, mrb_value str2)
{
    int idx1, idx2;
    mrb_encoding *enc1, *enc2;

    idx1 = mrb_enc_get_index(mrb, str1);
    idx2 = mrb_enc_get_index(mrb, str2);

    if (idx1 < 0 || idx2 < 0)
        return 0;

    if (idx1 == idx2) {
    return mrb_enc_from_index(mrb, idx1);
    }
    enc1 = mrb_enc_from_index(mrb, idx1);
    enc2 = mrb_enc_from_index(mrb, idx2);

    if (mrb_type(str2) == MRB_TT_STRING && RSTRING_LEN(str2) == 0)
    //return (idx1 == ENCINDEX_US_ASCII && mrb_enc_asciicompat(mrb, enc2)) ? enc2 : enc1;
    return enc1;
    if (mrb_type(str1) == MRB_TT_STRING && RSTRING_LEN(str1) == 0)
    //return (idx2 == ENCINDEX_US_ASCII && mrb_enc_asciicompat(mrb, enc1)) ? enc1 : enc2;
    return enc2;
    if (!mrb_enc_asciicompat(mrb, enc1) || !mrb_enc_asciicompat(mrb, enc2)) {
    return 0;
    }

    /* objects whose encoding is the same of contents */
    //if (mrb_type(str2)/*BUILTIN_TYPE(str2)*/ != MRB_TT_STRING && idx2 == ENCINDEX_US_ASCII)
    //return enc1;
    //if (mrb_type(str1)/*BUILTIN_TYPE(str1)*/ != MRB_TT_STRING && idx1 == ENCINDEX_US_ASCII)
    //return enc2;

    if (mrb_type(str1)/*BUILTIN_TYPE(str1)*/ != MRB_TT_STRING) {
    mrb_value tmp = str1;
    int idx0 = idx1;
    str1 = str2;
    str2 = tmp;
    idx1 = idx2;
    idx2 = idx0;
    }
    if (mrb_type(str1)/*BUILTIN_TYPE(str1)*/ == MRB_TT_STRING) {
    int cr1, cr2;

    cr1 = mrb_enc_str_coderange(mrb, str1);
    if (mrb_type(str2)/*BUILTIN_TYPE(str2)*/ == MRB_TT_STRING) {
        cr2 = mrb_enc_str_coderange(mrb, str2);
        if (cr1 != cr2) {
        /* may need to handle ENC_CODERANGE_BROKEN */
        if (cr1 == ENC_CODERANGE_7BIT) return enc2;
        if (cr2 == ENC_CODERANGE_7BIT) return enc1;
        }
        if (cr2 == ENC_CODERANGE_7BIT) {
        if (idx1 == ENCINDEX_ASCII) return enc2;
        return enc1;
        }
    }
    if (cr1 == ENC_CODERANGE_7BIT)
        return enc2;
    }
    return 0;
}

void
mrb_enc_copy(mrb_state *mrb, mrb_value obj1, mrb_value obj2)
{
    mrb_enc_associate_index(mrb, obj1, mrb_enc_get_index(mrb, obj2));
}


/*
 *  call-seq:
 *     obj.encoding   -> encoding
 *
 *  Returns the Encoding object that represents the encoding of obj.
 */

mrb_value
mrb_obj_encoding(mrb_state *mrb, mrb_value obj)
{
  mrb_encoding *enc = mrb_enc_get(mrb, obj);
  if (!enc) {
    mrb_raise(mrb, E_TYPE_ERROR, "unknown encoding");
  }
  return mrb_enc_from_encoding(mrb, enc);
}

int
mrb_enc_fast_mbclen(const char *p, const char *e, mrb_encoding *enc)
{
    return ONIGENC_MBC_ENC_LEN(enc, (UChar*)p, (UChar*)e);
}

int
mrb_enc_mbclen(const char *p, const char *e, mrb_encoding *enc)
{
    int n = ONIGENC_PRECISE_MBC_ENC_LEN(enc, (UChar*)p, (UChar*)e);
    if (MBCLEN_CHARFOUND_P(n) && MBCLEN_CHARFOUND_LEN(n) <= e-p)
        return MBCLEN_CHARFOUND_LEN(n);
    else {
        int min = mrb_enc_mbminlen(enc);
        return min <= e-p ? min : (int)(e-p);
    }
}

int
mrb_enc_precise_mbclen(const char *p, const char *e, mrb_encoding *enc)
{
    int n;
    if (e <= p)
        return ONIGENC_CONSTRUCT_MBCLEN_NEEDMORE(1);
    n = ONIGENC_PRECISE_MBC_ENC_LEN(enc, (UChar*)p, (UChar*)e);
    if (e-p < n)
        return ONIGENC_CONSTRUCT_MBCLEN_NEEDMORE(n-(int)(e-p));
    return n;
}

int
mrb_enc_ascget(mrb_state *mrb, const char *p, const char *e, int *len, mrb_encoding *enc)
{
    unsigned int c, l;
    if (e <= p)
        return -1;
    if (mrb_enc_asciicompat(mrb, enc)) {
        c = (unsigned char)*p;
        if (!ISASCII(c))
            return -1;
        if (len) *len = 1;
        return c;
    }
    l = mrb_enc_precise_mbclen(p, e, enc);
    if (!MBCLEN_CHARFOUND_P(l))
        return -1;
    c = mrb_enc_mbc_to_codepoint(p, e, enc);
    if (!mrb_enc_isascii(c, enc))
        return -1;
    if (len) *len = l;
    return c;
}

unsigned int
mrb_enc_codepoint_len(mrb_state *mrb, const char *p, const char *e, int *len_p, mrb_encoding *enc)
{
  int r;
  if (e <= p)
    mrb_raise(mrb, E_ARGUMENT_ERROR, "empty string");
  r = mrb_enc_precise_mbclen(p, e, enc);
  if (MBCLEN_CHARFOUND_P(r)) {
    if (len_p) *len_p = MBCLEN_CHARFOUND_LEN(r);
      return mrb_enc_mbc_to_codepoint(p, e, enc);
  }
  else
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid byte sequence in %s", mrb_enc_name(enc));
  return 0;
}

#undef mrb_enc_codepoint
unsigned int
mrb_enc_codepoint(mrb_state *mrb, const char *p, const char *e, mrb_encoding *enc)
{
    return mrb_enc_codepoint_len(mrb, p, e, 0, enc);
}

int
mrb_enc_codelen(mrb_state *mrb, int c, mrb_encoding *enc)
{
  int n = ONIGENC_CODE_TO_MBCLEN(enc,c);
  if (n == 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid codepoint 0x%x in %s", c, mrb_enc_name(enc));
  }
  return n;
}

int
mrb_enc_toupper(int c, mrb_encoding *enc)
{
    return (ONIGENC_IS_ASCII_CODE(c)?ONIGENC_ASCII_CODE_TO_UPPER_CASE(c):(c));
}

int
mrb_enc_tolower(int c, mrb_encoding *enc)
{
    return (ONIGENC_IS_ASCII_CODE(c)?ONIGENC_ASCII_CODE_TO_LOWER_CASE(c):(c));
}

/* 15.2.40.2.14 */
/*
 * call-seq:
 *   enc.inspect -> string
 *
 * Returns a string which represents the encoding for programmers.
 *
 *   Encoding::UTF_8.inspect       #=> "#<Encoding:UTF-8>"
 *   Encoding::ISO_2022_JP.inspect #=> "#<Encoding:ISO-2022-JP (dummy)>"
 */
static mrb_value
enc_inspect(mrb_state *mrb, mrb_value self)
{
  mrb_value str;
    //mrb_value str = mrb_sprintf("#<%s:%s%s>", mrb_obj_classname(mrb, self),
    //          mrb_enc_name((mrb_encoding*)(DATA_PTR(self))),
    //          (mrb_fixnum(enc_dummy_p(mrb, self)) ? " (dummy)" : ""));
  char buf[256];
  sprintf(buf, "#<%s:%s%s>", mrb_obj_classname(mrb, self),
              mrb_enc_name((mrb_encoding*)(DATA_PTR(self))),
              (mrb_enc_dummy_p((mrb_encoding*)(DATA_PTR(self))) ? " (dummy)" : ""));
  str = mrb_str_new(mrb, buf, strlen(buf));
  ENCODING_CODERANGE_SET(mrb, str, mrb_usascii_encindex(), ENC_CODERANGE_7BIT);
  return str;
}

/* 15.2.40.2.15 */
/* 15.2.40.2.18 */
/*
 * call-seq:
 *   enc.name -> string
 *
 * Returns the name of the encoding.
 *
 *   Encoding::UTF_8.name      #=> "UTF-8"
 */
static mrb_value
enc_name(mrb_state *mrb, mrb_value self)
{
    return mrb_usascii_str_new2(mrb, mrb_enc_name((mrb_encoding*)DATA_PTR(self)));
}

struct fn_arg {
  mrb_state *mrb;
  int (*func)(ANYARGS);
  void *a;
};

static int
fn_i(st_data_t key, st_data_t val, st_data_t arg) {
  struct fn_arg *a = (struct fn_arg*)arg;

  return (*a->func)(a->mrb, key, val, a->a);
}

static int
st_foreachNew(mrb_state *mrb, st_table *tbl, int (*func)(ANYARGS), void *a)
{
  struct fn_arg arg = {
    mrb,
    func,
    a,
  };

  return st_foreach(tbl, fn_i, (st_data_t)&arg);
}

static int
enc_names_i(mrb_state *mrb, st_data_t name, st_data_t idx, st_data_t args)
{
  mrb_value *arg = (mrb_value *)args;
  int iargs = mrb_fixnum(arg[0]);
  //if ((int)idx == (int)arg[0]) {
  if ((int)idx == iargs) {
    mrb_value str = mrb_usascii_str_new2(mrb, (char *)name);
    //OBJ_FREEZE(str);
    mrb_ary_push(mrb, arg[1], str);
  }
  return ST_CONTINUE;
}

/* 15.2.40.2.16 */
/*
 * call-seq:
 *   enc.names -> array
 *
 * Returns the list of name and aliases of the encoding.
 *
 *   Encoding::WINDOWS_31J.names  #=> ["Windows-31J", "CP932", "csWindows31J"]
 */
static mrb_value
enc_names(mrb_state *mrb, mrb_value self)
{
    mrb_value args[2];

    args[0] = mrb_fixnum_value(mrb_to_encoding_index(mrb, self));
    args[1] = mrb_ary_new_capa(mrb, 0);//mrb_ary_new2(0);
    st_foreachNew(mrb, enc_table.names, enc_names_i, args);
    return args[1];
}

/* 15.2.40.2.8  */
/*
 * call-seq:
 *   Encoding.list -> [enc1, enc2, ...]
 *
 * Returns the list of loaded encodings.
 *
 *   Encoding.list
 *   #=> [#<Encoding:ASCII-8BIT>, #<Encoding:UTF-8>,
 *         #<Encoding:ISO-2022-JP (dummy)>]
 *
 *   Encoding.find("US-ASCII")
 *   #=> #<Encoding:US-ASCII>
 *
 *   Encoding.list
 *   #=> [#<Encoding:ASCII-8BIT>, #<Encoding:UTF-8>,
 *         #<Encoding:US-ASCII>, #<Encoding:ISO-2022-JP (dummy)>]
 *
 */
static mrb_value
enc_list(mrb_state *mrb, mrb_value klass)
{
    struct RArray *ar = (struct RArray *)mrb_encoding_list.value.p;
    mrb_value ary = mrb_ary_new_capa(mrb, 0);//mrb_ary_new2(0);
    //mrb_ary_replace_m(mrb, ary/*, mmrb_encoding_list*/);
    mrb_ary_replace(mrb, mrb_ary_ptr(ary), ar->buf, enc_table.count);
    return ary;
}

/* 15.2.40.2.7  */
/*
 * call-seq:
 *   Encoding.find(string) -> enc
 *   Encoding.find(symbol) -> enc
 *
 * Search the encoding with specified <i>name</i>.
 * <i>name</i> should be a string or symbol.
 *
 *   Encoding.find("US-ASCII")  #=> #<Encoding:US-ASCII>
 *   Encoding.find(:Shift_JIS)  #=> #<Encoding:Shift_JIS>
 *
 * Names which this method accept are encoding names and aliases
 * including following special aliases
 *
 * "external"::   default external encoding
 * "internal"::   default internal encoding
 * "locale"::     locale encoding
 * "filesystem":: filesystem encoding
 *
 * An ArgumentError is raised when no encoding with <i>name</i>.
 * Only <code>Encoding.find("internal")</code> however returns nil
 * when no encoding named "internal", in other words, when Ruby has no
 * default internal encoding.
 */
static mrb_value
enc_find(mrb_state *mrb, mrb_value klass)
{
  mrb_value enc;
  mrb_get_args(mrb, "o", &enc);

  return mrb_enc_from_encoding(mrb, to_encoding(mrb, enc));
}

/* 15.2.40.2.2  */
/*
 * call-seq:
 *   Encoding.compatible?(str1, str2) -> enc or nil
 *
 * Checks the compatibility of two strings.
 * If they are compatible, means concatenatable,
 * returns an encoding which the concatenated string will be.
 * If they are not compatible, nil is returned.
 *
 *   Encoding.compatible?("\xa1".force_encoding("iso-8859-1"), "b")
 *   #=> #<Encoding:ISO-8859-1>
 *
 *   Encoding.compatible?(
 *     "\xa1".force_encoding("iso-8859-1"),
 *     "\xa1\xa1".force_encoding("euc-jp"))
 *   #=> nil
 *
 */
static mrb_value
enc_compatible_p(mrb_state *mrb, mrb_value klass)
{
    mrb_value str1;
    mrb_value str2;
    mrb_encoding *enc;
    mrb_get_args(mrb, "oo", &str1, &str2);
    if (!enc_capable(str1)) return mrb_nil_value();
    if (!enc_capable(str2)) return mrb_nil_value();
    enc = mrb_enc_compatible(mrb, str1, str2);
    if (!enc) return mrb_nil_value();
    return mrb_enc_from_encoding(mrb, enc);
}

/* 15.2.40.2.19 */
/* :nodoc: */
static mrb_value
enc_dump(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value self)
{
    //mrb_scan_args(argc, argv, "01", 0);
    return enc_name(mrb, self);
}

/* 15.2.40.2.11 */
/* :nodoc: */
static mrb_value
enc_load(mrb_state *mrb, mrb_value klass)
{
    mrb_value str;
    mrb_get_args(mrb, "o", &str);
    return enc_find(mrb, str);
}

mrb_encoding *
mrb_ascii8bit_encoding(mrb_state *mrb)
{
    if (!enc_table.list) {
    mrb_enc_init(mrb);
    }
    return enc_table.list[ENCINDEX_ASCII].enc;
}

int
mrb_ascii8bit_encindex(void)
{
    return ENCINDEX_ASCII;
}

mrb_encoding *
mrb_utf8_encoding(mrb_state *mrb)
{
    if (!enc_table.list) {
    mrb_enc_init(mrb);
    }
    return enc_table.list[ENCINDEX_UTF_8].enc;
}

int
mrb_utf8_encindex(void)
{
    return ENCINDEX_UTF_8;
}

mrb_encoding *
mrb_usascii_encoding(mrb_state *mrb)
{
  if (!enc_table.list) {
    mrb_enc_init(mrb);
  }
  return enc_table.list[ENCINDEX_US_ASCII].enc;
}

int
mrb_usascii_encindex(void)
{
    return ENCINDEX_US_ASCII;
}

int
mrb_locale_encindex(mrb_state *mrb)
{
    mrb_value charmap = mrb_locale_charmap(mrb, mrb_obj_value(mrb->encode_class));
    int idx;

    if (mrb_nil_p(charmap))
        idx = mrb_usascii_encindex();
    //else if ((idx = mrb_enc_find_index(StringValueCStr(charmap))) < 0)
    else if ((idx = mrb_enc_find_index(mrb, mrb_string_value_cstr(mrb, &charmap))) < 0)
        idx = mrb_ascii8bit_encindex();

    if (mrb_enc_registered("locale") < 0) enc_alias_internal("locale", idx);

    return idx;
}

mrb_encoding *
mrb_locale_encoding(mrb_state *mrb)
{
    return mrb_enc_from_index(mrb, mrb_locale_encindex(mrb));
}

static int
enc_set_filesystem_encoding(mrb_state *mrb)
{
    int idx;
#if defined NO_LOCALE_CHARMAP
    idx = mrb_enc_to_index(mrb_default_external_encoding(mrb));
#elif defined _WIN32 || defined __CYGWIN__
    char cp[sizeof(int) * 8 / 3 + 4];
    //snprintf(cp, sizeof cp, "CP%d", AreFileApisANSI() ? GetACP() : GetOEMCP());
    idx = mrb_enc_find_index(mrb, cp);
    if (idx < 0) idx = mrb_ascii8bit_encindex();
#else
    idx = mrb_enc_to_index(mrb_default_external_encoding(mrb));
#endif

    enc_alias_internal("filesystem", idx);
    return idx;
}

int
mrb_filesystem_encindex(void)
{
    int idx = mrb_enc_registered("filesystem");
    if (idx < 0)
    idx = mrb_ascii8bit_encindex();
    return idx;
}

mrb_encoding *
mrb_filesystem_encoding(mrb_state *mrb)
{
    return mrb_enc_from_index(mrb, mrb_filesystem_encindex());
}

struct default_encoding {
    int index;            /* -2 => not yet set, -1 => nil */
    mrb_encoding *enc;
};

static struct default_encoding default_external = {0};

static int
enc_set_default_encoding(mrb_state *mrb, struct default_encoding *def, mrb_value encoding, const char *name)
{
    int overridden = FALSE;

    if (def->index != -2)
    /* Already set */
    overridden = TRUE;

    if (mrb_nil_p(encoding)) {
    def->index = -1;
    def->enc = 0;
    st_insert(enc_table.names, (st_data_t)strdup(name),
          (st_data_t)UNSPECIFIED_ENCODING);
    }
    else {
    def->index = mrb_enc_to_index(mrb_to_encoding(mrb, encoding));
    def->enc = 0;
    enc_alias_internal(name, def->index);
    }

    if (def == &default_external)
    enc_set_filesystem_encoding(mrb);

    return overridden;
}

mrb_encoding *
mrb_default_external_encoding(mrb_state *mrb)
{
    if (default_external.enc) return default_external.enc;

    if (default_external.index >= 0) {
        default_external.enc = mrb_enc_from_index(mrb, default_external.index);
        return default_external.enc;
    }
    else {
        return mrb_locale_encoding(mrb);
    }
}

mrb_value
mrb_enc_default_external(mrb_state *mrb)
{
    return mrb_enc_from_encoding(mrb, mrb_default_external_encoding(mrb));
}

/* 15.2.40.2.3  */
/*
 * call-seq:
 *   Encoding.default_external -> enc
 *
 * Returns default external encoding.
 *
 * It is initialized by the locale or -E option.
 */
static mrb_value
get_default_external(mrb_state *mrb, mrb_value klass)
{
    return mrb_enc_default_external(mrb);
}

void
mrb_enc_set_default_external(mrb_state *mrb, mrb_value encoding)
{
    if (mrb_nil_p(encoding)) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "default external can not be nil");
    }
    enc_set_default_encoding(mrb, &default_external, encoding,
                            "external");
}

/* 15.2.40.2.4  */
/*
 * call-seq:
 *   Encoding.default_external = enc
 *
 * Sets default external encoding.
 */
static mrb_value
set_default_external(mrb_state *mrb, mrb_value klass)
{
  mrb_value encoding;
  mrb_get_args(mrb, "o", &encoding);
  mrb_warning("setting Encoding.default_external");
  mrb_enc_set_default_external(mrb, encoding);
  return encoding;
}

static struct default_encoding default_internal = {-2};

mrb_encoding *
mrb_default_internal_encoding(mrb_state *mrb)
{
    if (!default_internal.enc && default_internal.index >= 0) {
        default_internal.enc = mrb_enc_from_index(mrb, default_internal.index);
    }
    return default_internal.enc; /* can be NULL */
}

mrb_value
mrb_enc_default_internal(mrb_state *mrb)
{
    /* Note: These functions cope with default_internal not being set */
    return mrb_enc_from_encoding(mrb, mrb_default_internal_encoding(mrb));
}

/* 15.2.40.2.5  */
/*
 * call-seq:
 *   Encoding.default_internal -> enc
 *
 * Returns default internal encoding.
 *
 * It is initialized by the source internal_encoding or -E option.
 */
static mrb_value
get_default_internal(mrb_state *mrb, mrb_value klass)
{
    return mrb_enc_default_internal(mrb);
}

void
mrb_enc_set_default_internal(mrb_state *mrb, mrb_value encoding)
{
    enc_set_default_encoding(mrb, &default_internal, encoding,
                            "internal");
}

/* 15.2.40.2.6  */
/*
 * call-seq:
 *   Encoding.default_internal = enc or nil
 *
 * Sets default internal encoding.
 * Or removes default internal encoding when passed nil.
 */
static mrb_value
set_default_internal(mrb_state *mrb, mrb_value klass)
{
  mrb_value encoding;
  mrb_get_args(mrb, "o", &encoding);
  mrb_warning("setting Encoding.default_internal");
  mrb_enc_set_default_internal(mrb, encoding);
  return encoding;
}

#define digit(x) ((x) >= '0' && (x) <= '9')
#define strstart(s, n) (strncasecmp(s, n, strlen(n)) == 0)
#define C_CODESET "US-ASCII"     /* Return this as the encoding of the
                                  * C/POSIX locale. Could as well one day
                                  * become "UTF-8". */
#if defined _WIN32 || defined __CYGWIN__
#define JA_CODESET "Windows-31J"
#else
#define JA_CODESET "EUC-JP"
#endif

static char buf[16];

const char *
nl_langinfo_codeset(void)
{
  const char *l, *p;
  int n;

  if (((l = getenv("LC_ALL"))   && *l) ||
      ((l = getenv("LC_CTYPE")) && *l) ||
      ((l = getenv("LANG"))     && *l)) {
    /* check standardized locales */
    if (!strcmp(l, "C") || !strcmp(l, "POSIX"))
      return C_CODESET;
    /* check for encoding name fragment */
    p = strchr(l, '.');
    if (!p++) p = l;
    if (strstart(p, "UTF"))
	return "UTF-8";
    if ((n = 5, strstart(p, "8859-")) || (n = 9, strstart(p, "ISO-8859-"))) {
      if (digit(p[n])) {
	p += n;
	memcpy(buf, "ISO-8859-\0\0", 12);
	buf[9] = *p++;
	if (digit(*p)) buf[10] = *p++;
	return buf;
      }
    }
    if (strstart(p, "KOI8-R")) return "KOI8-R";
    if (strstart(p, "KOI8-U")) return "KOI8-U";
    if (strstart(p, "620")) return "TIS-620";
    if (strstart(p, "2312")) return "GB2312";
    if (strstart(p, "HKSCS")) return "Big5HKSCS";   /* no MIME charset */
    if (strstart(p, "BIG5")) return "Big5";
    if (strstart(p, "GBK")) return "GBK";           /* no MIME charset */
    if (strstart(p, "18030")) return "GB18030";     /* no MIME charset */
    if (strstart(p, "Shift_JIS") || strstart(p, "SJIS")) return "Windows-31J";
    /* check for conclusive modifier */
    if (strstart(p, "euro")) return "ISO-8859-15";
    /* check for language (and perhaps country) codes */
    if (strstart(l, "zh_TW")) return "Big5";
    if (strstart(l, "zh_HK")) return "Big5HKSCS";   /* no MIME charset */
    if (strstart(l, "zh")) return "GB2312";
    if (strstart(l, "ja")) return JA_CODESET;
    if (strstart(l, "ko")) return "EUC-KR";
    if (strstart(l, "ru")) return "KOI8-R";
    if (strstart(l, "uk")) return "KOI8-U";
    if (strstart(l, "pl") || strstart(l, "hr") ||
	strstart(l, "hu") || strstart(l, "cs") ||
	strstart(l, "sk") || strstart(l, "sl")) return "ISO-8859-2";
    if (strstart(l, "eo") || strstart(l, "mt")) return "ISO-8859-3";
    if (strstart(l, "el")) return "ISO-8859-7";
    if (strstart(l, "he")) return "ISO-8859-8";
    if (strstart(l, "tr")) return "ISO-8859-9";
    if (strstart(l, "th")) return "TIS-620";      /* or ISO-8859-11 */
    if (strstart(l, "lt")) return "ISO-8859-13";
    if (strstart(l, "cy")) return "ISO-8859-14";
    if (strstart(l, "ro")) return "ISO-8859-2";   /* or ISO-8859-16 */
    if (strstart(l, "am") || strstart(l, "vi")) return "UTF-8";
    /* Send me further rules if you like, but don't forget that we are
     * *only* interested in locale naming conventions on platforms
     * that do not already provide an nl_langinfo(CODESET) implementation. */
  }
  return NULL;
}

/* 15.2.40.2.9  */
/*
 * call-seq:
 *   Encoding.locale_charmap -> string
 *
 * Returns the locale charmap name.
 *
 *   Debian GNU/Linux
 *     LANG=C
 *       Encoding.locale_charmap  #=> "ANSI_X3.4-1968"
 *     LANG=ja_JP.EUC-JP
 *       Encoding.locale_charmap  #=> "EUC-JP"
 *
 *   SunOS 5
 *     LANG=C
 *       Encoding.locale_charmap  #=> "646"
 *     LANG=ja
 *       Encoding.locale_charmap  #=> "eucJP"
 *
 * The result is highly platform dependent.
 * So Encoding.find(Encoding.locale_charmap) may cause an error.
 * If you need some encoding object even for unknown locale,
 * Encoding.find("locale") can be used.
 *
 */
mrb_value
mrb_locale_charmap(mrb_state *mrb, mrb_value klass)
{
#if defined NO_LOCALE_CHARMAP
    return mrb_usascii_str_new2(mrb, "ASCII-8BIT");
#elif defined _WIN32 || defined __CYGWIN__
    const char *nl_langinfo_codeset(void);
    const char *codeset = nl_langinfo_codeset();
    char cp[sizeof(int) * 3 + 4];
    if (!codeset) {
    //snprintf(cp, sizeof(cp), "CP%d", GetConsoleCP());
    codeset = cp;
    }
    return mrb_usascii_str_new2(mrb, codeset);
#elif defined HAVE_LANGINFO_H
    char *codeset;
    codeset = nl_langinfo(CODESET);
    return mrb_usascii_str_new2(mrb, codeset);
#else
    return mrb_nil_value();
#endif
}
static void
set_encoding_const(mrb_state *mrb, const char *name, mrb_encoding *enc)
{
  mrb_value encoding = mrb_enc_from_encoding(mrb, enc);
  char *s = (char *)name;
  int haslower = 0, hasupper = 0, valid = 0;

  if (ISDIGIT(*s)) return;
  if (ISUPPER(*s)) {
    hasupper = 1;
    while (*++s && (ISALNUM(*s) || *s == '_')) {
        if (ISLOWER(*s)) haslower = 1;
    }
  }
  if (!*s) {
    if (s - name > ENCODING_NAMELEN_MAX) return;
    valid = 1;
    //mrb_define_const(mrb_cEncoding, name, encoding);
    mrb_define_const(mrb, mrb->encode_class, name, encoding);
  }
  if (!valid || haslower) {
    size_t len = s - name;
    if (len > ENCODING_NAMELEN_MAX) return;
    if (!haslower || !hasupper) {
      do {
        if (ISLOWER(*s)) haslower = 1;
        if (ISUPPER(*s)) hasupper = 1;
      } while (*++s && (!haslower || !hasupper));
      len = s - name;
    }
    len += strlen(s);
    if (len++ > ENCODING_NAMELEN_MAX) return;
    //MEMCPY(s = ALLOCA_N(char, len), name, char, len);
    memcpy(s = mrb_malloc(mrb, len), name, len);
    name = s;
    if (!valid) {
      if (ISLOWER(*s)) *s = ONIGENC_ASCII_CODE_TO_UPPER_CASE((int)*s);
      for (; *s; ++s) {
        if (!ISALNUM(*s)) *s = '_';
      }
      if (hasupper) {
        mrb_define_const(mrb, mrb->encode_class, name, encoding);
      }
    }
    if (haslower) {
      for (s = (char *)name; *s; ++s) {
      if (ISLOWER(*s)) *s = ONIGENC_ASCII_CODE_TO_UPPER_CASE((int)*s);
      }
      mrb_define_const(mrb, mrb->encode_class, name, encoding);
    }
  }
}
static int
mrb_enc_name_list_i(mrb_state *mrb, st_data_t name, st_data_t idx, mrb_value *arg)
{
    mrb_value ary = *arg;
    mrb_value str = mrb_usascii_str_new2(mrb, (char *)name);
    //OBJ_FREEZE(str);
    mrb_ary_push(mrb, ary, str);
    return ST_CONTINUE;
}

/* 15.2.40.2.10 */
/*
 * call-seq:
 *   Encoding.name_list -> ["enc1", "enc2", ...]
 *
 * Returns the list of available encoding names.
 *
 *   Encoding.name_list
 *   #=> ["US-ASCII", "ASCII-8BIT", "UTF-8",
 *         "ISO-8859-1", "Shift_JIS", "EUC-JP",
 *         "Windows-31J",
 *         "BINARY", "CP932", "eucJP"]
 *
 */

static mrb_value
mrb_enc_name_list(mrb_state *mrb, mrb_value klass)
{
    mrb_value ary = mrb_ary_new_capa(mrb, enc_table.names->num_entries);//mrb_ary_new2(enc_table.names->num_entries);
    st_foreachNew(mrb, enc_table.names, mrb_enc_name_list_i, &ary);
    return ary;
}

static int
mrb_enc_aliases_enc_i(mrb_state *mrb, st_data_t name, st_data_t orig, st_data_t arg)
{
    mrb_value *p = (mrb_value *)arg;
    mrb_value aliases = p[0], ary = p[1];
    int idx = (int)orig;
    mrb_value key, str = mrb_ary_ref(mrb, ary, idx);//mrb_ary_entry(ary, idx);

    if (mrb_nil_p(str)) {
    mrb_encoding *enc = mrb_enc_from_index(mrb, idx);

    if (!enc) return ST_CONTINUE;
    if (STRCASECMP((char*)name, mrb_enc_name(enc)) == 0) {
        return ST_CONTINUE;
    }
    str = mrb_usascii_str_new2(mrb, mrb_enc_name(enc));
    OBJ_FREEZE(str);
    mrb_ary_set(mrb, ary, idx, str);//rb_ary_store(ary, idx, str);
    }
    key = mrb_usascii_str_new2(mrb, (char *)name);
    OBJ_FREEZE(key);
    mrb_hash_set(mrb, aliases, key, str);
    return ST_CONTINUE;
}

/* 15.2.40.2.1  */
/*
 * call-seq:
 *   Encoding.aliases -> {"alias1" => "orig1", "alias2" => "orig2", ...}
 *
 * Returns the hash of available encoding alias and original encoding name.
 *
 *   Encoding.aliases
 *   #=> {"BINARY"=>"ASCII-8BIT", "ASCII"=>"US-ASCII", "ANSI_X3.4-1986"=>"US-ASCII",
 *         "SJIS"=>"Shift_JIS", "eucJP"=>"EUC-JP", "CP932"=>"Windows-31J"}
 *
 */

static mrb_value
mrb_enc_aliases(mrb_state *mrb, mrb_value klass)
{
    mrb_value aliases[2];
    aliases[0] = mrb_hash_new_capa(mrb, 0);
    aliases[1] = mrb_ary_new(mrb);
    st_foreachNew(mrb, enc_table.names, mrb_enc_aliases_enc_i, aliases);
    return aliases[0];
}

void
mrb_init_encoding(mrb_state *mrb)
{
#undef mrb_intern
#define mrb_intern(str) mrb_intern_const(str)
  mrb_value list;
  int i;
  struct RClass *s;

  s = mrb->encode_class = mrb_define_class(mrb, "Encoding", mrb->object_class);
  //mrb_undef_alloc_func(mrb_cEncoding);
  //mrb_undef_method(CLASS_OF(mrb_cEncoding), "new");
  mrb_define_class_method(mrb, s, "aliases",           mrb_enc_aliases,        ARGS_NONE()); /* 15.2.40.2.1  */
  mrb_define_class_method(mrb, s, "compatible?",       enc_compatible_p,       ARGS_REQ(2)); /* 15.2.40.2.2  */
  mrb_define_class_method(mrb, s, "default_external",  get_default_external,   ARGS_NONE()); /* 15.2.40.2.3  */
  mrb_define_class_method(mrb, s, "default_external=", set_default_external,   ARGS_REQ(1)); /* 15.2.40.2.4  */
  mrb_define_class_method(mrb, s, "default_internal",  get_default_internal,   ARGS_NONE()); /* 15.2.40.2.5  */
  mrb_define_class_method(mrb, s, "default_internal=", set_default_internal,   ARGS_REQ(1)); /* 15.2.40.2.6  */
  mrb_define_class_method(mrb, s, "find",              enc_find,               ARGS_REQ(1)); /* 15.2.40.2.7  */
  mrb_define_class_method(mrb, s, "list",              enc_list,               ARGS_NONE()); /* 15.2.40.2.8  */
  mrb_define_class_method(mrb, s, "locale_charmap",    mrb_locale_charmap,     ARGS_NONE()); /* 15.2.40.2.9  */
  mrb_define_class_method(mrb, s, "name_list",         mrb_enc_name_list,      ARGS_NONE()); /* 15.2.40.2.10 */
  mrb_define_class_method(mrb, s, "_load",             enc_load,               ARGS_REQ(1)); /* 15.2.40.2.11 */
  mrb_define_method(mrb, s,       "ascii_compatible?", enc_ascii_compatible_p, ARGS_NONE()); /* 15.2.40.2.12 */
  mrb_define_method(mrb, s,       "dummy?",            enc_dummy_p,            ARGS_NONE()); /* 15.2.40.2.13 */
  mrb_define_method(mrb, s,       "inspect",           enc_inspect,            ARGS_NONE()); /* 15.2.40.2.14 */
  mrb_define_method(mrb, s,       "name",              enc_name,               ARGS_NONE()); /* 15.2.40.2.15 */
  mrb_define_method(mrb, s,       "names",             enc_names,              ARGS_NONE()); /* 15.2.40.2.16 */
  mrb_define_method(mrb, s,       "replicate",         enc_replicate,          ARGS_REQ(1)); /* 15.2.40.2.17 */
  mrb_define_method(mrb, s,       "to_s",              enc_name,               ARGS_NONE()); /* 15.2.40.2.18 */
  mrb_define_method(mrb, s,       "_dump",             enc_dump,               ARGS_ANY());  /* 15.2.40.2.19 */

/* add kusuda --> */
  if (!enc_table.list) {
    mrb_enc_init(mrb);
  }
/* add kusuda --< */
  list = mrb_ary_new_capa(mrb, enc_table.count);//mrb_ary_new2(enc_table.count);
  RBASIC(list)->c = 0;
  mrb_encoding_list = list;
  //mrb_gc_register_mark_object(list);

  for (i = 0; i < enc_table.count; ++i) {
    mrb_ary_push(mrb, list, enc_new(mrb, enc_table.list[i].enc));
  }
}

/* locale insensitive functions */

#define ctype_test(c, ctype) \
    (mrb_isascii(c) && ONIGENC_IS_ASCII_CODE_CTYPE((c), ctype))

int mrb_isalnum(int c) { return ctype_test(c, ONIGENC_CTYPE_ALNUM); }
int mrb_isalpha(int c) { return ctype_test(c, ONIGENC_CTYPE_ALPHA); }
int mrb_isblank(int c) { return ctype_test(c, ONIGENC_CTYPE_BLANK); }
int mrb_iscntrl(int c) { return ctype_test(c, ONIGENC_CTYPE_CNTRL); }
int mrb_isdigit(int c) { return ctype_test(c, ONIGENC_CTYPE_DIGIT); }
int mrb_isgraph(int c) { return ctype_test(c, ONIGENC_CTYPE_GRAPH); }
int mrb_islower(int c) { return ctype_test(c, ONIGENC_CTYPE_LOWER); }
int mrb_isprint(int c) { return ctype_test(c, ONIGENC_CTYPE_PRINT); }
int mrb_ispunct(int c) { return ctype_test(c, ONIGENC_CTYPE_PUNCT); }
int mrb_isspace(int c) { return ctype_test(c, ONIGENC_CTYPE_SPACE); }
int mrb_isupper(int c) { return ctype_test(c, ONIGENC_CTYPE_UPPER); }
int mrb_isxdigit(int c) { return ctype_test(c, ONIGENC_CTYPE_XDIGIT); }

int
mrb_tolower(int c)
{
    return mrb_isascii(c) ? ONIGENC_ASCII_CODE_TO_LOWER_CASE(c) : c;
}

int
mrb_toupper(int c)
{
    return mrb_isascii(c) ? ONIGENC_ASCII_CODE_TO_UPPER_CASE(c) : c;
}
#endif //INCLUDE_ENCODING
