/*
** mrbtest - Test for Embeddable Ruby
**
** This program runs Ruby test programs in test/t directory
** against the current mruby implementation.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <mruby/variable.h>

void
mrb_init_mrbtest(mrb_state *);

/* Print a short remark for the user */
static void
print_hint(void)
{
  printf("mrbtest - Embeddable Ruby Test\n");
  printf("\nThis is a very early version, please test and report errors.\n");
  printf("Thanks :)\n\n");
}

static int
check_error(mrb_state *mrb)
{
  /* Error check */
  /* $ko_test and $kill_test should be 0 */
  mrb_value ko_test = mrb_gv_get(mrb, mrb_intern(mrb, "$ko_test"));
  mrb_value kill_test = mrb_gv_get(mrb, mrb_intern(mrb, "$kill_test"));

  return mrb_fixnum_p(ko_test) && mrb_fixnum(ko_test) == 0 && mrb_fixnum_p(kill_test) && mrb_fixnum(kill_test) == 0;
}

static int
eval_test(mrb_state *mrb)
{
  mrb_value return_value;
  const char *prog = "report()";

  /* evaluate the test */
  return_value = mrb_load_string(mrb, prog);
  /* did an exception occur? */
  if (mrb->exc) {
    mrb_p(mrb, return_value);
    mrb->exc = 0;
    return EXIT_FAILURE;
  }
  else if (!check_error(mrb)) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

static void
t_printstr(mrb_state *mrb, mrb_value obj)
{
  struct RString *str;
  char *s;
  int len;
   
  if (mrb_string_p(obj)) {
    str = mrb_str_ptr(obj);
    s = str->ptr;
    len = str->len;
    fwrite(s, len, 1, stdout);
  }
}

mrb_value
mrb_t_printstr(mrb_state *mrb, mrb_value self)
{
  mrb_value argv;

  mrb_get_args(mrb, "o", &argv);
  t_printstr(mrb, argv);

  return argv;
}

int
main(int argc, char **argv)
{
  mrb_state *mrb;
  struct RClass *krn;
  int ret;

  print_hint();

  /* new interpreter instance */
  mrb = mrb_open();
  if (mrb == NULL) {
    fprintf(stderr, "Invalid mrb_state, exiting test driver");
    return EXIT_FAILURE;
  }

  if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'v') {
    printf("verbose mode: enable\n\n");
    mrb_gv_set(mrb, mrb_intern(mrb, "$mrbtest_verbose"), mrb_true_value());
  }

  krn = mrb->kernel_module;
  mrb_define_method(mrb, krn, "__t_printstr__", mrb_t_printstr, MRB_ARGS_REQ(1));

  mrb_init_mrbtest(mrb);
  ret = eval_test(mrb);
  mrb_close(mrb);

  return ret;
}
