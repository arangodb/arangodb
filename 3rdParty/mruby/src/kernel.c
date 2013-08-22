/*
** kernel.c - Kernel module
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "error.h"

typedef enum {
    NOEX_PUBLIC    = 0x00,
    NOEX_NOSUPER   = 0x01,
    NOEX_PRIVATE   = 0x02,
    NOEX_PROTECTED = 0x04,
    NOEX_MASK      = 0x06,
    NOEX_BASIC     = 0x08,
    NOEX_UNDEF     = NOEX_NOSUPER,
    NOEX_MODFUNC   = 0x12,
    NOEX_SUPER     = 0x20,
    NOEX_VCALL     = 0x40,
    NOEX_RESPONDS  = 0x80
} mrb_method_flag_t;

mrb_bool
mrb_obj_basic_to_s_p(mrb_state *mrb, mrb_value obj)
{
    struct RProc *me = mrb_method_search(mrb, mrb_class(mrb, obj), mrb_intern2(mrb, "to_s", 4));
    if (me && MRB_PROC_CFUNC_P(me) && (me->body.func == mrb_any_to_s))
      return TRUE;
    return FALSE;
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
    return mrb_obj_iv_inspect(mrb, mrb_obj_ptr(obj));
  }
  return mrb_any_to_s(mrb, obj);
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
  mrb_bool eql_p;

  mrb_get_args(mrb, "o", &arg);
  eql_p = mrb_obj_equal(mrb, self, arg);

  return mrb_bool_value(eql_p);
}

static mrb_value
mrb_obj_not_equal_m(mrb_state *mrb, mrb_value self)
{
  mrb_value arg;
  mrb_bool eql_p;

  mrb_get_args(mrb, "o", &arg);
  eql_p = mrb_equal(mrb, self, arg);

  return mrb_bool_value(!eql_p);
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
  mrb_bool equal_p;

  mrb_get_args(mrb, "o", &arg);
  equal_p = mrb_equal(mrb, self, arg);

  return mrb_bool_value(equal_p);
}

/* 15.3.1.3.3  */
/* 15.3.1.3.33 */
/*
 *  Document-method: __id__
 *  Document-method: object_id
 *
 *  call-seq:
 *     obj.__id__       -> fixnum
 *     obj.object_id    -> fixnum
 *
 *  Returns an integer identifier for <i>obj</i>. The same number will
 *  be returned on all calls to <code>id</code> for a given object, and
 *  no two active objects will share an id.
 *  <code>Object#object_id</code> is a different concept from the
 *  <code>:name</code> notation, which returns the symbol id of
 *  <code>name</code>. Replaces the deprecated <code>Object#id</code>.
 */
static mrb_value
mrb_obj_id_m(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(mrb_obj_id(self));
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
mrb_f_send(mrb_state *mrb, mrb_value self)
{
  mrb_sym name;
  mrb_value block, *argv;
  int argc;

  mrb_get_args(mrb, "n*&", &name, &argv, &argc, &block);
  return mrb_funcall_with_block(mrb,self, name, argc, argv, block);
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
  mrb_callinfo *ci = mrb->c->ci;
  mrb_value *bp;
  mrb_bool given_p;

  bp = mrb->c->stbase + ci->stackidx + 1;
  ci--;
  if (ci <= mrb->c->cibase) {
    given_p = 0;
  }
  else {
    /* block_given? called within block; check upper scope */
    if (ci->proc->env && ci->proc->env->stack) {
      given_p = !(ci->proc->env->stack == mrb->c->stbase ||
                  mrb_nil_p(ci->proc->env->stack[1]));
    }
    else {
      if (ci->argc > 0) {
        bp += ci->argc;
      }
      given_p = !mrb_nil_p(*bp);
    }
  }

  return mrb_bool_value(given_p);
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
  struct RClass *klass = mrb_basic_ptr(obj)->c;

  if (klass->tt != MRB_TT_SCLASS)
    return klass;
  else {
    /* copy singleton(unnamed) class */
    struct RClass *clone = (struct RClass*)mrb_obj_alloc(mrb, klass->tt, mrb->class_class);

    if ((mrb_type(obj) == MRB_TT_CLASS) ||
      (mrb_type(obj) == MRB_TT_SCLASS)) { /* BUILTIN_TYPE(obj) == T_CLASS */
      clone->c = clone;
    }
    else {
      clone->c = mrb_singleton_class_clone(mrb, mrb_obj_value(klass));
    }

    clone->super = klass->super;
    if (klass->iv) {
      mrb_iv_copy(mrb, mrb_obj_value(clone), mrb_obj_value(klass));
      mrb_obj_iv_set(mrb, (struct RObject*)clone, mrb_intern2(mrb, "__attached__", 12), obj);
    }
    if (klass->mt) {
      clone->mt = kh_copy(mt, mrb, klass->mt);
    }
    else {
      clone->mt = kh_init(mt, mrb);
    }
    clone->tt = MRB_TT_SCLASS;
    return clone;
  }
}

static void
init_copy(mrb_state *mrb, mrb_value dest, mrb_value obj)
{
    switch (mrb_type(obj)) {
      case MRB_TT_OBJECT:
      case MRB_TT_CLASS:
      case MRB_TT_MODULE:
      case MRB_TT_SCLASS:
      case MRB_TT_HASH:
      case MRB_TT_DATA:
        mrb_iv_copy(mrb, dest, obj);
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
  struct RObject *p;
  mrb_value clone;

  if (mrb_special_const_p(self)) {
      mrb_raisef(mrb, E_TYPE_ERROR, "can't clone %S", self);
  }
  p = (struct RObject*)mrb_obj_alloc(mrb, mrb_type(self), mrb_obj_class(mrb, self));
  p->c = mrb_singleton_class_clone(mrb, self);
  clone = mrb_obj_value(p);
  init_copy(mrb, clone, self);

  return clone;
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
        mrb_raisef(mrb, E_TYPE_ERROR, "can't dup %S", obj);
    }
    p = mrb_obj_alloc(mrb, mrb_type(obj), mrb_obj_class(mrb, obj));
    dup = mrb_obj_value(p);
    init_copy(mrb, dup, obj);

    return dup;
}

static mrb_value
mrb_obj_extend(mrb_state *mrb, int argc, mrb_value *argv, mrb_value obj)
{
  int i;

  if (argc == 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (at least 1)");
  }
  for (i = 0; i < argc; i++) {
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

/* 15.3.1.3.15 */
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

mrb_value mrb_yield_internal(mrb_state *mrb, mrb_value b, int argc, mrb_value *argv, mrb_value self, struct RClass *c);

/* 15.3.1.3.18 */
/*
 *  call-seq:
 *     obj.instance_eval {| | block }                       -> obj
 *
 *  Evaluates the given block,within  the context of the receiver (_obj_).
 *  In order to set the context, the variable +self+ is set to _obj_ while
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
  mrb_value a, b;
  mrb_value cv;
  struct RClass *c;

  if (mrb_get_args(mrb, "|S&", &a, &b) == 1) {
    mrb_raise(mrb, E_NOTIMP_ERROR, "instance_eval with string not implemented");
  }
  switch (mrb_type(self)) {
  case MRB_TT_SYMBOL:
  case MRB_TT_FIXNUM:
  case MRB_TT_FLOAT:
    c = 0;
    break;
  default:
    cv = mrb_singleton_class(mrb, self);
    c = mrb_class_ptr(cv);
    break;
  }
  return mrb_yield_internal(mrb, b, 0, 0, self, c);
}

mrb_bool
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
  mrb_bool instance_of_p;

  mrb_get_args(mrb, "o", &arg);
  instance_of_p = mrb_obj_is_instance_of(mrb, self, mrb_class_ptr(arg));

  return mrb_bool_value(instance_of_p);
}

static void
valid_iv_name(mrb_state *mrb, mrb_sym iv_name_id, const char* s, size_t len)
{
  if (len < 2 || !(s[0] == '@' && s[1] != '@')) {
    mrb_name_error(mrb, iv_name_id, "`%S' is not allowed as an instance variable name", mrb_sym2str(mrb, iv_name_id));
  }
}

static void
check_iv_name(mrb_state *mrb, mrb_sym iv_name_id)
{
  const char *s;
  size_t len;

  s = mrb_sym2name_len(mrb, iv_name_id, &len);
  valid_iv_name(mrb, iv_name_id, s, len);
}

static mrb_sym
get_valid_iv_sym(mrb_state *mrb, mrb_value iv_name)
{
  mrb_sym iv_name_id;

  mrb_assert(mrb_symbol_p(iv_name) || mrb_string_p(iv_name));

  if (mrb_string_p(iv_name)) {
    iv_name_id = mrb_intern_cstr(mrb, RSTRING_PTR(iv_name));
    valid_iv_name(mrb, iv_name_id, RSTRING_PTR(iv_name), RSTRING_LEN(iv_name));
  }
  else {
    iv_name_id = mrb_symbol(iv_name);
    check_iv_name(mrb, iv_name_id);
  }

  return iv_name_id;
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
  mrb_sym mid;
  mrb_value sym;
  mrb_bool defined_p;

  mrb_get_args(mrb, "o", &sym);
  mid = get_valid_iv_sym(mrb, sym);
  defined_p = mrb_obj_iv_defined(mrb, mrb_obj_ptr(self), mid);

  return mrb_bool_value(defined_p);
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
  mrb_sym iv_name_id;
  mrb_value iv_name;

  mrb_get_args(mrb, "o", &iv_name);

  iv_name_id = get_valid_iv_sym(mrb, iv_name);
  return mrb_iv_get(mrb, self, iv_name_id);
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
  mrb_sym iv_name_id;
  mrb_value iv_name, val;

  mrb_get_args(mrb, "oo", &iv_name, &val);

  iv_name_id = get_valid_iv_sym(mrb, iv_name);
  mrb_iv_set(mrb, self, iv_name_id, val);
  return val;
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
  mrb_bool kind_of_p;

  mrb_get_args(mrb, "o", &arg);
  kind_of_p = mrb_obj_is_kind_of(mrb, self, mrb_class_ptr(arg));

  return mrb_bool_value(kind_of_p);
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

mrb_value
class_instance_method_list(mrb_state *mrb, mrb_bool recur, struct RClass* klass, int obj)
{
  mrb_value ary;
  struct RClass* oldklass;

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
mrb_obj_singleton_methods(mrb_state *mrb, mrb_bool recur, mrb_value obj)
{
  mrb_value ary;
  struct RClass* klass;

  klass = mrb_class(mrb, obj);
  ary = mrb_ary_new(mrb);
  if (klass && (klass->tt == MRB_TT_SCLASS)) {
      method_entry_loop(mrb, klass, ary);
      klass = klass->super;
  }
  if (recur) {
      while (klass && ((klass->tt == MRB_TT_SCLASS) || (klass->tt == MRB_TT_ICLASS))) {
        method_entry_loop(mrb, klass, ary);
        klass = klass->super;
      }
  }

  return ary;
}

mrb_value
mrb_obj_methods(mrb_state *mrb, mrb_bool recur, mrb_value obj, mrb_method_flag_t flag)
{
  if (recur)
      return class_instance_method_list(mrb, recur, mrb_class(mrb, obj), 0);
  else
      return mrb_obj_singleton_methods(mrb, recur, obj);
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
 *     k.methods[0..9]    #=> [:kMethod, :respond_to?, :nil?, :is_a?,
 *                        #    :class, :instance_variable_set,
 *                        #    :methods, :extend, :__send__, :instance_eval]
 *     k.methods.length   #=> 42
 */
mrb_value
mrb_obj_methods_m(mrb_state *mrb, mrb_value self)
{
  mrb_bool recur = TRUE;
  mrb_get_args(mrb, "|b", &recur);
  return mrb_obj_methods(mrb, recur, self, (mrb_method_flag_t)0); /* everything but private */
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
  mrb_bool recur = TRUE;
  mrb_get_args(mrb, "|b", &recur);
  return mrb_obj_methods(mrb, recur, self, NOEX_PRIVATE); /* private attribute not define */
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
  mrb_bool recur = TRUE;
  mrb_get_args(mrb, "|b", &recur);
  return mrb_obj_methods(mrb, recur, self, NOEX_PROTECTED); /* protected attribute not define */
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
  mrb_bool recur = TRUE;
  mrb_get_args(mrb, "|b", &recur);
  return mrb_obj_methods(mrb, recur, self, NOEX_PUBLIC); /* public attribute not define */
}

/* 15.3.1.2.12  */
/* 15.3.1.3.40 */
/*
 *  call-seq:
 *     raise
 *     raise(string)
 *     raise(exception [, string])
 *
 *  With no arguments, raises a <code>RuntimeError</code>
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
  mrb_value a[2], exc;
  int argc;


  argc = mrb_get_args(mrb, "|oo", &a[0], &a[1]);
  switch (argc) {
  case 0:
    mrb_raise(mrb, E_RUNTIME_ERROR, "");
    break;
  case 1:
    a[1] = mrb_check_string_type(mrb, a[0]);
    if (!mrb_nil_p(a[1])) {
      argc = 2;
      a[0] = mrb_obj_value(E_RUNTIME_ERROR);
    }
    /* fall through */
  default:
    exc = mrb_make_exception(mrb, argc, a);
    mrb_obj_iv_set(mrb, mrb_obj_ptr(exc), mrb_intern2(mrb, "lastpc", 6), mrb_voidp_value(mrb, mrb->c->ci->pc));
    mrb_exc_raise(mrb, exc);
    break;
  }
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
  mrb_value val;

  mrb_get_args(mrb, "n", &sym);
  check_iv_name(mrb, sym);
  val = mrb_iv_remove(mrb, self, sym);
  if (mrb_undef_p(val)) {
    mrb_name_error(mrb, sym, "instance variable %S not defined", mrb_sym2str(mrb, sym));
  }
  return val;
}

static inline mrb_bool
basic_obj_respond_to(mrb_state *mrb, mrb_value obj, mrb_sym id, int pub)
{
  return mrb_respond_to(mrb, obj, id);
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
  mrb_sym id, rtm_id;
  mrb_bool respond_to_p = TRUE;

  mrb_get_args(mrb, "*", &argv, &argc);
  mid = argv[0];
  if (argc > 1) priv = argv[1];
  else priv = mrb_nil_value();

  if (mrb_symbol_p(mid)) {
    id = mrb_symbol(mid);
  }
  else {
    mrb_value tmp;
    if (!mrb_string_p(mid)) {
      tmp = mrb_check_string_type(mrb, mid);
      if (mrb_nil_p(tmp)) {
        tmp = mrb_inspect(mrb, mid);
        mrb_raisef(mrb, E_TYPE_ERROR, "%S is not a symbol", tmp);
      }
    }
    tmp = mrb_check_intern_str(mrb, mid);
    if (mrb_nil_p(tmp)) {
      respond_to_p = FALSE;
    }
    else {
      id = mrb_symbol(tmp);
    }
  }

  if (respond_to_p) {
    respond_to_p = basic_obj_respond_to(mrb, self, id, !mrb_test(priv));
  }

  if (!respond_to_p) {
    rtm_id = mrb_intern2(mrb, "respond_to_missing?", 19);
    if (basic_obj_respond_to(mrb, self, rtm_id, !mrb_test(priv))) {
      return mrb_funcall_argv(mrb, self, rtm_id, argc, argv);
    }
  }
  return mrb_bool_value(respond_to_p);
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
  mrb_bool recur = TRUE;
  mrb_get_args(mrb, "|b", &recur);
  return mrb_obj_singleton_methods(mrb, recur, self);
}

void
mrb_init_kernel(mrb_state *mrb)
{
  struct RClass *krn;

  krn = mrb->kernel_module = mrb_define_module(mrb, "Kernel");
  mrb_define_class_method(mrb, krn, "block_given?",         mrb_f_block_given_p_m,           MRB_ARGS_NONE());    /* 15.3.1.2.2  */
  mrb_define_class_method(mrb, krn, "global_variables",     mrb_f_global_variables,          MRB_ARGS_NONE());    /* 15.3.1.2.4  */
  mrb_define_class_method(mrb, krn, "iterator?",            mrb_f_block_given_p_m,           MRB_ARGS_NONE());    /* 15.3.1.2.5  */
;     /* 15.3.1.2.11 */
  mrb_define_class_method(mrb, krn, "raise",                mrb_f_raise,                     MRB_ARGS_ANY());     /* 15.3.1.2.12 */

  mrb_define_method(mrb, krn, "singleton_class",            mrb_singleton_class,             MRB_ARGS_NONE());

  mrb_define_method(mrb, krn, "==",                         mrb_obj_equal_m,                 MRB_ARGS_REQ(1));    /* 15.3.1.3.1  */
  mrb_define_method(mrb, krn, "!=",                         mrb_obj_not_equal_m,             MRB_ARGS_REQ(1));
  mrb_define_method(mrb, krn, "===",                        mrb_equal_m,                     MRB_ARGS_REQ(1));    /* 15.3.1.3.2  */
  mrb_define_method(mrb, krn, "__id__",                     mrb_obj_id_m,                    MRB_ARGS_NONE());    /* 15.3.1.3.3  */
  mrb_define_method(mrb, krn, "__send__",                   mrb_f_send,                      MRB_ARGS_ANY());     /* 15.3.1.3.4  */
  mrb_define_method(mrb, krn, "block_given?",               mrb_f_block_given_p_m,           MRB_ARGS_NONE());    /* 15.3.1.3.6  */
  mrb_define_method(mrb, krn, "class",                      mrb_obj_class_m,                 MRB_ARGS_NONE());    /* 15.3.1.3.7  */
  mrb_define_method(mrb, krn, "clone",                      mrb_obj_clone,                   MRB_ARGS_NONE());    /* 15.3.1.3.8  */
  mrb_define_method(mrb, krn, "dup",                        mrb_obj_dup,                     MRB_ARGS_NONE());    /* 15.3.1.3.9  */
  mrb_define_method(mrb, krn, "eql?",                       mrb_obj_equal_m,                 MRB_ARGS_REQ(1));    /* 15.3.1.3.10 */
  mrb_define_method(mrb, krn, "equal?",                     mrb_obj_equal_m,                 MRB_ARGS_REQ(1));    /* 15.3.1.3.11 */
  mrb_define_method(mrb, krn, "extend",                     mrb_obj_extend_m,                MRB_ARGS_ANY());     /* 15.3.1.3.13 */
  mrb_define_method(mrb, krn, "global_variables",           mrb_f_global_variables,          MRB_ARGS_NONE());    /* 15.3.1.3.14 */
  mrb_define_method(mrb, krn, "hash",                       mrb_obj_hash,                    MRB_ARGS_NONE());    /* 15.3.1.3.15 */
  mrb_define_method(mrb, krn, "initialize_copy",            mrb_obj_init_copy,               MRB_ARGS_REQ(1));    /* 15.3.1.3.16 */
  mrb_define_method(mrb, krn, "inspect",                    mrb_obj_inspect,                 MRB_ARGS_NONE());    /* 15.3.1.3.17 */
  mrb_define_method(mrb, krn, "instance_eval",              mrb_obj_instance_eval,           MRB_ARGS_ANY());     /* 15.3.1.3.18 */
  mrb_define_method(mrb, krn, "instance_of?",               obj_is_instance_of,              MRB_ARGS_REQ(1));    /* 15.3.1.3.19 */
  mrb_define_method(mrb, krn, "instance_variable_defined?", mrb_obj_ivar_defined,            MRB_ARGS_REQ(1));    /* 15.3.1.3.20 */
  mrb_define_method(mrb, krn, "instance_variable_get",      mrb_obj_ivar_get,                MRB_ARGS_REQ(1));    /* 15.3.1.3.21 */
  mrb_define_method(mrb, krn, "instance_variable_set",      mrb_obj_ivar_set,                MRB_ARGS_REQ(2));    /* 15.3.1.3.22 */
  mrb_define_method(mrb, krn, "instance_variables",         mrb_obj_instance_variables,      MRB_ARGS_NONE());    /* 15.3.1.3.23 */
  mrb_define_method(mrb, krn, "is_a?",                      mrb_obj_is_kind_of_m,            MRB_ARGS_REQ(1));    /* 15.3.1.3.24 */
  mrb_define_method(mrb, krn, "iterator?",                  mrb_f_block_given_p_m,           MRB_ARGS_NONE());    /* 15.3.1.3.25 */
  mrb_define_method(mrb, krn, "kind_of?",                   mrb_obj_is_kind_of_m,            MRB_ARGS_REQ(1));    /* 15.3.1.3.26 */
  mrb_define_method(mrb, krn, "methods",                    mrb_obj_methods_m,               MRB_ARGS_OPT(1));    /* 15.3.1.3.31 */
  mrb_define_method(mrb, krn, "nil?",                       mrb_false,                       MRB_ARGS_NONE());    /* 15.3.1.3.32 */
  mrb_define_method(mrb, krn, "object_id",                  mrb_obj_id_m,                    MRB_ARGS_NONE());    /* 15.3.1.3.33 */
  mrb_define_method(mrb, krn, "private_methods",            mrb_obj_private_methods,         MRB_ARGS_OPT(1));    /* 15.3.1.3.36 */
  mrb_define_method(mrb, krn, "protected_methods",          mrb_obj_protected_methods,       MRB_ARGS_OPT(1));    /* 15.3.1.3.37 */
  mrb_define_method(mrb, krn, "public_methods",             mrb_obj_public_methods,          MRB_ARGS_OPT(1));    /* 15.3.1.3.38 */
  mrb_define_method(mrb, krn, "raise",                      mrb_f_raise,                     MRB_ARGS_ANY());     /* 15.3.1.3.40 */
  mrb_define_method(mrb, krn, "remove_instance_variable",   mrb_obj_remove_instance_variable,MRB_ARGS_REQ(1));    /* 15.3.1.3.41 */
  mrb_define_method(mrb, krn, "respond_to?",                obj_respond_to,                  MRB_ARGS_ANY());     /* 15.3.1.3.43 */
  mrb_define_method(mrb, krn, "send",                       mrb_f_send,                      MRB_ARGS_ANY());     /* 15.3.1.3.44 */
  mrb_define_method(mrb, krn, "singleton_methods",          mrb_obj_singleton_methods_m,     MRB_ARGS_OPT(1));    /* 15.3.1.3.45 */
  mrb_define_method(mrb, krn, "to_s",                       mrb_any_to_s,                    MRB_ARGS_NONE());    /* 15.3.1.3.46 */

  mrb_include_module(mrb, mrb->object_class, mrb->kernel_module);
  mrb_alias_method(mrb, mrb->module_class, mrb_intern2(mrb, "dup", 3), mrb_intern2(mrb, "clone", 5));
}
