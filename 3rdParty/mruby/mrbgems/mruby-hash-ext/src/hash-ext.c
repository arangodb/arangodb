/*
** hash.c - Hash class
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/hash.h"
#include "mruby/khash.h"
#include "mruby/string.h"
#include "mruby/variable.h"

/*
 * call-seq:
 *   hsh.values_at(key, ...)   -> array
 *
 * Return an array containing the values associated with the given keys.
 * Also see <code>Hash.select</code>.
 *
 *   h = { "cat" => "feline", "dog" => "canine", "cow" => "bovine" }
 *   h.values_at("cow", "cat")  #=> ["bovine", "feline"]
 */

mrb_value
mrb_hash_values_at(mrb_state *mrb, int argc, mrb_value *argv, mrb_value hash)
{
    mrb_value result = mrb_ary_new_capa(mrb, argc);
    long i;

    for (i=0; i<argc; i++) {
        mrb_ary_push(mrb, result, mrb_hash_get(mrb, hash, argv[i]));
    }
    return result;
}

static mrb_value
hash_values_at(mrb_state *mrb, mrb_value hash)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);

  return mrb_hash_values_at(mrb, argc, argv, hash);
}

void
mrb_mruby_hash_ext_gem_init(mrb_state *mrb)
{
  struct RClass *h;

  h = mrb->hash_class;

  mrb_define_method(mrb, h, "values_at",              hash_values_at,       MRB_ARGS_ANY());
}

void
mrb_mruby_hash_ext_gem_final(mrb_state *mrb)
{
}
