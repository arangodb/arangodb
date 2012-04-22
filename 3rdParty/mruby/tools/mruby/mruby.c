#include "mruby.h"
#include "mruby/proc.h"
#include "compile.h"
#include "dump.h"
#include "stdio.h"
#include "string.h"

void ruby_show_version(mrb_state *);
void ruby_show_copyright(mrb_state *);
void parser_dump(mrb_state*, struct mrb_ast_node*, int);
void codedump_all(mrb_state*, int);

struct _args {
  FILE *rfp;
  int mrbfile      : 1;
  int check_syntax : 1;
  int verbose      : 1;
};

static void
usage(const char *name)
{
  static const char *const usage_msg[] = {
  "switches:",
  "-b           load and execute RiteBinary (mrb) file",
  "-c           check syntax only",
  "-v           print version number, then run in verbose mode",
  "--verbose    run in verbose mode",
  "--version    print the version",
  "--copyright  print the copyright",
  NULL
  };
  const char *const *p = usage_msg;

  printf("Usage: %s [switches] programfile\n", name);
  while(*p)
  printf("  %s\n", *p++);
}

static int
parse_args(mrb_state *mrb, int argc, char **argv, struct _args *args)
{
  char **origargv = argv;

  memset(args, 0, sizeof(*args));

  for (argc--,argv++; argc > 0; argc--,argv++) {
    if (**argv == '-') {
      if (strlen(*argv) <= 1)
        return -1;

      switch ((*argv)[1]) {
      case 'b':
        args->mrbfile = 1;
        break;
      case 'c':
        args->check_syntax = 1;
        break;
      case 'v':
        ruby_show_version(mrb);
        args->verbose = 1;
        break;
      case '-':
        if (strcmp((*argv) + 2, "version") == 0) {
          ruby_show_version(mrb);
        }
        else if (strcmp((*argv) + 2, "verbose") == 0) {
          args->verbose = 1;
          break;
        }
        else if (strcmp((*argv) + 2, "copyright") == 0) {
          ruby_show_copyright(mrb);
        }
        else return -3;
        return 0;
      }
    }
    else if (args->rfp == NULL) {
      if ((args->rfp = fopen(*argv, args->mrbfile ? "rb" : "r")) == NULL) {
        printf("%s: Cannot open program file. (%s)\n", *origargv, *argv);
        return 0;
      }
    }
  }

  return 0;
}

static void
cleanup(struct _args *args)
{
  if (args->rfp)
    fclose(args->rfp);
}

int
main(int argc, char **argv)
{
  mrb_state *mrb = mrb_open();
  int n = -1;
  struct _args args;
  struct mrb_parser_state *p;

  n = parse_args(mrb, argc, argv, &args);
  if (n < 0 || args.rfp == NULL) {
    cleanup(&args);
    usage(argv[0]);
    return n;
  }

  if (args.mrbfile) {
    n = mrb_load_irep(mrb, args.rfp);
  }
  else {
    p = mrb_parse_file(mrb, args.rfp);
    if (!p || !p->tree || p->nerr) {
      cleanup(&args);
      return -1;
    }

    if (args.verbose)
      parser_dump(mrb, p->tree, 0);

    n = mrb_generate_code(mrb, p->tree);
    mrb_pool_close(p->pool);
  }

  if (n >= 0) {
    if (args.verbose)
      codedump_all(mrb, n);

    if (!args.check_syntax) {
      mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_nil_value());
      if (mrb->exc) {
	mrb_funcall(mrb, mrb_nil_value(), "p", 1, mrb_obj_value(mrb->exc));
      }
    }
  }

  cleanup(&args);

  return n;
}
