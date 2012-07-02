/*
** class.c - Class class
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include <stdarg.h>
#include <stdio.h>
#include "mruby/class.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/numeric.h"
#include "mruby/variable.h"
#include "mruby/array.h"
#include "error.h"

KHASH_DEFINE(iv, mrb_sym, mrb_value,     1, kh_int_hash_func, kh_int_hash_equal);
KHASH_DEFINE(mt, mrb_sym, struct RProc*, 1, kh_int_hash_func, kh_int_hash_equal);

typedef struct fc_result {
    mrb_sym name;
    struct RClass * klass;
    mrb_value path;
    struct RClass * track;
    struct fc_result *prev;
} fcresult_t;

void
mrb_gc_mark_mt(mrb_state *mrb, struct RClass *c)
{
  khiter_t k;
  khash_t(mt) *h = c->mt;

  if (!h) return;
  for (k = kh_begin(h); k != kh_end(h); k++) {
    if (kh_exist(h, k)){
      struct RProc *m = kh_value(h, k);
      if (m) {
	mrb_gc_mark(mrb, (struct RBasic*)m);
      }
    }
  }
}

size_t
mrb_gc_mark_mt_size(mrb_state *mrb, struct RClass *c)
{
  khash_t(mt) *h = c->mt;

  if (!h) return 0;
  return kh_size(h);
}

void
mrb_gc_free_mt(mrb_state *mrb, struct RClass *c)
{
  kh_destroy(mt, c->mt);
}

void
mrb_name_class(mrb_state *mrb, struct RClass *c, mrb_sym name)
{
  mrb_obj_iv_set(mrb, (struct RObject*)c,
                 mrb_intern(mrb, "__classid__"), mrb_symbol_value(name));
}

static mrb_sym
class_sym(mrb_state *mrb, struct RClass *c, struct RClass *outer)
{
  mrb_value name;

  name = mrb_obj_iv_get(mrb, (struct RObject*)c, mrb_intern(mrb, "__classid__"));
  if (mrb_nil_p(name)) {
    khash_t(iv)* h;
    khiter_t k;
    mrb_value v;

    if (!outer) outer = mrb->object_class;
    h = outer->iv;
    for (k = kh_begin(h); k != kh_end(h); k++) {
      if (!kh_exist(h,k)) continue;
      v = kh_value(h,k);
      if (mrb_type(v) == c->tt && mrb_class_ptr(v) == c) {
        return kh_key(h,k);
      }
    }
  }
  return SYM2ID(name);
}

static void
make_metaclass(mrb_state *mrb, struct RClass *c)
{
  struct RClass *sc;

  if (c->c->tt == MRB_TT_SCLASS) {
    return;
  }
  sc = (struct RClass*)mrb_obj_alloc(mrb, MRB_TT_SCLASS, mrb->class_class);
  sc->mt = 0;
  if (!c->super) {
    sc->super = mrb->class_class;
  }
  else {
    sc->super = c->super->c;
  }
  c->c = sc;
  mrb_field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)sc);
  mrb_field_write_barrier(mrb, (struct RBasic*)sc, (struct RBasic*)sc->super);
}

struct RClass*
mrb_define_module_id(mrb_state *mrb, mrb_sym name)
{
  struct RClass *m = mrb_module_new(mrb);

  m->mt = kh_init(mt, mrb);
  mrb_obj_iv_set(mrb, (struct RObject*)mrb->object_class,
             name, mrb_obj_value(m));
  mrb_name_class(mrb, m, name);

  return m;
}

struct RClass*
mrb_define_module(mrb_state *mrb, const char *name)
{
  return mrb_define_module_id(mrb, mrb_intern(mrb, name));
}

static void
setup_class(mrb_state *mrb, mrb_value outer, struct RClass *c, mrb_sym id)
{
  mrb_name_class(mrb, c, id);
  mrb_const_set(mrb, outer, id, mrb_obj_value(c));
  mrb_obj_iv_set(mrb, (struct RObject*)c,
                 mrb_intern(mrb, "__outer__"), outer);
}

struct RClass*
mrb_class_outer_module(mrb_state *mrb, struct RClass *c)
{
  mrb_value outer;

  outer = mrb_obj_iv_get(mrb, (struct RObject*)c, mrb_intern(mrb, "__outer__"));
  if (mrb_nil_p(outer)) return 0;
  return mrb_class_ptr(outer);
}

struct RClass*
mrb_vm_define_module(mrb_state *mrb, mrb_value outer, mrb_sym id)
{
  struct RClass *c;
  mrb_value v;

  if (mrb_const_defined(mrb, outer, id)) {
    v = mrb_const_get(mrb, outer, id);
    c = mrb_class_ptr(v);
  }
  else {
    c = mrb_module_new(mrb);
    setup_class(mrb, outer, c, id);
  }
  return c;
}

struct RClass*
mrb_define_class_id(mrb_state *mrb, mrb_sym name, struct RClass *super)
{
  struct RClass *c = mrb_class_new(mrb, super);

  mrb_obj_iv_set(mrb, (struct RObject*)mrb->object_class,
                 name, mrb_obj_value(c));
  mrb_name_class(mrb, c, name);

  return c;
}

struct RClass*
mrb_define_class(mrb_state *mrb, const char *name, struct RClass *super)
{
  struct RClass *c;
  c = mrb_define_class_id(mrb, mrb_intern(mrb, name), super);
  return c;
}

struct RClass*
mrb_vm_define_class(mrb_state *mrb, mrb_value outer, mrb_value super, mrb_sym id)
{
  struct RClass *c = 0;

  if (mrb_const_defined(mrb, outer, id)) {
    mrb_value v = mrb_const_get(mrb, outer, id);

    c = mrb_class_ptr(v);
    if (!mrb_nil_p(super) && (c->tt != MRB_TT_CLASS || c->super != mrb_class_ptr(super))) {
      c = 0;
    }
  }
  if (!c) {
    struct RClass *s = 0;

    if (!mrb_nil_p(super)) {
      mrb_check_type(mrb, super, MRB_TT_CLASS);
      s = mrb_class_ptr(super);
    }
    if (!s) {
      s = mrb->object_class;
    }
    c = mrb_class_new(mrb, s);
    setup_class(mrb, outer, c, id);
    mrb_funcall(mrb, mrb_obj_value(s), "inherited", 1, mrb_obj_value(c));
  }

  return c;
}

/*!
 * Defines a class under the namespace of \a outer.
 * \param outer  a class which contains the new class.
 * \param id     name of the new class
 * \param super  a class from which the new class will derive.
 *               NULL means \c Object class.
 * \return the created class
 * \throw TypeError if the constant name \a name is already taken but
 *                  the constant is not a \c Class.
 * \throw NameError if the class is already defined but the class can not
 *                  be reopened because its superclass is not \a super.
 * \post top-level constant named \a name refers the returned class.
 *
 * \note if a class named \a name is already defined and its superclass is
 *       \a super, the function just returns the defined class.
 */
struct RClass *
mrb_define_class_under(mrb_state *mrb, struct RClass *outer, const char *name, struct RClass *super)
{
  struct RClass * c;
  mrb_sym id = mrb_intern(mrb, name);

  if (mrb_const_defined_at(mrb, outer, id)) {
    c = mrb_class_from_sym(mrb, outer, id);
    if (c->tt != MRB_TT_CLASS) {
        mrb_raise(mrb, E_TYPE_ERROR, "%s is not a class", mrb_sym2name(mrb, id));
    }
    if (mrb_class_real(c->super) != super) {
        mrb_name_error(mrb, id, "%s is already defined", mrb_sym2name(mrb, id));
    }
    return c;
  }
  if (!super) {
    mrb_warn("no super class for `%s::%s', Object assumed",
             mrb_obj_classname(mrb, mrb_obj_value(outer)), mrb_sym2name(mrb, id));
  }
  c = mrb_class_new(mrb, super);
  setup_class(mrb, mrb_obj_value(outer), c, id);
  mrb_const_set(mrb, mrb_obj_value(outer), id, mrb_obj_value(c));

  return c;
}

struct RClass *
mrb_define_module_under(mrb_state *mrb, struct RClass *outer, const char *name)
{
  struct RClass * c;
  mrb_sym id = mrb_intern(mrb, name);

  if (mrb_const_defined_at(mrb, outer, id)) {
    c = mrb_class_from_sym(mrb, outer, id);
    if (c->tt != MRB_TT_MODULE) {
        mrb_raise(mrb, E_TYPE_ERROR, "%s is not a module", mrb_sym2name(mrb, id));
    }
    return c;
  }
  c = mrb_module_new(mrb);
  setup_class(mrb, mrb_obj_value(outer), c, id);
  mrb_const_set(mrb, mrb_obj_value(outer), id, mrb_obj_value(c));

  return c;
}

void
mrb_define_method_raw(mrb_state *mrb, struct RClass *c, mrb_sym mid, struct RProc *p)
{
  khash_t(mt) *h = c->mt;
  khiter_t k;

  if (!h) h = c->mt = kh_init(mt, mrb);
  k = kh_put(mt, h, mid);
  kh_value(h, k) = p;
}

void
mrb_define_method_id(mrb_state *mrb, struct RClass *c, mrb_sym mid, mrb_func_t func, int aspec)
{
  struct RProc *p;

  p = mrb_proc_new_cfunc(mrb, func);
  p->target_class = c;
  mrb_define_method_raw(mrb, c, mid, p);
}

void
mrb_define_method(mrb_state *mrb, struct RClass *c, const char *name, mrb_func_t func, int aspec)
{
  mrb_define_method_id(mrb, c, mrb_intern(mrb, name), func, aspec);
}

void
mrb_define_method_vm(mrb_state *mrb, struct RClass *c, mrb_sym name, mrb_value body)
{
  khash_t(mt) *h = c->mt;
  khiter_t k;

  if (!h) h = c->mt = kh_init(mt, mrb);
  k = kh_put(mt, h, name);
  kh_value(h, k) = mrb_proc_ptr(body);
}

static mrb_value
check_type(mrb_state *mrb, mrb_value val, enum mrb_vtype t, const char *c, const char *m)
{
  mrb_value tmp;

  tmp = mrb_check_convert_type(mrb, val, t, c, m);
  if (mrb_nil_p(tmp)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected %s", c);
  }
  return tmp;
}

static mrb_value
to_str(mrb_state *mrb, mrb_value val)
{
  return check_type(mrb, val, MRB_TT_STRING, "String", "to_str");
}

static mrb_value
to_ary(mrb_state *mrb, mrb_value val)
{
  return check_type(mrb, val, MRB_TT_ARRAY, "Array", "to_ary");
}

static mrb_value
to_hash(mrb_state *mrb, mrb_value val)
{
  return check_type(mrb, val, MRB_TT_HASH, "Hash", "to_hash");
}

/*
  retrieve arguments from mrb_state.

  mrb_get_args(mrb, format, ...)
  
  returns number of arguments parsed.

  fortmat specifiers:

   o: Object [mrb_value]
   S: String [mrb_value]
   A: Array [mrb_value]
   H: Hash [mrb_value]
   s: String [char*,int]
   z: String [char*]
   a: Array [mrb_value*,int]
   f: Float [mrb_float]
   i: Integer [mrb_int]
   n: Symbol [mrb_sym]
   &: Block [mrb_value]
   *: rest argument [mrb_value*,int]
   |: optional
 */
int
mrb_get_args(mrb_state *mrb, const char *format, ...)
{
  char c;
  int i = 0;
  mrb_value *sp = mrb->stack + 1;
  va_list ap;
  int argc = mrb->ci->argc;
  int opt = 0;

  va_start(ap, format);
  if (argc < 0) {
    struct RArray *a = mrb_ary_ptr(mrb->stack[1]);

    argc = a->len;
    sp = a->ptr;
  }
  while ((c = *format++)) {
    switch (c) {
    case '|': case '*': case '&':
      break;
    default:
      if (argc <= i && !opt) {
	mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
      }
    }

    switch (c) {
    case 'o':
      {
        mrb_value *p;

        p = va_arg(ap, mrb_value*);
	if (i < argc) {
	  *p = *sp++;
	  i++;
	}
      }
      break;
    case 'S':
      {
        mrb_value *p;

        p = va_arg(ap, mrb_value*);
	if (i < argc) {
	  *p = to_str(mrb, *sp++);
	  i++;
	}
      }
      break;
    case 'A':
      {
        mrb_value *p;

        p = va_arg(ap, mrb_value*);
	if (i < argc) {
	  *p = to_ary(mrb, *sp++);
	  i++;
	}
      }
      break;
    case 'H':
      {
        mrb_value *p;

        p = va_arg(ap, mrb_value*);
	if (i < argc) {
	  *p = to_hash(mrb, *sp++);
	  i++;
	}
      }
      break;
    case 's':
      {
	mrb_value ss;
        struct RString *s;
        char **ps = 0;
        int *pl = 0;

	ps = va_arg(ap, char**);
	pl = va_arg(ap, int*);
	if (i < argc) {
	  ss = to_str(mrb, *sp++);
	  s = mrb_str_ptr(ss);
	  *ps = s->ptr;
	  *pl = s->len;
	  i++;
	}
      }
      break;
    case 'z':
      {
	mrb_value ss;
        struct RString *s;
        char **ps;

	ps = va_arg(ap, char**);
	if (i < argc) {
	  ss = to_str(mrb, *sp++);
	  s = mrb_str_ptr(ss);
	  if (strlen(s->ptr) != s->len) {
	    mrb_raise(mrb, E_ARGUMENT_ERROR, "String contains NUL");
	  }
	  *ps = s->ptr;
	  i++;
	}
      }
      break;
    case 'a':
      {
	mrb_value aa;
        struct RArray *a;
        mrb_value **pb;
        int *pl;

	pb = va_arg(ap, mrb_value**);
	pl = va_arg(ap, int*);
	if (i < argc) {
	  aa = to_ary(mrb, *sp++);
	  a = mrb_ary_ptr(aa);
	  *pb = a->ptr;
	  *pl = a->len;
	  i++;
	}
      }
      break;
    case 'f':
      {
        mrb_float *p;

        p = va_arg(ap, mrb_float*);
	if (i < argc) {
	  switch (sp->tt) {
	  case MRB_TT_FLOAT:
	    *p = mrb_float(*sp);
	    break;
	  case MRB_TT_FIXNUM:
	    *p = (mrb_float)mrb_fixnum(*sp);
	    break;
	  case MRB_TT_FALSE:
	    *p = 0.0;
	    break;
	  default:
	    {
	      mrb_value tmp;

	      tmp = mrb_convert_type(mrb, *sp, MRB_TT_FLOAT, "Float", "to_f");
	      *p = mrb_float(tmp);
	    }
	    break;
	  }
	  sp++;
	  i++;
	}
      }
      break;
    case 'i':
      {
        mrb_int *p;

        p = va_arg(ap, mrb_int*);
	if (i < argc) {
	  switch (sp->tt) {
	  case MRB_TT_FIXNUM:
	    *p = mrb_fixnum(*sp);
	    break;
	  case MRB_TT_FLOAT:
	    {
	      mrb_float f = mrb_float(*sp);

	      if (!FIXABLE(f)) {
		mrb_raise(mrb, E_RANGE_ERROR, "float too big for int");
	      }
	      *p = (mrb_int)f;
	    }
	    break;
	  case MRB_TT_FALSE:
	    *p = 0;
	    break;
	  default:
	    {
	      mrb_value tmp;

	      tmp = mrb_convert_type(mrb, *sp, MRB_TT_FIXNUM, "Integer", "to_int");
	      *p = mrb_fixnum(tmp);
	    }
	    break;
	  }
	  sp++;
	  i++;
	}
      }
      break;
    case 'n':
      {
	mrb_sym *symp;

	symp = va_arg(ap, mrb_sym*);
	if (i < argc) {
	  mrb_value ss;

	  ss = *sp++;
	  if (mrb_type(ss) == MRB_TT_SYMBOL) {
	    *symp = mrb_symbol(ss);
	  }
	  else {
	    *symp = mrb_intern_str(mrb, to_str(mrb, ss));
	  }
	  i++;
	}
      }
      break;

    case '&':
      {
        mrb_value *p, *bp = mrb->stack + 1;

        p = va_arg(ap, mrb_value*);
        if (mrb->ci->argc > 0) {
          bp += mrb->ci->argc;
        }
        *p = *bp;
      }
      break;
    case '|':
      opt = 1;
      break;

    case '*':
      {
        mrb_value **var;
	int *pl;

        var = va_arg(ap, mrb_value**);
        pl = va_arg(ap, int*);
        if (argc > i) {
          *pl = argc-i;
          if (*pl > 0) {
	    *var = sp;
            i = argc;
          }
	  i = argc;
	  sp += *pl;
        }
        else {
          *pl = 0;
          *var = NULL;
        }
      }
      break;
    }
  }
  if (!c && argc > i) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }
  va_end(ap);
  return i;
}

static struct RClass*
boot_defclass(mrb_state *mrb, struct RClass *super)
{
  struct RClass *c;

  c = (struct RClass*)mrb_obj_alloc(mrb, MRB_TT_CLASS, mrb->class_class);
  c->super = super ? super : mrb->object_class;
  mrb_field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)super);
  c->mt = kh_init(mt, mrb);
  return c;
}

void
mrb_include_module(mrb_state *mrb, struct RClass *c, struct RClass *m)
{
  struct RClass *ic;

  ic = (struct RClass*)mrb_obj_alloc(mrb, MRB_TT_ICLASS, mrb->class_class);
  ic->c = m;
  ic->mt = m->mt;
  ic->iv = m->iv;
  ic->super = c->super;
  c->super = ic;
  mrb_field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)ic);
}

static mrb_value
mrb_mod_include(mrb_state *mrb, mrb_value klass)
{
  mrb_value mod;

  mrb_get_args(mrb, "o", &mod);
  mrb_check_type(mrb, mod, MRB_TT_MODULE);
  mrb_include_module(mrb, mrb_class_ptr(klass), mrb_class_ptr(mod));
  return mod;
}

static struct RClass *
mrb_singleton_class_ptr(mrb_state *mrb, struct RClass *c)
{
  struct RClass *sc;

  if (c->tt == MRB_TT_SCLASS) {
    return c;
  }
  sc = (struct RClass*)mrb_obj_alloc(mrb, MRB_TT_SCLASS, mrb->class_class);
  sc->mt = 0;
  sc->super = c;
  mrb_field_write_barrier(mrb, (struct RBasic*)sc, (struct RBasic*)c);

  return sc;
}

mrb_value
mrb_singleton_class(mrb_state *mrb, mrb_value v)
{
  struct RBasic *obj;

  switch (mrb_type(v)) {
  case MRB_TT_FALSE:
  case MRB_TT_TRUE:
  case MRB_TT_SYMBOL:
  case MRB_TT_FIXNUM:
  case MRB_TT_FLOAT:
    return mrb_nil_value();    /* should raise TypeError */
  default:
    break;
  }
  obj = mrb_object(v);
  obj->c = mrb_singleton_class_ptr(mrb, obj->c);
  return mrb_obj_value(obj->c);
}

void
mrb_define_singleton_method(mrb_state *mrb, struct RObject *o, const char *name, mrb_func_t func, int aspec)
{
  o->c = mrb_singleton_class_ptr(mrb, o->c);
  mrb_define_method_id(mrb, o->c, mrb_intern(mrb, name), func, aspec);
}

void
mrb_define_class_method(mrb_state *mrb, struct RClass *c, const char *name, mrb_func_t func, int aspec)
{
  mrb_define_singleton_method(mrb, (struct RObject*)c, name, func, aspec);
}

void
mrb_define_module_function(mrb_state *mrb, struct RClass *c, const char *name, mrb_func_t func, int aspec)
{
  mrb_define_class_method(mrb, c, name, func, aspec);
  mrb_define_method(mrb, c, name, func, aspec);
}

struct RProc*
mrb_method_search_vm(mrb_state *mrb, struct RClass **cp, mrb_sym mid)
{
  khiter_t k;
  struct RProc *m;
  struct RClass *c = *cp;

  while (c) {
    khash_t(mt) *h = c->mt;

    if (h) {
      k = kh_get(mt, h, mid);
      if (k != kh_end(h)) {
        m = kh_value(h, k);
        if (!m) break;
        *cp = c;
        return m;
      }
    }
    c = c->super;
  }
  return 0;                  /* no method */
}

struct RProc*
mrb_method_search(mrb_state *mrb, struct RClass* c, mrb_sym mid)
{
  struct RProc *m;

  m = mrb_method_search_vm(mrb, &c, mid);
  if (!m) {
    mrb_raise(mrb, E_NOMETHOD_ERROR, "no method named %s\n", mrb_sym2name(mrb, mid));
  }
  return m;
}

mrb_value
mrb_funcall(mrb_state *mrb, mrb_value self, const char *name, int argc,...)
{
  mrb_value args[16];
  va_list ap;
  int i;

  if (argc == 0) {
    for (i=0; i<5; i++) {
      args[i] = mrb_nil_value();
    }
  }
  else {
    va_start(ap, argc);
    // assert(argc < 16);
    for (i=0; i<argc; i++) {
      args[i] = va_arg(ap, mrb_value);
    }
    va_end(ap);
  }
  return mrb_funcall_argv(mrb, self, name, argc, args);
}


void
mrb_obj_call_init(mrb_state *mrb, mrb_value obj, int argc, mrb_value *argv)
{
  mrb_funcall_argv(mrb, obj, "initialize", argc, argv);
}

/*
 *  call-seq:
 *     class.new(args, ...)    ->  obj
 *
 *  Calls <code>allocate</code> to create a new object of
 *  <i>class</i>'s class, then invokes that object's
 *  <code>initialize</code> method, passing it <i>args</i>.
 *  This is the method that ends up getting called whenever
 *  an object is constructed using .new.
 *
 */
mrb_value
mrb_class_new_instance(mrb_state *mrb, int argc, mrb_value *argv, struct RClass * klass)
{
  mrb_value obj;
  struct RClass * c = (struct RClass*)mrb_obj_alloc(mrb, klass->tt, klass);
  c->super = klass;
  obj = mrb_obj_value(c);
  mrb_obj_call_init(mrb, obj, argc, argv);
  return obj;
}

mrb_value
mrb_class_new_instance_m(mrb_state *mrb, mrb_value klass)
{
  mrb_value *argv;
  mrb_value blk;
  struct RClass *k = mrb_class_ptr(klass);
  struct RClass *c;
  int argc;
  mrb_value obj;

  mrb_get_args(mrb, "*&", &argv, &argc, &blk);
  c = (struct RClass*)mrb_obj_alloc(mrb, k->tt, k);
  c->super = k;
  obj = mrb_obj_value(c);
  mrb_funcall_with_block(mrb, obj, "initialize", argc, argv, blk);

  return obj;
}

mrb_value
mrb_instance_new(mrb_state *mrb, mrb_value cv)
{
  struct RClass *c = mrb_class_ptr(cv);
  struct RObject *o;
  enum mrb_vtype ttype = MRB_INSTANCE_TT(c);
  mrb_value obj, blk;
  mrb_value *argv;
  int argc;

  if (ttype == 0) ttype = MRB_TT_OBJECT;
  o = (struct RObject*)mrb_obj_alloc(mrb, ttype, c);
  obj = mrb_obj_value(o);
  mrb_get_args(mrb, "*&", &argv, &argc, &blk);
  mrb_funcall_with_block(mrb, obj, "initialize", argc, argv, blk);

  return obj;
}

mrb_value
mrb_class_new_class(mrb_state *mrb, mrb_value cv)
{
  mrb_value super;
  struct RClass *new_class;

  if (mrb_get_args(mrb, "|o", &super) == 0) {
    super = mrb_obj_value(mrb->object_class);
  }
  new_class = mrb_class_new(mrb, mrb_class_ptr(super));
  return mrb_obj_value(new_class);
}

mrb_value
mrb_class_superclass(mrb_state *mrb, mrb_value klass)
{
  struct RClass *c;
  mrb_value superclass;

  c = mrb_class_ptr(klass);
  if (c->super)
    superclass = mrb_obj_value(mrb_class_real(c->super));
  else
    superclass = mrb_nil_value();

  return superclass;
}

static mrb_value
mrb_bob_init(mrb_state *mrb, mrb_value cv)
{
  return mrb_nil_value();
}

static mrb_value
mrb_bob_not(mrb_state *mrb, mrb_value cv)
{
  if (mrb_test(cv))
    return mrb_false_value();
  return mrb_true_value();
}

/* 15.3.1.3.30 */
/*
 *  call-seq:
 *     obj.method_missing(symbol [, *args] )   -> result
 *
 *  Invoked by Ruby when <i>obj</i> is sent a message it cannot handle.
 *  <i>symbol</i> is the symbol for the method called, and <i>args</i>
 *  are any arguments that were passed to it. By default, the interpreter
 *  raises an error when this method is called. However, it is possible
 *  to override the method to provide more dynamic behavior.
 *  If it is decided that a particular method should not be handled, then
 *  <i>super</i> should be called, so that ancestors can pick up the
 *  missing method.
 *  The example below creates
 *  a class <code>Roman</code>, which responds to methods with names
 *  consisting of roman numerals, returning the corresponding integer
 *  values.
 *
 *     class Roman
 *       def romanToInt(str)
 *         # ...
 *       end
 *       def method_missing(methId)
 *         str = methId.id2name
 *         romanToInt(str)
 *       end
 *     end
 *
 *     r = Roman.new
 *     r.iv      #=> 4
 *     r.xxiii   #=> 23
 *     r.mm      #=> 2000
 */
static mrb_value
mrb_bob_missing(mrb_state *mrb, mrb_value mod)
{
  mrb_value name, *a;
  int alen;

  mrb_get_args(mrb, "o*", &name, &a, &alen);
  if (!SYMBOL_P(name)) {
    mrb_raise(mrb, E_TYPE_ERROR, "name should be a symbol");
  }
  mrb_raise(mrb, E_NOMETHOD_ERROR, "no method named %s", mrb_sym2name(mrb, mrb_symbol(name)));
  /* not reached */
  return mrb_nil_value();
}

int
mrb_obj_respond_to(struct RClass* c, mrb_sym mid)
{
  khiter_t k;

  while (c) {
    khash_t(mt) *h = c->mt;

    if (h) {
      k = kh_get(mt, h, mid);
      if (k != kh_end(h))
        return 1; /* exist method */
    }
    c = c->super;
  }
  return 0;  /* no method */
}

int
mrb_respond_to(mrb_state *mrb, mrb_value obj, mrb_sym mid)
{
  return mrb_obj_respond_to(mrb_class(mrb, obj), mid);
}

mrb_value
mrb_class_path(mrb_state *mrb, struct RClass *c)
{
  mrb_value path;
  const char *name;
  int len;

  path = mrb_obj_iv_get(mrb, (struct RObject*)c, mrb_intern(mrb, "__classpath__"));
  if (mrb_nil_p(path)) {
    struct RClass *outer = mrb_class_outer_module(mrb, c);
    mrb_sym sym = class_sym(mrb, c, outer);
    if (outer && outer != mrb->object_class) {
      mrb_value base = mrb_class_path(mrb, outer);
      path = mrb_str_plus(mrb, base, mrb_str_new(mrb, "::", 2));
      name = mrb_sym2name_len(mrb, sym, &len);
      mrb_str_concat(mrb, path, mrb_str_new(mrb, name, len));
    }
    else if (sym == 0) {
      return mrb_nil_value();
    }
    else {
      name = mrb_sym2name_len(mrb, sym, &len);
      path = mrb_str_new(mrb, name, len);
    }
    mrb_obj_iv_set(mrb, (struct RObject*)c, mrb_intern(mrb, "__classpath__"), path);
  }
  return path;
}

struct RClass *
mrb_class_real(struct RClass* cl)
{
  while ((cl->tt == MRB_TT_SCLASS) || (cl->tt == MRB_TT_ICLASS)) {
    cl = cl->super;
  }
  return cl;
}

const char*
mrb_class_name(mrb_state *mrb, struct RClass* c)
{
  mrb_value path = mrb_class_path(mrb, c);
  if (mrb_nil_p(path)) return 0;
  return mrb_str_ptr(path)->ptr;
}

const char*
mrb_obj_classname(mrb_state *mrb, mrb_value obj)
{
  return mrb_class_name(mrb, mrb_class(mrb, obj));
}

/*!
 * Ensures a class can be derived from super.
 *
 * \param super a reference to an object.
 * \exception TypeError if \a super is not a Class or \a super is a singleton class.
 */
void
mrb_check_inheritable(mrb_state *mrb, struct RClass *super)
{
  if (super->tt != MRB_TT_CLASS) {
    mrb_raise(mrb, E_TYPE_ERROR, "superclass must be a Class (%s given)",
           mrb_obj_classname(mrb, mrb_obj_value(super)));
  }
  if (super->tt == MRB_TT_SCLASS) {
    mrb_raise(mrb, E_TYPE_ERROR, "can't make subclass of singleton class");
  }
  if (super == mrb->class_class) {
    mrb_raise(mrb, E_TYPE_ERROR, "can't make subclass of Class");
  }
}

/*!
 * Creates a new class.
 * \param super     a class from which the new class derives.
 * \exception TypeError \a super is not inheritable.
 * \exception TypeError \a super is the Class class.
 */
struct RClass *
mrb_class_new(mrb_state *mrb, struct RClass *super)
{
  struct RClass *c;

  if (super) {
    mrb_check_inheritable(mrb, super);
  }
  c = boot_defclass(mrb, super);
  if (super){
    MRB_SET_INSTANCE_TT(c, MRB_INSTANCE_TT(super));
  }
  make_metaclass(mrb, c);

  return c;
}

/*!
 * Creates a new module.
 */
struct RClass *
mrb_module_new(mrb_state *mrb)
{
  struct RClass *m = (struct RClass*)mrb_obj_alloc(mrb, MRB_TT_MODULE, mrb->module_class);
  m->mt = kh_init(mt, mrb);

  return m;
}

/*
 *  call-seq:
 *     obj.class    => class
 *
 *  Returns the class of <i>obj</i>, now preferred over
 *  <code>Object#type</code>, as an object's type in Ruby is only
 *  loosely tied to that object's class. This method must always be
 *  called with an explicit receiver, as <code>class</code> is also a
 *  reserved word in Ruby.
 *
 *     1.class      #=> Fixnum
 *     self.class   #=> Object
 */

struct RClass*
mrb_obj_class(mrb_state *mrb, mrb_value obj)
{
    return mrb_class_real(mrb_class(mrb, obj));
}

void
mrb_alias_method(mrb_state *mrb, struct RClass *c, mrb_sym a, mrb_sym b)
{
  struct RProc *m = mrb_method_search(mrb, c, b);

  mrb_define_method_vm(mrb, c, a, mrb_obj_value(m));
}

/*!
 * Defines an alias of a method.
 * \param klass  the class which the original method belongs to
 * \param name1  a new name for the method
 * \param name2  the original name of the method
 */
void
mrb_define_alias(mrb_state *mrb, struct RClass *klass, const char *name1, const char *name2)
{
  mrb_alias_method(mrb, klass, mrb_intern(mrb, name1), mrb_intern(mrb, name2));
}

/*
 * call-seq:
 *   mod.to_s   -> string
 *
 * Return a string representing this module or class. For basic
 * classes and modules, this is the name. For singletons, we
 * show information on the thing we're attached to as well.
 */

static mrb_value
mrb_mod_to_s(mrb_state *mrb, mrb_value klass)
{
  if (mrb_type(klass) == MRB_TT_SCLASS) {
    mrb_value s = mrb_str_new(mrb, "#<", 2);
    mrb_value v = mrb_iv_get(mrb, klass, mrb_intern(mrb, "__attached__"));

    mrb_str_cat2(mrb, s, "Class:");
    switch (mrb_type(v)) {
      case MRB_TT_CLASS:
      case MRB_TT_MODULE:
        mrb_str_append(mrb, s, mrb_inspect(mrb, v));
        break;
      default:
        mrb_str_append(mrb, s, mrb_any_to_s(mrb, v));
        break;
    }
    mrb_str_cat2(mrb, s, ">");

    return s;
  }
  else {
    struct RClass *c = mrb_class_ptr(klass);
    const char *cn = mrb_class_name(mrb, c);

    if (!cn) {
      char buf[256];
      int n = 0;

      switch (mrb_type(klass)) {
        case MRB_TT_CLASS:
          n = snprintf(buf, sizeof(buf), "#<Class:%p>", c);
          break;

        case MRB_TT_MODULE:
          n = snprintf(buf, sizeof(buf), "#<Module:%p>", c);
          break;

        default:
          break;
      }
      return mrb_str_dup(mrb, mrb_str_new(mrb, buf, n));
    }
    else {
      return mrb_str_dup(mrb, mrb_str_new_cstr(mrb, cn));
    }
  }
}

mrb_value
mrb_mod_alias(mrb_state *mrb, mrb_value mod)
{
  struct RClass *c = mrb_class_ptr(mod);
  mrb_value new_value, old_value;

  mrb_get_args(mrb, "oo", &new_value, &old_value);
  mrb_alias_method(mrb, c, mrb_symbol(new_value), mrb_symbol(old_value));
  return mrb_nil_value();
}


static void
undef_method(mrb_state *mrb, struct RClass *c, mrb_sym a)
{
  mrb_value m;

  m.tt = MRB_TT_PROC;
  m.value.p = 0;
  mrb_define_method_vm(mrb, c, a, m);
}

void
mrb_undef_method(mrb_state *mrb, struct RClass *c, const char *name)
{
  undef_method(mrb, c, mrb_intern(mrb, name));
}

mrb_value
mrb_mod_undef(mrb_state *mrb, mrb_value mod)
{
  struct RClass *c = mrb_class_ptr(mod);
  int argc;
  mrb_value *argv;

  mrb_get_args(mrb, "*", &argv, &argc);
  while (argc--) {
    undef_method(mrb, c, mrb_symbol(*argv));
    argv++;
  }
  return mrb_nil_value();
}

static mrb_value
mod_define_method(mrb_state *mrb, mrb_value self)
{
  struct RClass *c = mrb_class_ptr(self);
  mrb_sym mid;
  mrb_value blk;

  mrb_get_args(mrb, "n&", &mid, &blk);
  if (mrb_nil_p(blk)) {
    /* raise */
  }
  mrb_define_method_raw(mrb, c, mid, mrb_proc_ptr(blk));
  return blk;
}

static mrb_sym
mrb_sym_value(mrb_state *mrb, mrb_value val)
{
  if(val.tt == MRB_TT_STRING) {
    return mrb_intern_str(mrb, val);
  }
  else if(val.tt != MRB_TT_SYMBOL) {
    mrb_value obj = mrb_funcall(mrb, val, "inspect", 0);
    mrb_raise(mrb, E_TYPE_ERROR, "%s is not a symbol",
         mrb_string_value_ptr(mrb, obj));
  }
  return mrb_symbol(val);
}

mrb_value
mrb_mod_const_defined(mrb_state *mrb, mrb_value mod)
{
  mrb_value sym;
  mrb_get_args(mrb, "o", &sym);
  if(mrb_const_defined(mrb, mod, mrb_sym_value(mrb, sym))) {
    return mrb_true_value();
  }
  return mrb_false_value();
}

mrb_value
mrb_mod_const_get(mrb_state *mrb, mrb_value mod)
{
  mrb_value sym;
  mrb_get_args(mrb, "o", &sym);
  return mrb_const_get(mrb, mod, mrb_sym_value(mrb, sym));
}

mrb_value
mrb_mod_const_set(mrb_state *mrb, mrb_value mod)
{
  mrb_value sym, value;
  mrb_get_args(mrb, "oo", &sym, &value);
  mrb_const_set(mrb, mod, mrb_sym_value(mrb, sym), value);
  return value;
}


static mrb_value
mrb_mod_eqq(mrb_state *mrb, mrb_value mod)
{
  mrb_value obj;

  mrb_get_args(mrb, "o", &obj);
  if (!mrb_obj_is_kind_of(mrb, obj, mrb_class_ptr(mod)))
    return mrb_false_value();
  return mrb_true_value();
}

void
mrb_init_class(mrb_state *mrb)
{
  struct RClass *bob;           /* BasicObject */
  struct RClass *obj;           /* Object */
  struct RClass *mod;           /* Module */
  struct RClass *cls;           /* Class */
  //struct RClass *krn;    /* Kernel */

  /* boot class hierarchy */
  bob = boot_defclass(mrb, 0);
  obj = boot_defclass(mrb, bob); mrb->object_class = obj;
  mod = boot_defclass(mrb, obj); mrb->module_class = mod;/* obj -> mod */
  cls = boot_defclass(mrb, mod); mrb->class_class = cls; /* obj -> cls */
  /* fix-up loose ends */
  bob->c = obj->c = mod->c = cls->c = cls;
  make_metaclass(mrb, bob);
  make_metaclass(mrb, obj);
  make_metaclass(mrb, mod);
  make_metaclass(mrb, cls);

  /* name basic classes */
  mrb_define_const(mrb, obj, "BasicObject", mrb_obj_value(bob));
  mrb_define_const(mrb, obj, "Object", mrb_obj_value(obj));
  mrb_define_const(mrb, obj, "Module", mrb_obj_value(mod));
  mrb_define_const(mrb, obj, "Class", mrb_obj_value(cls));

  /* name each classes */
  mrb_name_class(mrb, bob, mrb_intern(mrb, "BasicObject"));
  mrb_name_class(mrb, obj, mrb_intern(mrb, "Object"));
  mrb_name_class(mrb, mod, mrb_intern(mrb, "Module"));
  mrb_name_class(mrb, cls, mrb_intern(mrb, "Class"));

  mrb_undef_method(mrb, mod, "new");
  MRB_SET_INSTANCE_TT(cls, MRB_TT_CLASS);
  mrb_define_method(mrb, bob, "initialize", mrb_bob_init, ARGS_NONE());
  mrb_define_method(mrb, bob, "!", mrb_bob_not, ARGS_NONE());
  mrb_define_method(mrb, bob, "method_missing", mrb_bob_missing, ARGS_ANY());        /* 15.3.1.3.30 */
  mrb_define_class_method(mrb, cls, "new", mrb_class_new_class, ARGS_ANY());
  mrb_define_method(mrb, cls, "superclass", mrb_class_superclass, ARGS_NONE());      /* 15.2.3.3.4 */
  mrb_define_method(mrb, cls, "new", mrb_instance_new, ARGS_ANY());                  /* 15.2.3.3.3 */
  mrb_define_method(mrb, cls, "inherited", mrb_bob_init, ARGS_REQ(1));
  mrb_define_method(mrb, mod, "include", mrb_mod_include, ARGS_REQ(1));              /* 15.2.2.4.27 */

  mrb_define_method(mrb, mod, "to_s", mrb_mod_to_s, ARGS_NONE());
  mrb_define_method(mrb, mod, "alias_method", mrb_mod_alias, ARGS_ANY());            /* 15.2.2.4.8 */
  mrb_define_method(mrb, mod, "undef_method", mrb_mod_undef, ARGS_ANY());            /* 15.2.2.4.41 */
  mrb_define_method(mrb, mod, "const_defined?", mrb_mod_const_defined, ARGS_REQ(1)); /* 15.2.2.4.20 */
  mrb_define_method(mrb, mod, "const_get", mrb_mod_const_get, ARGS_REQ(1));          /* 15.2.2.4.21 */
  mrb_define_method(mrb, mod, "const_set", mrb_mod_const_set, ARGS_REQ(2));          /* 15.2.2.4.23 */
  mrb_define_method(mrb, mod, "define_method", mod_define_method, ARGS_REQ(1));

  mrb_define_method(mrb, mod, "===", mrb_mod_eqq, ARGS_REQ(1));
}
