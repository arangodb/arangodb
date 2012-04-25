/*
** enum.c - Enumerable module
** 
** See Copyright Notice in mruby.h
*/

#include "mruby.h"

#if 0

#include "mruby/struct.h"
#include "mruby/array.h"

static inline mrb_value
mrb_call0(mrb_state *mrb, mrb_value recv, mrb_sym mid, int argc, const mrb_value *argv,
         call_type scope, mrb_value self)
{
  return mrb_funcall(mrb, recv, mrb_sym2name(mrb, mid), argc, argv);
}
static inline mrb_value
mrb_call(mrb_state *mrb, mrb_value recv, mrb_sym mid, int argc, const mrb_value *argv, call_type scope)
{
    return mrb_call0(mrb, recv, mid, argc, argv, scope, mrb_fixnum_value(0)/*Qundef*/);
}

mrb_value rb_mEnumerable;
static mrb_sym id_each, id_eqq, id_cmp, id_next, id_size;

struct iter_method_arg {
    mrb_value obj;
    mrb_sym mid;
    int argc;
    mrb_value *argv;
};

static mrb_value
iterate_method(mrb_state *mrb, void *obj)
{
    const struct iter_method_arg * arg =
      (struct iter_method_arg *) obj;

    return mrb_call(mrb, arg->obj, arg->mid, arg->argc, arg->argv, CALL_FCALL);
}

#ifndef ANYARGS
# ifdef __cplusplus
#   define ANYARGS ...
# else
#   define ANYARGS
# endif
#endif

mrb_value
mrb_iterate(mrb_state *mrb,
           mrb_value (* it_proc) (mrb_state *, void*), void *data1,
           mrb_value (* bl_proc) (ANYARGS),
           void *data2)
{
    mrb_value retval = mrb_nil_value();
    retval = (*bl_proc) (data2);
    retval = (*it_proc) (mrb, data1);
  return retval;
}

mrb_value
mrb_block_call(mrb_state *mrb, mrb_value obj, mrb_sym mid, int argc, mrb_value * argv,
              mrb_value (*bl_proc) (ANYARGS),
              void *data2)
{
    struct iter_method_arg arg;

    arg.obj = obj;
    arg.mid = mid;
    arg.argc = argc;
    arg.argv = argv;
    return mrb_iterate(mrb, iterate_method, &arg, bl_proc, data2);
}

static mrb_value
enum_values_pack(mrb_state *mrb, int argc, mrb_value *argv)
{
    if (argc == 0) return mrb_nil_value();
    if (argc == 1) return argv[0];
    return mrb_ary_new4(mrb, argc, argv);
}

#define ENUM_WANT_SVALUE(mrb) do { \
    i = enum_values_pack(mrb, argc, argv); \
} while (0)

#define enum_yield mrb_yield_values2
mrb_value
mrb_yield_values2(int argc, const mrb_value *argv)
{
    //return mrb_yield_0(argc, argv);
    return mrb_nil_value(); /* dummy */
}

static mrb_value
grep_i(mrb_state *mrb, mrb_value i, mrb_value args, int argc, mrb_value *argv)
{
    mrb_value *arg = &args;
    ENUM_WANT_SVALUE(mrb);

    if (RTEST(mrb_funcall(mrb, arg[0], "===", 1, i))) {
      mrb_ary_push(mrb, arg[1], i);
    }
    return mrb_nil_value();
}

static mrb_value
grep_iter_i(mrb_state *mrb, mrb_value i, mrb_value args, int argc, mrb_value *argv)
{
    mrb_value *arg = &args;
    ENUM_WANT_SVALUE(mrb);

    if (RTEST(mrb_funcall(mrb, arg[0], "===", 1, i))) {
      mrb_ary_push(mrb, arg[1], mrb_yield(i));
    }
    return mrb_nil_value();
}

/* 15.3.2.2.9  */
/*
 *  call-seq:
 *     enum.grep(pattern)                   -> array
 *     enum.grep(pattern) {| obj | block }  -> array
 *
 *  Returns an array of every element in <i>enum</i> for which
 *  <code>Pattern === element</code>. If the optional <em>block</em> is
 *  supplied, each matching element is passed to it, and the block's
 *  result is stored in the output array.
 *
 *     (1..100).grep 38..44   #=> [38, 39, 40, 41, 42, 43, 44]
 *     c = IO.constants
 *     c.grep(/SEEK/)         #=> [:SEEK_SET, :SEEK_CUR, :SEEK_END]
 *     res = c.grep(/SEEK/) {|v| IO.const_get(v) }
 *     res                    #=> [0, 1, 2]
 *
 */

static mrb_value
enum_grep(mrb_state *mrb, mrb_value obj)
{
  mrb_value ary = mrb_ary_new(mrb);
  mrb_value arg[2];
  mrb_value pat;

  mrb_get_args(mrb, "o", &pat);

  arg[0] = pat;
  arg[1] = ary;

  mrb_block_call(mrb, obj, id_each, 0, 0, mrb_block_given_p() ? grep_iter_i : grep_i, arg);

  return ary;
}

/*
 *  call-seq:
 *     enum.count                   -> int
 *     enum.count(item)             -> int
 *     enum.count {| obj | block }  -> int
 *
 *  Returns the number of items in <i>enum</i>, where #size is called
 *  if it responds to it, otherwise the items are counted through
 *  enumeration.  If an argument is given, counts the number of items
 *  in <i>enum</i>, for which equals to <i>item</i>.  If a block is
 *  given, counts the number of elements yielding a true value.
 *
 *     ary = [1, 2, 4, 2]
 *     ary.count             #=> 4
 *     ary.count(2)          #=> 2
 *     ary.count{|x|x%2==0}  #=> 3
 *
 */

void
mrb_iter_break(void)
{
  //vm_iter_break(GET_THREAD()); /* dummy */
}

static mrb_value
find_i(mrb_state *mrb, mrb_value i, mrb_value *memo, int argc, mrb_value *argv)
{
    ENUM_WANT_SVALUE(mrb);

    if (RTEST(mrb_yield(i))) {
      *memo = i;
      mrb_iter_break();
    }
    return mrb_nil_value();
}

/* 15.3.2.2.4  */
/* 15.3.2.2.7  */
/*
 *  call-seq:
 *     enum.detect(ifnone = nil) {| obj | block }  -> obj or nil
 *     enum.find(ifnone = nil)   {| obj | block }  -> obj or nil
 *     enum.detect(ifnone = nil)                   -> an_enumerator
 *     enum.find(ifnone = nil)                     -> an_enumerator
 *
 *  Passes each entry in <i>enum</i> to <em>block</em>. Returns the
 *  first for which <em>block</em> is not false.  If no
 *  object matches, calls <i>ifnone</i> and returns its result when it
 *  is specified, or returns <code>nil</code> otherwise.
 *
 *  If no block is given, an enumerator is returned instead.
 *
 *     (1..10).detect  {|i| i % 5 == 0 and i % 7 == 0 }   #=> nil
 *     (1..100).detect {|i| i % 5 == 0 and i % 7 == 0 }   #=> 35
 *
 */

static mrb_value
enum_find(mrb_state *mrb, int argc, mrb_value *argv, mrb_value obj)
{
    mrb_value memo;
    mrb_value if_none;

    memo.tt = MRB_TT_FREE;
    //mrb_scan_args(argc, argv, "01", &if_none);
    if_none = argv[0];
    //RETURN_ENUMERATOR(obj, argc, argv);
    mrb_block_call(mrb, obj, id_each, 0, 0, find_i, &memo);
    if (memo.tt != MRB_TT_FREE) {
      return memo;
    }
    if (!mrb_nil_p(if_none)) {
      return mrb_funcall(mrb, if_none, "call", 0, 0);
    }
    return mrb_nil_value();
}

static mrb_value
enum_find_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return enum_find(mrb, argc, argv, self);
}

/*
 *  call-seq:
 *     enum.find_index(value)            -> int or nil
 *     enum.find_index {| obj | block }  -> int or nil
 *     enum.find_index                   -> an_enumerator
 *
 *  Compares each entry in <i>enum</i> with <em>value</em> or passes
 *  to <em>block</em>.  Returns the index for the first for which the
 *  evaluated value is non-false.  If no object matches, returns
 *  <code>nil</code>
 *
 *  If neither block nor argument is given, an enumerator is returned instead.
 *
 *     (1..10).find_index  {|i| i % 5 == 0 and i % 7 == 0 }   #=> nil
 *     (1..100).find_index {|i| i % 5 == 0 and i % 7 == 0 }   #=> 34
 *     (1..100).find_index(50)                                #=> 49
 *
 */

static mrb_value
find_all_i(mrb_state *mrb, mrb_value i, mrb_value ary, int argc, mrb_value *argv)
{
    ENUM_WANT_SVALUE(mrb);

    if (RTEST(mrb_yield(i))) {
      mrb_ary_push(mrb, ary, i);
    }
    return mrb_nil_value();
}

/* 15.3.2.2.8  */
/* 15.3.2.2.18 */
/*
 *  call-seq:
 *     enum.find_all {| obj | block }  -> array
 *     enum.select   {| obj | block }  -> array
 *     enum.find_all                   -> an_enumerator
 *     enum.select                     -> an_enumerator
 *
 *  Returns an array containing all elements of <i>enum</i> for which
 *  <em>block</em> is not <code>false</code> (see also
 *  <code>Enumerable#reject</code>).
 *
 *  If no block is given, an enumerator is returned instead.
 *
 *
 *     (1..10).find_all {|i|  i % 3 == 0 }   #=> [3, 6, 9]
 *
 */

static mrb_value
enum_find_all(mrb_state *mrb, mrb_value obj)
{
    mrb_value ary;

    //RETURN_ENUMERATOR(obj, 0, 0);

    ary = mrb_ary_new(mrb);
    mrb_block_call(mrb, obj, id_each, 0, 0, find_all_i, &ary);

    return ary;
}

static mrb_value
reject_i(mrb_state *mrb, mrb_value i, mrb_value ary, int argc, mrb_value *argv)
{
    ENUM_WANT_SVALUE(mrb);

    if (!RTEST(mrb_yield(i))) {
      mrb_ary_push(mrb, ary, i);
    }
    return mrb_nil_value();
}

/* 15.3.2.2.17 */
/*
 *  call-seq:
 *     enum.reject {| obj | block }  -> array
 *     enum.reject                   -> an_enumerator
 *
 *  Returns an array for all elements of <i>enum</i> for which
 *  <em>block</em> is false (see also <code>Enumerable#find_all</code>).
 *
 *  If no block is given, an enumerator is returned instead.
 *
 *     (1..10).reject {|i|  i % 3 == 0 }   #=> [1, 2, 4, 5, 7, 8, 10]
 *
 */

static mrb_value
enum_reject(mrb_state *mrb, mrb_value obj)
{
    mrb_value ary;

    //RETURN_ENUMERATOR(obj, 0, 0);

    ary = mrb_ary_new(mrb);
    mrb_block_call(mrb, obj, id_each, 0, 0, reject_i, &ary);

    return ary;
}

static mrb_value
collect_i(mrb_state *mrb, mrb_value i, mrb_value ary, int argc, mrb_value *argv)
{
    mrb_ary_push(mrb, ary, enum_yield(argc, argv));

    return mrb_nil_value();
}

static mrb_value
collect_all(mrb_state *mrb, mrb_value i, mrb_value ary, int argc, mrb_value *argv)
{
    //mrb_thread_check_ints(); /* dummy */
    mrb_ary_push(mrb, ary, enum_values_pack(mrb, argc, argv));

    return mrb_nil_value();
}

/* 15.3.2.2.3  */
/* 15.3.2.2.12 */
/*
 *  call-seq:
 *     enum.collect {| obj | block }  -> array
 *     enum.map     {| obj | block }  -> array
 *     enum.collect                   -> an_enumerator
 *     enum.map                       -> an_enumerator
 *
 *  Returns a new array with the results of running <em>block</em> once
 *  for every element in <i>enum</i>.
 *
 *  If no block is given, an enumerator is returned instead.
 *
 *     (1..4).collect {|i| i*i }   #=> [1, 4, 9, 16]
 *     (1..4).collect { "cat"  }   #=> ["cat", "cat", "cat", "cat"]
 *
 */

static mrb_value
enum_collect(mrb_state *mrb, mrb_value obj)
{
    mrb_value ary;

    //RETURN_ENUMERATOR(obj, 0, 0);

    ary = mrb_ary_new(mrb);
    mrb_block_call(mrb, obj, id_each, 0, 0, collect_i, &ary);

    return ary;
}

/* 15.3.2.2.6  */
/* 15.3.2.2.20 */
/*
 *  call-seq:
 *     enum.to_a      ->    array
 *     enum.entries   ->    array
 *
 *  Returns an array containing the items in <i>enum</i>.
 *
 *     (1..7).to_a                       #=> [1, 2, 3, 4, 5, 6, 7]
 *     { 'a'=>1, 'b'=>2, 'c'=>3 }.to_a   #=> [["a", 1], ["b", 2], ["c", 3]]
 */
static mrb_value
enum_to_a(mrb_state *mrb, int argc, mrb_value *argv, mrb_value obj)
{
    mrb_value ary = mrb_ary_new(mrb);

    mrb_block_call(mrb, obj, id_each, argc, argv, collect_all, &ary);

    return ary;
}

static mrb_value
enum_to_a_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return enum_to_a(mrb, argc, argv, self);
}

static mrb_value
inject_i(mrb_state *mrb, mrb_value i, mrb_value p, int argc, mrb_value *argv)
{
    mrb_value *memo = &p;

    ENUM_WANT_SVALUE(mrb);

    if (memo[0].tt == MRB_TT_FREE) {
      memo[0] = i;
    }
    else {
      memo[0] = mrb_yield_values(2, memo[0], i);
    }
    return mrb_nil_value();
}

static mrb_value
inject_op_i(mrb_state *mrb, mrb_value i, mrb_value p, int argc, mrb_value *argv)
{
    mrb_value *memo = &p;

    ENUM_WANT_SVALUE(mrb);

    if (memo[0].tt == MRB_TT_FREE) {
      memo[0] = i;
    }
    else {
      memo[0] = mrb_funcall(mrb, memo[0], mrb_sym2name(mrb, SYM2ID(memo[1])), 1, i);
    }
    return mrb_nil_value();
}

/* 15.3.2.2.11 */
/*
 *  call-seq:
 *     enum.inject(initial, sym) -> obj
 *     enum.inject(sym)          -> obj
 *     enum.inject(initial) {| memo, obj | block }  -> obj
 *     enum.inject          {| memo, obj | block }  -> obj
 *
 *     enum.reduce(initial, sym) -> obj
 *     enum.reduce(sym)          -> obj
 *     enum.reduce(initial) {| memo, obj | block }  -> obj
 *     enum.reduce          {| memo, obj | block }  -> obj
 *
 *  Combines all elements of <i>enum</i> by applying a binary
 *  operation, specified by a block or a symbol that names a
 *  method or operator.
 *
 *  If you specify a block, then for each element in <i>enum</i>
 *  the block is passed an accumulator value (<i>memo</i>) and the element.
 *  If you specify a symbol instead, then each element in the collection
 *  will be passed to the named method of <i>memo</i>.
 *  In either case, the result becomes the new value for <i>memo</i>.
 *  At the end of the iteration, the final value of <i>memo</i> is the
 *  return value fo the method.
 *
 *  If you do not explicitly specify an <i>initial</i> value for <i>memo</i>,
 *  then uses the first element of collection is used as the initial value
 *  of <i>memo</i>.
 *
 *  Examples:
 *
 *     # Sum some numbers
 *     (5..10).reduce(:+)                            #=> 45
 *     # Same using a block and inject
 *     (5..10).inject {|sum, n| sum + n }            #=> 45
 *     # Multiply some numbers
 *     (5..10).reduce(1, :*)                         #=> 151200
 *     # Same using a block
 *     (5..10).inject(1) {|product, n| product * n } #=> 151200
 *     # find the longest word
 *     longest = %w{ cat sheep bear }.inject do |memo,word|
 *        memo.length > word.length ? memo : word
 *     end
 *     longest                                       #=> "sheep"
 *
 */
static mrb_value
enum_inject(mrb_state *mrb, int argc, mrb_value *argv, mrb_value obj)
{
  mrb_value memo[2];
  mrb_value (*iter)(mrb_state *mrb, mrb_value, mrb_value, int, mrb_value*) = inject_i;

  //switch (mrb_scan_args(argc, argv, "02", &memo[0], &memo[1])) {
  switch (argc) {
    case 0:
      memo[0].tt = MRB_TT_FREE;
      break;
    case 1:
      if (mrb_block_given_p()) {
        break;
      }
      memo[1] = mrb_symbol_value(mrb_to_id(mrb, argv[0]));
      memo[0].tt = MRB_TT_FREE;
      iter = inject_op_i;
      break;
    case 2:
      if (mrb_block_given_p()) {
        mrb_warning("given block not used");
      }
      memo[0] = argv[0];
      memo[1] = mrb_symbol_value(mrb_to_id(mrb, argv[1]));
      iter = inject_op_i;
      break;
  }
  mrb_block_call(mrb, obj, id_each, 0, 0, iter, memo);
  if (memo[0].tt == MRB_TT_FREE) return mrb_nil_value();
  return memo[0];
}

static mrb_value
enum_inject_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return enum_inject(mrb, argc, argv, self);
}

static mrb_value
partition_i(mrb_state *mrb, mrb_value i, mrb_value *ary, int argc, mrb_value *argv)
{
    ENUM_WANT_SVALUE(mrb);

    if (RTEST(mrb_yield(i))) {
      mrb_ary_push(mrb, ary[0], i);
    }
    else {
      mrb_ary_push(mrb, ary[1], i);
    }
    return mrb_nil_value();
}

/* 15.3.2.2.16 */
/*
 *  call-seq:
 *     enum.partition {| obj | block }  -> [ true_array, false_array ]
 *     enum.partition                   -> an_enumerator
 *
 *  Returns two arrays, the first containing the elements of
 *  <i>enum</i> for which the block evaluates to true, the second
 *  containing the rest.
 *
 *  If no block is given, an enumerator is returned instead.
 *
 *     (1..6).partition {|i| (i&1).zero?}   #=> [[2, 4, 6], [1, 3, 5]]
 *
 */

static mrb_value
enum_partition(mrb_state *mrb, mrb_value obj)
{
    mrb_value ary[2];

    //RETURN_ENUMERATOR(obj, 0, 0);

    ary[0] = mrb_ary_new(mrb);
    ary[1] = mrb_ary_new(mrb);
    mrb_block_call(mrb, obj, id_each, 0, 0, partition_i, ary);

    return mrb_assoc_new(mrb, ary[0], ary[1]);
}

/* 15.3.2.2.19 */
/*
 *  call-seq:
 *     enum.sort                     -> array
 *     enum.sort {| a, b | block }   -> array
 *
 *  Returns an array containing the items in <i>enum</i> sorted,
 *  either according to their own <code><=></code> method, or by using
 *  the results of the supplied block. The block should return -1, 0, or
 *  +1 depending on the comparison between <i>a</i> and <i>b</i>. As of
 *  Ruby 1.8, the method <code>Enumerable#sort_by</code> implements a
 *  built-in Schwartzian Transform, useful when key computation or
 *  comparison is expensive.
 *
 *     %w(rhea kea flea).sort         #=> ["flea", "kea", "rhea"]
 *     (1..10).sort {|a,b| b <=> a}   #=> [10, 9, 8, 7, 6, 5, 4, 3, 2, 1]
 */

static mrb_value
enum_sort(mrb_state *mrb, mrb_value obj)
{
    return mrb_ary_sort(mrb, enum_to_a(mrb, 0, 0, obj));
}

/*
 *  call-seq:
 *     enum.sort_by {| obj | block }    -> array
 *     enum.sort_by                     -> an_enumerator
 *
 *  Sorts <i>enum</i> using a set of keys generated by mapping the
 *  values in <i>enum</i> through the given block.
 *
 *  If no block is given, an enumerator is returned instead.
 *
 *     %w{ apple pear fig }.sort_by {|word| word.length}
 *                   #=> ["fig", "pear", "apple"]
 *
 *  The current implementation of <code>sort_by</code> generates an
 *  array of tuples containing the original collection element and the
 *  mapped value. This makes <code>sort_by</code> fairly expensive when
 *  the keysets are simple
 *
 *     require 'benchmark'
 *
 *     a = (1..100000).map {rand(100000)}
 *
 *     Benchmark.bm(10) do |b|
 *       b.report("Sort")    { a.sort }
 *       b.report("Sort by") { a.sort_by {|a| a} }
 *     end
 *
 *  <em>produces:</em>
 *
 *     user     system      total        real
 *     Sort        0.180000   0.000000   0.180000 (  0.175469)
 *     Sort by     1.980000   0.040000   2.020000 (  2.013586)
 *
 *  However, consider the case where comparing the keys is a non-trivial
 *  operation. The following code sorts some files on modification time
 *  using the basic <code>sort</code> method.
 *
 *     files = Dir["*"]
 *     sorted = files.sort {|a,b| File.new(a).mtime <=> File.new(b).mtime}
 *     sorted   #=> ["mon", "tues", "wed", "thurs"]
 *
 *  This sort is inefficient: it generates two new <code>File</code>
 *  objects during every comparison. A slightly better technique is to
 *  use the <code>Kernel#test</code> method to generate the modification
 *  times directly.
 *
 *     files = Dir["*"]
 *     sorted = files.sort { |a,b|
 *       test(?M, a) <=> test(?M, b)
 *     }
 *     sorted   #=> ["mon", "tues", "wed", "thurs"]
 *
 *  This still generates many unnecessary <code>Time</code> objects. A
 *  more efficient technique is to cache the sort keys (modification
 *  times in this case) before the sort. Perl users often call this
 *  approach a Schwartzian Transform, after Randal Schwartz. We
 *  construct a temporary array, where each element is an array
 *  containing our sort key along with the filename. We sort this array,
 *  and then extract the filename from the result.
 *
 *     sorted = Dir["*"].collect { |f|
 *        [test(?M, f), f]
 *     }.sort.collect { |f| f[1] }
 *     sorted   #=> ["mon", "tues", "wed", "thurs"]
 *
 *  This is exactly what <code>sort_by</code> does internally.
 *
 *     sorted = Dir["*"].sort_by {|f| test(?M, f)}
 *     sorted   #=> ["mon", "tues", "wed", "thurs"]
 */

#define ENUMFUNC(name) mrb_block_given_p() ? name##_iter_i : name##_i

#define DEFINE_ENUMFUNCS(mrb, name) \
static mrb_value enum_##name##_func(mrb_value result, mrb_value *memo); \
\
static mrb_value \
name##_i(mrb_state *mrb, mrb_value i, mrb_value *memo, int argc, mrb_value *argv) \
{ \
    return enum_##name##_func(enum_values_pack(mrb, argc, argv), memo); \
} \
\
static mrb_value \
name##_iter_i(mrb_state *mrb,mrb_value i, mrb_value *memo, int argc, mrb_value *argv) \
{ \
    return enum_##name##_func(enum_yield(argc, argv), memo); \
} \
\
static mrb_value \
enum_##name##_func(mrb_value result, mrb_value *memo)

DEFINE_ENUMFUNCS(mrb, all)
{
    if (!RTEST(result)) {
      *memo = mrb_false_value();
      mrb_iter_break();
    }
    return mrb_nil_value();
}

/* 15.3.2.2.1  */
/*
 *  call-seq:
 *     enum.all? [{|obj| block } ]   -> true or false
 *
 *  Passes each element of the collection to the given block. The method
 *  returns <code>true</code> if the block never returns
 *  <code>false</code> or <code>nil</code>. If the block is not given,
 *  Ruby adds an implicit block of <code>{|obj| obj}</code> (that is
 *  <code>all?</code> will return <code>true</code> only if none of the
 *  collection members are <code>false</code> or <code>nil</code>.)
 *
 *     %w{ant bear cat}.all? {|word| word.length >= 3}   #=> true
 *     %w{ant bear cat}.all? {|word| word.length >= 4}   #=> false
 *     [ nil, true, 99 ].all?                            #=> false
 *
 */

static mrb_value
enum_all(mrb_state *mrb, mrb_value obj)
{
    mrb_value result = mrb_true_value();

    mrb_block_call(mrb, obj, id_each, 0, 0, ENUMFUNC(all), &result);
    return result;
}

DEFINE_ENUMFUNCS(mrb, any)
{
    if (RTEST(result)) {
      *memo = mrb_true_value();
      mrb_iter_break();
    }
    return mrb_nil_value();
}

/* 15.3.2.2.2  */
/*
 *  call-seq:
 *     enum.any? [{|obj| block } ]   -> true or false
 *
 *  Passes each element of the collection to the given block. The method
 *  returns <code>true</code> if the block ever returns a value other
 *  than <code>false</code> or <code>nil</code>. If the block is not
 *  given, Ruby adds an implicit block of <code>{|obj| obj}</code> (that
 *  is <code>any?</code> will return <code>true</code> if at least one
 *  of the collection members is not <code>false</code> or
 *  <code>nil</code>.
 *
 *     %w{ant bear cat}.any? {|word| word.length >= 3}   #=> true
 *     %w{ant bear cat}.any? {|word| word.length >= 4}   #=> true
 *     [ nil, true, 99 ].any?                            #=> true
 *
 */

static mrb_value
enum_any(mrb_state *mrb, mrb_value obj)
{
    mrb_value result = mrb_false_value();

    mrb_block_call(mrb, obj, id_each, 0, 0, ENUMFUNC(any), &result);
    return result;
}

static mrb_value
min_i(mrb_state *mrb, mrb_value i, mrb_value *memo, int argc, mrb_value *argv)
{
    mrb_value cmp;

    ENUM_WANT_SVALUE(mrb);

    if (memo->tt == MRB_TT_FREE) {
      *memo = i;
    }
    else {
      cmp = mrb_funcall(mrb, i, "<=>", 1, *memo);
      if (mrb_cmpint(mrb, cmp, i, *memo) < 0) {
          *memo = i;
      }
    }
    return mrb_nil_value();
}

static mrb_value
min_ii(mrb_state *mrb, mrb_value i, mrb_value *memo, int argc, mrb_value *argv)
{
    mrb_value cmp;

    ENUM_WANT_SVALUE(mrb);

    if (memo->tt == MRB_TT_FREE) {
      *memo = i;
    }
    else {
      cmp = mrb_yield_values(2, i, *memo);
      if (mrb_cmpint(mrb, cmp, i, *memo) < 0) {
          *memo = i;
      }
    }
    return mrb_nil_value();
}

/* 15.3.2.2.14 */
/*
 *  call-seq:
 *     enum.min                    -> obj
 *     enum.min {| a,b | block }   -> obj
 *
 *  Returns the object in <i>enum</i> with the minimum value. The
 *  first form assumes all objects implement <code>Comparable</code>;
 *  the second uses the block to return <em>a <=> b</em>.
 *
 *     a = %w(albatross dog horse)
 *     a.min                                  #=> "albatross"
 *     a.min {|a,b| a.length <=> b.length }   #=> "dog"
 */

static mrb_value
enum_min(mrb_state *mrb, mrb_value obj)
{
    mrb_value result;
    result.tt = MRB_TT_FREE;

    if (mrb_block_given_p()) {
      mrb_block_call(mrb, obj, id_each, 0, 0, min_ii, &result);
    }
    else {
      mrb_block_call(mrb, obj, id_each, 0, 0, min_i, &result);
    }
    if (result.tt == MRB_TT_FREE) return mrb_nil_value();
    return result;
}

static mrb_value
max_i(mrb_state *mrb, mrb_value i, mrb_value *memo, int argc, mrb_value *argv)
{
    mrb_value cmp;

    ENUM_WANT_SVALUE(mrb);

    if (memo->tt == MRB_TT_FREE) {
      *memo = i;
    }
    else {
      cmp = mrb_funcall(mrb, i, "<=>", 1, *memo);
      if (mrb_cmpint(mrb, cmp, i, *memo) > 0) {
          *memo = i;
      }
    }
    return mrb_nil_value();
}

static mrb_value
max_ii(mrb_state *mrb, mrb_value i, mrb_value *memo, int argc, mrb_value *argv)
{
    mrb_value cmp;

    ENUM_WANT_SVALUE(mrb);

    if (memo->tt == MRB_TT_FREE) {
      *memo = i;
    }
    else {
      cmp = mrb_yield_values(2, i, *memo);
      if (mrb_cmpint(mrb, cmp, i, *memo) > 0) {
          *memo = i;
      }
    }
    return mrb_nil_value();
}

/* 15.3.2.2.13 */
/*
 *  call-seq:
 *     enum.max                   -> obj
 *     enum.max {|a,b| block }    -> obj
 *
 *  Returns the object in _enum_ with the maximum value. The
 *  first form assumes all objects implement <code>Comparable</code>;
 *  the second uses the block to return <em>a <=> b</em>.
 *
 *     a = %w(albatross dog horse)
 *     a.max                                  #=> "horse"
 *     a.max {|a,b| a.length <=> b.length }   #=> "albatross"
 */

static mrb_value
enum_max(mrb_state *mrb, mrb_value obj)
{
    mrb_value result;
    result.tt = MRB_TT_FREE;

    if (mrb_block_given_p()) {
      mrb_block_call(mrb, obj, id_each, 0, 0, max_ii, &result);
    }
    else {
      mrb_block_call(mrb, obj, id_each, 0, 0, max_i, &result);
    }
    if (result.tt == MRB_TT_FREE) return mrb_nil_value();
    return result;
}

static mrb_value
member_i(mrb_state *mrb, mrb_value iter, mrb_value *memo, int argc, mrb_value *argv)
{
    if (mrb_equal(mrb, enum_values_pack(mrb, argc, argv), memo[0])) {
      memo[1] = mrb_true_value();
      mrb_iter_break();
    }
    return mrb_nil_value();
}

/* 15.3.2.2.10 */
/* 15.3.2.2.15 */
/*
 *  call-seq:
 *     enum.include?(obj)     -> true or false
 *     enum.member?(obj)      -> true or false
 *
 *  Returns <code>true</code> if any member of <i>enum</i> equals
 *  <i>obj</i>. Equality is tested using <code>==</code>.
 *
 *     IO.constants.include? :SEEK_SET          #=> true
 *     IO.constants.include? :SEEK_NO_FURTHER   #=> false
 *
 */

static mrb_value
enum_member(mrb_state *mrb, mrb_value obj)
{
  mrb_value memo[2];
  mrb_value val;

  mrb_get_args(mrb, "o", &val);

  memo[0] = val;
  memo[1] = mrb_false_value();
  mrb_block_call(mrb, obj, id_each, 0, 0, member_i, memo);
  return memo[1];
}

static mrb_value
each_with_index_i(mrb_state *mrb, mrb_value i, long *memo, int argc, void *argv)
{
    long n = (*memo)++;

    return mrb_yield_values(2, enum_values_pack(mrb, argc, argv), mrb_fixnum_value(n));
}

/* 15.3.2.2.5  */
/*
 *  call-seq:
 *     enum.each_with_index(*args) {|obj, i| block }   ->  enum
 *     enum.each_with_index(*args)                     ->  an_enumerator
 *
 *  Calls <em>block</em> with two arguments, the item and its index,
 *  for each item in <i>enum</i>.  Given arguments are passed through
 *  to #each().
 *
 *  If no block is given, an enumerator is returned instead.
 *
 *     hash = Hash.new
 *     %w(cat dog wombat).each_with_index {|item, index|
 *       hash[item] = index
 *     }
 *     hash   #=> {"cat"=>0, "dog"=>1, "wombat"=>2}
 *
 */

static mrb_value
enum_each_with_index(mrb_state *mrb, int argc, mrb_value *argv, mrb_value obj)
{
    long memo;

    //RETURN_ENUMERATOR(obj, argc, argv);

    memo = 0;
    mrb_block_call(mrb, obj, id_each, argc, argv, each_with_index_i, &memo);
    return obj;
}

static mrb_value
enum_each_with_index_m(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return enum_each_with_index(mrb, argc, argv, self);
}

/*
 *  call-seq:
 *     enum.reverse_each(*args) {|item| block }   ->  enum
 *     enum.reverse_each(*args)                   ->  an_enumerator
 *
 *  Builds a temporary array and traverses that array in reverse order.
 *
 *  If no block is given, an enumerator is returned instead.
 *
 */

#endif

/*
 *  The <code>Enumerable</code> mixin provides collection classes with
 *  several traversal and searching methods, and with the ability to
 *  sort. The class must provide a method <code>each</code>, which
 *  yields successive members of the collection. If
 *  <code>Enumerable#max</code>, <code>#min</code>, or
 *  <code>#sort</code> is used, the objects in the collection must also
 *  implement a meaningful <code><=></code> operator, as these methods
 *  rely on an ordering between members of the collection.
 */

void
mrb_init_enumerable(mrb_state *mrb)
{
  struct RClass *cenum;
//#undef mrb_intern
//#define mrb_intern(str) mrb_intern_const(str)

  cenum = mrb_define_module(mrb, "Enumerable");

#if 0
  //mrb_define_class_method(mrb, cenum, "all?",            enum_all,               ARGS_NONE()); /* 15.3.2.2.1  */
  //mrb_define_class_method(mrb, cenum, "any?",            enum_any,               ARGS_NONE()); /* 15.3.2.2.2  */
  //mrb_define_class_method(mrb, cenum, "collect",         enum_collect,           ARGS_NONE()); /* 15.3.2.2.3  */
  //mrb_define_class_method(mrb, cenum, "detect",          enum_find_m,            ARGS_ANY());  /* 15.3.2.2.4  */
  //mrb_define_class_method(mrb, cenum, "each_with_index", enum_each_with_index_m, ARGS_ANY());  /* 15.3.2.2.5  */
  mrb_define_class_method(mrb, cenum, "entries",         enum_to_a_m,            ARGS_ANY());  /* 15.3.2.2.6  */
  //mrb_define_class_method(mrb, cenum, "find",            enum_find_m,            ARGS_ANY());  /* 15.3.2.2.7  */
  //mrb_define_class_method(mrb, cenum, "find_all",        enum_find_all,          ARGS_NONE()); /* 15.3.2.2.8  */
  //mrb_define_class_method(mrb, cenum, "grep",            enum_grep,              ARGS_REQ(1)); /* 15.3.2.2.9  */
  mrb_define_class_method(mrb, cenum, "include?",        enum_member,            ARGS_REQ(1)); /* 15.3.2.2.10 */
  //mrb_define_class_method(mrb, cenum, "inject",          enum_inject_m,          ARGS_ANY());  /* 15.3.2.2.11 */
  //mrb_define_class_method(mrb, cenum, "map",             enum_collect,           ARGS_NONE()); /* 15.3.2.2.12 */
  //mrb_define_class_method(mrb, cenum, "max",             enum_max,               ARGS_NONE()); /* 15.3.2.2.13 */
  //mrb_define_class_method(mrb, cenum, "min",             enum_min,               ARGS_NONE()); /* 15.3.2.2.14 */
  mrb_define_class_method(mrb, cenum, "member?",         enum_member,            ARGS_REQ(1)); /* 15.3.2.2.15 */
  //mrb_define_class_method(mrb, cenum, "partition",       enum_partition,         ARGS_NONE()); /* 15.3.2.2.16 */
  //mrb_define_class_method(mrb, cenum, "reject",          enum_reject,            ARGS_NONE()); /* 15.3.2.2.17 */
  //mrb_define_class_method(mrb, cenum, "select",          enum_find_all,          ARGS_NONE()); /* 15.3.2.2.18 */
  //mrb_define_class_method(mrb, cenum, "sort",            enum_sort,              ARGS_NONE()); /* 15.3.2.2.19 */
  mrb_define_class_method(mrb, cenum, "to_a",            enum_to_a_m,            ARGS_ANY());  /* 15.3.2.2.20 */
  id_eqq  = mrb_intern(mrb, "===");
  id_each = mrb_intern(mrb, "each");
  id_cmp  = mrb_intern(mrb, "<=>");
  id_next = mrb_intern(mrb, "next");
  id_size = mrb_intern(mrb, "size");
#endif
}

