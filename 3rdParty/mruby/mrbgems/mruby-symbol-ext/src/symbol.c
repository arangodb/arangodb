#include "mruby.h"
#include "mruby/khash.h"
#include "mruby/array.h"

typedef struct symbol_name {
  size_t len;
  const char *name;
} symbol_name;

KHASH_DECLARE(n2s, symbol_name, mrb_sym, 1)

/*
 *  call-seq:
 *     Symbol.all_symbols    => array
 *
 *  Returns an array of all the symbols currently in Ruby's symbol
 *  table.
 *
 *     Symbol.all_symbols.size    #=> 903
 *     Symbol.all_symbols[1,20]   #=> [:floor, :ARGV, :Binding, :symlink,
 *                                     :chown, :EOFError, :$;, :String,
 *                                     :LOCK_SH, :"setuid?", :$<,
 *                                     :default_proc, :compact, :extend,
 *                                     :Tms, :getwd, :$=, :ThreadGroup,
 *                                     :wait2, :$>]
 */
static mrb_value
mrb_sym_all_symbols(mrb_state *mrb, mrb_value self)
{
  khiter_t k;
  mrb_sym sym;
  khash_t(n2s) *h = mrb->name2sym;
  mrb_value ary = mrb_ary_new_capa(mrb, kh_size(h));

  for (k = kh_begin(h); k != kh_end(h); k++) {
    if (kh_exist(h, k)) {
      sym = kh_value(h, k);
      mrb_ary_push(mrb, ary, mrb_symbol_value(sym));
    }
  }

  return ary;
}

void
mrb_mruby_symbol_ext_gem_init(mrb_state* mrb)
{
  struct RClass *s = mrb->symbol_class;
  mrb_define_class_method(mrb, s, "all_symbols", mrb_sym_all_symbols, MRB_ARGS_NONE());
}

void
mrb_mruby_symbol_ext_gem_final(mrb_state* mrb)
{
}
