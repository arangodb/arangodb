/*
** compile.h - mruby parser
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_COMPILE_H
#define MRUBY_COMPILE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "mruby.h"
#include <stdio.h>
#include <setjmp.h>

typedef struct mrb_ast_node {
  struct mrb_ast_node *car, *cdr;
} mrb_ast_node;

#include <stdio.h>

enum mrb_lex_state_enum {
    EXPR_BEG,                   /* ignore newline, +/- is a sign. */
    EXPR_END,                   /* newline significant, +/- is an operator. */
    EXPR_ENDARG,                /* ditto, and unbound braces. */
    EXPR_ENDFN,                 /* ditto, and unbound braces. */
    EXPR_ARG,                   /* newline significant, +/- is an operator. */
    EXPR_CMDARG,                /* newline significant, +/- is an operator. */
    EXPR_MID,                   /* newline significant, +/- is an operator. */
    EXPR_FNAME,                 /* ignore newline, no reserved words. */
    EXPR_DOT,                   /* right after `.' or `::', no reserved words. */
    EXPR_CLASS,                 /* immediate after `class', no here document. */
    EXPR_VALUE,                 /* alike EXPR_BEG but label is disallowed. */
    EXPR_MAX_STATE
};

struct mrb_parser_message {
  int lineno;
  int column;
  char* message;
};

struct mrb_parser_state {
  mrb_state *mrb;
  struct mrb_pool *pool;
  mrb_ast_node *cells;
  const char *s, *send;
  FILE *f;
  int lineno;
  int column;
  const char *filename;

  enum mrb_lex_state_enum lstate;
  int sterm;

  unsigned int cond_stack;
  unsigned int cmdarg_stack;
  int paren_nest;
  int lpar_beg;

  mrb_ast_node *pb;
  char buf[1024];
  int bidx;

  mrb_ast_node *heredoc;

  int in_def, in_single, cmd_start;
  mrb_ast_node *locals;

  void *ylval;

  int nerr;
  int nwarn;
  mrb_ast_node *tree, *begin_tree;

  int capture_errors;
  struct mrb_parser_message error_buffer[10];
  struct mrb_parser_message warn_buffer[10];

  jmp_buf jmp;
};

/* parser structure */
struct mrb_parser_state* mrb_parser_new(mrb_state*);
const char *mrb_parser_filename(struct mrb_parser_state*, const char*);
int mrb_parser_lineno(struct mrb_parser_state*, int);
void mrb_parser_parse(struct mrb_parser_state*);

/* utility functions */
struct mrb_parser_state* mrb_parse_file(mrb_state*,FILE*);
struct mrb_parser_state* mrb_parse_string(mrb_state*,const char*);
struct mrb_parser_state* mrb_parse_nstring(mrb_state*,const char*,size_t);
int mrb_generate_code(mrb_state*, mrb_ast_node*);

int mrb_compile_file(mrb_state*,FILE*);
int mrb_compile_string(mrb_state*,char*);
int mrb_compile_nstring(mrb_state*,char*,size_t);

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif /* MRUBY_COMPILE_H */
