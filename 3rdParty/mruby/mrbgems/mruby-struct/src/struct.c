/*
** struct.c - Struct class
**
** See Copyright Notice in mruby.h
*/

#include <string.h>
#include <stdarg.h>
#include "mruby.h"
#include "mruby/array.h"
#include "mruby/string.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/variable.h"

#define RSTRUCT_ARY(st) mrb_ary_ptr(st)
#define RSTRUCT_LEN(st) RSTRUCT_ARY(st)->len
#define RSTRUCT_PTR(st) RSTRUCT_ARY(st)->ptr

static struct RClass *
struct_class(mrb_state *mrb)
{
  return mrb_class_get(mrb, "Struct");
}

static inline mrb_value
struct_ivar_get(mrb_state *mrb, mrb_value c, mrb_sym id)
{
  struct RClass* kclass;
  struct RClass* sclass = struct_class(mrb);
  mrb_value ans;

  for (;;) {
    ans = mrb_iv_get(mrb, c, id);
    if (!mrb_nil_p(ans)) return ans;
    kclass = RCLASS_SUPER(c);
    if (kclass == 0 || kclass == sclass)
      return mrb_nil_value();
    c = mrb_obj_value(kclass);
  }
}

mrb_value
mrb_struct_iv_get(mrb_state *mrb, mrb_value c, const char *name)
{
  return struct_ivar_get(mrb, c, mrb_intern_cstr(mrb, name));
}

mrb_value
mrb_struct_s_members(mrb_state *mrb, mrb_value klass)
{
  mrb_value members = struct_ivar_get(mrb, klass, mrb_intern2(mrb, "__members__", 11));

  if (mrb_nil_p(members)) {
    mrb_raise(mrb, E_TYPE_ERROR, "uninitialized struct");
  }
  if (!mrb_array_p(members)) {
    mrb_raise(mrb, E_TYPE_ERROR, "corrupted struct");
  }
  return members;
}

mrb_value
mrb_struct_members(mrb_state *mrb, mrb_value s)
{
  mrb_value members = mrb_struct_s_members(mrb, mrb_obj_value(mrb_obj_class(mrb, s)));
  if (!strcmp(mrb_class_name(mrb, mrb_obj_class(mrb, s)), "Struct")) {
    if (RSTRUCT_LEN(s) != RARRAY_LEN(members)) {
      mrb_raisef(mrb, E_TYPE_ERROR,
                 "struct size differs (%S required %S given)",
                 mrb_fixnum_value(RARRAY_LEN(members)), mrb_fixnum_value(RSTRUCT_LEN(s)));
    }
  }
  return members;
}

static mrb_value
mrb_struct_s_members_m(mrb_state *mrb, mrb_value klass)
{
  mrb_value members, ary;
  mrb_value *p, *pend;

  members = mrb_struct_s_members(mrb, klass);
  ary = mrb_ary_new_capa(mrb, RARRAY_LEN(members));
  p = RARRAY_PTR(members); pend = p + RARRAY_LEN(members);
  while (p < pend) {
    mrb_ary_push(mrb, ary, *p);
    p++;
  }
  return ary;
}

/* 15.2.18.4.6  */
/*
 *  call-seq:
 *     struct.members    -> array
 *
 *  Returns an array of strings representing the names of the instance
 *  variables.
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     joe.members   #=> [:name, :address, :zip]
 */

static mrb_value
mrb_struct_members_m(mrb_state *mrb, mrb_value obj)
{
  return mrb_struct_s_members_m(mrb, mrb_obj_value(mrb_obj_class(mrb, obj)));
}

mrb_value
mrb_struct_getmember(mrb_state *mrb, mrb_value obj, mrb_sym id)
{
  mrb_value members, slot, *ptr, *ptr_members;
  mrb_int i, len;

  ptr = RSTRUCT_PTR(obj);
  members = mrb_struct_members(mrb, obj);
  ptr_members = RARRAY_PTR(members);
  slot = mrb_symbol_value(id);
  len = RARRAY_LEN(members);
  for (i=0; i<len; i++) {
    if (mrb_obj_equal(mrb, ptr_members[i], slot)) {
      return ptr[i];
    }
  }
  mrb_raisef(mrb, E_INDEX_ERROR, "%S is not struct member", mrb_sym2str(mrb, id));
  return mrb_nil_value();       /* not reached */
}

static mrb_value
mrb_struct_ref(mrb_state *mrb, mrb_value obj)
{
  return mrb_struct_getmember(mrb, obj, mrb->c->ci->mid);
}

static mrb_value mrb_struct_ref0(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[0];}
static mrb_value mrb_struct_ref1(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[1];}
static mrb_value mrb_struct_ref2(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[2];}
static mrb_value mrb_struct_ref3(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[3];}
static mrb_value mrb_struct_ref4(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[4];}
static mrb_value mrb_struct_ref5(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[5];}
static mrb_value mrb_struct_ref6(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[6];}
static mrb_value mrb_struct_ref7(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[7];}
static mrb_value mrb_struct_ref8(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[8];}
static mrb_value mrb_struct_ref9(mrb_state* mrb, mrb_value obj) {return RSTRUCT_PTR(obj)[9];}

#define numberof(array) (int)(sizeof(array) / sizeof((array)[0]))
#define N_REF_FUNC numberof(ref_func)

static mrb_value (*const ref_func[])(mrb_state*, mrb_value) = {
  mrb_struct_ref0,
  mrb_struct_ref1,
  mrb_struct_ref2,
  mrb_struct_ref3,
  mrb_struct_ref4,
  mrb_struct_ref5,
  mrb_struct_ref6,
  mrb_struct_ref7,
  mrb_struct_ref8,
  mrb_struct_ref9,
};

mrb_sym
mrb_id_attrset(mrb_state *mrb, mrb_sym id)
{
  const char *name;
  char *buf;
  size_t len;
  mrb_sym mid;

  name = mrb_sym2name_len(mrb, id, &len);
  buf = (char *)mrb_malloc(mrb, len+2);
  memcpy(buf, name, len);
  buf[len] = '=';
  buf[len+1] = '\0';

  mid = mrb_intern2(mrb, buf, len+1);
  mrb_free(mrb, buf);
  return mid;
}

static mrb_value
mrb_struct_set(mrb_state *mrb, mrb_value obj, mrb_value val)
{
  const char *name;
  size_t i, len;
  mrb_sym mid;
  mrb_value members, slot, *ptr, *ptr_members;

  /* get base id */
  name = mrb_sym2name_len(mrb, mrb->c->ci->mid, &len);
  mid = mrb_intern2(mrb, name, len-1); /* omit last "=" */

  members = mrb_struct_members(mrb, obj);
  ptr_members = RARRAY_PTR(members);
  len = RARRAY_LEN(members);
  ptr = RSTRUCT_PTR(obj);
  for (i=0; i<len; i++) {
    slot = ptr_members[i];
    if (mrb_symbol(slot) == mid) {
      return ptr[i] = val;
    }
  }
  mrb_raisef(mrb, E_INDEX_ERROR, "`%S' is not a struct member",
             mrb_sym2str(mrb, mid));
  return mrb_nil_value();            /* not reached */
}

static mrb_value
mrb_struct_set_m(mrb_state *mrb, mrb_value obj)
{
  mrb_value val;

  mrb_get_args(mrb, "o", &val);
  return mrb_struct_set(mrb, obj, val);
}

#define is_notop_id(id) (id)//((id)>tLAST_TOKEN)
#define is_local_id(id) (is_notop_id(id))//&&((id)&ID_SCOPE_MASK)==ID_LOCAL)
int
mrb_is_local_id(mrb_sym id)
{
  return is_local_id(id);
}

#define is_const_id(id) (is_notop_id(id))//&&((id)&ID_SCOPE_MASK)==ID_CONST)
int
mrb_is_const_id(mrb_sym id)
{
  return is_const_id(id);
}

static mrb_value
make_struct(mrb_state *mrb, mrb_value name, mrb_value members, struct RClass * klass)
{
  mrb_value nstr, *ptr_members;
  mrb_sym id;
  mrb_int i, len;
  struct RClass *c;

  if (mrb_nil_p(name)) {
    c = mrb_class_new(mrb, klass);
  }
  else {
    /* old style: should we warn? */
    name = mrb_str_to_str(mrb, name);
    id = mrb_obj_to_sym(mrb, name);
    if (!mrb_is_const_id(id)) {
      mrb_name_error(mrb, id, "identifier %S needs to be constant", name);
    }
    if (mrb_const_defined_at(mrb, klass, id)) {
      mrb_warn(mrb, "redefining constant Struct::%S", name);
      //?rb_mod_remove_const(klass, mrb_sym2name(mrb, id));
    }
    c = mrb_define_class_under(mrb, klass, RSTRING_PTR(name), klass);
  }
  MRB_SET_INSTANCE_TT(c, MRB_TT_ARRAY);
  nstr = mrb_obj_value(c);
  mrb_iv_set(mrb, nstr, mrb_intern2(mrb, "__members__", 11), members);

  mrb_define_class_method(mrb, c, "new", mrb_instance_new, MRB_ARGS_ANY());
  mrb_define_class_method(mrb, c, "[]", mrb_instance_new, MRB_ARGS_ANY());
  mrb_define_class_method(mrb, c, "members", mrb_struct_s_members_m, MRB_ARGS_NONE());
  //RSTRUCT(nstr)->basic.c->super = c->c;
  ptr_members = RARRAY_PTR(members);
  len = RARRAY_LEN(members);
  for (i=0; i< len; i++) {
    mrb_sym id = mrb_symbol(ptr_members[i]);
    if (mrb_is_local_id(id) || mrb_is_const_id(id)) {
      if (i < N_REF_FUNC) {
        mrb_define_method_id(mrb, c, id, ref_func[i], MRB_ARGS_NONE());
      }
      else {
        mrb_define_method_id(mrb, c, id, mrb_struct_ref, MRB_ARGS_NONE());
      }
      mrb_define_method_id(mrb, c, mrb_id_attrset(mrb, id), mrb_struct_set_m, MRB_ARGS_REQ(1));
    }
  }
  return nstr;
}

mrb_value
mrb_struct_define(mrb_state *mrb, const char *name, ...)
{
  va_list ar;
  mrb_value nm, ary;
  char *mem;

  if (!name) nm = mrb_nil_value();
  else nm = mrb_str_new_cstr(mrb, name);
  ary = mrb_ary_new(mrb);

  va_start(ar, name);
  while ((mem = va_arg(ar, char*)) != 0) {
    mrb_sym slot = mrb_intern(mrb, mem);
    mrb_ary_push(mrb, ary, mrb_symbol_value(slot));
  }
  va_end(ar);

  return make_struct(mrb, nm, ary, struct_class(mrb));
}

/* 15.2.18.3.1  */
/*
 *  call-seq:
 *     Struct.new( [aString] [, aSym]+> )    -> StructClass
 *     StructClass.new(arg, ...)             -> obj
 *     StructClass[arg, ...]                 -> obj
 *
 *  Creates a new class, named by <i>aString</i>, containing accessor
 *  methods for the given symbols. If the name <i>aString</i> is
 *  omitted, an anonymous structure class will be created. Otherwise,
 *  the name of this struct will appear as a constant in class
 *  <code>Struct</code>, so it must be unique for all
 *  <code>Struct</code>s in the system and should start with a capital
 *  letter. Assigning a structure class to a constant effectively gives
 *  the class the name of the constant.
 *
 *  <code>Struct::new</code> returns a new <code>Class</code> object,
 *  which can then be used to create specific instances of the new
 *  structure. The number of actual parameters must be
 *  less than or equal to the number of attributes defined for this
 *  class; unset parameters default to <code>nil</code>.  Passing too many
 *  parameters will raise an <code>ArgumentError</code>.
 *
 *  The remaining methods listed in this section (class and instance)
 *  are defined for this generated class.
 *
 *     # Create a structure with a name in Struct
 *     Struct.new("Customer", :name, :address)    #=> Struct::Customer
 *     Struct::Customer.new("Dave", "123 Main")   #=> #<struct Struct::Customer name="Dave", address="123 Main">
 *
 *     # Create a structure named by its constant
 *     Customer = Struct.new(:name, :address)     #=> Customer
 *     Customer.new("Dave", "123 Main")           #=> #<struct Customer name="Dave", address="123 Main">
 */
static mrb_value
mrb_struct_s_def(mrb_state *mrb, mrb_value klass)
{
  mrb_value name, rest;
  mrb_value *pargv;
  int argcnt;
  mrb_int i;
  mrb_value b, st;
  mrb_sym id;
  mrb_value *argv;
  int argc;

  name = mrb_nil_value();
  rest = mrb_nil_value();
  mrb_get_args(mrb, "*&", &argv, &argc, &b);
  if (argc == 0) { /* special case to avoid crash */
    rest = mrb_ary_new(mrb);
  }
  else {
    if (argc > 0) name = argv[0];
    if (argc > 1) rest = argv[1];
    if (mrb_array_p(rest)) {
      if (!mrb_nil_p(name) && mrb_symbol_p(name)) {
        /* 1stArgument:symbol -> name=nil rest=argv[0]-[n] */
        mrb_ary_unshift(mrb, rest, name);
        name = mrb_nil_value();
      }
    }
    else {
      pargv = &argv[1];
      argcnt = argc-1;
      if (!mrb_nil_p(name) && mrb_symbol_p(name)) {
        /* 1stArgument:symbol -> name=nil rest=argv[0]-[n] */
        name = mrb_nil_value();
        pargv = &argv[0];
        argcnt++;
      }
      rest = mrb_ary_new_from_values(mrb, argcnt, pargv);
    }
    for (i=0; i<RARRAY_LEN(rest); i++) {
      id = mrb_obj_to_sym(mrb, RARRAY_PTR(rest)[i]);
      RARRAY_PTR(rest)[i] = mrb_symbol_value(id);
    }
  }
  st = make_struct(mrb, name, rest, struct_class(mrb));
  if (!mrb_nil_p(b)) {
    mrb_funcall(mrb, b, "call", 1, &st);
  }

  return st;
}

static int
num_members(mrb_state *mrb, struct RClass *klass)
{
  mrb_value members;

  members = struct_ivar_get(mrb, mrb_obj_value(klass), mrb_intern2(mrb, "__members__", 11));
  if (!mrb_array_p(members)) {
    mrb_raise(mrb, E_TYPE_ERROR, "broken members");
  }
  return RARRAY_LEN(members);
}

/* 15.2.18.4.8  */
/*
 */
static mrb_value
mrb_struct_initialize_withArg(mrb_state *mrb, int argc, mrb_value *argv, mrb_value self)
{
  struct RClass *klass = mrb_obj_class(mrb, self);
  int i, n;

  n = num_members(mrb, klass);
  if (n < argc) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "struct size differs");
  }

  for (i = 0; i < argc; i++) {
    mrb_ary_set(mrb, self, i, argv[i]);
  }
  for (i = argc; i < n; i++) {
    mrb_ary_set(mrb, self, i, mrb_nil_value());
  }
  return self;
}

static mrb_value
mrb_struct_initialize_m(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_struct_initialize_withArg(mrb, argc, argv, self);
}

mrb_value
mrb_struct_initialize(mrb_state *mrb, mrb_value self, mrb_value values)
{
  return mrb_struct_initialize_withArg(mrb, RARRAY_LEN(values), RARRAY_PTR(values), self);
}

static mrb_value
inspect_struct(mrb_state *mrb, mrb_value s, int recur)
{
  const char *cn = mrb_class_name(mrb, mrb_obj_class(mrb, s));
  mrb_value members, str = mrb_str_new(mrb, "#<struct ", 9);
  mrb_value *ptr, *ptr_members;
  mrb_int i, len;

  if (cn) {
    mrb_str_append(mrb, str, mrb_str_new_cstr(mrb, cn));
  }
  if (recur) {
    return mrb_str_cat2(mrb, str, ":...>");
  }

  members = mrb_struct_members(mrb, s);
  ptr_members = RARRAY_PTR(members);
  ptr = RSTRUCT_PTR(s);
  len = RSTRUCT_LEN(s);
  for (i=0; i<len; i++) {
    mrb_value slot;
    mrb_sym id;

    if (i > 0) {
      mrb_str_cat2(mrb, str, ", ");
    }
    else if (cn) {
      mrb_str_cat2(mrb, str, " ");
    }
    slot = ptr_members[i];
    id = mrb_symbol(slot);
    if (mrb_is_local_id(id) || mrb_is_const_id(id)) {
      const char *name;
      size_t len;

      name = mrb_sym2name_len(mrb, id, &len);
      mrb_str_append(mrb, str, mrb_str_new(mrb, name, len));
    }
    else {
      mrb_str_append(mrb, str, mrb_inspect(mrb, slot));
    }
    mrb_str_cat2(mrb, str, "=");
    mrb_str_append(mrb, str, mrb_inspect(mrb, ptr[i]));
  }
  mrb_str_cat2(mrb, str, ">");

  return str;
}

/*
 * call-seq:
 *   struct.to_s      -> string
 *   struct.inspect   -> string
 *
 * Describe the contents of this struct in a string.
 */
static mrb_value
mrb_struct_inspect(mrb_state *mrb, mrb_value s)
{
  return inspect_struct(mrb, s, 0);
}

/* 15.2.18.4.9  */
/* :nodoc: */
mrb_value
mrb_struct_init_copy(mrb_state *mrb, mrb_value copy)
{
  mrb_value s;
  int i, len;

  mrb_get_args(mrb, "o", &s);

  if (mrb_obj_equal(mrb, copy, s)) return copy;
  if (!mrb_obj_is_instance_of(mrb, s, mrb_obj_class(mrb, copy))) {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }
  if (!mrb_array_p(s)) {
    mrb_raise(mrb, E_TYPE_ERROR, "corrupted struct");
  }
  if (RSTRUCT_LEN(copy) != RSTRUCT_LEN(s)) {
    mrb_raise(mrb, E_TYPE_ERROR, "struct size mismatch");
  }
  len = RSTRUCT_LEN(copy);
  for (i = 0; i < len; i++) {
    mrb_ary_set(mrb, copy, i, RSTRUCT_PTR(s)[i]);
  }
  return copy;
}

static mrb_value
mrb_struct_aref_id(mrb_state *mrb, mrb_value s, mrb_sym id)
{
  mrb_value *ptr, members, *ptr_members;
  mrb_int i, len;

  ptr = RSTRUCT_PTR(s);
  members = mrb_struct_members(mrb, s);
  ptr_members = RARRAY_PTR(members);
  len = RARRAY_LEN(members);
  for (i=0; i<len; i++) {
    if (mrb_symbol(ptr_members[i]) == id) {
      return ptr[i];
    }
  }
  mrb_raisef(mrb, E_INDEX_ERROR, "no member '%S' in struct", mrb_sym2str(mrb, id));
  return mrb_nil_value();       /* not reached */
}

/* 15.2.18.4.2  */
/*
 *  call-seq:
 *     struct[symbol]    -> anObject
 *     struct[fixnum]    -> anObject
 *
 *  Attribute Reference---Returns the value of the instance variable
 *  named by <i>symbol</i>, or indexed (0..length-1) by
 *  <i>fixnum</i>. Will raise <code>NameError</code> if the named
 *  variable does not exist, or <code>IndexError</code> if the index is
 *  out of range.
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *
 *     joe["name"]   #=> "Joe Smith"
 *     joe[:name]    #=> "Joe Smith"
 *     joe[0]        #=> "Joe Smith"
 */
mrb_value
mrb_struct_aref_n(mrb_state *mrb, mrb_value s, mrb_value idx)
{
  mrb_int i;

  if (mrb_string_p(idx)) {
    mrb_value sym = mrb_check_intern_str(mrb, idx);

    if (mrb_nil_p(sym)) {
      mrb_raisef(mrb, E_INDEX_ERROR, "no member '%S' in struct", idx);
    }
    idx = sym;
  }
  if (mrb_symbol_p(idx)) {
    return mrb_struct_aref_id(mrb, s, mrb_symbol(idx));
  }

  i = mrb_fixnum(idx);
  if (i < 0) i = RSTRUCT_LEN(s) + i;
  if (i < 0)
      mrb_raisef(mrb, E_INDEX_ERROR,
                 "offset %S too small for struct(size:%S)",
                 mrb_fixnum_value(i), mrb_fixnum_value(RSTRUCT_LEN(s)));
  if (RSTRUCT_LEN(s) <= i)
    mrb_raisef(mrb, E_INDEX_ERROR,
               "offset %S too large for struct(size:%S)",
               mrb_fixnum_value(i), mrb_fixnum_value(RSTRUCT_LEN(s)));
  return RSTRUCT_PTR(s)[i];
}

mrb_value
mrb_struct_aref(mrb_state *mrb, mrb_value s)
{
  mrb_value idx;

  mrb_get_args(mrb, "o", &idx);
  return mrb_struct_aref_n(mrb, s, idx);
}

static mrb_value
mrb_struct_aset_id(mrb_state *mrb, mrb_value s, mrb_sym id, mrb_value val)
{
  mrb_value members, *ptr, *ptr_members;
  mrb_int i, len;

  members = mrb_struct_members(mrb, s);
  len = RARRAY_LEN(members);
  if (RSTRUCT_LEN(s) != len) {
    mrb_raisef(mrb, E_TYPE_ERROR,
               "struct size differs (%S required %S given)",
               mrb_fixnum_value(len), mrb_fixnum_value(RSTRUCT_LEN(s)));
  }
  ptr = RSTRUCT_PTR(s);
  ptr_members = RARRAY_PTR(members);
  for (i=0; i<len; i++) {
    if (mrb_symbol(ptr_members[i]) == id) {
      ptr[i] = val;
      return val;
    }
  }
  mrb_raisef(mrb, E_INDEX_ERROR, "no member '%S' in struct", mrb_sym2str(mrb, id));
  return val;                   /* not reach */
}

/* 15.2.18.4.3  */
/*
 *  call-seq:
 *     struct[symbol] = obj    -> obj
 *     struct[fixnum] = obj    -> obj
 *
 *  Attribute Assignment---Assigns to the instance variable named by
 *  <i>symbol</i> or <i>fixnum</i> the value <i>obj</i> and
 *  returns it. Will raise a <code>NameError</code> if the named
 *  variable does not exist, or an <code>IndexError</code> if the index
 *  is out of range.
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *
 *     joe["name"] = "Luke"
 *     joe[:zip]   = "90210"
 *
 *     joe.name   #=> "Luke"
 *     joe.zip    #=> "90210"
 */

mrb_value
mrb_struct_aset(mrb_state *mrb, mrb_value s)
{
  mrb_int i;
  mrb_value idx;
  mrb_value val;

  mrb_get_args(mrb, "oo", &idx, &val);

  if (mrb_string_p(idx) || mrb_symbol_p(idx)) {
    return mrb_struct_aset_id(mrb, s, mrb_obj_to_sym(mrb, idx), val);
  }

  i = mrb_fixnum(idx);
  if (i < 0) i = RSTRUCT_LEN(s) + i;
  if (i < 0) {
    mrb_raisef(mrb, E_INDEX_ERROR,
               "offset %S too small for struct(size:%S)",
               mrb_fixnum_value(i), mrb_fixnum_value(RSTRUCT_LEN(s)));
  }
  if (RSTRUCT_LEN(s) <= i) {
    mrb_raisef(mrb, E_INDEX_ERROR,
               "offset %S too large for struct(size:%S)",
               mrb_fixnum_value(i), mrb_fixnum_value(RSTRUCT_LEN(s)));
  }
  return RSTRUCT_PTR(s)[i] = val;
}

/* 15.2.18.4.1  */
/*
 *  call-seq:
 *     struct == other_struct     -> true or false
 *
 *  Equality---Returns <code>true</code> if <i>other_struct</i> is
 *  equal to this one: they must be of the same class as generated by
 *  <code>Struct::new</code>, and the values of all instance variables
 *  must be equal (according to <code>Object#==</code>).
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe   = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     joejr = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     jane  = Customer.new("Jane Doe", "456 Elm, Anytown NC", 12345)
 *     joe == joejr   #=> true
 *     joe == jane    #=> false
 */

static mrb_value
mrb_struct_equal(mrb_state *mrb, mrb_value s)
{
  mrb_value s2;
  mrb_value *ptr, *ptr2;
  mrb_int i, len;
  mrb_bool equal_p;

  mrb_get_args(mrb, "o", &s2);
  if (mrb_obj_equal(mrb, s, s2)) {
    equal_p = 1;
  }
  else if (!strcmp(mrb_class_name(mrb, mrb_obj_class(mrb, s)), "Struct") ||
           mrb_obj_class(mrb, s) != mrb_obj_class(mrb, s2)) {
    equal_p = 0;
  }
  else if (RSTRUCT_LEN(s) != RSTRUCT_LEN(s2)) {
    mrb_bug(mrb, "inconsistent struct"); /* should never happen */
    equal_p = 0; /* This substuture is just to suppress warnings. never called. */
  }
  else {
    ptr = RSTRUCT_PTR(s);
    ptr2 = RSTRUCT_PTR(s2);
    len = RSTRUCT_LEN(s);
    equal_p = 1;
    for (i=0; i<len; i++) {
      if (!mrb_equal(mrb, ptr[i], ptr2[i])) {
        equal_p = 0;
        break;
      }
    }
  }

  return mrb_bool_value(equal_p);
}

/* 15.2.18.4.12(x)  */
/*
 * code-seq:
 *   struct.eql?(other)   -> true or false
 *
 * Two structures are equal if they are the same object, or if all their
 * fields are equal (using <code>eql?</code>).
 */
static mrb_value
mrb_struct_eql(mrb_state *mrb, mrb_value s)
{
  mrb_value s2;
  mrb_value *ptr, *ptr2;
  mrb_int i, len;
  mrb_bool eql_p;

  mrb_get_args(mrb, "o", &s2);
  if (mrb_obj_equal(mrb, s, s2)) {
    eql_p = 1;
  }
  else if (strcmp(mrb_class_name(mrb, mrb_obj_class(mrb, s2)), "Struct") ||
           mrb_obj_class(mrb, s) != mrb_obj_class(mrb, s2)) {
    eql_p = 0;
  }
  else if (RSTRUCT_LEN(s) != RSTRUCT_LEN(s2)) {
    mrb_bug(mrb, "inconsistent struct"); /* should never happen */
    eql_p = 0; /* This substuture is just to suppress warnings. never called. */
  }
  else {
    ptr = RSTRUCT_PTR(s);
    ptr2 = RSTRUCT_PTR(s2);
    len = RSTRUCT_LEN(s);
    eql_p = 1;
    for (i=0; i<len; i++) {
      if (!mrb_eql(mrb, ptr[i], ptr2[i])) {
        eql_p = 0;
        break;
      }
    }
  }

  return mrb_bool_value(eql_p);
}

/*
 *  A <code>Struct</code> is a convenient way to bundle a number of
 *  attributes together, using accessor methods, without having to write
 *  an explicit class.
 *
 *  The <code>Struct</code> class is a generator of specific classes,
 *  each one of which is defined to hold a set of variables and their
 *  accessors. In these examples, we'll call the generated class
 *  ``<i>Customer</i>Class,'' and we'll show an example instance of that
 *  class as ``<i>Customer</i>Inst.''
 *
 *  In the descriptions that follow, the parameter <i>symbol</i> refers
 *  to a symbol, which is either a quoted string or a
 *  <code>Symbol</code> (such as <code>:name</code>).
 */
void
mrb_mruby_struct_gem_init(mrb_state* mrb)
{
  struct RClass *st;
  st = mrb_define_class(mrb, "Struct",  mrb->object_class);

  mrb_define_class_method(mrb, st, "new",             mrb_struct_s_def,       MRB_ARGS_ANY());  /* 15.2.18.3.1  */

  mrb_define_method(mrb, st,       "==",              mrb_struct_equal,       MRB_ARGS_REQ(1)); /* 15.2.18.4.1  */
  mrb_define_method(mrb, st,       "[]",              mrb_struct_aref,        MRB_ARGS_REQ(1)); /* 15.2.18.4.2  */
  mrb_define_method(mrb, st,       "[]=",             mrb_struct_aset,        MRB_ARGS_REQ(2)); /* 15.2.18.4.3  */
  mrb_define_method(mrb, st,       "members",         mrb_struct_members_m,   MRB_ARGS_NONE()); /* 15.2.18.4.6  */
  mrb_define_method(mrb, st,       "initialize",      mrb_struct_initialize_m,MRB_ARGS_ANY());  /* 15.2.18.4.8  */
  mrb_define_method(mrb, st,       "initialize_copy", mrb_struct_init_copy,   MRB_ARGS_REQ(1)); /* 15.2.18.4.9  */
  mrb_define_method(mrb, st,       "inspect",         mrb_struct_inspect,     MRB_ARGS_NONE()); /* 15.2.18.4.10(x)  */
  mrb_define_alias(mrb, st,        "to_s", "inspect");                                      /* 15.2.18.4.11(x)  */
  mrb_define_method(mrb, st,       "eql?",            mrb_struct_eql,         MRB_ARGS_REQ(1)); /* 15.2.18.4.12(x)  */
}

void
mrb_mruby_struct_gem_final(mrb_state* mrb)
{
}
