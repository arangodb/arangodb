/*
** mrbtest - Test for Embeddable Ruby
**
** This program runs Ruby test programs in test/t directory
** against the current mruby implementation.
*/

#include <string.h>

#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#include <compile.h>

void
mrb_init_mrbtest(mrb_state *);

/* Print a short remark for the user */
void print_hint(void)
{
  printf("mrbtest - Embeddable Ruby Test\n");
  printf("\nThis is a very early version, please test and report errors.\n");
  printf("Thanks :)\n\n");
}

int
main(void)
{
  struct mrb_parser_state *parser;
  mrb_state *mrb_interpreter;
  mrb_value mrb_return_value;
  int byte_code;

  print_hint();

  /* new interpreter instance */
  mrb_interpreter = mrb_open();
  mrb_init_mrbtest(mrb_interpreter);
  parser = mrb_parse_nstring_ext(mrb_interpreter, "report()", strlen("report()"));

  /* generate bytecode */
  byte_code = mrb_generate_code(mrb_interpreter, parser->tree);

  /* evaluate the bytecode */
  mrb_return_value = mrb_run(mrb_interpreter,
    /* pass a proc for evaulation */
    mrb_proc_new(mrb_interpreter, mrb_interpreter->irep[byte_code]),
    mrb_top_self(mrb_interpreter));
  /* did an exception occur? */
  if (mrb_interpreter->exc) {
    mrb_p(mrb_interpreter, mrb_return_value);
    mrb_interpreter->exc = 0;
  }
  else {
    /* no */
  }

  return 0;
}
