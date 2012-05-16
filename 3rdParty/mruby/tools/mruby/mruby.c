#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/array.h"
#include "mruby/string.h"
#include "compile.h"
#include "mruby/dump.h"
#include <stdio.h>
#include <string.h>

void ruby_show_version(mrb_state *);
void ruby_show_copyright(mrb_state *);
void parser_dump(mrb_state*, struct mrb_ast_node*, int);
void codedump_all(mrb_state*, int);

struct _args {
  FILE *rfp;
  char* cmdline;
  int mrbfile      : 1;
  int check_syntax : 1;
  int verbose      : 1;
  int argc;
  char** argv;
};

static void
usage(const char *name)
{
  static const char *const usage_msg[] = {
  "switches:",
  "-b           load and execute RiteBinary (mrb) file",
  "-c           check syntax only",
  "-e 'command' one line of script",
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
    if (argv[0][0] != '-') break;

    if (strlen(*argv) <= 1)
      return -1;

    switch ((*argv)[1]) {
    case 'b':
      args->mrbfile = 1;
      break;
    case 'c':
      args->check_syntax = 1;
      break;
    case 'e':
      if (argc > 1) {
        argc--; argv++;
        if (!args->cmdline) {
          char *buf;

          buf = mrb_malloc(mrb, strlen(argv[0])+1);
          strcpy(buf, argv[0]);
          args->cmdline = buf;
        }
        else {
          args->cmdline = mrb_realloc(mrb, args->cmdline, strlen(args->cmdline)+strlen(argv[0])+2);
          strcat(args->cmdline, "\n");
          strcat(args->cmdline, argv[0]);
        }
      }
      else {
        printf("%s: No code specified for -e\n", *origargv);
        return 0;
      }
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


  if (args->rfp == NULL && args->cmdline == NULL && (args->rfp = fopen(*argv, args->mrbfile ? "rb" : "r")) == NULL) {
    printf("%s: Cannot open program file. (%s)\n", *origargv, *argv);
    return 0;
  }
  args->argv = mrb_realloc(mrb, args->argv, sizeof(char*) * (argc + 1));
  memcpy(args->argv, argv, (argc+1) * sizeof(char*));
  args->argc = argc;

  return 0;
}

static void
cleanup(mrb_state *mrb, struct _args *args)
{
  if (args->rfp)
    fclose(args->rfp);
  if (args->cmdline)
    mrb_free(mrb, args->cmdline);
  if (args->argv)
    mrb_free(mrb, args->argv);
}

int
main(int argc, char **argv)
{
  mrb_state *mrb = mrb_open();
  int n = -1;
  int i;
  struct _args args;
  struct mrb_parser_state *p;

  n = parse_args(mrb, argc, argv, &args);
  if (n < 0 || (args.cmdline == NULL && args.rfp == NULL)) {
    cleanup(mrb, &args);
    usage(argv[0]);
    return n;
  }

  if (args.mrbfile) {
    n = mrb_load_irep(mrb, args.rfp);
  }
  else {
    if (args.cmdline) {
      p = mrb_parse_string(mrb, (char*)args.cmdline);
    }
    else {
      p = mrb_parse_file(mrb, args.rfp);
    }
    if (!p || !p->tree || p->nerr) {
      cleanup(mrb, &args);
      return -1;
    }

    if (args.verbose)
      parser_dump(mrb, p->tree, 0);

    n = mrb_generate_code(mrb, p->tree);
    mrb_pool_close(p->pool);
  }

  if (n >= 0) {
    mrb_value ARGV = mrb_ary_new(mrb);
    for (i = 0; i < args.argc; i++) {
      mrb_ary_push(mrb, ARGV, mrb_str_new(mrb, args.argv[i], strlen(args.argv[i])));
    }
    mrb_define_global_const(mrb, "ARGV", ARGV);

    if (args.verbose)
      codedump_all(mrb, n);

    if (!args.check_syntax) {
      mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_top_self(mrb));
      if (mrb->exc) {
        mrb_p(mrb, mrb_obj_value(mrb->exc));
      }
    }
  }

  cleanup(mrb, &args);

  return n > 0 ? 0 : 1;
}
