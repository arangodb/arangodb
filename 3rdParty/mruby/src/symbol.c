/*
** symbol.c - Symbol class
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/khash.h"
#include <string.h>

#include <stdarg.h>
#include <string.h>
#include "mruby/string.h"
#include <ctype.h>
#include "mruby/class.h"
#include "mruby/variable.h"
#include <stdio.h>

/* ------------------------------------------------------ */
KHASH_MAP_INIT_INT(s2n, const char*);
KHASH_MAP_INIT_STR(n2s, mrb_sym);
/* ------------------------------------------------------ */
mrb_sym
mrb_intern(mrb_state *mrb, const char *name)
{
  khash_t(n2s) *h = mrb->name2sym;
  khash_t(s2n) *rh = mrb->sym2name;
  khiter_t k;
  size_t len;
  char *p;
  mrb_sym sym;

  k = kh_get(n2s, h, name);
  if (k != kh_end(h))
    return kh_value(h, k);

  sym = ++mrb->symidx;
  len = strlen(name);
  p = mrb_malloc(mrb, len+1);
  memcpy(p, name, len);
  p[len] = 0;
  k = kh_put(n2s, h, p);
  kh_value(h, k) = sym;

  k = kh_put(s2n, rh, sym);
  kh_value(rh, k) = p;

  return sym;
}

const char*
mrb_sym2name(mrb_state *mrb, mrb_sym sym)
{
  khash_t(s2n) *h = mrb->sym2name;
  khiter_t k;

  k = kh_get(s2n, h, sym);
  if (k == kh_end(h)) {
    return NULL;                /* missing */
  }
  return kh_value(h, k);
}

void
mrb_free_symtbls(mrb_state *mrb)
{
  khash_t(s2n) *h = mrb->sym2name;
  khiter_t k;

  for (k = kh_begin(h); k != kh_end(h); ++k)
    if (kh_exist(h, k)) mrb_free(mrb, (char*)kh_value(h, k));
  kh_destroy(s2n,mrb->sym2name);
  kh_destroy(n2s,mrb->name2sym);
}

void
mrb_init_symtbl(mrb_state *mrb)
{
  mrb->name2sym = kh_init(n2s, mrb);
  mrb->sym2name = kh_init(s2n, mrb);
}

/**********************************************************************
 * Document-class: Symbol
 *
 *  <code>Symbol</code> objects represent names and some strings
 *  inside the Ruby
 *  interpreter. They are generated using the <code>:name</code> and
 *  <code>:"string"</code> literals
 *  syntax, and by the various <code>to_sym</code> methods. The same
 *  <code>Symbol</code> object will be created for a given name or string
 *  for the duration of a program's execution, regardless of the context
 *  or meaning of that name. Thus if <code>Fred</code> is a constant in
 *  one context, a method in another, and a class in a third, the
 *  <code>Symbol</code> <code>:Fred</code> will be the same object in
 *  all three contexts.
 *
 *     module One
 *       class Fred
 *       end
 *       $f1 = :Fred
 *     end
 *     module Two
 *       Fred = 1
 *       $f2 = :Fred
 *     end
 *     def Fred()
 *     end
 *     $f3 = :Fred
 *     $f1.object_id   #=> 2514190
 *     $f2.object_id   #=> 2514190
 *     $f3.object_id   #=> 2514190
 *
 */


/* 15.2.11.3.1  */
/*
 *  call-seq:
 *     sym == obj   -> true or false
 *
 *  Equality---If <i>sym</i> and <i>obj</i> are exactly the same
 *  symbol, returns <code>true</code>.
 */

static mrb_value
sym_equal(mrb_state *mrb, mrb_value sym1)
{
  mrb_value sym2;

  mrb_get_args(mrb, "o", &sym2);
  if (mrb_obj_equal(mrb, sym1, sym2)) return mrb_true_value();
    return mrb_false_value();
}

/* 15.2.11.3.2  */
/* 15.2.11.3.3  */
/*
 *  call-seq:
 *     sym.id2name   -> string
 *     sym.to_s      -> string
 *
 *  Returns the name or string corresponding to <i>sym</i>.
 *
 *     :fred.id2name   #=> "fred"
 */
mrb_value
mrb_sym_to_s(mrb_state *mrb, mrb_value sym)
{
  mrb_sym id = SYM2ID(sym);

#ifdef INCLUDE_REGEXP
  //return str_new3(mrb_cString, mrb_id2str(id));
  return str_new3(mrb, mrb_obj_class(mrb, sym), mrb_str_new_cstr(mrb, mrb_sym2name(mrb, id)));
#else
  return mrb_str_new_cstr(mrb, mrb_sym2name(mrb, id)); //mrb_str_new2(mrb_id2name(SYM2ID(sym)));
#endif

}

/* 15.2.11.3.4  */
/*
 * call-seq:
 *   sym.to_sym   -> sym
 *   sym.intern   -> sym
 *
 * In general, <code>to_sym</code> returns the <code>Symbol</code> corresponding
 * to an object. As <i>sym</i> is already a symbol, <code>self</code> is returned
 * in this case.
 */

static mrb_value
sym_to_sym(mrb_state *mrb, mrb_value sym)
{
    return sym;
}

/* 15.2.11.3.5(x)  */
/*
 *  call-seq:
 *     sym.inspect    -> string
 *
 *  Returns the representation of <i>sym</i> as a symbol literal.
 *
 *     :fred.inspect   #=> ":fred"
 */

static mrb_value
sym_inspect(mrb_state *mrb, mrb_value sym)
{
#ifdef INCLUDE_ENCODING
  #define STR_ENC_GET(mrb, str) mrb_enc_from_index(mrb, ENCODING_GET(mrb, str))
  mrb_value str;
  mrb_sym id = SYM2ID(sym);
  mrb_encoding *enc;
  const char *ptr;
  long len;
  char *dest;
  mrb_encoding *resenc = mrb_default_internal_encoding(mrb);

  if (resenc == NULL) resenc = mrb_default_external_encoding(mrb);
  sym = mrb_str_new_cstr(mrb, mrb_sym2name(mrb, id));//mrb_id2str(id);
  enc = STR_ENC_GET(mrb, sym);
  ptr = RSTRING_PTR(sym);
  len = RSTRING_LEN(sym);
  if ((resenc != enc && !mrb_str_is_ascii_only_p(mrb, sym)) || len != (long)strlen(ptr) ||
    !mrb_enc_symname_p(ptr, enc) || !sym_printable(mrb, ptr, ptr + len, enc)) {
    str = mrb_str_inspect(mrb, sym);
    len = RSTRING_LEN(str);
    mrb_str_resize(mrb, str, len + 1);
    dest = RSTRING_PTR(str);
    memmove(dest + 1, dest, len);
    dest[0] = ':';
  }
  else {
    char *dest;
    str = mrb_enc_str_new(mrb, 0, len + 1, enc);
    dest = RSTRING_PTR(str);
    dest[0] = ':';
    memcpy(dest + 1, ptr, len);
  }
  return str;
#else
  mrb_value str;
  const char *name;
  mrb_sym id = SYM2ID(sym);

  name = mrb_sym2name(mrb, id); //mrb_id2name(id);
  str = mrb_str_new(mrb, 0, strlen(name)+1);
  RSTRING(str)->buf[0] = ':';
  strcpy(RSTRING(str)->buf+1, name);
  if (!mrb_symname_p(name)) {
    str = mrb_str_dump(mrb, str);
    strncpy(RSTRING(str)->buf, ":\"", 2);
  }
  return str;
#endif
}


void
mrb_init_symbols(mrb_state *mrb)
{
  struct RClass *sym;

  sym = mrb->symbol_class = mrb_define_class(mrb, "Symbol", mrb->object_class);

  mrb_define_method(mrb, sym, "===",             sym_equal,               ARGS_REQ(1));              /* 15.2.11.3.1  */
  mrb_define_method(mrb, sym, "id2name",         mrb_sym_to_s,            ARGS_NONE());              /* 15.2.11.3.2  */
  mrb_define_method(mrb, sym, "to_s",            mrb_sym_to_s,            ARGS_NONE());              /* 15.2.11.3.3  */
  mrb_define_method(mrb, sym, "to_sym",          sym_to_sym,              ARGS_NONE());              /* 15.2.11.3.4  */

  mrb_define_method(mrb, sym, "inspect",         sym_inspect,             ARGS_NONE());              /* 15.2.11.3.5(x)  */
}
