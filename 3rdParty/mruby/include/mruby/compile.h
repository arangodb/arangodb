/*
** mruby/compile.h - mruby parser
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_COMPILE_H
#define MRUBY_COMPILE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "mruby.h"
#include <setjmp.h>

struct mrb_parser_state;
/* load context */
typedef struct mrbc_context {
  mrb_sym *syms;
  int slen;
  char *filename;
  short lineno;
  int (*partial_hook)(struct mrb_parser_state*);
  void *partial_data;
  struct RClass *target_class;
  mrb_bool capture_errors:1;
  mrb_bool dump_result:1;
  mrb_bool no_exec:1;
} mrbc_context;

mrbc_context* mrbc_context_new(mrb_state *mrb);
void mrbc_context_free(mrb_state *mrb, mrbc_context *cxt);
const char *mrbc_filename(mrb_state *mrb, mrbc_context *c, const char *s);
void mrbc_partial_hook(mrb_state *mrb, mrbc_context *c, int (*partial_hook)(struct mrb_parser_state*), void*data);

/* AST node structure */
typedef struct mrb_ast_node {
  struct mrb_ast_node *car, *cdr;
  short lineno;
} mrb_ast_node;

/* lexer states */
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

/* saved error message */
struct mrb_parser_message {
  int lineno;
  int column;
  char* message;
};

#define STR_FUNC_PARSING 0x01
#define STR_FUNC_EXPAND  0x02
#define STR_FUNC_REGEXP  0x04
#define STR_FUNC_WORD    0x08
#define STR_FUNC_SYMBOL  0x10
#define STR_FUNC_ARRAY   0x20
#define STR_FUNC_HEREDOC 0x40
#define STR_FUNC_XQUOTE  0x80

enum mrb_string_type {
  str_not_parsing  = (0),
  str_squote   = (STR_FUNC_PARSING),
  str_dquote   = (STR_FUNC_PARSING|STR_FUNC_EXPAND),
  str_regexp   = (STR_FUNC_PARSING|STR_FUNC_REGEXP|STR_FUNC_EXPAND),
  str_sword    = (STR_FUNC_PARSING|STR_FUNC_WORD|STR_FUNC_ARRAY),
  str_dword    = (STR_FUNC_PARSING|STR_FUNC_WORD|STR_FUNC_ARRAY|STR_FUNC_EXPAND),
  str_ssym     = (STR_FUNC_PARSING|STR_FUNC_SYMBOL),
  str_ssymbols = (STR_FUNC_PARSING|STR_FUNC_SYMBOL|STR_FUNC_ARRAY),
  str_dsymbols = (STR_FUNC_PARSING|STR_FUNC_SYMBOL|STR_FUNC_ARRAY|STR_FUNC_EXPAND),
  str_heredoc  = (STR_FUNC_PARSING|STR_FUNC_HEREDOC),
  str_xquote   = (STR_FUNC_PARSING|STR_FUNC_XQUOTE|STR_FUNC_EXPAND),
};

/* heredoc structure */
struct mrb_parser_heredoc_info {
  mrb_bool allow_indent:1;
  mrb_bool line_head:1;
  enum mrb_string_type type;
  const char *term;
  int term_len;
  mrb_ast_node *doc;
};

#ifndef MRB_PARSER_BUF_SIZE
# define MRB_PARSER_BUF_SIZE 1024
#endif

/* parser structure */
struct mrb_parser_state {
  mrb_state *mrb;
  struct mrb_pool *pool;
  mrb_ast_node *cells;
  const char *s, *send;
#ifdef ENABLE_STDIO
  FILE *f;
#endif
  mrbc_context *cxt;
  char *filename;
  int lineno;
  int column;

  enum mrb_lex_state_enum lstate;
  mrb_ast_node *lex_strterm; /* (type nest_level beg . end) */

  unsigned int cond_stack;
  unsigned int cmdarg_stack;
  int paren_nest;
  int lpar_beg;
  int in_def, in_single, cmd_start;
  mrb_ast_node *locals;

  mrb_ast_node *pb;
  char buf[MRB_PARSER_BUF_SIZE];
  int bidx;

  mrb_ast_node *heredocs;	/* list of mrb_parser_heredoc_info* */
  mrb_ast_node *parsing_heredoc;
  mrb_bool heredoc_starts_nextline:1;
  mrb_bool heredoc_end_now:1; /* for mirb */

  void *ylval;

  size_t nerr;
  size_t nwarn;
  mrb_ast_node *tree;

  int capture_errors;
  struct mrb_parser_message error_buffer[10];
  struct mrb_parser_message warn_buffer[10];

  jmp_buf jmp;
};

struct mrb_parser_state* mrb_parser_new(mrb_state*);
void mrb_parser_free(struct mrb_parser_state*);
void mrb_parser_parse(struct mrb_parser_state*,mrbc_context*);

/* utility functions */
#ifdef ENABLE_STDIO
struct mrb_parser_state* mrb_parse_file(mrb_state*,FILE*,mrbc_context*);
#endif
struct mrb_parser_state* mrb_parse_string(mrb_state*,const char*,mrbc_context*);
struct mrb_parser_state* mrb_parse_nstring(mrb_state*,const char*,int,mrbc_context*);
int mrb_generate_code(mrb_state*, struct mrb_parser_state*);

/* program load functions */
#ifdef ENABLE_STDIO
mrb_value mrb_load_file(mrb_state*,FILE*);
#endif
mrb_value mrb_load_string(mrb_state *mrb, const char *s);
mrb_value mrb_load_nstring(mrb_state *mrb, const char *s, int len);
#ifdef ENABLE_STDIO
mrb_value mrb_load_file_cxt(mrb_state*,FILE*, mrbc_context *cxt);
#endif
mrb_value mrb_load_string_cxt(mrb_state *mrb, const char *s, mrbc_context *cxt);
mrb_value mrb_load_nstring_cxt(mrb_state *mrb, const char *s, int len, mrbc_context *cxt);

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif /* MRUBY_COMPILE_H */
