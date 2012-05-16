/*
** kernel.c - Kernel module
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/string.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mruby/proc.h"

#include "mruby/range.h"
#include "mruby/array.h"
#include "mruby/hash.h"
#include "mruby/class.h"
#include "mruby/struct.h"
#include "mruby/variable.h"
#include "mruby/khash.h"
#include "error.h"
#include "method.h"

#ifdef INCLUDE_REGEXP
#include "re.h"
#include "regint.h"
#endif

KHASH_MAP_INIT_INT(mt, struct RProc*);
KHASH_MAP_INIT_INT(iv, mrb_value);

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

struct obj_ivar_tag {
  mrb_value obj;
  int (*func)(mrb_sym key, mrb_value val, void * arg);
  void * arg;
};

static mrb_value
inspect_obj(mrb_state *mrb, mrb_value obj, mrb_value str, int recur)
{
  if (recur) {
    mrb_str_cat2(mrb, str, " ...");
  }
  else {
    khiter_t k;
    kh_iv_t *h = RCLASS_IV_TBL(obj);

    if (h) {
      for (k = kh_begin(h); k != kh_end(h); k++) {
        if (kh_exist(h, k)){
          mrb_sym id = kh_key(h, k);
          mrb_value value = kh_value(h, k);

          /* need not to show internal data */
          if (RSTRING_PTR(str)[0] == '-') { /* first element */
            RSTRING_PTR(str)[0] = '#';
            mrb_str_cat2(mrb, str, " ");
          }
          else {
            mrb_str_cat2(mrb, str, ", ");
          }
          mrb_str_cat2(mrb, str, mrb_sym2name(mrb, id));
          mrb_str_cat2(mrb, str, "=");
          mrb_str_append(mrb, str, mrb_inspect(mrb, value));
        }
      }
    }
  }
  mrb_str_cat2(mrb, str, ">");
  RSTRING_PTR(str)[0] = '#';

  return str;
}

int
mrb_obj_basic_to_s_p(mrb_state *mrb, mrb_value obj)
{
    //const mrb_method_entry_t *me = mrb_method_entry(CLASS_OF(obj), mrb_intern("to_s"));
    //if (me && me->def && me->def->type == VM_METHOD_TYPE_CFUNC &&
    //me->def->body.cfunc.func == mrb_any_to_s)
    struct RProc *me = mrb_method_search(mrb, mrb_class(mrb, obj), mrb_intern(mrb, "to_s"));
    if (me && MRB_PROC_CFUNC_P(me) && (me->body.func == mrb_any_to_s))
      return 1;
    return 0;
}

/* 15.3.1.3.17 */
/*
 *  call-seq:
 *     obj.inspect   -> string
 *
 *  Returns a string containing a human-readable representation of
 *  <i>obj</i>. If not overridden and no instance variables, uses the
 *  <code>to_s</code> method to generate the string.
 *  <i>obj</i>.  If not overridden, uses the <code>to_s</code> method to
 *  generate the string.
 *
 *     [ 1, 2, 3..4, 'five' ].inspect   #=> "[1, 2, 3..4, \"five\"]"
 *     Time.new.inspect                 #=> "2008-03-08 19:43:39 +0900"
 */
mrb_value
mrb_obj_inspect(mrb_state *mrb, mrb_value obj)
{
  if ((mrb_type(obj) == MRB_TT_OBJECT) && mrb_obj_basic_to_s_p(mrb, obj)) {
    long len = ROBJECT_NUMIV(obj);

    if (len > 0) {
      mrb_value str;
      const char *c = mrb_obj_classname(mrb, obj);

      str = mrb_sprintf(mrb, "-<%s:%p", c, (void*)&obj);
      return inspect_obj(mrb, obj, str, 0);
    }
    return mrb_any_to_s(mrb, obj);
  }
  else if (mrb_nil_p(obj)) {
    return mrb_str_new_cstr(mrb, "nil");
  }
  return mrb_funcall(mrb, obj, "to_s", 0, 0);
}

/* 15.3.1.3.1  */
/* 15.3.1.3.10 */
/* 15.3.1.3.11 */
/*
 *  call-seq:
 *     obj == other        -> true or false
 *     obj.equal?(other)   -> true or false
 *     obj.eql?(other)     -> true or false
 *
 *  Equality---At the <code>Object</code> level, <code>==</code> returns
 *  <code>true</code> only if <i>obj</i> and <i>other</i> are the
 *  same object. Typically, this method is overridden in descendant
 *  classes to provide class-specific meaning.
 *
 *  Unlike <code>==</code>, the <code>equal?</code> method should never be
 *  overridden by subclasses: it is used to determine object identity
 *  (that is, <code>a.equal?(b)</code> iff <code>a</code> is the same
 *  object as <code>b</code>).
 *
 *  The <code>eql?</code> method returns <code>true</code> if
 *  <i>obj</i> and <i>anObject</i> have the same value. Used by
 *  <code>Hash</code> to test members for equality.  For objects of
 *  class <code>Object</code>, <code>eql?</code> is synonymous with
 *  <code>==</code>. Subclasses normally continue this tradition, but
 *  there are exceptions. <code>Numeric</code> types, for example,
 *  perform type conversion across <code>==</code>, but not across
 *  <code>eql?</code>, so:
 *
 *     1 == 1.0     #=> true
 *     1.eql? 1.0   #=> false
 */
static mrb_value
mrb_obj_equal_m(mrb_state *mrb, mrb_value self)
{
  mrb_value arg;

  mrb_get_args(mrb, "o", &arg);
  if (mrb_obj_equal(mrb, self, arg)) {
    return mrb_true_value();
  }
  else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_obj_not_equal_m(mrb_state *mrb, mrb_value self)
{
  mrb_value arg;

  mrb_get_args(mrb, "o", &arg);
  if (mrb_obj_equal(mrb, self, arg)) {
    return mrb_false_value();
  }
  else {
    return mrb_true_value();
  }
}

/* 15.3.1.3.2  */
/*
 *  call-seq:
 *     obj === other   -> true or false
 *
 *  Case Equality---For class <code>Object</code>, effectively the same
 *  as calling  <code>#==</code>, but typically overridden by descendants
 *  to provide meaningful semantics in <code>case</code> statements.
 */
static mrb_value
mrb_equal_m(mrb_state *mrb, mrb_value self)
{
  mrb_value arg;

  mrb_get_args(mrb, "o", &arg);
  if (mrb_equal(mrb, self, arg)){
    return mrb_true_value();
  }
  else {
    return mrb_false_value();
  }
}

/* 15.3.1.3.3  */
/* 15.3.1.3.33 */
/*
 *  call-seq:
 *     obj.hash    -> fixnum
 *
 *  Generates a <code>Fixnum</code> hash value for this object. This
 *  function must have the property that <code>a.eql?(b)</code> implies
 *  <code>a.hash == b.hash</code>. The hash value is used by class
 *  <code>Hash</code>. Any hash value that exceeds the capacity of a
 *  <code>Fixnum</code> will be truncated before being used.
 */
static mrb_value
mrb_obj_id_m(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(mrb_obj_id(self));
}

mrb_value
send_internal(int argc, mrb_value *argv, mrb_value recv, enum call_type ctype)
{
    return mrb_nil_value(); /* dummy */
}

mrb_value
mrb_f_send(int argc, mrb_value *argv, mrb_value recv)
{
    return send_internal(argc, argv, recv, CALL_FCALL);
}

/* 15.3.1.3.4  */
/* 15.3.1.3.44 */
/*
 *  call-seq:
 *     obj.send(symbol [, args...])        -> obj
 *     obj.__send__(symbol [, args...])      -> obj
 *
 *  Invokes the method identified by _symbol_, passing it any
 *  arguments specified. You can use <code>__send__</code> if the name
 *  +send+ clashes with an existing method in _obj_.
 *
 *     class Klass
 *       def hello(*args)
 *         "Hello " + args.join(' ')
 *       end
 *     end
 *     k = Klass.new
 *     k.send :hello, "gentle", "readers"   #=> "Hello gentle readers"
 */
static mrb_value
mrb_f_send_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_f_send(argc, argv, self);
}

/* 15.3.1.2.1  */
/* 15.3.1.3.5  */
/*
 *  call-seq:
 *     spawn([env,] command... [,options])     -> pid
 *     Process.spawn([env,] command... [,options])     -> pid
 *
 *  spawn executes specified command and return its pid.
 */
static mrb_value
mrb_f_spawn_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return (mrb_f_send(argc, argv, self));
}

static mrb_value
mrb_f_block_given_p(void)
{
  return mrb_false_value(); /* dummy */
}

/* 15.3.1.2.2  */
/* 15.3.1.2.5  */
/* 15.3.1.3.6  */
/* 15.3.1.3.25 */
/*
 *  call-seq:
 *     block_given?   -> true or false
 *     iterator?      -> true or false
 *
 *  Returns <code>true</code> if <code>yield</code> would execute a
 *  block in the current context. The <code>iterator?</code> form
 *  is mildly deprecated.
 *
 *     def try
 *       if block_given?
 *         yield
 *       else
 *         "no block"
 *       end
 *     end
 *     try                  #=> "no block"
 *     try { "hello" }      #=> "hello"
 *     try do "hello" end   #=> "hello"
 */
static mrb_value
mrb_f_block_given_p_m(mrb_state *mrb, mrb_value self)
{
  return mrb_f_block_given_p();
}

/* 15.3.1.3.7  */
/*
 *  call-seq:
 *     obj.class    -> class
 *
 *  Returns the class of <i>obj</i>. This method must always be
 *  called with an explicit receiver, as <code>class</code> is also a
 *  reserved word in Ruby.
 *
 *     1.class      #=> Fixnum
 *     self.class   #=> Object
 */
static mrb_value
mrb_obj_class_m(mrb_state *mrb, mrb_value self)
{
  return mrb_obj_value(mrb_obj_class(mrb, self));
}

struct RClass*
mrb_singleton_class_clone(mrb_state *mrb, mrb_value obj)
{
  struct RClass *klass = RBASIC(obj)->c;

  //if (!FL_TEST(klass, FL_SINGLETON))
      //return klass;
  if (klass->tt != MRB_TT_SCLASS)
    return klass;
  else {
      //struct clone_method_data data;
      /* copy singleton(unnamed) class */
      //VALUE clone = class_alloc(RBASIC(klass)->flags, 0);
    struct RClass *clone = mrb_obj_alloc(mrb, klass->tt, mrb->class_class);
    //clone->super = objklass->super;

      if ((mrb_type(obj) == MRB_TT_CLASS) ||
          (mrb_type(obj) == MRB_TT_SCLASS)) { /* BUILTIN_TYPE(obj) == T_CLASS */
          clone->c = clone;
      }
      else {
          clone->c = mrb_singleton_class_clone(mrb, mrb_obj_value(klass));
      }

      clone->super = klass->super;
      if (klass->iv) {
          clone->iv = klass->iv;
      }
    clone->mt = kh_init(mt, mrb);
      clone->tt = MRB_TT_SCLASS;
      return clone;
  }
}

static void
init_copy(mrb_state *mrb, mrb_value dest, mrb_value obj)
{
    //if (OBJ_FROZEN(dest)) {
    //    rb_raise(rb_eTypeError, "[bug] frozen object (%s) allocated", rb_obj_classname(dest));
    //}
    //RBASIC(dest)->flags &= ~(T_MASK|FL_EXIVAR);
    //RBASIC(dest)->flags |= RBASIC(obj)->flags & (T_MASK|FL_EXIVAR|FL_TAINT);
    //if (FL_TEST(obj, FL_EXIVAR)) {
    //    mrb_copy_generic_ivar(dest, obj);
    //}
    //mrb_gc_copy_finalizer(dest, obj);
    switch (mrb_type(obj)) {
      case MRB_TT_OBJECT:
      case MRB_TT_CLASS:
      case MRB_TT_MODULE:
        if (ROBJECT(dest)->iv) {
            ROBJECT(dest)->iv = 0;
        }
        if (ROBJECT(obj)->iv) {
            ROBJECT(dest)->iv = ROBJECT(obj)->iv;
        }
        break;

      default:
        break;
    }
    mrb_funcall(mrb, dest, "initialize_copy", 1, obj);
}

/* 15.3.1.3.8  */
/*
 *  call-seq:
 *     obj.clone -> an_object
 *
 *  Produces a shallow copy of <i>obj</i>---the instance variables of
 *  <i>obj</i> are copied, but not the objects they reference. Copies
 *  the frozen state of <i>obj</i>. See also the discussion
 *  under <code>Object#dup</code>.
 *
 *     class Klass
 *        attr_accessor :str
 *     end
 *     s1 = Klass.new      #=> #<Klass:0x401b3a38>
 *     s1.str = "Hello"    #=> "Hello"
 *     s2 = s1.clone       #=> #<Klass:0x401b3998 @str="Hello">
 *     s2.str[1,4] = "i"   #=> "i"
 *     s1.inspect          #=> "#<Klass:0x401b3a38 @str=\"Hi\">"
 *     s2.inspect          #=> "#<Klass:0x401b3998 @str=\"Hi\">"
 *
 *  This method may have class-specific behavior.  If so, that
 *  behavior will be documented under the #+initialize_copy+ method of
 *  the class.
 *
 *  Some Class(True False Nil Symbol Fixnum Float) Object  cannot clone.
 */
mrb_value
mrb_obj_clone(mrb_state *mrb, mrb_value self)
{
  struct RObject *clone;

  if (mrb_special_const_p(self)) {
      mrb_raise(mrb, E_TYPE_ERROR, "can't clone %s", mrb_obj_classname(mrb, self));
  }
  clone = mrb_obj_alloc(mrb, self.tt, mrb_obj_class(mrb, self));
  clone->c = mrb_singleton_class_clone(mrb, self);
  init_copy(mrb, mrb_obj_value(clone), self);
  //1-9-2 no bug mrb_funcall(mrb, clone, "initialize_clone", 1, self);
  //RBASIC(clone)->flags |= RBASIC(obj)->flags & FL_FREEZE;

  return mrb_obj_value(clone);
}

/* 15.3.1.3.9  */
/*
 *  call-seq:
 *     obj.dup -> an_object
 *
 *  Produces a shallow copy of <i>obj</i>---the instance variables of
 *  <i>obj</i> are copied, but not the objects they reference.
 *  <code>dup</code> copies the frozen state of <i>obj</i>. See also
 *  the discussion under <code>Object#clone</code>. In general,
 *  <code>clone</code> and <code>dup</code> may have different semantics
 *  in descendant classes. While <code>clone</code> is used to duplicate
 *  an object, including its internal state, <code>dup</code> typically
 *  uses the class of the descendant object to create the new instance.
 *
 *  This method may have class-specific behavior.  If so, that
 *  behavior will be documented under the #+initialize_copy+ method of
 *  the class.
 */

mrb_value
mrb_obj_dup(mrb_state *mrb, mrb_value obj)
{
    struct RBasic *p;
    mrb_value dup;

    if (mrb_special_const_p(obj)) {
        mrb_raise(mrb, E_TYPE_ERROR, "can't dup %s", mrb_obj_classname(mrb, obj));
    }
    p = mrb_obj_alloc(mrb, mrb_type(obj), mrb_obj_class(mrb, obj));
    //init_copy(dup, obj);
    dup = mrb_obj_value(p);
    mrb_funcall(mrb, dup, "initialize_copy", 1, obj);

    return dup;
}

/* 15.3.1.2.3  */
/* 15.3.1.3.12 */
/*
 *  call-seq:
 *     eval(string [, binding [, filename [,lineno]]])  -> obj
 *
 *  Evaluates the Ruby expression(s) in <em>string</em>. If
 *  <em>binding</em> is given, which must be a <code>Binding</code>
 *  object, the evaluation is performed in its context. If the
 *  optional <em>filename</em> and <em>lineno</em> parameters are
 *  present, they will be used when reporting syntax errors.
 *
 *     def getBinding(str)
 *       return binding
 *     end
 *     str = "hello"
 *     eval "str + ' Fred'"                      #=> "hello Fred"
 *     eval "str + ' Fred'", getBinding("bye")   #=> "bye Fred"
 */
mrb_value
mrb_f_eval(int argc, mrb_value *argv, mrb_value self)
{
  return mrb_false_value(); /* dummy */
}
mrb_value
mrb_f_eval_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_f_eval(argc, argv, self);
}

static mrb_value
mrb_obj_extend(mrb_state *mrb, int argc, mrb_value *argv, mrb_value obj)
{
  int i;

  if (argc == 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (at least 1)");
  }
  for (i = 0; i < argc; i++) {
    //Check_Type(argv[i], T_MODULE);
    mrb_check_type(mrb, argv[i], MRB_TT_MODULE);
  }
  while (argc--) {
    mrb_funcall(mrb, argv[argc], "extend_object", 1, obj);
    mrb_funcall(mrb, argv[argc], "extended", 1, obj);
  }
  return obj;
}

/* 15.3.1.3.13 */
/*
 *  call-seq:
 *     obj.extend(module, ...)    -> obj
 *
 *  Adds to _obj_ the instance methods from each module given as a
 *  parameter.
 *
 *     module Mod
 *       def hello
 *         "Hello from Mod.\n"
 *       end
 *     end
 *
 *     class Klass
 *       def hello
 *         "Hello from Klass.\n"
 *       end
 *     end
 *
 *     k = Klass.new
 *     k.hello         #=> "Hello from Klass.\n"
 *     k.extend(Mod)   #=> #<Klass:0x401b3bc8>
 *     k.hello         #=> "Hello from Mod.\n"
 */
mrb_value
mrb_obj_extend_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_obj_extend(mrb, argc, argv, self);
}

/* 15.3.1.2.4  */
/* 15.3.1.3.14 */
/*
 *  call-seq:
 *     global_variables    -> array
 *
 *  Returns an array of the names of global variables.
 *
 *     global_variables.grep /std/   #=> [:$stdin, :$stdout, :$stderr]
 */
//mrb_value
//mrb_f_global_variables(mrb_state *mrb, mrb_value self)

/* 15.3.1.3.15 */
mrb_value
mrb_obj_hash(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(mrb_obj_id(self));
}

/* 15.3.1.3.16 */
mrb_value
mrb_obj_init_copy(mrb_state *mrb, mrb_value self)
{
  mrb_value orig;

  mrb_get_args(mrb, "o", &orig);
  if (mrb_obj_equal(mrb, self, orig)) return self;
  if ((mrb_type(self) != mrb_type(orig)) || (mrb_obj_class(mrb, self) != mrb_obj_class(mrb, orig))) {
      mrb_raise(mrb, E_TYPE_ERROR, "initialize_copy should take same class object");
  }
  return self;
}

/* 15.3.1.3.18 */
/*
 *  call-seq:
 *     obj.instance_eval(string [, filename [, lineno]] )   -> obj
 *     obj.instance_eval {| | block }                       -> obj
 *
 *  Evaluates a string containing Ruby source code, or the given block,
 *  within the context of the receiver (_obj_). In order to set the
 *  context, the variable +self+ is set to _obj_ while
 *  the code is executing, giving the code access to _obj_'s
 *  instance variables. In the version of <code>instance_eval</code>
 *  that takes a +String+, the optional second and third
 *  parameters supply a filename and starting line number that are used
 *  when reporting compilation errors.
 *
 *     class KlassWithSecret
 *       def initialize
 *         @secret = 99
 *       end
 *     end
 *     k = KlassWithSecret.new
 *     k.instance_eval { @secret }   #=> 99
 */
mrb_value
mrb_obj_instance_eval(mrb_state *mrb, mrb_value self)
{
  mrb_value b;

  mrb_get_args(mrb, "&", &b);
  return mrb_yield_with_self(mrb, b, 0, 0, self);
}

int
mrb_obj_is_instance_of(mrb_state *mrb, mrb_value obj, struct RClass* c)
{
  if (mrb_obj_class(mrb, obj) == c) return TRUE;
  return FALSE;
}

/* 15.3.1.3.19 */
/*
 *  call-seq:
 *     obj.instance_of?(class)    -> true or false
 *
 *  Returns <code>true</code> if <i>obj</i> is an instance of the given
 *  class. See also <code>Object#kind_of?</code>.
 */
static mrb_value
obj_is_instance_of(mrb_state *mrb, mrb_value self)
{
  mrb_value arg;

  mrb_get_args(mrb, "o", &arg);
  if (mrb_obj_is_instance_of(mrb, self, mrb_class_ptr(arg))){
    return mrb_true_value();
  }
  else {
    return mrb_false_value();
  }
}

/* 15.3.1.3.20 */
/*
 *  call-seq:
 *     obj.instance_variable_defined?(symbol)    -> true or false
 *
 *  Returns <code>true</code> if the given instance variable is
 *  defined in <i>obj</i>.
 *
 *     class Fred
 *       def initialize(p1, p2)
 *         @a, @b = p1, p2
 *       end
 *     end
 *     fred = Fred.new('cat', 99)
 *     fred.instance_variable_defined?(:@a)    #=> true
 *     fred.instance_variable_defined?("@b")   #=> true
 *     fred.instance_variable_defined?("@c")   #=> false
 */
mrb_value
mrb_obj_ivar_defined(mrb_state *mrb, mrb_value self)
{
  mrb_value arg;
  khiter_t k;
  kh_iv_t *h = RCLASS_IV_TBL(self);
  mrb_sym mid;

  mrb_get_args(mrb, "o", &arg);
  mid = mrb_to_id(mrb, arg);

  if (h) {
    k = kh_get(iv, h, mid);
    if (k != kh_end(h)) {
      return mrb_true_value();
    }
  }
  return mrb_false_value();
}

/* 15.3.1.3.21 */
/*
 *  call-seq:
 *     obj.instance_variable_get(symbol)    -> obj
 *
 *  Returns the value of the given instance variable, or nil if the
 *  instance variable is not set. The <code>@</code> part of the
 *  variable name should be included for regular instance
 *  variables. Throws a <code>NameError</code> exception if the
 *  supplied symbol is not valid as an instance variable name.
 *
 *     class Fred
 *       def initialize(p1, p2)
 *         @a, @b = p1, p2
 *       end
 *     end
 *     fred = Fred.new('cat', 99)
 *     fred.instance_variable_get(:@a)    #=> "cat"
 *     fred.instance_variable_get("@b")   #=> 99
 */
mrb_value
mrb_obj_ivar_get(mrb_state *mrb, mrb_value self)
{
  mrb_value arg;
  mrb_sym id;

  mrb_get_args(mrb, "o", &arg);
  id = mrb_to_id(mrb, arg);

  //if (!mrb_is_instance_id(id)) {
  //    mrb_name_error(mrb, id, "`%s' is not allowed as an instance variable name", mrb_sym2name(mrb, id));
  //}
  return mrb_iv_get(mrb, self, id);
}

/* 15.3.1.3.22 */
/*
 *  call-seq:
 *     obj.instance_variable_set(symbol, obj)    -> obj
 *
 *  Sets the instance variable names by <i>symbol</i> to
 *  <i>object</i>, thereby frustrating the efforts of the class's
 *  author to attempt to provide proper encapsulation. The variable
 *  did not have to exist prior to this call.
 *
 *     class Fred
 *       def initialize(p1, p2)
 *         @a, @b = p1, p2
 *       end
 *     end
 *     fred = Fred.new('cat', 99)
 *     fred.instance_variable_set(:@a, 'dog')   #=> "dog"
 *     fred.instance_variable_set(:@c, 'cat')   #=> "cat"
 *     fred.inspect                             #=> "#<Fred:0x401b3da8 @a=\"dog\", @b=99, @c=\"cat\">"
 */
mrb_value
mrb_obj_ivar_set(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  mrb_value val;
  mrb_sym id;

  mrb_get_args(mrb, "oo", &key, &val);
  id = mrb_to_id(mrb, key);
  mrb_iv_set(mrb, self, id, val);
  return val;
}

/* 15.3.1.3.23 */
/*
 *  call-seq:
 *     obj.instance_variables    -> array
 *
 *  Returns an array of instance variable names for the receiver. Note
 *  that simply defining an accessor does not create the corresponding
 *  instance variable.
 *
 *     class Fred
 *       attr_accessor :a1
 *       def initialize
 *         @iv = 3
 *       end
 *     end
 *     Fred.new.instance_variables   #=> [:@iv]
 */
mrb_value
mrb_obj_instance_variables(mrb_state *mrb, mrb_value self)
{
    mrb_value ary;
    kh_iv_t *h = RCLASS_IV_TBL(self);
    khint_t i;
    const char* p;

    ary = mrb_ary_new(mrb);
    if (h) {
      for (i=0;i<kh_end(h);i++) {
        if (kh_exist(h, i)) {
          p = mrb_sym2name(mrb, kh_key(h,i));
          if (*p == '@') {
            if (mrb_type(kh_value(h, i)) != MRB_TT_UNDEF)
              mrb_ary_push(mrb, ary, mrb_str_new_cstr(mrb, p));
          }
        }
      }
    }
    return ary;
}

/* 15.3.1.3.24 */
/* 15.3.1.3.26 */
/*
 *  call-seq:
 *     obj.is_a?(class)       -> true or false
 *     obj.kind_of?(class)    -> true or false
 *
 *  Returns <code>true</code> if <i>class</i> is the class of
 *  <i>obj</i>, or if <i>class</i> is one of the superclasses of
 *  <i>obj</i> or modules included in <i>obj</i>.
 *
 *     module M;    end
 *     class A
 *       include M
 *     end
 *     class B < A; end
 *     class C < B; end
 *     b = B.new
 *     b.instance_of? A   #=> false
 *     b.instance_of? B   #=> true
 *     b.instance_of? C   #=> false
 *     b.instance_of? M   #=> false
 *     b.kind_of? A       #=> true
 *     b.kind_of? B       #=> true
 *     b.kind_of? C       #=> false
 *     b.kind_of? M       #=> true
 */
mrb_value
mrb_obj_is_kind_of_m(mrb_state *mrb, mrb_value self)
{
  mrb_value arg;

  mrb_get_args(mrb, "o", &arg);
  if (mrb_obj_is_kind_of(mrb, self, mrb_class_ptr(arg))) {
    return mrb_true_value();
  }
  else {
    return mrb_false_value();
  }
}

/* 15.3.1.2.6  */
/* 15.3.1.3.27 */
/*
 * call-seq:
 *   lambda { |...| block }  -> a_proc
 *
 * Equivalent to <code>Proc.new</code>, except the resulting Proc objects
 * check the number of parameters passed when called.
 */
mrb_value
proc_lambda(mrb_state *mrb, mrb_value self)
{
  //return mrb_block_lambda();
  return mrb_nil_value(); /* dummy */
}

/* 15.3.1.2.7  */
/* 15.3.1.3.28 */
/*
 *  call-seq:
 *     local_variables    -> array
 *
 *  Returns the names of the current local variables.
 *
 *     fred = 1
 *     for i in 1..10
 *        # ...
 *     end
 *     local_variables   #=> [:fred, :i]
 */
mrb_value
mrb_f_local_variables(mrb_state *mrb, mrb_value self)
{
  mrb_value ary;

  ary = mrb_ary_new(mrb);
  return ary; /* dummy */
}

/* 15.3.1.2.8  */
/* 15.3.1.3.29 */
/*
 *  call-seq:
 *     loop { block }
 *     loop            -> an_enumerator
 *
 *  Repeatedly executes the block.
 *
 *  If no block is given, an enumerator is returned instead.
 *
 *     loop do
 *       print "Input: "
 *       line = gets
 *       break if !line or line =~ /^qQ/
 *       # ...
 *     end
 *
 *  StopIteration raised in the block breaks the loop.
 */
mrb_value
mrb_f_loop(mrb_state *mrb, mrb_value self)
{
  return mrb_nil_value();            /* dummy */
}

static void
method_entry_loop(mrb_state *mrb, struct RClass* klass, mrb_value ary)
{
  khint_t i;

  khash_t(mt) *h = klass->mt;
  if (!h) return;
  for (i=0;i<kh_end(h);i++) {
    if (kh_exist(h, i)) {
      mrb_ary_push(mrb, ary, mrb_symbol_value(kh_key(h,i)));
    }
  }
}

static mrb_value
class_instance_method_list(mrb_state *mrb, int argc, mrb_value *argv, struct RClass* klass, int obj)
{
  mrb_value ary;
  int recur;
  struct RClass* oldklass;

  if (argc == 0) {
      recur = TRUE;
  }
  else {
      mrb_value r;
      mrb_get_args(mrb, "o", &r);
      recur = mrb_test(r);
  }

  ary = mrb_ary_new(mrb);
  oldklass = 0;
  while (klass && (klass != oldklass)) {
    method_entry_loop(mrb, klass, ary);
    if ((klass->tt == MRB_TT_ICLASS) ||
        (klass->tt == MRB_TT_SCLASS)) {
    }
    else {
      if (!recur) break;
    }
    oldklass = klass;
    klass = klass->super;
  }

  return ary;
}

mrb_value
mrb_obj_singleton_methods(mrb_state *mrb, int argc, mrb_value *argv, mrb_value obj)
{
  mrb_value recur, ary;
  struct RClass* klass;

  if (argc == 0) {
      recur = mrb_true_value();
  }
  else {
      //mrb_scan_args(argc, argv, "01", &recur);
      recur = argv[0];
  }
  klass = mrb_class(mrb, obj);
  ary = mrb_ary_new(mrb);
  if (klass && (klass->tt == MRB_TT_SCLASS)) {
      method_entry_loop(mrb, klass, ary);
      klass = klass->super;
  }
  if (RTEST(recur)) {
      while (klass && ((klass->tt == MRB_TT_SCLASS) || (klass->tt == MRB_TT_ICLASS))) {
        method_entry_loop(mrb, klass, ary);
        klass = klass->super;
      }
  }

  return ary;
}

mrb_value
mrb_obj_methods(mrb_state *mrb, int argc, mrb_value *argv, mrb_value obj, mrb_method_flag_t flag)
{
retry:
  if (argc == 0) {
      return class_instance_method_list(mrb, argc, argv, mrb_class(mrb, obj), 0);
  }
  else {
      mrb_value recur;

      //mrb_scan_args(argc, argv, "1", &recur);
      recur = argv[0];
      if (mrb_test(recur)) {
          argc = 0;
          goto retry;
      }
      return mrb_obj_singleton_methods(mrb, argc, argv, obj);
  }
}
/* 15.3.1.3.31 */
/*
 *  call-seq:
 *     obj.methods    -> array
 *
 *  Returns a list of the names of methods publicly accessible in
 *  <i>obj</i>. This will include all the methods accessible in
 *  <i>obj</i>'s ancestors.
 *
 *     class Klass
 *       def kMethod()
 *       end
 *     end
 *     k = Klass.new
 *     k.methods[0..9]    #=> [:kMethod, :freeze, :nil?, :is_a?,
 *                        #    :class, :instance_variable_set,
 *                        #    :methods, :extend, :__send__, :instance_eval]
 *     k.methods.length   #=> 42
 */
mrb_value
mrb_obj_methods_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_obj_methods(mrb, argc, argv, self, 0); /* everything but private */
}

/* 15.3.1.3.32 */
/*
 * call_seq:
 *   nil.nil?               -> true
 *   <anything_else>.nil?   -> false
 *
 * Only the object <i>nil</i> responds <code>true</code> to <code>nil?</code>.
 */
mrb_value
mrb_false(mrb_state *mrb, mrb_value self)
{
  return mrb_false_value();
}

/* 15.3.1.2.10  */
/* 15.3.1.3.35 */
/*
 *  call-seq:
 *     print(obj, ...)    -> nil
 *
 *  Prints each object in turn to <code>$stdout</code>. If the output
 *  field separator (<code>$,</code>) is not +nil+, its
 *  contents will appear between each field. If the output record
 *  separator (<code>$\\</code>) is not +nil+, it will be
 *  appended to the output. If no arguments are given, prints
 *  <code>$_</code>. Objects that aren't strings will be converted by
 *  calling their <code>to_s</code> method.
 *
 *     print "cat", [1,2,3], 99, "\n"
 *     $, = ", "
 *     $\ = "\n"
 *     print "cat", [1,2,3], 99
 *
 *  <em>produces:</em>
 *
 *     cat12399
 *     cat, 1, 2, 3, 99
 */

/* 15.3.1.3.36 */
/*
 *  call-seq:
 *     obj.private_methods(all=true)   -> array
 *
 *  Returns the list of private methods accessible to <i>obj</i>. If
 *  the <i>all</i> parameter is set to <code>false</code>, only those methods
 *  in the receiver will be listed.
 */
mrb_value
mrb_obj_private_methods(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_obj_methods(mrb, argc, argv, self, NOEX_PRIVATE); /* private attribute not define */
}

/* 15.3.1.3.37 */
/*
 *  call-seq:
 *     obj.protected_methods(all=true)   -> array
 *
 *  Returns the list of protected methods accessible to <i>obj</i>. If
 *  the <i>all</i> parameter is set to <code>false</code>, only those methods
 *  in the receiver will be listed.
 */
mrb_value
mrb_obj_protected_methods(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_obj_methods(mrb, argc, argv, self, NOEX_PROTECTED); /* protected attribute not define */
}

/* 15.3.1.3.38 */
/*
 *  call-seq:
 *     obj.public_methods(all=true)   -> array
 *
 *  Returns the list of public methods accessible to <i>obj</i>. If
 *  the <i>all</i> parameter is set to <code>false</code>, only those methods
 *  in the receiver will be listed.
 */
mrb_value
mrb_obj_public_methods(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_obj_methods(mrb, argc, argv, self, NOEX_PUBLIC); /* public attribute not define */
}

/* 15.3.1.2.11  */
/* 15.3.1.3.39 */
/*
 *  call-seq:
 *     puts(obj, ...)    -> nil
 *
 *  Equivalent to
 *
 *      $stdout.puts(obj, ...)
 */

static mrb_value
get_errinfo(mrb_state *mrb)
{
    //return get_thread_errinfo(GET_THREAD());
    return mrb_str_new_cstr(mrb, "error!!"); /* dummy */
}

/* 15.3.1.2.12  */
/* 15.3.1.3.40 */
/*
 *  call-seq:
 *     raise
 *     raise(string)
 *     raise(exception [, string [, array]])
 *     fail
 *     fail(string)
 *     fail(exception [, string [, array]])
 *
 *  With no arguments, raises the exception in <code>$!</code> or raises
 *  a <code>RuntimeError</code> if <code>$!</code> is +nil+.
 *  With a single +String+ argument, raises a
 *  +RuntimeError+ with the string as a message. Otherwise,
 *  the first parameter should be the name of an +Exception+
 *  class (or an object that returns an +Exception+ object when sent
 *  an +exception+ message). The optional second parameter sets the
 *  message associated with the exception, and the third parameter is an
 *  array of callback information. Exceptions are caught by the
 *  +rescue+ clause of <code>begin...end</code> blocks.
 *
 *     raise "Failed to create socket"
 *     raise ArgumentError, "No parameters", caller
 */
mrb_value
mrb_f_raise(mrb_state *mrb, mrb_value self)
{
  mrb_value err;
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 0) {
    err = get_errinfo(mrb);
    if (!mrb_nil_p(err)) {
        argc = 1;
        argv[0] = err;
      }
  }
  mrb_exc_raise(mrb, mrb_make_exception(mrb, argc, argv));
  return mrb_nil_value();            /* not reached */
}

/* 15.3.1.3.41 */
/*
 *  call-seq:
 *     obj.remove_instance_variable(symbol)    -> obj
 *
 *  Removes the named instance variable from <i>obj</i>, returning that
 *  variable's value.
 *
 *     class Dummy
 *       attr_reader :var
 *       def initialize
 *         @var = 99
 *       end
 *       def remove
 *         remove_instance_variable(:@var)
 *       end
 *     end
 *     d = Dummy.new
 *     d.var      #=> 99
 *     d.remove   #=> 99
 *     d.var      #=> nil
 */
mrb_value
mrb_obj_remove_instance_variable(mrb_state *mrb, mrb_value self)
{
  mrb_sym sym;
  mrb_value name;
  khash_t(iv) *h;
  khiter_t k;
  mrb_value val;
  mrb_value Qundef = mrb_undef_value();

  mrb_get_args(mrb, "o", &name);
  sym = mrb_to_id(mrb, name);
  //if (OBJ_FROZEN(obj)) mrb_error_frozen("object");
  //if (!mrb_is_instance_id(id)) {
  //    mrb_name_error(mrb, id, "`%s' is not allowed as an instance variable name", mrb_sym2name(mrb, id));
  //}
  switch (mrb_type(self)) {
    case MRB_TT_OBJECT:
    case MRB_TT_CLASS:
    case MRB_TT_MODULE:
      if (!mrb_obj_ptr(self)->iv) break;
      h = mrb_obj_ptr(self)->iv;
      if (!h) break;
      k = kh_get(iv, h, sym);
      if (k != kh_end(h)) {
        val = kh_value(h, k);
        if (!mrb_obj_equal(mrb, val, Qundef)) {
          kh_value(h, k) = Qundef;
          return val;
        }
      }
      break;
    default:
      break;
  }
  mrb_name_error(mrb, sym, "instance variable %s not defined", mrb_sym2name(mrb, sym));
  return mrb_nil_value();            /* not reached */
}

/* 15.3.1.2.13  */
/* 15.3.1.3.42 */
/*
 *  call-seq:
 *     require(string)    -> true or false
 *
 *  Ruby tries to load the library named _string_, returning
 *  +true+ if successful. If the filename does not resolve to
 *  an absolute path, it will be searched for in the directories listed
 *  in <code>$:</code>. If the file has the extension ``.rb'', it is
 *  loaded as a source file; if the extension is ``.so'', ``.o'', or
 *  ``.dll'', or whatever the default shared library extension is on
 *  the current platform, Ruby loads the shared library as a Ruby
 *  extension. Otherwise, Ruby tries adding ``.rb'', ``.so'', and so on
 *  to the name. The name of the loaded feature is added to the array in
 *  <code>$"</code>. A feature will not be loaded if its name already
 *  appears in <code>$"</code>. The file name is converted to an absolute
 *  path, so ``<code>require 'a'; require './a'</code>'' will not load
 *  <code>a.rb</code> twice.
 *
 *     require "my-library.rb"
 *     require "db-driver"
 */
mrb_value
mrb_f_require(mrb_state *mrb, mrb_value self)
{
  mrb_value fname;

  mrb_get_args(mrb, "o", &fname);
  return mrb_nil_value(); /* dummy */
}


static inline int
basic_obj_respond_to(mrb_state *mrb, mrb_value obj, mrb_sym id, int pub)
{
  return mrb_respond_to(mrb, obj, id);
  //return TRUE;
}
/* 15.3.1.3.43 */
/*
 *  call-seq:
 *     obj.respond_to?(symbol, include_private=false) -> true or false
 *
 *  Returns +true+ if _obj_ responds to the given
 *  method. Private methods are included in the search only if the
 *  optional second parameter evaluates to +true+.
 *
 *  If the method is not implemented,
 *  as Process.fork on Windows, File.lchmod on GNU/Linux, etc.,
 *  false is returned.
 *
 *  If the method is not defined, <code>respond_to_missing?</code>
 *  method is called and the result is returned.
 */
mrb_value
obj_respond_to(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;
  mrb_value mid, priv;
  mrb_sym id;

  mrb_get_args(mrb, "*", &argv, &argc);
  //mrb_scan_args(argc, argv, "11", &mid, &priv);
  mid = argv[0];
  if (argc > 1) priv = argv[1];
  else priv = mrb_nil_value();
  id = mrb_to_id(mrb, mid);
  if (basic_obj_respond_to(mrb, self, id, !RTEST(priv)))
    return mrb_true_value();
  return mrb_false_value();
}

/* 15.3.1.3.45 */
/*
 *  call-seq:
 *     obj.singleton_methods(all=true)    -> array
 *
 *  Returns an array of the names of singleton methods for <i>obj</i>.
 *  If the optional <i>all</i> parameter is true, the list will include
 *  methods in modules included in <i>obj</i>.
 *  Only public and protected singleton methods are returned.
 *
 *     module Other
 *       def three() end
 *     end
 *
 *     class Single
 *       def Single.four() end
 *     end
 *
 *     a = Single.new
 *
 *     def a.one()
 *     end
 *
 *     class << a
 *       include Other
 *       def two()
 *       end
 *     end
 *
 *     Single.singleton_methods    #=> [:four]
 *     a.singleton_methods(false)  #=> [:two, :one]
 *     a.singleton_methods         #=> [:two, :one, :three]
 */
mrb_value
mrb_obj_singleton_methods_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_obj_singleton_methods(mrb, argc, argv, self);
}

mrb_value mrb_f_sprintf(mrb_state *mrb, mrb_value obj); /* in sprintf.c */

void
mrb_init_kernel(mrb_state *mrb)
{
  struct RClass *krn;

  krn = mrb->kernel_module = mrb_define_module(mrb, "Kernel");
  mrb_define_class_method(mrb, krn, "'",                    mrb_f_spawn_m,                   ARGS_ANY());     /* 15.3.1.2.1  */
  mrb_define_class_method(mrb, krn, "block_given?",         mrb_f_block_given_p_m,           ARGS_NONE());    /* 15.3.1.2.2  */
  mrb_define_class_method(mrb, krn, "eval",                 mrb_f_eval_m,                    ARGS_ANY());     /* 15.3.1.2.3  */
  mrb_define_class_method(mrb, krn, "global_variables",     mrb_f_global_variables,          ARGS_NONE());    /* 15.3.1.2.4  */
  mrb_define_class_method(mrb, krn, "iterator?",            mrb_f_block_given_p_m,           ARGS_NONE());    /* 15.3.1.2.5  */
  mrb_define_class_method(mrb, krn, "lambda",               proc_lambda,                     ARGS_NONE());    /* 15.3.1.2.6  */
  mrb_define_class_method(mrb, krn, "local_variables",      mrb_f_local_variables,           ARGS_NONE());    /* 15.3.1.2.7  */
  mrb_define_class_method(mrb, krn, "loop",                 mrb_f_loop,                      ARGS_NONE());    /* 15.3.1.2.8  */
;     /* 15.3.1.2.11 */
  mrb_define_class_method(mrb, krn, "raise",                mrb_f_raise,                     ARGS_ANY());     /* 15.3.1.2.12 */
  mrb_define_class_method(mrb, krn, "require",              mrb_f_require,                   ARGS_REQ(1));    /* 15.3.1.2.13 */

  mrb_define_method(mrb, krn, "singleton_class",            mrb_singleton_class,             ARGS_NONE());

  mrb_define_method(mrb, krn, "==",                         mrb_obj_equal_m,                 ARGS_REQ(1));    /* 15.3.1.3.1  */
  mrb_define_method(mrb, krn, "!=",                         mrb_obj_not_equal_m,             ARGS_REQ(1));
  mrb_define_method(mrb, krn, "===",                        mrb_equal_m,                     ARGS_REQ(1));    /* 15.3.1.3.2  */
  mrb_define_method(mrb, krn, "__id__",                     mrb_obj_id_m,                    ARGS_NONE());    /* 15.3.1.3.3  */
  mrb_define_method(mrb, krn, "__send__",                   mrb_f_send_m,                    ARGS_ANY());     /* 15.3.1.3.4  */
  mrb_define_method(mrb, krn, "'",                          mrb_f_spawn_m,                   ARGS_ANY());     /* 15.3.1.3.5  *//* "spawn"->"'" */
  mrb_define_method(mrb, krn, "block_given?",               mrb_f_block_given_p_m,           ARGS_NONE());    /* 15.3.1.3.6  */
  mrb_define_method(mrb, krn, "class",                      mrb_obj_class_m,                 ARGS_NONE());    /* 15.3.1.3.7  */
  mrb_define_method(mrb, krn, "clone",                      mrb_obj_clone,                   ARGS_NONE());    /* 15.3.1.3.8  */
  mrb_define_method(mrb, krn, "dup",                        mrb_obj_dup,                     ARGS_NONE());    /* 15.3.1.3.9  */
  mrb_define_method(mrb, krn, "eql?",                       mrb_obj_equal_m,                 ARGS_REQ(1));    /* 15.3.1.3.10 */
  mrb_define_method(mrb, krn, "equal?",                     mrb_obj_equal_m,                 ARGS_REQ(1));    /* 15.3.1.3.11 */
  mrb_define_method(mrb, krn, "eval",                       mrb_f_eval_m,                    ARGS_ANY());     /* 15.3.1.3.12 */
  mrb_define_method(mrb, krn, "extend",                     mrb_obj_extend_m,                ARGS_ANY());     /* 15.3.1.3.13 */
  mrb_define_method(mrb, krn, "global_variables",           mrb_f_global_variables,          ARGS_NONE());    /* 15.3.1.3.14 */
  mrb_define_method(mrb, krn, "hash",                       mrb_obj_hash,                    ARGS_NONE());    /* 15.3.1.3.15 */
  mrb_define_method(mrb, krn, "initialize_copy",            mrb_obj_init_copy,               ARGS_REQ(1));    /* 15.3.1.3.16 */
  mrb_define_method(mrb, krn, "inspect",                    mrb_obj_inspect,                 ARGS_NONE());    /* 15.3.1.3.17 */
  mrb_define_method(mrb, krn, "instance_eval",              mrb_obj_instance_eval,           ARGS_ANY());     /* 15.3.1.3.18 */
  mrb_define_method(mrb, krn, "instance_of?",               obj_is_instance_of,              ARGS_REQ(1));    /* 15.3.1.3.19 */
  mrb_define_method(mrb, krn, "instance_variable_defined?", mrb_obj_ivar_defined,            ARGS_REQ(1));    /* 15.3.1.3.20 */
  mrb_define_method(mrb, krn, "instance_variable_get",      mrb_obj_ivar_get,                ARGS_REQ(1));    /* 15.3.1.3.21 */
  mrb_define_method(mrb, krn, "instance_variable_set",      mrb_obj_ivar_set,                ARGS_REQ(2));    /* 15.3.1.3.22 */
  mrb_define_method(mrb, krn, "instance_variables",         mrb_obj_instance_variables,      ARGS_NONE());    /* 15.3.1.3.23 */
  mrb_define_method(mrb, krn, "is_a?",                      mrb_obj_is_kind_of_m,            ARGS_REQ(1));    /* 15.3.1.3.24 */
  mrb_define_method(mrb, krn, "iterator?",                  mrb_f_block_given_p_m,           ARGS_NONE());    /* 15.3.1.3.25 */
  mrb_define_method(mrb, krn, "kind_of?",                   mrb_obj_is_kind_of_m,            ARGS_REQ(1));    /* 15.3.1.3.26 */
  mrb_define_method(mrb, krn, "lambda",                     proc_lambda,                     ARGS_NONE());    /* 15.3.1.3.27 */
  mrb_define_method(mrb, krn, "local_variables",            mrb_f_local_variables,           ARGS_NONE());    /* 15.3.1.3.28 */
  mrb_define_method(mrb, krn, "loop",                       mrb_f_loop,                      ARGS_NONE());    /* 15.3.1.3.29 */
  mrb_define_method(mrb, krn, "methods",                    mrb_obj_methods_m,               ARGS_ANY());     /* 15.3.1.3.31 */
  mrb_define_method(mrb, krn, "nil?",                       mrb_false,                       ARGS_NONE());    /* 15.3.1.3.32 */
  mrb_define_method(mrb, krn, "object_id",                  mrb_obj_id_m,                    ARGS_NONE());    /* 15.3.1.3.33 */
  mrb_define_method(mrb, krn, "private_methods",            mrb_obj_private_methods,         ARGS_ANY());     /* 15.3.1.3.36 */
  mrb_define_method(mrb, krn, "protected_methods",          mrb_obj_protected_methods,       ARGS_ANY());     /* 15.3.1.3.37 */
  mrb_define_method(mrb, krn, "public_methods",             mrb_obj_public_methods,          ARGS_ANY());     /* 15.3.1.3.38 */
  mrb_define_method(mrb, krn, "raise",                      mrb_f_raise,                     ARGS_ANY());     /* 15.3.1.3.40 */
  mrb_define_method(mrb, krn, "remove_instance_variable",   mrb_obj_remove_instance_variable,ARGS_REQ(1));    /* 15.3.1.3.41 */
  mrb_define_method(mrb, krn, "require",                    mrb_f_require,                   ARGS_REQ(1));    /* 15.3.1.3.42 */
  mrb_define_method(mrb, krn, "respond_to?",                obj_respond_to,                  ARGS_ANY());     /* 15.3.1.3.43 */
  mrb_define_method(mrb, krn, "send",                       mrb_f_send_m,                    ARGS_ANY());     /* 15.3.1.3.44 */
  mrb_define_method(mrb, krn, "singleton_methods",          mrb_obj_singleton_methods_m,     ARGS_ANY());     /* 15.3.1.3.45 */
  mrb_define_method(mrb, krn, "to_s",                       mrb_any_to_s,                    ARGS_NONE());    /* 15.3.1.3.46 */

  mrb_define_method(mrb, krn, "sprintf",                    mrb_f_sprintf,                   ARGS_ANY());     /* in sprintf.c */
  mrb_define_method(mrb, krn, "format",                     mrb_f_sprintf,                   ARGS_ANY());     /* in sprintf.c */

  mrb_include_module(mrb, mrb->object_class, mrb->kernel_module);
}
