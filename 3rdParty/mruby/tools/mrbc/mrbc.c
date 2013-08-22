#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/dump.h"
#include "mruby/proc.h"

#define RITEBIN_EXT ".mrb"
#define C_EXT       ".c"

void mrb_show_version(mrb_state *);
void mrb_show_copyright(mrb_state *);
void parser_dump(mrb_state*, struct mrb_ast_node*, int);
void codedump_all(mrb_state*, int);

struct mrbc_args {
  int argc;
  char **argv;
  int idx;
  const char *prog;
  const char *outfile;
  const char *initname;
  mrb_bool check_syntax : 1;
  mrb_bool verbose      : 1;
  mrb_bool debug_info   : 1;
};

static void
usage(const char *name)
{
  static const char *const usage_msg[] = {
  "switches:",
  "-c           check syntax only",
  "-o<outfile>  place the output into <outfile>",
  "-v           print version number, then turn on verbose mode",
  "-g           produce debugging information",
  "-B<symbol>   binary <symbol> output in C language format",
  "--verbose    run at verbose mode",
  "--version    print the version",
  "--copyright  print the copyright",
  NULL
  };
  const char *const *p = usage_msg;

  printf("Usage: %s [switches] programfile\n", name);
  while (*p)
    printf("  %s\n", *p++);
}

static char *
get_outfilename(mrb_state *mrb, char *infile, char *ext)
{
  size_t infilelen;
  size_t extlen;
  char *outfile;
  char *p;

  infilelen = strlen(infile);
  extlen = strlen(ext);
  outfile = (char*)mrb_malloc(mrb, infilelen + extlen + 1);
  memcpy(outfile, infile, infilelen + 1);
  if (*ext) {
    if ((p = strrchr(outfile, '.')) == NULL)
      p = outfile + infilelen;
    memcpy(p, ext, extlen + 1);
  }

  return outfile;
}

static int
parse_args(mrb_state *mrb, int argc, char **argv, struct mrbc_args *args)
{
  char *outfile = NULL;
  static const struct mrbc_args args_zero = { 0 };
  int i;

  *args = args_zero;
  args->argc = argc;
  args->argv = argv;
  args->prog = argv[0];

  for (i=1; i<argc; i++) {
    if (argv[i][0] == '-') {
      switch ((argv[i])[1]) {
      case 'o':
        if (args->outfile) {
          fprintf(stderr, "%s: an output file is already specified. (%s)\n",
                  args->prog, outfile);
          return -1;
        }
        if (argv[i][2] == '\0' && argv[i+1]) {
          i++;
          args->outfile = get_outfilename(mrb, argv[i], "");
        }
        else {
          args->outfile = get_outfilename(mrb, argv[i] + 2, "");
        }
        break;
      case 'B':
        if (argv[i][2] == '\0' && argv[i+1]) {
          i++;
          args->initname = argv[i];
        }
        else {
          args->initname = argv[i]+2;
        }
        if (*args->initname == '\0') {
          fprintf(stderr, "%s: function name is not specified.\n", args->prog);
          return -1;
        }
        break;
      case 'c':
        args->check_syntax = 1;
        break;
      case 'v':
        if (!args->verbose) mrb_show_version(mrb);
        args->verbose = 1;
        break;
      case 'g':
        args->debug_info = 1;
        break;
      case 'h':
        return -1;
      case '-':
        if (argv[i][1] == '\n') {
          return i;
        }
        if (strcmp(argv[i] + 2, "version") == 0) {
          mrb_show_version(mrb);
          exit(EXIT_SUCCESS);
        }
        else if (strcmp(argv[i] + 2, "verbose") == 0) {
          args->verbose = 1;
          break;
        }
        else if (strcmp(argv[i] + 2, "copyright") == 0) {
          mrb_show_copyright(mrb);
          exit(EXIT_SUCCESS);
        }
        return -1;
      default:
        return i;
      }
    }
    else {
      break; 
    }
  }
  return i;
}

static void
cleanup(mrb_state *mrb, struct mrbc_args *args)
{
  if (args->outfile)
    mrb_free(mrb, (void*)args->outfile);
  mrb_close(mrb);
}

static int
partial_hook(struct mrb_parser_state *p)
{
  mrbc_context *c = p->cxt;
  struct mrbc_args *args = (struct mrbc_args *)c->partial_data;
  const char *fn;

  if (p->f) fclose(p->f);
  if (args->idx >= args->argc) {
    p->f = NULL;
    return -1;
  }
  fn = args->argv[args->idx++];
  p->f = fopen(fn, "r");
  if (p->f == NULL) {
    fprintf(stderr, "%s: cannot open program file. (%s)\n", args->prog, fn);
    return -1;
  }
  mrbc_filename(p->mrb, c, fn);
  p->filename = c->filename;
  p->lineno = 1;
  return 0;
}

static int
load_file(mrb_state *mrb, struct mrbc_args *args)
{
  mrbc_context *c;
  mrb_value result;
  char *input = args->argv[args->idx];
  FILE *infile;

  c = mrbc_context_new(mrb);
  if (args->verbose)
    c->dump_result = 1;
  c->no_exec = 1;
  if (input[0] == '-' && input[1] == '\0') {
    infile = stdin;
  }
  else if ((infile = fopen(input, "r")) == NULL) {
    fprintf(stderr, "%s: cannot open program file. (%s)\n", args->prog, input);
    return EXIT_FAILURE;
  }
  mrbc_filename(mrb, c, input);
  args->idx++;
  if (args->idx < args->argc) {
    mrbc_partial_hook(mrb, c, partial_hook, (void*)args);
  }

  result = mrb_load_file_cxt(mrb, infile, c);
  if (mrb_undef_p(result) || mrb_fixnum(result) < 0) {
    mrbc_context_free(mrb, c);
    return EXIT_FAILURE;
  }
  mrbc_context_free(mrb, c);
  return EXIT_SUCCESS;
}

static int
dump_file(mrb_state *mrb, FILE *wfp, const char *outfile, struct mrbc_args *args)
{
  int n = MRB_DUMP_OK;

  if (args->initname) {
    n = mrb_dump_irep_cfunc(mrb, 0, args->debug_info, wfp, args->initname);
    if (n == MRB_DUMP_INVALID_ARGUMENT) {
      fprintf(stderr, "%s: invalid C language symbol name\n", args->initname);
    }
  }
  else {
    n = mrb_dump_irep_binary(mrb, 0, args->debug_info, wfp);
  }
  if (n != MRB_DUMP_OK) {
    fprintf(stderr, "%s: error in mrb dump (%s) %d\n", args->prog, outfile, n);
  }
  return n;
}

int
main(int argc, char **argv)
{
  mrb_state *mrb = mrb_open();
  int n, result;
  struct mrbc_args args;
  FILE *wfp;

  if (mrb == NULL) {
    fputs("Invalid mrb_state, exiting mrbc\n", stderr);
    return EXIT_FAILURE;
  }

  n = parse_args(mrb, argc, argv, &args);
  if (n < 0) {
    cleanup(mrb, &args);
    usage(argv[0]);
    return EXIT_FAILURE;
  }
  if (n == argc) {
    fprintf(stderr, "%s: no program file given\n", args.prog);
    return EXIT_FAILURE;
  }
  if (args.outfile == NULL) {
    if (n + 1 == argc) {
      args.outfile = get_outfilename(mrb, argv[n], args.initname ? C_EXT : RITEBIN_EXT);
    }
    else {
      fprintf(stderr, "%s: output file should be specified to compile multiple files\n", args.prog);
      return EXIT_FAILURE;
    }
  }

  args.idx = n;
  if (load_file(mrb, &args) == EXIT_FAILURE) {
    cleanup(mrb, &args);
    return EXIT_FAILURE;
  }
  if (args.check_syntax) {
    printf("%s:%s:Syntax OK", args.prog, argv[n]);
  }

  if (args.check_syntax) {
    cleanup(mrb, &args);
    return EXIT_SUCCESS;
  }

  if (args.outfile) {
    if (strcmp("-", args.outfile) == 0) {
      wfp = stdout;
    }
    else if ((wfp = fopen(args.outfile, "wb")) == NULL) {
      fprintf(stderr, "%s: cannot open output file:(%s)\n", args.prog, args.outfile);
      return EXIT_FAILURE;
    }
  }
  else {
    fprintf(stderr, "Output file is required\n");
    return EXIT_FAILURE;
  }
  result = dump_file(mrb, wfp, args.outfile, &args);
  fclose(wfp);
  cleanup(mrb, &args);
  if (result != MRB_DUMP_OK) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

void
mrb_init_mrblib(mrb_state *mrb)
{
}

#ifndef DISABLE_GEMS
void
mrb_init_mrbgems(mrb_state *mrb)
{
}

void
mrb_final_mrbgems(mrb_state *mrb)
{
}
#endif
