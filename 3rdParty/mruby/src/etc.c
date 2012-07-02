/*
** etc.c -
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/string.h"
#include "error.h"
#include "mruby/numeric.h"
#include "mruby/data.h"

struct RData*
mrb_data_object_alloc(mrb_state *mrb, struct RClass *klass, void *ptr, const struct mrb_data_type *type)
{
  struct RData *data;

  data = (struct RData*)mrb_obj_alloc(mrb, MRB_TT_DATA, klass);
  data->data = ptr;
  data->type = (struct mrb_data_type*) type;

  return data;
}

void *
mrb_get_datatype(mrb_state *mrb, mrb_value obj, const struct mrb_data_type *type)
{
  if (SPECIAL_CONST_P(obj) || (mrb_type(obj) != MRB_TT_DATA)) {
    return NULL;
  }
  if (DATA_TYPE(obj) != type) {
    return NULL;
  }
  return DATA_PTR(obj);
}

void *
mrb_check_datatype(mrb_state *mrb, mrb_value obj, const struct mrb_data_type *type)
{
  static const char mesg[] = "wrong argument type %s (expected %s)";

  if (SPECIAL_CONST_P(obj) || (mrb_type(obj) != MRB_TT_DATA)) {
    mrb_check_type(mrb, obj, MRB_TT_DATA);
  }
  if (DATA_TYPE(obj) != type) {
    const char *etype = DATA_TYPE(obj)->struct_name;
    mrb_raise(mrb, E_TYPE_ERROR, mesg, etype, type->struct_name);
  }
  return DATA_PTR(obj);
}

mrb_value
mrb_lastline_get(mrb_state *mrb)
{
  //mrb_value *var = mrb_svar(0);
  //if (var) {
  //  return *var;
  //}
  //return mrb_nil_value();
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc < 1) {
    return mrb_nil_value();
  }
  else
  {
    return argv[0];
  }
}

/* ------------------------------------------------ */
/*
 * Calls func(obj, arg, recursive), where recursive is non-zero if the
 * current method is called recursively on obj
 */

mrb_value
mrb_exec_recursive(mrb_state *mrb, mrb_value (*func) (mrb_state *, mrb_value, mrb_value, int), mrb_value obj, void *arg)
{
  //  return mrb_exec_recursive(mrb, io_puts_ary, line, &out);
  return func(mrb, obj, *(mrb_value*)arg, 0);
}

/*
 * Calls func(obj, arg, recursive), where recursive is non-zero if the
 * current method is called recursively on the ordered pair <obj, paired_obj>
 */

mrb_sym
mrb_to_id(mrb_state *mrb, mrb_value name)
{
  mrb_value tmp;
  mrb_sym id;

  switch (mrb_type(name)) {
    default:
      tmp = mrb_check_string_type(mrb, name);
      if (mrb_nil_p(tmp)) {
        tmp = mrb_inspect(mrb, name);
        mrb_raise(mrb, E_TYPE_ERROR, "%s is not a symbol",
             RSTRING_PTR(tmp));
      }
      name = tmp;
      /* fall through */
    case MRB_TT_STRING:
      name = mrb_str_intern(mrb, name);
      /* fall through */
    case MRB_TT_SYMBOL:
      return SYM2ID(name);
  }
  return id;
}

/*
 * call-seq:
 *   proc   { |...| block }  -> a_proc
 *
 * Equivalent to <code>Proc.new</code>.
 */

mrb_value
mrb_block_proc(void)
{
  return mrb_nil_value();//proc_new(mrb_cProc, FALSE);
}

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

int
mrb_obj_id(mrb_value obj)
{
    /*
     *                32-bit mrb_value space
     *          MSB ------------------------ LSB
     *  false   00000000000000000000000000000000
     *  true    00000000000000000000000000000010
     *  nil     00000000000000000000000000000100
     *  undef   00000000000000000000000000000110
     *  symbol  ssssssssssssssssssssssss00001110
     *  object  oooooooooooooooooooooooooooooo00        = 0 (mod sizeof(RVALUE))
     *  fixnum  fffffffffffffffffffffffffffffff1
     *
     *                    object_id space
     *                                       LSB
     *  false   00000000000000000000000000000000
     *  true    00000000000000000000000000000010
     *  nil     00000000000000000000000000000100
     *  undef   00000000000000000000000000000110
     *  symbol   000SSSSSSSSSSSSSSSSSSSSSSSSSSS0        S...S % A = 4 (S...S = s...s * A + 4)
     *  object   oooooooooooooooooooooooooooooo0        o...o % A = 0
     *  fixnum  fffffffffffffffffffffffffffffff1        bignum if required
     *
     *  where A = sizeof(RVALUE)/4
     *
     *  sizeof(RVALUE) is
     *  20 if 32-bit, double is 4-byte aligned
     *  24 if 32-bit, double is 8-byte aligned
     *  40 if 64-bit
     */
    /*
     *                128-bit mrb_value space
     *          MSB -------- LSB
     *  x86           [0,1]             [2,3]            [4,5]            [6,7]            [8,9]            [A,B]            [C,D]            [E,F]
     *          7                6                5                4                3                2                1                0
     *          0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF
     *          FEDCBA9876543210 FEDCBA9876543210 FEDCBA9876543210 FEDCBA9876543210 FEDCBA9876543210 FEDCBA9876543210 FEDCBA9876543210 FEDCBA9876543210
     *  false   0000000000000000 0000000000000000 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxx00000001 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx
     *  true    0000000000000001 0000000000000000 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxx00000010 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx
     *  nil     0000000000000001 0000000000000000 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxx00000001 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx
     *  undef   0000000000000000 0000000000000000 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxx00000101 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx
     *  symbol  ssssssssssssssss ssssssssssssssss xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxx00000100 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx
     *  object  oooooooooooooooo oooooooooooooo00        = 0 (mod sizeof(RVALUE))
     (1)fixnum  0000000000000001 0000000000000000 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxx00000011 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx
     *  float   0000000000000001 0000000000000000 0000000000000000 0000000000000000 xxxxxxxx00000011 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx
     *          <--                          mrb_float                          --> xxxxxxxx00001101 xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx
     *
     *                    object_id space
     *                                       LSB
     *  false   0000000000000000 0000000000000000
     *  true    0000000000000000 0000000000000010
     *  nil     0000000000000000 0000000000000100
     *  undef   0000000000000000 0000000000000110
     *  symbol  000SSSSSSSSSSSS SSSSSSSSSSSSSSS0        S...S % A = 4 (S...S = s...s * A + 4)
     *  object  ooooooooooooooo ooooooooooooooo0        o...o % A = 0
     *  fixnum  ffffffffffffffff fffffffffffffff1        bignum if required
     *
     *  where A = sizeof(RVALUE)/4
     *
     *  sizeof(RVALUE) is
     *  20 if 32-bit, double is 4-byte aligned
     *  24 if 32-bit, double is 8-byte aligned
     *  40 if 64-bit
     */
    /* tt:0_27 */
  switch (mrb_type(obj)) {
    case  MRB_TT_FREE:
      return 0; /* not define */
    case  MRB_TT_FALSE:
      if (mrb_nil_p(obj))
        return 4;
      return 0;
    case  MRB_TT_TRUE:
      return 2;
    case  MRB_TT_FIXNUM:
      return mrb_fixnum(obj)*2+1; /* odd number */
    case  MRB_TT_SYMBOL:
      return SYM2ID(obj) * 2;
    case  MRB_TT_UNDEF:
      return 0; /* not define */
    case  MRB_TT_FLOAT:
      return (int)mrb_float(obj)*2; /* even number */
    case  MRB_TT_OBJECT:
    case  MRB_TT_CLASS:
    case  MRB_TT_MODULE:
    case  MRB_TT_ICLASS:
    case  MRB_TT_SCLASS:
    case  MRB_TT_PROC:
    case  MRB_TT_ARRAY:
    case  MRB_TT_HASH:
    case  MRB_TT_STRING:
    case  MRB_TT_RANGE:
    case  MRB_TT_REGEX:
    case  MRB_TT_STRUCT:
    case  MRB_TT_EXCEPTION:
    case  MRB_TT_MATCH:
    case  MRB_TT_FILE:
    case  MRB_TT_DATA:
    case  MRB_TT_THREAD:
    case  MRB_TT_THREADGRP:
    default:
      return mrb_fixnum(obj); /* even number */
  }
}

