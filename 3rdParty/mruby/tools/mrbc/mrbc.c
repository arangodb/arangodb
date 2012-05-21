#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/dump.h"
#include "mruby/cdump.h"
#include "compile.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RITEBIN_EXT ".mrb"
#define C_EXT       ".c"
void ruby_show_version(mrb_state *);
void ruby_show_copyright(mrb_state *);
void parser_dump(mrb_state*, struct mrb_ast_node*, int);
void codedump_all(mrb_state*, int);

struct _args {
  FILE *rfp;
  FILE *wfp;
  char *initname;
  char *ext;
  int check_syntax : 1;
  int dump_type    : 2;
  int verbose      : 1;
};

static void
usage(const char *name)
{
  static const char *const usage_msg[] = {
  "switches:",
  "-c           check syntax only",
  "-o<outfile>  place the output into <outfile>",
  "-v           print version number, then trun on verbose mode",
  "-B<symbol>   binary <symbol> output in C language format",
  "-C<func>     function <func> output in C language format",
  "--verbose    run at verbose mode",
  "--version    print the version",
  "--copyright  print the copyright",
  NULL
  };
  const char *const *p = usage_msg;

  printf("Usage: %s [switches] programfile\n", name);
  while(*p)
  printf("  %s\n", *p++);
}

static char *
get_outfilename(char *infile, char *ext)
{
  char *outfile;
  char *p;

  outfile = (char*)malloc(strlen(infile) + strlen(ext) + 1);
  strcpy(outfile, infile);
  if (*ext) {
    if ((p = strrchr(outfile, '.')) == NULL)
      p = &outfile[strlen(outfile)];
    strcpy(p, ext);
  }

  return outfile;
}

static int
parse_args(mrb_state *mrb, int argc, char **argv, struct _args *args)
{
  char *infile = NULL;
  char *outfile = NULL;
  char **origargv = argv;

  memset(args, 0, sizeof(*args));
  args->ext = RITEBIN_EXT;

  for (argc--,argv++; argc > 0; argc--,argv++) {
    if (**argv == '-') {
      if (strlen(*argv) <= 1)
        return -1;

      switch ((*argv)[1]) {
      case 'o':
        outfile = get_outfilename((*argv) + 2, "");
        break;
      case 'B':
      case 'C':
        args->ext = C_EXT;
        args->initname = (*argv) + 2;
        if (*args->initname == '\0') {
          printf("%s: Function name is not specified.\n", *origargv);
          return -2;
        }
        args->dump_type = ((*argv)[1] == 'B') ? DUMP_TYPE_BIN : DUMP_TYPE_CODE;
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
      infile = *argv;
      if ((args->rfp = fopen(infile, "r")) == NULL) {
        printf("%s: Cannot open program file. (%s)\n", *origargv, infile);
        return 0;
      }
    }
  }

  if (infile == NULL)
    return -4;
  if (args->check_syntax)
    return 0;

  if (outfile == NULL)
    outfile = get_outfilename(infile, args->ext);

  if ((args->wfp = fopen(outfile, "wb")) == NULL) {
    printf("%s: Cannot open output file. (%s)\n", *origargv, outfile);
    return 0;
  }

  return 0;
}

static void
cleanup(struct _args *args)
{
  if (args->rfp)
    fclose(args->rfp);
  if (args->wfp)
    fclose(args->wfp);
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

  p = mrb_parse_file(mrb, args.rfp);
  if (!p || !p->tree || p->nerr) {
    cleanup(&args);
    return -1;
  }

  if (args.verbose)
    parser_dump(mrb, p->tree, 0);

  n = mrb_generate_code(mrb, p->tree);
  mrb_pool_close(p->pool);

  if (args.verbose)
    codedump_all(mrb, n);

  if (n < 0 || args.check_syntax) {
    cleanup(&args);
    return n;
  }
  if (args.initname) {
    if (args.dump_type == DUMP_TYPE_BIN)
      n = mrb_bdump_irep(mrb, n, args.wfp, args.initname);
    else
      n = mrb_cdump_irep(mrb, n, args.wfp, args.initname);
  }
  else {
    n = mrb_dump_irep(mrb, n, args.wfp);
  }

  cleanup(&args);

  return n;
}

void
mrb_init_ext(mrb_state *mrb)
{
}

void
mrb_init_mrblib(mrb_state *mrb)
{
}

