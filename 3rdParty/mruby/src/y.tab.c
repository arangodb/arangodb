
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 7 "./parse.y"

#undef PARSER_TEST
#undef PARSER_DEBUG

#define YYDEBUG 1
#define YYERROR_VERBOSE 1
/*
 * Force yacc to use our memory management.  This is a little evil because
 * the macros assume that "parser_state *p" is in scope
 */
#define YYMALLOC(n)    mrb_malloc(p->mrb, (n))
#define YYFREE(o)      mrb_free(p->mrb, (o))
#define YYSTACK_USE_ALLOCA 0

#include "mruby.h"
#include "mruby/compile.h"
#include "node.h"
#include "st.h"

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#define YYLEX_PARAM p

typedef mrb_ast_node node;
typedef struct mrb_parser_state parser_state;

static int yylex(void *lval, parser_state *p);
static void yyerror(parser_state *p, const char *s);
static void yywarn(parser_state *p, const char *s);
static void yywarning(parser_state *p, const char *s);
static void backref_error(parser_state *p, node *n);

#define identchar(c) (isalnum(c) || (c) == '_' || !isascii(c))

typedef unsigned int stack_type;

#define BITSTACK_PUSH(stack, n)	((stack) = ((stack)<<1)|((n)&1))
#define BITSTACK_POP(stack)	((stack) = (stack) >> 1)
#define BITSTACK_LEXPOP(stack)	((stack) = ((stack) >> 1) | ((stack) & 1))
#define BITSTACK_SET_P(stack)	((stack)&1)

#define COND_PUSH(n)	BITSTACK_PUSH(p->cond_stack, (n))
#define COND_POP()	BITSTACK_POP(p->cond_stack)
#define COND_LEXPOP()	BITSTACK_LEXPOP(p->cond_stack)
#define COND_P()	BITSTACK_SET_P(p->cond_stack)

#define CMDARG_PUSH(n)	BITSTACK_PUSH(p->cmdarg_stack, (n))
#define CMDARG_POP()	BITSTACK_POP(p->cmdarg_stack)
#define CMDARG_LEXPOP()	BITSTACK_LEXPOP(p->cmdarg_stack)
#define CMDARG_P()	BITSTACK_SET_P(p->cmdarg_stack)

static mrb_sym
intern_gen(parser_state *p, const char *s)
{
  return mrb_intern(p->mrb, s);
}
#define intern(s) intern_gen(p,(s))

static void
cons_free_gen(parser_state *p, node *cons)
{
  cons->cdr = p->cells;
  p->cells = cons;
}
#define cons_free(c) cons_free_gen(p, (c))

static void*
parser_palloc(parser_state *p, size_t size)
{
  void *m = mrb_pool_alloc(p->pool, size);

  if (!m) {
    longjmp(p->jmp, 1);
  }
  return m;
}

static node*
cons_gen(parser_state *p, node *car, node *cdr)
{
  node *c;

  if (p->cells) {
    c = p->cells;
    p->cells = p->cells->cdr;
  }
  else {
    c = parser_palloc(p, sizeof(mrb_ast_node));
  }

  c->car = car;
  c->cdr = cdr;
  return c;
}
#define cons(a,b) cons_gen(p,(a),(b))

static node*
list1_gen(parser_state *p, node *a)
{
  return cons(a, 0);
}
#define list1(a) list1_gen(p, (a))

static node*
list2_gen(parser_state *p, node *a, node *b)
{
  return cons(a, cons(b,0));
}
#define list2(a,b) list2_gen(p, (a),(b))

static node*
list3_gen(parser_state *p, node *a, node *b, node *c)
{
  return cons(a, cons(b, cons(c,0)));
}
#define list3(a,b,c) list3_gen(p, (a),(b),(c))

static node*
list4_gen(parser_state *p, node *a, node *b, node *c, node *d)
{
  return cons(a, cons(b, cons(c, cons(d, 0))));
}
#define list4(a,b,c,d) list4_gen(p, (a),(b),(c),(d))

static node*
list5_gen(parser_state *p, node *a, node *b, node *c, node *d, node *e)
{
  return cons(a, cons(b, cons(c, cons(d, cons(e, 0)))));
}
#define list5(a,b,c,d,e) list5_gen(p, (a),(b),(c),(d),(e))

static node*
list6_gen(parser_state *p, node *a, node *b, node *c, node *d, node *e, node *f)
{
  return cons(a, cons(b, cons(c, cons(d, cons(e, cons(f, 0))))));
}
#define list6(a,b,c,d,e,f) list6_gen(p, (a),(b),(c),(d),(e),(f))

static node*
append_gen(parser_state *p, node *a, node *b)
{
  node *c = a;

  if (!a) return b;
  while (c->cdr) {
    c = c->cdr;
  }
  if (b) {
    c->cdr = b;
  }
  return a;
}
#define append(a,b) append_gen(p,(a),(b))
#define push(a,b) append_gen(p,(a),list1(b))

static char*
parser_strndup(parser_state *p, const char *s, size_t len)
{
  char *b = parser_palloc(p, len+1);

  memcpy(b, s, len);
  b[len] = '\0';
  return b;
}
#define strndup(s,len) parser_strndup(p, s, len)

static char*
parser_strdup(parser_state *p, const char *s)
{
  return parser_strndup(p, s, strlen(s));
}
#undef strdup
#define strdup(s) parser_strdup(p, s)

// xxx -----------------------------

static node*
local_switch(parser_state *p)
{
  node *prev = p->locals;

  p->locals = cons(0, 0);
  return prev;
}

static void
local_resume(parser_state *p, node *prev)
{
  p->locals = prev;
}

static void
local_nest(parser_state *p)
{
  p->locals = cons(0, p->locals);
}

static void
local_unnest(parser_state *p)
{
  p->locals = p->locals->cdr;
}

static int
local_var_p(parser_state *p, mrb_sym sym)
{
  node *l = p->locals;

  while (l) {
    node *n = l->car;
    while (n) {
      if ((mrb_sym)n->car == sym) return 1;
      n = n->cdr;
    }
    l = l->cdr;
  }
  return 0;
}

static void
local_add_f(parser_state *p, mrb_sym sym)
{
  p->locals->car = push(p->locals->car, (node*)sym);
}

static void
local_add(parser_state *p, mrb_sym sym)
{
  if (!local_var_p(p, sym)) {
    local_add_f(p, sym);
  }
}

// (:scope (vars..) (prog...))
static node*
new_scope(parser_state *p, node *body)
{
  return cons((node*)NODE_SCOPE, cons(p->locals->car, body));
}

// (:begin prog...)
static node*
new_begin(parser_state *p, node *body)
{
  if (body) 
    return list2((node*)NODE_BEGIN, body);
  return cons((node*)NODE_BEGIN, 0);
}

#define newline_node(n) (n)

// (:rescue body rescue else)
static node*
new_rescue(parser_state *p, node *body, node *resq, node *els)
{
  return list4((node*)NODE_RESCUE, body, resq, els);
}

// (:ensure body ensure)
static node*
new_ensure(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_ENSURE, cons(a, cons(0, b)));
}

// (:nil)
static node*
new_nil(parser_state *p)
{
  return list1((node*)NODE_NIL);
}

// (:true)
static node*
new_true(parser_state *p)
{
  return list1((node*)NODE_TRUE);
}

// (:false)
static node*
new_false(parser_state *p)
{
  return list1((node*)NODE_FALSE);
}

// (:alias new old)
static node*
new_alias(parser_state *p, mrb_sym a, mrb_sym b)
{
  return cons((node*)NODE_ALIAS, cons((node*)a, (node*)b));
}

// (:if cond then else)
static node*
new_if(parser_state *p, node *a, node *b, node *c)
{
  return list4((node*)NODE_IF, a, b, c);
}

// (:unless cond then else)
static node*
new_unless(parser_state *p, node *a, node *b, node *c)
{
  return list4((node*)NODE_IF, a, c, b);
}

// (:while cond body)
static node*
new_while(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_WHILE, cons(a, b));
}

// (:until cond body)
static node*
new_until(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_UNTIL, cons(a, b));
}

// (:for var obj body)
static node*
new_for(parser_state *p, node *v, node *o, node *b)
{
  return list4((node*)NODE_FOR, v, o, b);
}

// (:case a ((when ...) body) ((when...) body))
static node*
new_case(parser_state *p, node *a, node *b)
{
  node *n = list2((node*)NODE_CASE, a);
  node *n2 = n;

  while (n2->cdr) {
    n2 = n2->cdr;
  }
  n2->cdr = b;
  return n;
}

// (:postexe a)
static node*
new_postexe(parser_state *p, node *a)
{
  return cons((node*)NODE_POSTEXE, a);
}

// (:self)
static node*
new_self(parser_state *p)
{
  return list1((node*)NODE_SELF);
}

// (:call a b c)
static node*
new_call(parser_state *p, node *a, mrb_sym b, node *c)
{
  return list4((node*)NODE_CALL, a, (node*)b, c);
}

// (:fcall self mid args)
static node*
new_fcall(parser_state *p, mrb_sym b, node *c)
{
  return list4((node*)NODE_FCALL, new_self(p), (node*)b, c);
}

#if 0
// (:vcall self mid)
static node*
new_vcall(parser_state *p, mrb_sym b)
{
  return list3((node*)NODE_VCALL, new_self(p), (node*)b);
}
#endif

// (:super . c)
static node*
new_super(parser_state *p, node *c)
{
  return cons((node*)NODE_SUPER, c);
}

// (:zsuper)
static node*
new_zsuper(parser_state *p)
{
  return list1((node*)NODE_ZSUPER);
}

// (:yield . c)
static node*
new_yield(parser_state *p, node *c)
{
  if (c) {
    if (c->cdr) {
      yyerror(p, "both block arg and actual block given");
    }
    return cons((node*)NODE_YIELD, c->car);
  }
  return cons((node*)NODE_YIELD, 0);
}

// (:return . c)
static node*
new_return(parser_state *p, node *c)
{
  return cons((node*)NODE_RETURN, c);
}

// (:break . c)
static node*
new_break(parser_state *p, node *c)
{
  return cons((node*)NODE_BREAK, c);
}

// (:next . c)
static node*
new_next(parser_state *p, node *c)
{
  return cons((node*)NODE_NEXT, c);
}

// (:redo)
static node*
new_redo(parser_state *p)
{
  return list1((node*)NODE_REDO);
}

// (:retry)
static node*
new_retry(parser_state *p)
{
  return list1((node*)NODE_RETRY);
}

// (:dot2 a b)
static node*
new_dot2(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_DOT2, cons(a, b));
}

// (:dot3 a b)
static node*
new_dot3(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_DOT3, cons(a, b));
}

// (:colon2 b c)
static node*
new_colon2(parser_state *p, node *b, mrb_sym c)
{
  return cons((node*)NODE_COLON2, cons(b, (node*)c));
}

// (:colon3 . c)
static node*
new_colon3(parser_state *p, mrb_sym c)
{
  return cons((node*)NODE_COLON3, (node*)c);
}

// (:and a b)
static node*
new_and(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_AND, cons(a, b));
}

// (:or a b)
static node*
new_or(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_OR, cons(a, b));
}

// (:array a...)
static node*
new_array(parser_state *p, node *a)
{
  return cons((node*)NODE_ARRAY, a);
}

// (:splat . a)
static node*
new_splat(parser_state *p, node *a)
{
  return cons((node*)NODE_SPLAT, a);
}

// (:hash (k . v) (k . v)...)
static node*
new_hash(parser_state *p, node *a)
{
  return cons((node*)NODE_HASH, a);
}

// (:sym . a)
static node*
new_sym(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_SYM, (node*)sym);
}

static mrb_sym
new_strsym(parser_state *p, node* str)
{
  const char *s = (const char*)str->cdr->car;
  size_t len = (size_t)str->cdr->cdr;

  return mrb_intern2(p->mrb, s, len);
}

// (:lvar . a)
static node*
new_lvar(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_LVAR, (node*)sym);
}

// (:gvar . a)
static node*
new_gvar(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_GVAR, (node*)sym);
}

// (:ivar . a)
static node*
new_ivar(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_IVAR, (node*)sym);
}

// (:cvar . a)
static node*
new_cvar(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_CVAR, (node*)sym);
}

// (:const . a)
static node*
new_const(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_CONST, (node*)sym);
}

// (:undef a...)
static node*
new_undef(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_UNDEF, (node*)sym);
}

// (:class class super body)
static node*
new_class(parser_state *p, node *c, node *s, node *b)
{
  return list4((node*)NODE_CLASS, c, s, cons(p->locals->car, b));
}

// (:sclass obj body)
static node*
new_sclass(parser_state *p, node *o, node *b)
{
  return list3((node*)NODE_SCLASS, o, cons(p->locals->car, b));
}

// (:module module body)
static node*
new_module(parser_state *p, node *m, node *b)
{
  return list3((node*)NODE_MODULE, m, cons(p->locals->car, b));
}

// (:def m lv (arg . body))
static node*
new_def(parser_state *p, mrb_sym m, node *a, node *b)
{
  return list5((node*)NODE_DEF, (node*)m, p->locals->car, a, b);
}

// (:sdef obj m lv (arg . body))
static node*
new_sdef(parser_state *p, node *o, mrb_sym m, node *a, node *b)
{
  return list6((node*)NODE_SDEF, o, (node*)m, p->locals->car, a, b);
}

// (:arg . sym)
static node*
new_arg(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_ARG, (node*)sym);
}

// (m o r m2 b)
// m: (a b c)
// o: ((a . e1) (b . e2))
// r: a
// m2: (a b c)
// b: a
static node*
new_args(parser_state *p, node *m, node *opt, mrb_sym rest, node *m2, mrb_sym blk)
{
  node *n;

  n = cons(m2, (node*)blk);
  n = cons((node*)rest, n);
  n = cons(opt, n);
  return cons(m, n);
}

// (:block_arg . a)
static node*
new_block_arg(parser_state *p, node *a)
{
  return cons((node*)NODE_BLOCK_ARG, a);
}

// (:block arg body)
static node*
new_block(parser_state *p, node *a, node *b)
{
  return list4((node*)NODE_BLOCK, p->locals->car, a, b);
}

// (:lambda arg body)
static node*
new_lambda(parser_state *p, node *a, node *b)
{
  return list4((node*)NODE_LAMBDA, p->locals->car, a, b);
}

// (:asgn lhs rhs)
static node*
new_asgn(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_ASGN, cons(a, b));
}

// (:masgn mlhs=(pre rest post)  mrhs)
static node*
new_masgn(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_MASGN, cons(a, b));
}

// (:asgn lhs rhs)
static node*
new_op_asgn(parser_state *p, node *a, mrb_sym op, node *b)
{
  return list4((node*)NODE_OP_ASGN, a, (node*)op, b);
}

// (:int . i)
static node*
new_int(parser_state *p, const char *s, int base)
{
  return list3((node*)NODE_INT, (node*)strdup(s), (node*)(intptr_t)base);
}

// (:float . i)
static node*
new_float(parser_state *p, const char *s)
{
  return cons((node*)NODE_FLOAT, (node*)strdup(s));
}

// (:str . (s . len))
static node*
new_str(parser_state *p, const char *s, size_t len)
{
  return cons((node*)NODE_STR, cons((node*)strndup(s, len), (node*)len));
}

// (:dstr . a)
static node*
new_dstr(parser_state *p, node *a)
{
  return cons((node*)NODE_DSTR, a);
}

// (:dsym . a)
static node*
new_dsym(parser_state *p, node *a)
{
  return cons((node*)NODE_DSYM, new_dstr(p, a));
}

// (:backref . n)
static node*
new_back_ref(parser_state *p, int n)
{
  return cons((node*)NODE_BACK_REF, (node*)(intptr_t)n);
}

// (:nthref . n)
static node*
new_nth_ref(parser_state *p, int n)
{
  return cons((node*)NODE_NTH_REF, (node*)(intptr_t)n);
}

static void
new_bv(parser_state *p, mrb_sym id)
{
}

// xxx -----------------------------

// (:call a op)
static node*
call_uni_op(parser_state *p, node *recv, char *m)
{
  return new_call(p, recv, intern(m), 0);
}

// (:call a op b)
static node*
call_bin_op(parser_state *p, node *recv, char *m, node *arg1)
{
  return new_call(p, recv, intern(m), list1(list1(arg1)));
}

// (:match (a . b))
static node*
match_op(parser_state *p, node *a, node *b)
{
  return cons((node*)NODE_MATCH, cons((node*)a, (node*)b));
}


static void
args_with_block(parser_state *p, node *a, node *b)
{
  if (b) {
    if (a->cdr) {
      yyerror(p, "both block arg and actual block given");
    }
    a->cdr = b;
  }
}

static void
call_with_block(parser_state *p, node *a, node *b)
{
  node *n;

  if (a->car == (node*)NODE_SUPER ||
      a->car == (node*)NODE_ZSUPER) {
    if (!a->cdr) a->cdr = cons(0, b);
    else {
      args_with_block(p, a->cdr, b);
    }
  }
  else {
    n = a->cdr->cdr->cdr;
    if (!n->car) n->car = cons(0, b);
    else {
      args_with_block(p, n->car, b);
    }
  }
}

static node*
negate_lit(parser_state *p, node *n)
{
  return cons((node*)NODE_NEGATE, n);
}

static node*
cond(node *n)
{
  return n;
}

static node*
ret_args(parser_state *p, node *n)
{
  if (n->cdr) {
    yyerror(p, "block argument should not be given");
  }
  if (!n->car->cdr) return n->car->car;
  return new_array(p, n->car);
}

static void
assignable(parser_state *p, node *lhs)
{
  switch ((int)(intptr_t)lhs->car) {
  case NODE_LVAR:
    local_add(p, (mrb_sym)lhs->cdr);
    break;
  default:
    break;
  }
}

static node*
var_reference(parser_state *p, node *lhs)
{
  node *n;

  switch ((int)(intptr_t)lhs->car) {
  case NODE_LVAR:
    if (!local_var_p(p, (mrb_sym)lhs->cdr)) {
      n = new_fcall(p, (mrb_sym)lhs->cdr, 0);
      cons_free(lhs);
      return n;
    }
    break;
  default:
    break;
  }
  return lhs;
}

// xxx -----------------------------



/* Line 189 of yacc.c  */
#line 907 "./y.tab.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     keyword_class = 258,
     keyword_module = 259,
     keyword_def = 260,
     keyword_undef = 261,
     keyword_begin = 262,
     keyword_rescue = 263,
     keyword_ensure = 264,
     keyword_end = 265,
     keyword_if = 266,
     keyword_unless = 267,
     keyword_then = 268,
     keyword_elsif = 269,
     keyword_else = 270,
     keyword_case = 271,
     keyword_when = 272,
     keyword_while = 273,
     keyword_until = 274,
     keyword_for = 275,
     keyword_break = 276,
     keyword_next = 277,
     keyword_redo = 278,
     keyword_retry = 279,
     keyword_in = 280,
     keyword_do = 281,
     keyword_do_cond = 282,
     keyword_do_block = 283,
     keyword_do_LAMBDA = 284,
     keyword_return = 285,
     keyword_yield = 286,
     keyword_super = 287,
     keyword_self = 288,
     keyword_nil = 289,
     keyword_true = 290,
     keyword_false = 291,
     keyword_and = 292,
     keyword_or = 293,
     keyword_not = 294,
     modifier_if = 295,
     modifier_unless = 296,
     modifier_while = 297,
     modifier_until = 298,
     modifier_rescue = 299,
     keyword_alias = 300,
     keyword_BEGIN = 301,
     keyword_END = 302,
     keyword__LINE__ = 303,
     keyword__FILE__ = 304,
     keyword__ENCODING__ = 305,
     tIDENTIFIER = 306,
     tFID = 307,
     tGVAR = 308,
     tIVAR = 309,
     tCONSTANT = 310,
     tCVAR = 311,
     tLABEL = 312,
     tINTEGER = 313,
     tFLOAT = 314,
     tCHAR = 315,
     tREGEXP = 316,
     tSTRING = 317,
     tSTRING_PART = 318,
     tNTH_REF = 319,
     tBACK_REF = 320,
     tREGEXP_END = 321,
     tUPLUS = 322,
     tUMINUS = 323,
     tPOW = 324,
     tCMP = 325,
     tEQ = 326,
     tEQQ = 327,
     tNEQ = 328,
     tGEQ = 329,
     tLEQ = 330,
     tANDOP = 331,
     tOROP = 332,
     tMATCH = 333,
     tNMATCH = 334,
     tDOT2 = 335,
     tDOT3 = 336,
     tAREF = 337,
     tASET = 338,
     tLSHFT = 339,
     tRSHFT = 340,
     tCOLON2 = 341,
     tCOLON3 = 342,
     tOP_ASGN = 343,
     tASSOC = 344,
     tLPAREN = 345,
     tLPAREN_ARG = 346,
     tRPAREN = 347,
     tLBRACK = 348,
     tLBRACE = 349,
     tLBRACE_ARG = 350,
     tSTAR = 351,
     tAMPER = 352,
     tLAMBDA = 353,
     tSYMBEG = 354,
     tREGEXP_BEG = 355,
     tWORDS_BEG = 356,
     tQWORDS_BEG = 357,
     tSTRING_BEG = 358,
     tSTRING_DVAR = 359,
     tLAMBEG = 360,
     tLOWEST = 361,
     tUMINUS_NUM = 362,
     idNULL = 363,
     idRespond_to = 364,
     idIFUNC = 365,
     idCFUNC = 366,
     id_core_set_method_alias = 367,
     id_core_set_variable_alias = 368,
     id_core_undef_method = 369,
     id_core_define_method = 370,
     id_core_define_singleton_method = 371,
     id_core_set_postexe = 372,
     tLAST_TOKEN = 373
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 844 "./parse.y"

    node *nd;
    mrb_sym id;
    int num;
    unsigned int stack;
    const struct vtable *vars;



/* Line 214 of yacc.c  */
#line 1071 "./y.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 1083 "./y.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   10359

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  144
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  146
/* YYNRULES -- Number of rules.  */
#define YYNRULES  522
/* YYNRULES -- Number of states.  */
#define YYNSTATES  926

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   373

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     143,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   121,     2,     2,     2,   119,   114,     2,
     139,   140,   117,   115,   138,   116,   137,   118,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   109,   142,
     111,   107,   110,   108,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   136,     2,   141,   113,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   134,   112,   135,   122,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   120,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     4,     7,    10,    12,    14,    18,    21,
      23,    24,    30,    35,    38,    40,    42,    46,    49,    50,
      55,    58,    62,    66,    70,    74,    78,    83,    85,    89,
      93,   100,   106,   112,   118,   124,   128,   132,   136,   140,
     142,   146,   150,   152,   156,   160,   164,   167,   169,   171,
     173,   175,   177,   182,   183,   189,   192,   196,   201,   207,
     212,   218,   221,   224,   227,   230,   233,   235,   239,   241,
     245,   247,   250,   254,   260,   263,   268,   271,   276,   278,
     282,   284,   288,   291,   295,   297,   300,   302,   307,   311,
     315,   319,   323,   326,   328,   330,   335,   339,   343,   347,
     351,   354,   356,   358,   360,   363,   365,   369,   371,   373,
     375,   377,   379,   381,   383,   385,   387,   388,   393,   395,
     397,   399,   401,   403,   405,   407,   409,   411,   413,   415,
     417,   419,   421,   423,   425,   427,   429,   431,   433,   435,
     437,   439,   441,   443,   445,   447,   449,   451,   453,   455,
     457,   459,   461,   463,   465,   467,   469,   471,   473,   475,
     477,   479,   481,   483,   485,   487,   489,   491,   493,   495,
     497,   499,   501,   503,   505,   507,   509,   511,   513,   515,
     517,   519,   521,   523,   525,   527,   529,   533,   539,   543,
     549,   556,   562,   568,   574,   580,   585,   589,   593,   597,
     601,   605,   609,   613,   617,   621,   626,   631,   634,   637,
     641,   645,   649,   653,   657,   661,   665,   669,   673,   677,
     681,   685,   689,   692,   695,   699,   703,   707,   711,   718,
     720,   722,   724,   727,   732,   735,   739,   741,   743,   745,
     747,   750,   755,   758,   760,   763,   766,   771,   773,   774,
     777,   780,   783,   785,   787,   790,   794,   799,   803,   808,
     811,   813,   815,   817,   819,   821,   823,   824,   829,   830,
     835,   836,   840,   844,   848,   851,   855,   859,   861,   866,
     870,   872,   877,   881,   884,   886,   889,   890,   895,   902,
     909,   910,   911,   919,   920,   921,   929,   935,   940,   941,
     942,   952,   953,   960,   961,   962,   971,   972,   978,   979,
     986,   987,   988,   998,  1000,  1002,  1004,  1006,  1008,  1010,
    1012,  1015,  1017,  1019,  1021,  1027,  1029,  1032,  1034,  1036,
    1038,  1042,  1044,  1048,  1050,  1055,  1062,  1066,  1072,  1075,
    1080,  1082,  1086,  1093,  1102,  1107,  1114,  1119,  1122,  1129,
    1132,  1137,  1144,  1147,  1152,  1155,  1160,  1162,  1164,  1166,
    1170,  1172,  1177,  1179,  1184,  1186,  1190,  1192,  1194,  1199,
    1201,  1205,  1209,  1210,  1216,  1219,  1224,  1230,  1236,  1239,
    1244,  1249,  1253,  1257,  1261,  1264,  1266,  1271,  1272,  1278,
    1279,  1285,  1291,  1293,  1295,  1302,  1304,  1306,  1308,  1310,
    1313,  1315,  1318,  1320,  1322,  1324,  1326,  1328,  1331,  1335,
    1336,  1341,  1342,  1348,  1350,  1352,  1357,  1360,  1362,  1364,
    1366,  1368,  1370,  1373,  1375,  1377,  1380,  1383,  1385,  1387,
    1389,  1391,  1393,  1395,  1397,  1399,  1401,  1403,  1405,  1407,
    1409,  1411,  1413,  1415,  1416,  1421,  1424,  1428,  1431,  1438,
    1447,  1452,  1459,  1464,  1471,  1474,  1479,  1486,  1489,  1494,
    1497,  1502,  1504,  1505,  1507,  1509,  1511,  1513,  1515,  1517,
    1519,  1523,  1525,  1529,  1533,  1537,  1539,  1543,  1545,  1549,
    1551,  1553,  1556,  1558,  1560,  1562,  1565,  1568,  1570,  1572,
    1573,  1578,  1580,  1583,  1585,  1589,  1593,  1596,  1598,  1600,
    1602,  1604,  1606,  1608,  1610,  1612,  1614,  1616,  1618,  1620,
    1621,  1623,  1624,  1626,  1629,  1632,  1633,  1635,  1637,  1639,
    1641,  1643,  1646
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     145,     0,    -1,    -1,   146,   147,    -1,   148,   282,    -1,
     289,    -1,   149,    -1,   148,   288,   149,    -1,     1,   149,
      -1,   154,    -1,    -1,    46,   150,   134,   147,   135,    -1,
     152,   238,   216,   241,    -1,   153,   282,    -1,   289,    -1,
     154,    -1,   153,   288,   154,    -1,     1,   154,    -1,    -1,
      45,   175,   155,   175,    -1,     6,   177,    -1,   154,    40,
     158,    -1,   154,    41,   158,    -1,   154,    42,   158,    -1,
     154,    43,   158,    -1,   154,    44,   154,    -1,    47,   134,
     152,   135,    -1,   156,    -1,   164,   107,   159,    -1,   253,
      88,   159,    -1,   212,   136,   186,   285,    88,   159,    -1,
     212,   137,    51,    88,   159,    -1,   212,   137,    55,    88,
     159,    -1,   212,    86,    55,    88,   159,    -1,   212,    86,
      51,    88,   159,    -1,   255,    88,   159,    -1,   171,   107,
     193,    -1,   164,   107,   182,    -1,   164,   107,   193,    -1,
     157,    -1,   171,   107,   159,    -1,   171,   107,   156,    -1,
     159,    -1,   157,    37,   157,    -1,   157,    38,   157,    -1,
      39,   283,   157,    -1,   121,   159,    -1,   181,    -1,   157,
      -1,   163,    -1,   160,    -1,   231,    -1,   231,   281,   279,
     188,    -1,    -1,    95,   162,   222,   152,   135,    -1,   278,
     188,    -1,   278,   188,   161,    -1,   212,   137,   279,   188,
      -1,   212,   137,   279,   188,   161,    -1,   212,    86,   279,
     188,    -1,   212,    86,   279,   188,   161,    -1,    32,   188,
      -1,    31,   188,    -1,    30,   187,    -1,    21,   187,    -1,
      22,   187,    -1,   166,    -1,    90,   165,   284,    -1,   166,
      -1,    90,   165,   284,    -1,   168,    -1,   168,   167,    -1,
     168,    96,   170,    -1,   168,    96,   170,   138,   169,    -1,
     168,    96,    -1,   168,    96,   138,   169,    -1,    96,   170,
      -1,    96,   170,   138,   169,    -1,    96,    -1,    96,   138,
     169,    -1,   170,    -1,    90,   165,   284,    -1,   167,   138,
      -1,   168,   167,   138,    -1,   167,    -1,   168,   167,    -1,
     252,    -1,   212,   136,   186,   285,    -1,   212,   137,    51,
      -1,   212,    86,    51,    -1,   212,   137,    55,    -1,   212,
      86,    55,    -1,    87,    55,    -1,   255,    -1,   252,    -1,
     212,   136,   186,   285,    -1,   212,   137,    51,    -1,   212,
      86,    51,    -1,   212,   137,    55,    -1,   212,    86,    55,
      -1,    87,    55,    -1,   255,    -1,    51,    -1,    55,    -1,
      87,   172,    -1,   172,    -1,   212,    86,   172,    -1,    51,
      -1,    55,    -1,    52,    -1,   179,    -1,   180,    -1,   174,
      -1,   249,    -1,   175,    -1,   175,    -1,    -1,   177,   138,
     178,   176,    -1,   112,    -1,   113,    -1,   114,    -1,    70,
      -1,    71,    -1,    72,    -1,    78,    -1,    79,    -1,   110,
      -1,    74,    -1,   111,    -1,    75,    -1,    73,    -1,    84,
      -1,    85,    -1,   115,    -1,   116,    -1,   117,    -1,    96,
      -1,   118,    -1,   119,    -1,    69,    -1,   121,    -1,   122,
      -1,    67,    -1,    68,    -1,    82,    -1,    83,    -1,    48,
      -1,    49,    -1,    50,    -1,    46,    -1,    47,    -1,    45,
      -1,    37,    -1,     7,    -1,    21,    -1,    16,    -1,     3,
      -1,     5,    -1,    26,    -1,    15,    -1,    14,    -1,    10,
      -1,     9,    -1,    36,    -1,    20,    -1,    25,    -1,     4,
      -1,    22,    -1,    34,    -1,    39,    -1,    38,    -1,    23,
      -1,     8,    -1,    24,    -1,    30,    -1,    33,    -1,    32,
      -1,    13,    -1,    35,    -1,     6,    -1,    17,    -1,    31,
      -1,    11,    -1,    12,    -1,    18,    -1,    19,    -1,   171,
     107,   181,    -1,   171,   107,   181,    44,   181,    -1,   253,
      88,   181,    -1,   253,    88,   181,    44,   181,    -1,   212,
     136,   186,   285,    88,   181,    -1,   212,   137,    51,    88,
     181,    -1,   212,   137,    55,    88,   181,    -1,   212,    86,
      51,    88,   181,    -1,   212,    86,    55,    88,   181,    -1,
      87,    55,    88,   181,    -1,   255,    88,   181,    -1,   181,
      80,   181,    -1,   181,    81,   181,    -1,   181,   115,   181,
      -1,   181,   116,   181,    -1,   181,   117,   181,    -1,   181,
     118,   181,    -1,   181,   119,   181,    -1,   181,    69,   181,
      -1,   120,    58,    69,   181,    -1,   120,    59,    69,   181,
      -1,    67,   181,    -1,    68,   181,    -1,   181,   112,   181,
      -1,   181,   113,   181,    -1,   181,   114,   181,    -1,   181,
      70,   181,    -1,   181,   110,   181,    -1,   181,    74,   181,
      -1,   181,   111,   181,    -1,   181,    75,   181,    -1,   181,
      71,   181,    -1,   181,    72,   181,    -1,   181,    73,   181,
      -1,   181,    78,   181,    -1,   181,    79,   181,    -1,   121,
     181,    -1,   122,   181,    -1,   181,    84,   181,    -1,   181,
      85,   181,    -1,   181,    76,   181,    -1,   181,    77,   181,
      -1,   181,   108,   181,   283,   109,   181,    -1,   194,    -1,
     181,    -1,   289,    -1,   192,   286,    -1,   192,   138,   276,
     286,    -1,   276,   286,    -1,   139,   186,   284,    -1,   289,
      -1,   184,    -1,   289,    -1,   187,    -1,   192,   138,    -1,
     192,   138,   276,   138,    -1,   276,   138,    -1,   163,    -1,
     192,   191,    -1,   276,   191,    -1,   192,   138,   276,   191,
      -1,   190,    -1,    -1,   189,   187,    -1,    97,   182,    -1,
     138,   190,    -1,   289,    -1,   182,    -1,    96,   182,    -1,
     192,   138,   182,    -1,   192,   138,    96,   182,    -1,   192,
     138,   182,    -1,   192,   138,    96,   182,    -1,    96,   182,
      -1,   242,    -1,   243,    -1,   247,    -1,   254,    -1,   255,
      -1,    52,    -1,    -1,     7,   195,   151,    10,    -1,    -1,
      91,   157,   196,   284,    -1,    -1,    91,   197,   284,    -1,
      90,   152,   140,    -1,   212,    86,    55,    -1,    87,    55,
      -1,    93,   183,   141,    -1,    94,   275,   135,    -1,    30,
      -1,    31,   139,   187,   284,    -1,    31,   139,   284,    -1,
      31,    -1,    39,   139,   157,   284,    -1,    39,   139,   284,
      -1,   278,   233,    -1,   232,    -1,   232,   233,    -1,    -1,
      98,   198,   227,   228,    -1,    11,   158,   213,   152,   215,
      10,    -1,    12,   158,   213,   152,   216,    10,    -1,    -1,
      -1,    18,   199,   158,   214,   200,   152,    10,    -1,    -1,
      -1,    19,   201,   158,   214,   202,   152,    10,    -1,    16,
     158,   282,   236,    10,    -1,    16,   282,   236,    10,    -1,
      -1,    -1,    20,   217,    25,   203,   158,   214,   204,   152,
      10,    -1,    -1,     3,   173,   256,   205,   151,    10,    -1,
      -1,    -1,     3,    84,   157,   206,   287,   207,   151,    10,
      -1,    -1,     4,   173,   208,   151,    10,    -1,    -1,     5,
     174,   209,   258,   151,    10,    -1,    -1,    -1,     5,   273,
     281,   210,   174,   211,   258,   151,    10,    -1,    21,    -1,
      22,    -1,    23,    -1,    24,    -1,   194,    -1,   287,    -1,
      13,    -1,   287,    13,    -1,   287,    -1,    27,    -1,   216,
      -1,    14,   158,   213,   152,   215,    -1,   289,    -1,    15,
     152,    -1,   171,    -1,   164,    -1,   261,    -1,    90,   220,
     284,    -1,   218,    -1,   219,   138,   218,    -1,   219,    -1,
     219,   138,    96,   261,    -1,   219,   138,    96,   261,   138,
     219,    -1,   219,   138,    96,    -1,   219,   138,    96,   138,
     219,    -1,    96,   261,    -1,    96,   261,   138,   219,    -1,
      96,    -1,    96,   138,   219,    -1,   263,   138,   266,   138,
     269,   272,    -1,   263,   138,   266,   138,   269,   138,   263,
     272,    -1,   263,   138,   266,   272,    -1,   263,   138,   266,
     138,   263,   272,    -1,   263,   138,   269,   272,    -1,   263,
     138,    -1,   263,   138,   269,   138,   263,   272,    -1,   263,
     272,    -1,   266,   138,   269,   272,    -1,   266,   138,   269,
     138,   263,   272,    -1,   266,   272,    -1,   266,   138,   263,
     272,    -1,   269,   272,    -1,   269,   138,   263,   272,    -1,
     271,    -1,   289,    -1,   223,    -1,   112,   224,   112,    -1,
      77,    -1,   112,   221,   224,   112,    -1,   283,    -1,   283,
     142,   225,   283,    -1,   226,    -1,   225,   138,   226,    -1,
      51,    -1,   260,    -1,   139,   259,   224,   140,    -1,   259,
      -1,   105,   152,   135,    -1,    29,   152,    10,    -1,    -1,
      28,   230,   222,   152,    10,    -1,   163,   229,    -1,   231,
     281,   279,   185,    -1,   231,   281,   279,   185,   233,    -1,
     231,   281,   279,   188,   229,    -1,   278,   184,    -1,   212,
     137,   279,   185,    -1,   212,    86,   279,   184,    -1,   212,
      86,   280,    -1,   212,   137,   184,    -1,   212,    86,   184,
      -1,    32,   184,    -1,    32,    -1,   212,   136,   186,   285,
      -1,    -1,   134,   234,   222,   152,   135,    -1,    -1,    26,
     235,   222,   152,    10,    -1,    17,   192,   213,   152,   237,
      -1,   216,    -1,   236,    -1,     8,   239,   240,   213,   152,
     238,    -1,   289,    -1,   182,    -1,   193,    -1,   289,    -1,
      89,   171,    -1,   289,    -1,     9,   152,    -1,   289,    -1,
     251,    -1,   248,    -1,    60,    -1,    62,    -1,   103,    62,
      -1,   103,   244,    62,    -1,    -1,    63,   245,   152,   135,
      -1,    -1,   244,    63,   246,   152,   135,    -1,    61,    -1,
     249,    -1,    99,   103,   244,    62,    -1,    99,   250,    -1,
     174,    -1,    54,    -1,    53,    -1,    56,    -1,    62,    -1,
     103,    62,    -1,    58,    -1,    59,    -1,   120,    58,    -1,
     120,    59,    -1,    51,    -1,    54,    -1,    53,    -1,    56,
      -1,    55,    -1,   252,    -1,   252,    -1,    34,    -1,    33,
      -1,    35,    -1,    36,    -1,    49,    -1,    48,    -1,    64,
      -1,    65,    -1,   287,    -1,    -1,   111,   257,   158,   287,
      -1,     1,   287,    -1,   139,   259,   284,    -1,   259,   287,
      -1,   263,   138,   267,   138,   269,   272,    -1,   263,   138,
     267,   138,   269,   138,   263,   272,    -1,   263,   138,   267,
     272,    -1,   263,   138,   267,   138,   263,   272,    -1,   263,
     138,   269,   272,    -1,   263,   138,   269,   138,   263,   272,
      -1,   263,   272,    -1,   267,   138,   269,   272,    -1,   267,
     138,   269,   138,   263,   272,    -1,   267,   272,    -1,   267,
     138,   263,   272,    -1,   269,   272,    -1,   269,   138,   263,
     272,    -1,   271,    -1,    -1,    55,    -1,    54,    -1,    53,
      -1,    56,    -1,   260,    -1,    51,    -1,   261,    -1,    90,
     220,   284,    -1,   262,    -1,   263,   138,   262,    -1,    51,
     107,   182,    -1,    51,   107,   212,    -1,   265,    -1,   266,
     138,   265,    -1,   264,    -1,   267,   138,   264,    -1,   117,
      -1,    96,    -1,   268,    51,    -1,   268,    -1,   114,    -1,
      97,    -1,   270,    51,    -1,   138,   271,    -1,   289,    -1,
     254,    -1,    -1,   139,   274,   157,   284,    -1,   289,    -1,
     276,   286,    -1,   277,    -1,   276,   138,   277,    -1,   182,
      89,   182,    -1,    57,   182,    -1,    51,    -1,    55,    -1,
      52,    -1,    51,    -1,    55,    -1,    52,    -1,   179,    -1,
      51,    -1,    52,    -1,   179,    -1,   137,    -1,    86,    -1,
      -1,   288,    -1,    -1,   143,    -1,   283,   140,    -1,   283,
     141,    -1,    -1,   143,    -1,   138,    -1,   142,    -1,   143,
      -1,   287,    -1,   288,   142,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   999,   999,   999,  1009,  1015,  1019,  1023,  1027,  1033,
    1035,  1034,  1049,  1075,  1081,  1085,  1089,  1093,  1099,  1099,
    1103,  1107,  1111,  1115,  1119,  1123,  1127,  1134,  1135,  1139,
    1143,  1147,  1151,  1155,  1160,  1164,  1169,  1173,  1177,  1181,
    1184,  1188,  1195,  1196,  1200,  1204,  1208,  1212,  1215,  1222,
    1223,  1226,  1227,  1231,  1230,  1243,  1247,  1252,  1256,  1261,
    1265,  1270,  1274,  1278,  1282,  1286,  1292,  1296,  1302,  1303,
    1309,  1313,  1317,  1321,  1325,  1329,  1333,  1337,  1341,  1345,
    1351,  1352,  1358,  1362,  1368,  1372,  1378,  1382,  1386,  1390,
    1394,  1398,  1404,  1410,  1417,  1421,  1425,  1429,  1433,  1437,
    1443,  1449,  1456,  1460,  1463,  1467,  1471,  1477,  1478,  1479,
    1480,  1485,  1492,  1493,  1496,  1502,  1506,  1506,  1512,  1513,
    1514,  1515,  1516,  1517,  1518,  1519,  1520,  1521,  1522,  1523,
    1524,  1525,  1526,  1527,  1528,  1529,  1530,  1531,  1532,  1533,
    1534,  1535,  1536,  1537,  1538,  1539,  1542,  1542,  1542,  1543,
    1543,  1544,  1544,  1544,  1545,  1545,  1545,  1545,  1546,  1546,
    1546,  1547,  1547,  1547,  1548,  1548,  1548,  1548,  1549,  1549,
    1549,  1549,  1550,  1550,  1550,  1550,  1551,  1551,  1551,  1551,
    1552,  1552,  1552,  1552,  1553,  1553,  1556,  1560,  1564,  1568,
    1572,  1576,  1580,  1584,  1588,  1593,  1598,  1603,  1607,  1611,
    1615,  1619,  1623,  1627,  1631,  1635,  1639,  1643,  1647,  1651,
    1655,  1659,  1663,  1667,  1671,  1675,  1679,  1683,  1687,  1691,
    1695,  1704,  1708,  1712,  1716,  1720,  1724,  1728,  1732,  1736,
    1742,  1749,  1750,  1754,  1758,  1764,  1770,  1771,  1774,  1775,
    1776,  1780,  1784,  1790,  1794,  1798,  1802,  1806,  1812,  1812,
    1823,  1829,  1833,  1839,  1843,  1847,  1851,  1857,  1861,  1865,
    1871,  1872,  1873,  1874,  1875,  1876,  1881,  1880,  1891,  1891,
    1895,  1895,  1899,  1903,  1907,  1911,  1915,  1919,  1923,  1927,
    1931,  1935,  1939,  1943,  1947,  1948,  1954,  1953,  1966,  1973,
    1980,  1980,  1980,  1986,  1986,  1986,  1992,  1998,  2003,  2005,
    2002,  2012,  2011,  2024,  2029,  2023,  2042,  2041,  2054,  2053,
    2066,  2067,  2066,  2080,  2084,  2088,  2092,  2098,  2105,  2106,
    2107,  2110,  2111,  2114,  2115,  2123,  2124,  2130,  2134,  2137,
    2141,  2147,  2151,  2157,  2161,  2165,  2169,  2173,  2177,  2181,
    2185,  2189,  2195,  2199,  2203,  2207,  2211,  2215,  2219,  2223,
    2227,  2231,  2235,  2239,  2243,  2247,  2251,  2257,  2258,  2265,
    2270,  2275,  2282,  2286,  2292,  2293,  2296,  2301,  2304,  2308,
    2314,  2318,  2325,  2324,  2337,  2347,  2351,  2356,  2363,  2367,
    2371,  2375,  2379,  2383,  2387,  2391,  2395,  2402,  2401,  2412,
    2411,  2423,  2431,  2440,  2443,  2450,  2453,  2457,  2458,  2461,
    2465,  2468,  2472,  2475,  2476,  2479,  2480,  2481,  2485,  2492,
    2491,  2504,  2502,  2516,  2519,  2523,  2530,  2537,  2538,  2539,
    2540,  2541,  2545,  2551,  2552,  2553,  2557,  2563,  2567,  2571,
    2575,  2579,  2585,  2591,  2595,  2599,  2603,  2607,  2611,  2618,
    2627,  2628,  2631,  2636,  2635,  2644,  2651,  2657,  2663,  2667,
    2671,  2675,  2679,  2683,  2687,  2691,  2695,  2699,  2703,  2707,
    2711,  2715,  2720,  2726,  2731,  2736,  2741,  2748,  2752,  2759,
    2763,  2769,  2773,  2779,  2786,  2793,  2797,  2803,  2807,  2813,
    2814,  2817,  2822,  2829,  2830,  2833,  2840,  2844,  2851,  2856,
    2856,  2878,  2879,  2885,  2889,  2895,  2899,  2905,  2906,  2907,
    2910,  2911,  2912,  2913,  2916,  2917,  2918,  2921,  2922,  2925,
    2926,  2929,  2930,  2933,  2936,  2939,  2940,  2941,  2944,  2945,
    2948,  2949,  2953
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "keyword_class", "keyword_module",
  "keyword_def", "keyword_undef", "keyword_begin", "keyword_rescue",
  "keyword_ensure", "keyword_end", "keyword_if", "keyword_unless",
  "keyword_then", "keyword_elsif", "keyword_else", "keyword_case",
  "keyword_when", "keyword_while", "keyword_until", "keyword_for",
  "keyword_break", "keyword_next", "keyword_redo", "keyword_retry",
  "keyword_in", "keyword_do", "keyword_do_cond", "keyword_do_block",
  "keyword_do_LAMBDA", "keyword_return", "keyword_yield", "keyword_super",
  "keyword_self", "keyword_nil", "keyword_true", "keyword_false",
  "keyword_and", "keyword_or", "keyword_not", "modifier_if",
  "modifier_unless", "modifier_while", "modifier_until", "modifier_rescue",
  "keyword_alias", "keyword_BEGIN", "keyword_END", "keyword__LINE__",
  "keyword__FILE__", "keyword__ENCODING__", "tIDENTIFIER", "tFID", "tGVAR",
  "tIVAR", "tCONSTANT", "tCVAR", "tLABEL", "tINTEGER", "tFLOAT", "tCHAR",
  "tREGEXP", "tSTRING", "tSTRING_PART", "tNTH_REF", "tBACK_REF",
  "tREGEXP_END", "tUPLUS", "tUMINUS", "tPOW", "tCMP", "tEQ", "tEQQ",
  "tNEQ", "tGEQ", "tLEQ", "tANDOP", "tOROP", "tMATCH", "tNMATCH", "tDOT2",
  "tDOT3", "tAREF", "tASET", "tLSHFT", "tRSHFT", "tCOLON2", "tCOLON3",
  "tOP_ASGN", "tASSOC", "tLPAREN", "tLPAREN_ARG", "tRPAREN", "tLBRACK",
  "tLBRACE", "tLBRACE_ARG", "tSTAR", "tAMPER", "tLAMBDA", "tSYMBEG",
  "tREGEXP_BEG", "tWORDS_BEG", "tQWORDS_BEG", "tSTRING_BEG",
  "tSTRING_DVAR", "tLAMBEG", "tLOWEST", "'='", "'?'", "':'", "'>'", "'<'",
  "'|'", "'^'", "'&'", "'+'", "'-'", "'*'", "'/'", "'%'", "tUMINUS_NUM",
  "'!'", "'~'", "idNULL", "idRespond_to", "idIFUNC", "idCFUNC",
  "id_core_set_method_alias", "id_core_set_variable_alias",
  "id_core_undef_method", "id_core_define_method",
  "id_core_define_singleton_method", "id_core_set_postexe", "tLAST_TOKEN",
  "'{'", "'}'", "'['", "'.'", "','", "'('", "')'", "']'", "';'", "'\\n'",
  "$accept", "program", "$@1", "top_compstmt", "top_stmts", "top_stmt",
  "@2", "bodystmt", "compstmt", "stmts", "stmt", "$@3", "command_asgn",
  "expr", "expr_value", "command_call", "block_command", "cmd_brace_block",
  "$@4", "command", "mlhs", "mlhs_inner", "mlhs_basic", "mlhs_item",
  "mlhs_list", "mlhs_post", "mlhs_node", "lhs", "cname", "cpath", "fname",
  "fsym", "fitem", "undef_list", "$@5", "op", "reswords", "arg",
  "arg_value", "aref_args", "paren_args", "opt_paren_args",
  "opt_call_args", "call_args", "command_args", "@6", "block_arg",
  "opt_block_arg", "args", "mrhs", "primary", "$@7", "$@8", "$@9", "@10",
  "$@11", "$@12", "$@13", "$@14", "$@15", "$@16", "@17", "@18", "@19",
  "@20", "@21", "$@22", "@23", "primary_value", "then", "do", "if_tail",
  "opt_else", "for_var", "f_marg", "f_marg_list", "f_margs", "block_param",
  "opt_block_param", "block_param_def", "opt_bv_decl", "bv_decls", "bvar",
  "f_larglist", "lambda_body", "do_block", "$@24", "block_call",
  "method_call", "brace_block", "$@25", "$@26", "case_body", "cases",
  "opt_rescue", "exc_list", "exc_var", "opt_ensure", "literal", "string",
  "string_interp", "@27", "@28", "regexp", "symbol", "basic_symbol", "sym",
  "numeric", "variable", "var_lhs", "var_ref", "backref", "superclass",
  "$@29", "f_arglist", "f_args", "f_bad_arg", "f_norm_arg", "f_arg_item",
  "f_arg", "f_opt", "f_block_opt", "f_block_optarg", "f_optarg",
  "restarg_mark", "f_rest_arg", "blkarg_mark", "f_block_arg",
  "opt_f_block_arg", "singleton", "$@30", "assoc_list", "assocs", "assoc",
  "operation", "operation2", "operation3", "dot_or_colon", "opt_terms",
  "opt_nl", "rparen", "rbracket", "trailer", "term", "terms", "none", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,    61,    63,    58,
      62,    60,   124,    94,    38,    43,    45,    42,    47,    37,
     362,    33,   126,   363,   364,   365,   366,   367,   368,   369,
     370,   371,   372,   373,   123,   125,    91,    46,    44,    40,
      41,    93,    59,    10
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   144,   146,   145,   147,   148,   148,   148,   148,   149,
     150,   149,   151,   152,   153,   153,   153,   153,   155,   154,
     154,   154,   154,   154,   154,   154,   154,   154,   154,   154,
     154,   154,   154,   154,   154,   154,   154,   154,   154,   154,
     156,   156,   157,   157,   157,   157,   157,   157,   158,   159,
     159,   160,   160,   162,   161,   163,   163,   163,   163,   163,
     163,   163,   163,   163,   163,   163,   164,   164,   165,   165,
     166,   166,   166,   166,   166,   166,   166,   166,   166,   166,
     167,   167,   168,   168,   169,   169,   170,   170,   170,   170,
     170,   170,   170,   170,   171,   171,   171,   171,   171,   171,
     171,   171,   172,   172,   173,   173,   173,   174,   174,   174,
     174,   174,   175,   175,   176,   177,   178,   177,   179,   179,
     179,   179,   179,   179,   179,   179,   179,   179,   179,   179,
     179,   179,   179,   179,   179,   179,   179,   179,   179,   179,
     179,   179,   179,   179,   179,   179,   180,   180,   180,   180,
     180,   180,   180,   180,   180,   180,   180,   180,   180,   180,
     180,   180,   180,   180,   180,   180,   180,   180,   180,   180,
     180,   180,   180,   180,   180,   180,   180,   180,   180,   180,
     180,   180,   180,   180,   180,   180,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     182,   183,   183,   183,   183,   184,   185,   185,   186,   186,
     186,   186,   186,   187,   187,   187,   187,   187,   189,   188,
     190,   191,   191,   192,   192,   192,   192,   193,   193,   193,
     194,   194,   194,   194,   194,   194,   195,   194,   196,   194,
     197,   194,   194,   194,   194,   194,   194,   194,   194,   194,
     194,   194,   194,   194,   194,   194,   198,   194,   194,   194,
     199,   200,   194,   201,   202,   194,   194,   194,   203,   204,
     194,   205,   194,   206,   207,   194,   208,   194,   209,   194,
     210,   211,   194,   194,   194,   194,   194,   212,   213,   213,
     213,   214,   214,   215,   215,   216,   216,   217,   217,   218,
     218,   219,   219,   220,   220,   220,   220,   220,   220,   220,
     220,   220,   221,   221,   221,   221,   221,   221,   221,   221,
     221,   221,   221,   221,   221,   221,   221,   222,   222,   223,
     223,   223,   224,   224,   225,   225,   226,   226,   227,   227,
     228,   228,   230,   229,   231,   231,   231,   231,   232,   232,
     232,   232,   232,   232,   232,   232,   232,   234,   233,   235,
     233,   236,   237,   237,   238,   238,   239,   239,   239,   240,
     240,   241,   241,   242,   242,   243,   243,   243,   243,   245,
     244,   246,   244,   247,   248,   248,   249,   250,   250,   250,
     250,   250,   250,   251,   251,   251,   251,   252,   252,   252,
     252,   252,   253,   254,   254,   254,   254,   254,   254,   254,
     255,   255,   256,   257,   256,   256,   258,   258,   259,   259,
     259,   259,   259,   259,   259,   259,   259,   259,   259,   259,
     259,   259,   259,   260,   260,   260,   260,   261,   261,   262,
     262,   263,   263,   264,   265,   266,   266,   267,   267,   268,
     268,   269,   269,   270,   270,   271,   272,   272,   273,   274,
     273,   275,   275,   276,   276,   277,   277,   278,   278,   278,
     279,   279,   279,   279,   280,   280,   280,   281,   281,   282,
     282,   283,   283,   284,   285,   286,   286,   286,   287,   287,
     288,   288,   289
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     2,     1,     1,     3,     2,     1,
       0,     5,     4,     2,     1,     1,     3,     2,     0,     4,
       2,     3,     3,     3,     3,     3,     4,     1,     3,     3,
       6,     5,     5,     5,     5,     3,     3,     3,     3,     1,
       3,     3,     1,     3,     3,     3,     2,     1,     1,     1,
       1,     1,     4,     0,     5,     2,     3,     4,     5,     4,
       5,     2,     2,     2,     2,     2,     1,     3,     1,     3,
       1,     2,     3,     5,     2,     4,     2,     4,     1,     3,
       1,     3,     2,     3,     1,     2,     1,     4,     3,     3,
       3,     3,     2,     1,     1,     4,     3,     3,     3,     3,
       2,     1,     1,     1,     2,     1,     3,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     0,     4,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     3,     5,     3,     5,
       6,     5,     5,     5,     5,     4,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     4,     4,     2,     2,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     2,     2,     3,     3,     3,     3,     6,     1,
       1,     1,     2,     4,     2,     3,     1,     1,     1,     1,
       2,     4,     2,     1,     2,     2,     4,     1,     0,     2,
       2,     2,     1,     1,     2,     3,     4,     3,     4,     2,
       1,     1,     1,     1,     1,     1,     0,     4,     0,     4,
       0,     3,     3,     3,     2,     3,     3,     1,     4,     3,
       1,     4,     3,     2,     1,     2,     0,     4,     6,     6,
       0,     0,     7,     0,     0,     7,     5,     4,     0,     0,
       9,     0,     6,     0,     0,     8,     0,     5,     0,     6,
       0,     0,     9,     1,     1,     1,     1,     1,     1,     1,
       2,     1,     1,     1,     5,     1,     2,     1,     1,     1,
       3,     1,     3,     1,     4,     6,     3,     5,     2,     4,
       1,     3,     6,     8,     4,     6,     4,     2,     6,     2,
       4,     6,     2,     4,     2,     4,     1,     1,     1,     3,
       1,     4,     1,     4,     1,     3,     1,     1,     4,     1,
       3,     3,     0,     5,     2,     4,     5,     5,     2,     4,
       4,     3,     3,     3,     2,     1,     4,     0,     5,     0,
       5,     5,     1,     1,     6,     1,     1,     1,     1,     2,
       1,     2,     1,     1,     1,     1,     1,     2,     3,     0,
       4,     0,     5,     1,     1,     4,     2,     1,     1,     1,
       1,     1,     2,     1,     1,     2,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     0,     4,     2,     3,     2,     6,     8,
       4,     6,     4,     6,     2,     4,     6,     2,     4,     2,
       4,     1,     0,     1,     1,     1,     1,     1,     1,     1,
       3,     1,     3,     3,     3,     1,     3,     1,     3,     1,
       1,     2,     1,     1,     1,     2,     2,     1,     1,     0,
       4,     1,     2,     1,     3,     3,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       1,     0,     1,     2,     2,     0,     1,     1,     1,     1,
       1,     2,     0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,     0,     0,     0,     0,     0,   266,
       0,     0,   509,   290,   293,     0,   313,   314,   315,   316,
     277,   280,   385,   435,   434,   436,   437,   511,     0,    10,
       0,   439,   438,   427,   265,   429,   428,   431,   430,   423,
     424,   405,   413,   406,   440,   441,     0,     0,     0,     0,
     270,   522,   522,    78,   286,     0,     0,     0,     0,     0,
       3,   509,     6,     9,    27,    39,    42,    50,    49,     0,
      66,     0,    70,    80,     0,    47,   229,     0,    51,   284,
     260,   261,   262,   404,   414,   403,   433,     0,   263,   264,
     248,     5,     8,   313,   314,   277,   280,   385,     0,   102,
     103,     0,     0,     0,     0,   105,     0,   317,     0,   433,
     264,     0,   306,   156,   166,   157,   179,   153,   172,   162,
     161,   182,   183,   177,   160,   159,   155,   180,   184,   185,
     164,   154,   167,   171,   173,   165,   158,   174,   181,   176,
     175,   168,   178,   163,   152,   170,   169,   151,   149,   150,
     146,   147,   148,   107,   109,   108,   142,   143,   139,   121,
     122,   123,   130,   127,   129,   124,   125,   144,   145,   131,
     132,   136,   126,   128,   118,   119,   120,   133,   134,   135,
     137,   138,   140,   141,   489,   308,   110,   111,   488,     0,
     175,   168,   178,   163,   146,   147,   107,   108,     0,   112,
     115,    20,   113,     0,     0,    48,     0,     0,     0,   433,
       0,   264,     0,   518,   519,   509,     0,   520,   510,     0,
       0,     0,   328,   327,     0,     0,   433,   264,     0,     0,
       0,     0,   243,   230,   253,    64,   247,   522,   522,   493,
      65,    63,   511,    62,     0,   522,   384,    61,   511,   512,
       0,    18,     0,     0,   207,     0,   208,   274,     0,     0,
       0,   509,    15,   511,    68,    14,   268,   511,     0,   515,
     515,   231,     0,     0,   515,   491,     0,     0,    76,     0,
      86,    93,   462,   419,   418,   420,   421,     0,   417,   416,
     407,   409,     0,   425,   426,    46,   222,   223,     4,   510,
       0,     0,     0,     0,     0,     0,     0,   372,   374,     0,
      82,     0,    74,    71,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   522,     0,   508,   507,     0,   389,   387,   285,
       0,     0,   378,    55,   283,   303,   102,   103,   104,   425,
     426,     0,   443,   301,   442,     0,   522,     0,     0,     0,
     462,   310,     0,   116,     0,   522,   274,   319,     0,   318,
       0,     0,   522,     0,     0,     0,     0,     0,     0,     0,
     521,     0,     0,   274,     0,   522,     0,   298,   496,   254,
     250,     0,     0,   244,   252,     0,   245,   511,     0,   279,
     249,   511,   239,   522,   522,   238,   511,   282,    45,     0,
       0,     0,     0,     0,     0,    17,   511,   272,    13,   510,
      67,   511,   271,   275,   517,   516,   232,   517,   234,   276,
     492,    92,    84,     0,    79,     0,     0,   522,     0,   468,
     465,   464,   463,   466,     0,   480,   484,   483,   479,   462,
       0,   369,   467,   469,   471,   522,   477,   522,   482,   522,
       0,   461,   422,     0,     0,   408,   411,     0,     0,     7,
      21,    22,    23,    24,    25,    43,    44,   522,     0,    28,
      37,     0,    38,   511,     0,    72,    83,    41,    40,     0,
     186,   253,    36,   204,   212,   217,   218,   219,   214,   216,
     226,   227,   220,   221,   197,   198,   224,   225,   511,   213,
     215,   209,   210,   211,   199,   200,   201,   202,   203,   500,
     505,   501,   506,   383,   248,   381,   511,   500,   502,   501,
     503,   382,   522,   500,   501,   248,   522,   522,    29,   188,
      35,   196,    53,    56,     0,   445,     0,     0,   102,   103,
     106,     0,   511,   522,     0,   511,   462,     0,     0,     0,
       0,   267,   522,   522,   395,   522,   320,   186,   504,   273,
     511,   500,   501,   522,     0,     0,   297,   322,   291,   321,
     294,   504,   273,   511,   500,   501,     0,   495,     0,   255,
     251,   522,   494,   278,   513,   235,   240,   242,   281,    19,
       0,    26,   195,    69,    16,   269,   515,    85,    77,    89,
      91,   511,   500,   501,     0,   468,     0,   340,   331,   333,
     511,   329,   511,     0,     0,   287,     0,   454,   487,     0,
     457,   481,     0,   459,   485,   415,     0,     0,   205,   206,
     360,   511,     0,   358,   357,   259,     0,    81,    75,     0,
       0,     0,     0,     0,     0,   380,    59,     0,   386,     0,
       0,   237,   379,    57,   236,   375,    52,     0,     0,     0,
     522,   304,     0,     0,   386,   307,   490,   511,     0,   447,
     311,   114,   117,   396,   397,   522,   398,     0,   522,   325,
       0,     0,   323,     0,     0,   386,     0,     0,     0,   296,
       0,     0,     0,     0,   386,     0,   256,   246,   522,    11,
     233,    87,   473,   511,     0,   338,     0,   470,     0,   362,
       0,     0,   472,   522,   522,   486,   522,   478,   522,   522,
     410,     0,   468,   511,     0,   522,   475,   522,   522,   356,
       0,     0,   257,    73,   187,     0,    34,   193,    33,   194,
      60,   514,     0,    31,   191,    32,   192,    58,   376,   377,
       0,     0,   189,     0,     0,   444,   302,   446,   309,   462,
       0,     0,   400,   326,     0,    12,   402,     0,   288,     0,
     289,   255,   522,     0,     0,   299,   241,   330,   341,     0,
     336,   332,   368,     0,   371,   370,     0,   450,     0,   452,
       0,   458,     0,   455,   460,   412,     0,     0,   359,   347,
     349,     0,   352,     0,   354,   373,   258,   228,    30,   190,
     390,   388,     0,     0,     0,     0,   399,     0,    94,   101,
       0,   401,     0,   392,   393,   391,   292,   295,     0,     0,
     339,     0,   334,   366,   511,   364,   367,   522,   522,   522,
     522,     0,   474,   361,   522,   522,   522,   476,   522,   522,
      54,   305,     0,   100,     0,   522,     0,   522,   522,     0,
     337,     0,     0,   363,   451,     0,   448,   453,   456,   274,
       0,     0,   344,     0,   346,   353,     0,   350,   355,   312,
     504,    99,   511,   500,   501,   394,   324,   300,   335,   365,
     522,   504,   273,   522,   522,   522,   522,   386,   449,   345,
       0,   342,   348,   351,   522,   343
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    60,    61,    62,   252,   374,   375,   261,
     262,   419,    64,    65,   206,    66,    67,   553,   680,    68,
      69,   263,    70,    71,    72,   444,    73,   207,   105,   106,
     199,   200,   692,   201,   570,   186,   187,    75,   234,   268,
     533,   672,   411,   412,   243,   244,   236,   403,   413,   492,
      76,   203,   431,   267,   282,   219,   712,   220,   713,   596,
     848,   557,   554,   774,   368,   370,   569,   779,   255,   378,
     588,   701,   702,   225,   628,   629,   630,   743,   652,   653,
     728,   854,   855,   460,   635,   308,   487,    78,    79,   354,
     547,   546,   389,   845,   573,   695,   781,   785,    80,    81,
     292,   474,   647,    82,    83,    84,   289,    85,   209,   210,
      88,   211,   363,   556,   567,   568,   462,   463,   464,   465,
     466,   746,   747,   467,   468,   469,   470,   735,   637,   189,
     369,   273,   414,   239,    90,   561,   535,   346,   216,   408,
     409,   668,   436,   379,   218,   265
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -704
static const yytype_int16 yypact[] =
{
    -704,   125,  2594,  -704,  6940,  8652,  8961,  5048,  6700,  -704,
    8331,  8331,  4529,  -704,  -704,  8755,  7154,  7154,  -704,  -704,
    7154,  5766,  5878,  -704,  -704,  -704,  -704,    35,  6700,  -704,
       1,  -704,  -704,  5170,  5294,  -704,  -704,  5418,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  8438,  8438,    58,  3846,
    8331,  7368,  7689,  6214,  -704,  6460,   260,   298,  8545,  8438,
    -704,   281,  -704,   722,  -704,   398,  -704,  -704,   140,    52,
    -704,    70,  8858,  -704,   117,  2393,   315,   324,    43,    83,
    -704,  -704,  -704,  -704,  -704,  -704,   387,   103,  -704,   517,
     139,  -704,  -704,  -704,  -704,  -704,    81,   102,   127,   212,
     319,  8331,    92,  3989,   382,  -704,    76,  -704,   344,  -704,
    -704,   139,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
      48,    53,    56,    68,  -704,  -704,  -704,  -704,  -704,  -704,
     213,   225,  -704,   228,  -704,   269,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,    43,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  6580,  -704,
    -704,   159,  -704,  3055,   276,   398,    91,   217,   353,    62,
     270,   206,    91,  -704,  -704,   281,   343,  -704,   239,  8331,
    8331,   333,  -704,  -704,   384,   369,    71,   111,  8438,  8438,
    8438,  8438,  -704,  2393,   338,  -704,  -704,   291,   295,  -704,
    -704,  -704,  4403,  -704,  7154,  7154,  -704,  -704,  4657,  -704,
    8331,  -704,   309,  4132,  -704,   386,   379,   560,  7047,  3846,
     310,   281,   722,   311,   355,  -704,   398,   311,   323,    85,
     182,  -704,   338,   336,   182,  -704,   413,  9064,   335,   392,
     417,   438,   720,  -704,  -704,  -704,  -704,   433,  -704,  -704,
    -704,  -704,   443,   415,   419,  -704,  -704,  -704,  -704,  4783,
    8331,  8331,  8331,  8331,  7047,  8331,  8331,  -704,  -704,  7796,
    -704,  3846,  6324,   348,  7796,  8438,  8438,  8438,  8438,  8438,
    8438,  8438,  8438,  8438,  8438,  8438,  8438,  8438,  8438,  8438,
    8438,  8438,  8438,  8438,  8438,  8438,  8438,  8438,  8438,  8438,
    8438,  2211,  7154,  9324,  -704,  -704, 10237,  -704,  -704,  -704,
    8545,  8545,  -704,   396,  -704,   398,  -704,   449,  -704,  -704,
    -704,   281,  -704,  -704,  -704,  9397,  7154,  9470,  3055,  8331,
    1055,  -704,   435,  -704,   489,   500,   271,  -704,  3189,   499,
    8438,  9543,  7154,  9616,  8438,  8438,  3447,   343,  7903,   504,
    -704,    84,    84,   113,  9689,  7154,  9762,  -704,  -704,  -704,
    -704,  8438,  7261,  -704,  -704,  7475,  -704,   311,   402,  -704,
    -704,   311,  -704,   381,   389,  -704,   134,  -704,  -704,  6700,
    3575,   421,  9543,  9616,  8438,   722,   311,  -704,  -704,  4908,
     410,   311,  -704,  -704,  7582,  -704,  -704,  7689,  -704,  -704,
    -704,   449,    70,  9064,  -704,  9064,  9835,  7154,  9908,   445,
    -704,  -704,  -704,  -704,   999,  -704,  -704,  -704,  -704,   907,
     101,  -704,  -704,  -704,  -704,   425,  -704,   428,   518,   455,
     525,  -704,  -704,   526,  4132,  -704,  -704,  8438,  8438,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,    55,  8438,  -704,
     457,   474,  -704,   311,  9064,   480,  -704,  -704,  -704,   510,
    1872,  -704,  -704,   379,  2110,  2110,  2110,  2110,   709,   709,
    2455,  1117,  2110,  2110,  1982,  1982,   513,   513, 10183,   709,
     709,   819,   819,   723,   567,   567,   379,   379,   379,  2771,
    5990,  2912,  6102,  -704,   102,  -704,   311,   572,  -704,   601,
    -704,  -704,  5878,  -704,  -704,  1031,    55,    55,  -704,  2290,
    -704,  2393,  -704,  -704,   281,  -704,  8331,  3055,   340,    40,
    -704,   102,   311,   102,   611,   134,   907,  3055,   281,  6820,
    6700,  -704,  8010,   607,  -704,   565,  -704,  2891,  5542,  5654,
     311,   279,   280,   607,   613,   133,  -704,  -704,  -704,  -704,
    -704,    63,    75,   311,   119,   131,  8331,  -704,  8438,   338,
    -704,   295,  -704,  -704,  -704,  -704,  7261,  7475,  -704,  -704,
     492,  -704,  2393,    24,   722,  -704,   182,   348,  -704,   340,
      40,   311,    46,   282,  8438,  -704,   999,   141,  -704,   495,
     311,  -704,   311,  4275,  4132,  -704,   907,  -704,  -704,   907,
    -704,  -704,   586,  -704,  -704,  -704,   503,  4132,   379,   379,
    -704,   671,  4275,  -704,  -704,   506,  8117,  -704,  -704,  9064,
    8545,  8438,   540,  8545,  8545,  -704,   396,   509,   621,  8545,
    8545,  -704,  -704,   396,  -704,    83,   140,  4275,  4132,  8438,
      55,  -704,   281,   649,  -704,  -704,  -704,   311,   652,  -704,
    -704,  -704,  -704,   457,  -704,   575,  -704,  3718,   657,  -704,
    8331,   662,  -704,  8438,  8438,   308,  8438,  8438,   663,  -704,
    8224,  3318,  4275,  4275,   174,    84,  -704,  -704,   537,  -704,
    -704,   373,  -704,   311,   792,   539,  1011,  -704,   541,   538,
     672,   552,  -704,   550,   554,  -704,   557,  -704,   566,   557,
    -704,   576,   608,   311,   602,   579,  -704,   581,   582,  -704,
     711,  8438,   591,  -704,  2393,  8438,  -704,  2393,  -704,  2393,
    -704,  -704,  8545,  -704,  2393,  -704,  2393,  -704,  -704,  -704,
     725,   598,  2393,  4132,  3055,  -704,  -704,  -704,  -704,  1055,
    9167,    91,  -704,  -704,  4275,  -704,  -704,    91,  -704,  8438,
    -704,  -704,   143,   728,   730,  -704,  7475,  -704,   609,   792,
     462,  -704,  -704,   726,  -704,  -704,   907,  -704,   586,  -704,
     586,  -704,   586,  -704,  -704,  -704,  9270,   637,  -704,  1065,
    -704,  1065,  -704,   586,  -704,  -704,   612,  2393,  -704,  2393,
    -704,  -704,   617,   743,  3055,   699,  -704,   465,   417,   438,
    3055,  -704,  3189,  -704,  -704,  -704,  -704,  -704,  4275,   792,
     609,   792,   618,  -704,   209,  -704,  -704,   557,   619,   557,
     557,   703,   479,  -704,   632,   634,   557,  -704,   645,   557,
    -704,  -704,   776,   449,  9981,  7154, 10054,   500,   565,   777,
     609,   792,   726,  -704,  -704,   586,  -704,  -704,  -704,  -704,
   10127,  1065,  -704,   586,  -704,  -704,   586,  -704,  -704,  -704,
     144,    40,   311,   142,   148,  -704,  -704,  -704,   609,  -704,
     557,   650,   656,   557,   653,   557,   557,   150,  -704,  -704,
     586,  -704,  -704,  -704,   557,  -704
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -704,  -704,  -704,   376,  -704,    34,  -704,  -342,   -43,  -704,
      37,  -704,  -269,    69,     2,   -50,  -704,  -521,  -704,    -5,
     783,  -138,    23,   -48,  -242,  -379,     3,  1457,   -69,   794,
      14,   -21,  -704,  -704,  -704,  -282,  -704,  1203,    99,  -704,
      -4,   256,  -272,   100,     5,  -704,  -359,  -216,    12,  -266,
       4,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,   641,  -209,
    -361,   -76,  -518,  -704,  -668,  -651,   177,  -704,  -419,  -704,
    -560,  -704,   -71,  -704,  -704,   128,  -704,  -704,  -704,   -78,
    -704,  -704,  -364,  -704,   -65,  -704,  -704,  -704,  -704,  -704,
     519,  -704,  -704,  -704,  -704,    -3,  -704,  -704,  1010,  1702,
     806,  1371,  -704,  -704,    36,  -228,  -700,  -317,  -547,   109,
    -602,  -703,    -1,   183,  -704,  -561,  -704,  -248,   234,  -704,
    -704,  -704,     0,  -375,  2114,  -299,  -704,   642,    47,   -25,
    -161,  -488,  -234,    -8,     6,    -2
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -523
static const yytype_int16 yytable[] =
{
      91,   349,   250,   386,   217,   202,   260,   251,   295,   107,
     107,   232,   232,   212,   215,   232,   238,   238,   246,   107,
     238,   185,   406,   584,   313,   202,   564,   247,   237,   237,
     602,   590,   237,   358,   471,   443,   438,   737,    92,    63,
     440,    63,   534,   600,   542,   497,   600,   545,   502,   271,
     275,   270,   274,   217,   461,   698,   278,   107,   801,   532,
     260,   540,   602,   269,   540,   708,   618,   299,   563,   288,
     536,   -88,   264,   798,   684,   734,   107,   361,   738,   205,
     205,   205,   534,   532,   542,   540,   352,   417,   -97,   732,
     748,   744,   705,   246,   562,   353,   -94,   563,   364,   532,
     -99,   540,   430,   856,   377,   714,   432,   352,   298,   347,
     580,   587,   532,   257,   540,   658,   235,   240,   867,   266,
     241,   426,   471,   593,   563,     3,  -273,   677,   678,   344,
     633,   -67,   650,   721,  -435,   253,  -101,   631,  -100,  -434,
     532,   540,  -436,   356,   -96,   760,   377,   357,   850,   563,
    -432,   272,   767,   -88,  -437,   -96,   -98,   -97,   697,   309,
     388,   -98,   -81,   -95,   532,   347,   540,   651,   307,   -94,
     355,   305,   306,   493,   248,   621,  -273,  -273,   249,  -501,
     345,   801,   856,   817,   -88,  -435,   -88,   362,   867,   -88,
    -434,   350,   625,  -436,   450,   451,   452,   453,   406,   -95,
     880,   -89,  -500,   443,   737,  -437,   634,   217,   310,   -86,
     421,   471,   288,   -91,  -501,   683,   260,   348,   213,   214,
     242,   391,   392,   434,   314,   688,   213,   214,   435,   442,
     908,   632,   602,   213,   214,   404,   404,   232,  -497,   232,
     232,   245,   238,   415,   238,   858,   603,   600,   600,   -93,
     605,   -92,   443,   217,   237,   608,   237,   -88,   865,   489,
     868,   773,   387,   732,   498,   613,   248,   429,   260,   -90,
     615,   710,   732,   348,   843,   213,   214,   249,   245,   724,
     753,   107,   264,  -500,   -96,   -96,   -97,   -97,   205,   205,
     -98,   -98,   -95,   -95,   385,   425,   560,   373,  -427,  -439,
     548,   550,   480,   481,   482,   483,   694,   -90,   428,   631,
     725,  -438,   -87,  -101,  -427,   495,   107,   416,   471,   418,
     437,   491,   290,   291,   380,   435,   491,   398,   399,   400,
     914,   376,   657,   479,   264,   575,    63,   232,   687,   541,
     415,   484,   407,   583,   410,  -498,  -497,   882,  -427,  -427,
    -439,  -497,   249,   555,   795,  -431,   293,   294,   384,   424,
     388,   232,  -438,   541,   415,  -427,  -504,   706,   707,   205,
     205,   205,   205,   574,   485,   486,   711,   232,  -100,   541,
     415,   390,   720,   589,   589,   717,   -96,   -98,   393,   -90,
     232,   497,   541,   415,   397,   617,   789,   442,   609,  -386,
     585,  -317,   601,   749,   686,  -431,  -431,   631,   490,   631,
     341,   404,   404,   501,   917,   -95,   202,   443,    91,   541,
     -90,   602,   -90,   213,   214,   -90,  -504,   401,   844,   402,
     365,   646,   833,   405,   616,   305,   306,   600,   565,   381,
     359,   360,   232,   420,   541,   415,   442,   107,   315,   107,
     427,  -317,  -317,  -498,   249,  -431,  -431,    63,  -498,  -386,
     342,   343,   -66,   638,   433,   638,   614,   638,   441,   727,
     394,   439,   422,   445,  -504,  -432,  -504,  -504,   446,  -500,
     366,   367,   631,   852,   477,   654,   496,   501,   478,   382,
     383,   552,   872,   662,   -94,   472,   291,   472,   107,   571,
     597,   599,   717,  -433,   272,   475,   476,  -386,   572,  -386,
    -386,   667,   576,   625,   586,   450,   451,   452,   453,   606,
     395,   396,   382,   423,  -264,   -86,   777,   607,   447,   448,
     665,   471,   631,   599,   631,  -274,   272,   667,   671,   666,
     674,   671,   604,   674,   654,   654,   681,   673,   -81,   691,
     676,   874,   624,  -433,  -433,   667,   611,   665,   682,   671,
     689,   674,   797,   636,   631,   890,   639,   202,   667,   641,
     696,   699,   840,   699,  -264,  -264,   644,   563,   842,   700,
     697,   699,   315,   690,   491,  -274,  -274,   655,   645,   476,
     730,   731,   532,   642,   540,  -253,   667,   768,   715,   404,
     851,   875,   876,   902,   741,   351,   718,   729,   532,   750,
     498,   442,   656,   756,   758,   366,   367,   660,   659,   763,
     765,   685,   697,   709,  -101,   205,   729,   719,   336,   337,
     338,   339,   340,   726,   770,   771,   315,   625,   740,   450,
     451,   452,   453,    77,  -254,    77,   108,   108,   424,   755,
     761,   208,   208,   208,   783,   -93,   224,   208,   208,   776,
     669,   208,   778,   107,   780,   205,   784,  -100,   792,   793,
     794,   693,   788,   790,   775,   796,   454,   799,   654,   -96,
     803,   802,   804,   456,   338,   339,   340,   805,   806,   670,
      77,   208,   808,   782,   279,   810,   786,   716,   -92,   208,
     457,   640,   787,   643,   812,   599,   272,   589,   -98,   762,
     -88,   815,   828,   279,   818,   816,   404,   819,   729,   821,
     823,   825,   742,   722,   450,   451,   452,   453,   -95,  -255,
     832,   638,   638,   831,   638,   830,   638,   638,   846,   -90,
     847,   841,   208,   638,    77,   638,   638,   849,   736,   863,
    -256,   739,   870,   871,   873,   752,   881,   885,   889,   -87,
     745,   454,   300,   301,   302,   303,   304,   455,   456,   205,
     891,   449,   893,   450,   451,   452,   453,   853,   315,   450,
     451,   452,   453,   896,   107,   457,   899,   907,   458,  -500,
     699,   920,   315,   328,   329,  -501,   610,   877,   222,   878,
     112,   675,   906,   723,   769,   879,   473,   328,   329,   791,
     454,   909,   905,   188,   249,   834,   455,   456,   864,   733,
     107,   333,   334,   335,   336,   337,   338,   339,   340,   883,
       0,   371,     0,     0,   457,     0,     0,   458,   336,   337,
     338,   339,   340,   625,    77,   450,   451,   452,   453,     0,
     826,     0,     0,     0,     0,   638,   638,   638,   638,   459,
     208,   208,   638,   638,   638,     0,   638,   638,     0,     0,
     232,     0,   541,   415,     0,   574,   699,   667,     0,     0,
       0,     0,   626,   208,     0,   208,   208,     0,   315,   208,
       0,   208,     0,     0,    77,   272,     0,     0,     0,    77,
      77,     0,     0,   328,   329,     0,     0,     0,   638,     0,
       0,   638,   638,   638,   638,   857,     0,   859,   279,     0,
       0,   860,   638,     0,     0,     0,     0,     0,     0,     0,
     866,     0,   869,   335,   336,   337,   338,   339,   340,     0,
      77,   208,   208,   208,   208,    77,   208,   208,     0,     0,
     208,     0,    77,   279,     0,   208,     0,     0,   449,     0,
     450,   451,   452,   453,     0,     0,     0,   807,   809,     0,
     811,     0,   813,   814,     0,     0,     0,     0,     0,   820,
       0,   822,   824,   208,     0,     0,     0,     0,     0,     0,
       0,   208,   208,     0,   910,     0,     0,   454,     0,     0,
     913,     0,   915,   455,   456,   916,     0,   208,     0,    77,
     208,     0,    86,     0,    86,   109,   109,   109,     0,    77,
       0,   457,     0,   208,   458,   226,     0,    77,     0,   924,
       0,  -522,     0,     0,     0,     0,   208,     0,     0,  -522,
    -522,  -522,     0,     0,  -522,  -522,  -522,     0,  -522,     0,
     625,     0,   450,   451,   452,   453,     0,  -522,  -522,    86,
       0,    77,   625,   280,   450,   451,   452,   453,  -522,  -522,
      77,  -522,  -522,  -522,  -522,  -522,     0,     0,     0,     0,
       0,     0,   280,     0,   279,     0,   279,     0,   208,   626,
       0,   884,   886,   887,   888,   627,     0,     0,   892,   894,
     895,   626,   897,   898,     0,     0,   449,   800,   450,   451,
     452,   453,     0,    86,     0,    77,   742,  -522,   450,   451,
     452,   453,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   279,     0,     0,     0,     0,
       0,     0,     0,     0,   918,   454,     0,   919,   921,   922,
     923,   455,   456,     0,     0,   454,     0,     0,   925,     0,
       0,   455,   456,     0,     0,  -522,  -522,     0,  -522,   457,
     245,  -522,   458,  -522,  -522,     0,     0,     0,     0,   457,
       0,     0,   458,     0,     0,     0,   315,   316,   317,   318,
     319,   320,   321,   322,   566,   324,   325,   208,    77,     0,
       0,   328,   329,     0,     0,     0,     0,     0,    77,     0,
       0,     0,     0,    86,     0,     0,     0,     0,     0,   233,
     233,     0,     0,   233,     0,     0,     0,   331,   332,   333,
     334,   335,   336,   337,   338,   339,   340,   208,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   254,
     256,     0,     0,     0,   233,   233,     0,     0,     0,     0,
       0,   296,   297,    86,     0,     0,     0,     0,    86,    86,
       0,     0,     0,     0,    77,    77,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   280,    77,     0,
       0,     0,     0,    77,     0,     0,     0,     0,     0,     0,
     279,   208,     0,     0,   208,   208,     0,     0,     0,    86,
     208,   208,     0,     0,    86,     0,     0,     0,    77,    77,
       0,    86,   280,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    77,     0,
       0,   208,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    77,    77,    77,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    89,     0,    89,   110,   110,    86,     0,
       0,     0,     0,     0,     0,     0,   227,     0,    86,     0,
       0,     0,     0,     0,     0,     0,    86,     0,     0,     0,
       0,     0,     0,   208,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    77,    77,     0,     0,     0,     0,
      89,   837,     0,     0,   281,    77,     0,     0,     0,     0,
      86,   233,   233,   233,   296,     0,     0,     0,     0,    86,
       0,     0,     0,   281,     0,   233,     0,   233,   233,     0,
       0,     0,     0,   280,     0,   280,     0,   862,     0,    74,
       0,    74,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   223,     0,    89,    77,     0,     0,     0,     0,
       0,    77,     0,    77,    86,     0,     0,     0,     0,    77,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   280,     0,    74,     0,     0,     0,
       0,     0,   233,     0,     0,     0,   208,   500,   503,   504,
     505,   506,   507,   508,   509,   510,   511,   512,   513,   514,
     515,   516,   517,   518,   519,   520,   521,   522,   523,   524,
     525,   526,   527,   528,     0,   233,     0,     0,     0,     0,
       0,     0,     0,   549,   551,     0,     0,     0,     0,     0,
      74,     0,     0,     0,     0,     0,     0,    86,     0,   233,
       0,     0,     0,     0,    89,     0,     0,    86,     0,     0,
       0,     0,     0,   577,     0,   233,     0,   549,   551,     0,
       0,   233,     0,     0,     0,     0,     0,     0,   233,     0,
       0,     0,     0,     0,   233,   233,     0,     0,   233,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    89,     0,     0,   612,     0,    89,
      89,     0,     0,     0,     0,     0,     0,   233,     0,     0,
     233,     0,     0,    86,    86,     0,     0,     0,   281,     0,
     233,     0,     0,     0,     0,     0,     0,    86,     0,     0,
      74,     0,    86,     0,     0,     0,     0,     0,     0,   280,
      89,     0,     0,     0,     0,    89,     0,     0,     0,     0,
     648,   649,    89,   281,     0,     0,     0,    86,    86,     0,
       0,   233,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    87,     0,    87,    86,     0,     0,
      74,     0,     0,     0,     0,    74,    74,     0,     0,     0,
       0,    86,    86,    86,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
       0,    87,     0,     0,     0,     0,    74,    89,     0,     0,
       0,    74,     0,     0,     0,     0,     0,     0,    74,     0,
       0,   499,     0,     0,     0,   233,     0,     0,     0,     0,
       0,     0,     0,    86,    86,     0,     0,     0,     0,     0,
     838,    89,     0,     0,    86,     0,     0,     0,     0,     0,
      89,   233,     0,     0,     0,    87,     0,     0,     0,   233,
     233,     0,     0,     0,   281,     0,   281,     0,     0,     0,
       0,     0,     0,     0,     0,    74,   109,   233,     0,     0,
       0,     0,     0,     0,     0,    74,     0,     0,     0,     0,
       0,     0,     0,    74,    86,    89,     0,     0,     0,     0,
      86,     0,    86,     0,     0,     0,     0,     0,    86,   233,
       0,     0,     0,   577,   754,   281,   757,   759,     0,     0,
       0,     0,   764,   766,     0,     0,     0,    74,     0,     0,
       0,     0,   772,     0,     0,     0,    74,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    87,   757,   759,     0,   764,
     766,     0,     0,   233,     0,     0,   661,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    89,     0,
       0,    74,     0,     0,     0,     0,     0,     0,    89,     0,
       0,   315,   316,   317,   318,   319,   320,   321,   322,   323,
     324,   325,   326,   327,   233,    87,   328,   329,   827,     0,
      87,    87,     0,     0,     0,   829,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     330,     0,   331,   332,   333,   334,   335,   336,   337,   338,
     339,   340,   829,     0,     0,     0,     0,     0,     0,   233,
       0,    87,     0,     0,    89,    89,    87,     0,     0,     0,
    -230,     0,     0,    87,    74,     0,     0,     0,    89,     0,
       0,     0,     0,    89,    74,     0,     0,     0,     0,     0,
     281,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    89,    89,
       0,   315,   316,   317,   318,   319,   320,   321,   322,   323,
     324,   325,  -523,  -523,     0,     0,   328,   329,    89,     0,
      87,     0,     0,     0,     0,     0,     0,     0,   233,     0,
      87,     0,    89,    89,    89,     0,     0,     0,    87,     0,
      74,    74,   331,   332,   333,   334,   335,   336,   337,   338,
     339,   340,     0,     0,    74,     0,     0,     0,     0,    74,
       0,     0,     0,     0,     0,     0,     0,   499,     0,   111,
     111,     0,    87,     0,     0,     0,     0,     0,     0,   111,
       0,    87,     0,     0,    74,    74,     0,     0,     0,     0,
       0,     0,     0,     0,    89,    89,     0,     0,     0,     0,
       0,   839,     0,     0,    74,    89,     0,     0,     0,     0,
     111,   111,     0,     0,     0,   111,   111,   111,    74,    74,
      74,     0,     0,   111,     0,     0,    87,     0,     0,   315,
    -523,  -523,  -523,  -523,   320,   321,   111,   110,  -523,  -523,
       0,     0,     0,     0,   328,   329,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    89,     0,     0,     0,     0,
       0,    89,     0,    89,     0,     0,     0,     0,     0,    89,
     331,   332,   333,   334,   335,   336,   337,   338,   339,   340,
      74,    74,     0,     0,     0,     0,     0,   836,     0,     0,
       0,    74,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    87,
       0,     0,   529,   530,     0,     0,   531,     0,     0,    87,
       0,     0,     0,     0,     0,     0,     0,     0,   156,   157,
     158,   159,   160,   161,   162,   163,   164,     0,     0,   165,
     166,    74,     0,   167,   168,   169,   170,    74,     0,    74,
       0,     0,     0,     0,     0,    74,     0,   171,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   172,   173,   174,   175,   176,   177,   178,   179,   180,
     181,     0,   182,   183,   679,    87,    87,     0,     0,     0,
       0,     0,   111,   111,   111,   111,     0,     0,     0,    87,
     245,     0,     0,     0,    87,     0,     0,     0,     0,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,     0,     0,   328,   329,     0,     0,     0,    87,
      87,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   111,     0,     0,     0,     0,     0,     0,   330,    87,
     331,   332,   333,   334,   335,   336,   337,   338,   339,   340,
       0,     0,     0,    87,    87,    87,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   111,     0,     0,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,     0,     0,     0,     0,     0,
       0,     0,   315,   316,   317,   318,   319,   320,   321,   322,
     323,   324,   325,   326,   327,    87,    87,   328,   329,     0,
       0,     0,     0,     0,     0,     0,    87,     0,     0,     0,
       0,     0,     0,     0,   111,     0,     0,     0,   111,   111,
       0,   330,   111,   331,   332,   333,   334,   335,   336,   337,
     338,   339,   340,     0,     0,   111,   111,     0,     0,   111,
       0,     0,     0,     0,   315,   316,   317,   318,   319,   320,
     321,     0,     0,   324,   325,     0,    87,     0,   111,   328,
     329,     0,    87,     0,    87,     0,     0,     0,   111,     0,
      87,   111,     0,     0,     0,     0,     0,   111,     0,   111,
       0,     0,     0,     0,     0,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   340,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   111,   111,     0,  -522,     4,     0,     5,     6,     7,
       8,     9,   111,     0,     0,    10,    11,     0,   111,     0,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,    28,
      29,    30,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    48,     0,     0,    49,    50,   111,    51,    52,     0,
      53,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   111,     0,    57,    58,    59,     0,     0,     0,
     111,   111,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -522,  -522,   111,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     111,  -504,     0,   111,     0,   111,     0,     0,     0,  -504,
    -504,  -504,     0,     0,     0,  -504,  -504,     0,  -504,     0,
       0,     0,     0,   111,     0,     0,     0,  -504,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -504,  -504,
       0,  -504,  -504,  -504,  -504,  -504,     0,   111,   111,     0,
     111,   111,     0,     0,   111,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -504,  -504,  -504,  -504,  -504,  -504,  -504,  -504,  -504,  -504,
    -504,  -504,  -504,     0,     0,  -504,  -504,  -504,     0,   663,
       0,     0,     0,     0,     0,   111,     0,     0,     0,   111,
       0,     0,     0,     0,     0,     0,     0,     0,   -97,  -504,
       0,  -504,  -504,  -504,  -504,  -504,  -504,  -504,  -504,  -504,
    -504,     0,     0,     0,   111,     0,     0,     0,     0,     0,
       0,     0,     0,   111,     0,  -504,  -504,  -504,  -504,   -89,
     111,  -504,  -273,  -504,  -504,     0,     0,     0,     0,     0,
    -273,  -273,  -273,     0,     0,     0,  -273,  -273,     0,  -273,
     111,     0,     0,     0,     0,   661,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -273,
    -273,     0,  -273,  -273,  -273,  -273,  -273,     0,     0,     0,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,     0,     0,   328,   329,     0,     0,     0,
       0,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,     0,     0,  -273,  -273,  -273,   330,
     664,   331,   332,   333,   334,   335,   336,   337,   338,   339,
     340,     0,     0,     0,     0,     0,     0,     0,     0,   -99,
    -273,     0,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -273,  -273,  -273,
     -91,     0,  -273,     0,  -273,  -273,   258,     0,     5,     6,
       7,     8,     9,  -522,  -522,  -522,    10,    11,     0,     0,
    -522,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
      28,     0,    30,    31,    32,     0,    33,    34,    35,    36,
      37,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    48,     0,     0,    49,    50,     0,    51,    52,
       0,    53,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    57,    58,    59,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     258,     0,     5,     6,     7,     8,     9,  -522,  -522,  -522,
      10,    11,     0,  -522,  -522,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,     0,    30,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,     0,     0,    49,
      50,     0,    51,    52,     0,    53,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    57,
      58,    59,     0,     0,     0,     0,     0,     0,     0,   258,
       0,     5,     6,     7,     8,     9,     0,     0,  -522,    10,
      11,  -522,  -522,  -522,    12,  -522,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,     0,    30,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    48,     0,     0,    49,    50,
       0,    51,    52,     0,    53,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    57,    58,
      59,     0,     0,     0,     0,     0,     0,     0,   258,     0,
       5,     6,     7,     8,     9,     0,     0,  -522,    10,    11,
    -522,  -522,  -522,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,     0,    28,     0,    30,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    48,     0,     0,    49,    50,     0,
      51,    52,     0,    53,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    57,    58,    59,
       0,     0,     0,     0,     0,     0,     4,     0,     5,     6,
       7,     8,     9,     0,     0,     0,    10,    11,     0,  -522,
    -522,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
      28,    29,    30,    31,    32,     0,    33,    34,    35,    36,
      37,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    48,     0,     0,    49,    50,     0,    51,    52,
       0,    53,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    57,    58,    59,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -522,     0,     0,     0,     0,     0,     0,  -522,  -522,   258,
       0,     5,     6,     7,     8,     9,     0,  -522,  -522,    10,
      11,     0,     0,     0,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,     0,    30,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    48,     0,     0,    49,    50,
       0,    51,    52,     0,    53,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    57,    58,
      59,     0,     0,     0,     0,     0,     0,   258,     0,     5,
       6,     7,     8,     9,     0,     0,     0,    10,    11,     0,
    -522,  -522,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,     0,    30,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    48,     0,     0,   259,    50,     0,    51,
      52,     0,    53,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    57,    58,    59,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -522,     0,  -522,  -522,
     258,     0,     5,     6,     7,     8,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,     0,    30,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,     0,     0,    49,
      50,     0,    51,    52,     0,    53,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    57,
      58,    59,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -522,
       0,  -522,  -522,   258,     0,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,     0,    30,
      31,    32,     0,    33,    34,    35,    36,    37,    38,     0,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    48,
       0,     0,    49,    50,     0,    51,    52,     0,    53,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    57,    58,    59,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -522,     0,     0,
       0,     0,     0,     0,  -522,  -522,   258,     0,     5,     6,
       7,     8,     9,     0,     0,  -522,    10,    11,     0,     0,
       0,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
      28,     0,    30,    31,    32,     0,    33,    34,    35,    36,
      37,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    48,     0,     0,    49,    50,     0,    51,    52,
       0,    53,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    57,    58,    59,     0,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,     0,  -522,  -522,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    98,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
     228,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     204,     0,     0,   103,    50,     0,    51,    52,     0,   229,
     230,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    57,   231,    59,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,   249,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   204,     0,     0,   103,
      50,     0,    51,    52,     0,     0,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    57,
      58,    59,     0,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,   213,   214,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   204,     0,     0,   103,    50,     0,
      51,    52,     0,     0,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    57,    58,    59,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     8,
       9,     0,     0,     0,    10,    11,     0,     0,     0,    12,
     249,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      48,     0,     0,    49,    50,     0,    51,    52,     0,    53,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    57,    58,    59,     0,     0,     0,     0,
       0,     5,     6,     7,     8,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,   390,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,     0,    30,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    48,     0,     0,    49,    50,
       0,    51,    52,     0,    53,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    57,    58,
      59,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     390,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,     0,     0,     0,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,     0,     0,
       0,     0,     0,   147,   148,   149,   150,   151,   152,   153,
     154,    35,    36,   155,    38,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   156,   157,   158,   159,   160,
     161,   162,   163,   164,     0,     0,   165,   166,     0,     0,
     167,   168,   169,   170,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   171,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,     0,   182,
     183,     0,     0,  -497,  -497,  -497,     0,  -497,     0,     0,
       0,  -497,  -497,     0,     0,     0,  -497,   184,  -497,  -497,
    -497,  -497,  -497,  -497,  -497,     0,  -497,     0,     0,     0,
    -497,  -497,  -497,  -497,  -497,  -497,  -497,     0,     0,  -497,
       0,     0,     0,     0,     0,     0,     0,     0,  -497,  -497,
       0,  -497,  -497,  -497,  -497,  -497,  -497,  -497,  -497,  -497,
    -497,  -497,  -497,     0,  -497,  -497,     0,  -497,  -497,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -497,     0,     0,
    -497,  -497,     0,  -497,  -497,     0,  -497,  -497,  -497,  -497,
       0,     0,     0,  -497,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -497,  -497,  -497,     0,     0,     0,     0,  -499,  -499,  -499,
       0,  -499,     0,     0,  -497,  -499,  -499,     0,     0,  -497,
    -499,     0,  -499,  -499,  -499,  -499,  -499,  -499,  -499,     0,
    -499,     0,     0,     0,  -499,  -499,  -499,  -499,  -499,  -499,
    -499,     0,     0,  -499,     0,     0,     0,     0,     0,     0,
       0,     0,  -499,  -499,     0,  -499,  -499,  -499,  -499,  -499,
    -499,  -499,  -499,  -499,  -499,  -499,  -499,     0,  -499,  -499,
       0,  -499,  -499,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -499,     0,     0,  -499,  -499,     0,  -499,  -499,     0,
    -499,  -499,  -499,  -499,     0,     0,     0,  -499,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -499,  -499,  -499,     0,     0,     0,
       0,  -498,  -498,  -498,     0,  -498,     0,     0,  -499,  -498,
    -498,     0,     0,  -499,  -498,     0,  -498,  -498,  -498,  -498,
    -498,  -498,  -498,     0,  -498,     0,     0,     0,  -498,  -498,
    -498,  -498,  -498,  -498,  -498,     0,     0,  -498,     0,     0,
       0,     0,     0,     0,     0,     0,  -498,  -498,     0,  -498,
    -498,  -498,  -498,  -498,  -498,  -498,  -498,  -498,  -498,  -498,
    -498,     0,  -498,  -498,     0,  -498,  -498,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -498,     0,     0,  -498,  -498,
       0,  -498,  -498,     0,  -498,  -498,  -498,  -498,     0,     0,
       0,  -498,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -498,  -498,
    -498,     0,     0,     0,     0,  -500,  -500,  -500,     0,  -500,
       0,     0,  -498,  -500,  -500,     0,     0,  -498,  -500,     0,
    -500,  -500,  -500,  -500,  -500,  -500,  -500,     0,     0,     0,
       0,     0,  -500,  -500,  -500,  -500,  -500,  -500,  -500,     0,
       0,  -500,     0,     0,     0,     0,     0,     0,     0,     0,
    -500,  -500,     0,  -500,  -500,  -500,  -500,  -500,  -500,  -500,
    -500,  -500,  -500,  -500,  -500,     0,  -500,  -500,     0,  -500,
    -500,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -500,
     703,     0,  -500,  -500,     0,  -500,  -500,     0,  -500,  -500,
    -500,  -500,     0,     0,     0,  -500,     0,     0,     0,   -97,
       0,     0,     0,     0,     0,     0,     0,  -501,  -501,  -501,
       0,  -501,  -500,  -500,  -500,  -501,  -501,     0,     0,     0,
    -501,     0,  -501,  -501,  -501,  -501,  -501,  -501,  -501,     0,
       0,  -500,     0,     0,  -501,  -501,  -501,  -501,  -501,  -501,
    -501,     0,     0,  -501,     0,     0,     0,     0,     0,     0,
       0,     0,  -501,  -501,     0,  -501,  -501,  -501,  -501,  -501,
    -501,  -501,  -501,  -501,  -501,  -501,  -501,     0,  -501,  -501,
       0,  -501,  -501,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -501,   704,     0,  -501,  -501,     0,  -501,  -501,     0,
    -501,  -501,  -501,  -501,     0,     0,     0,  -501,     0,     0,
       0,   -99,     0,     0,     0,     0,     0,     0,     0,  -248,
    -248,  -248,     0,  -248,  -501,  -501,  -501,  -248,  -248,     0,
       0,     0,  -248,     0,  -248,  -248,  -248,  -248,  -248,  -248,
    -248,     0,     0,  -501,     0,     0,  -248,  -248,  -248,  -248,
    -248,  -248,  -248,     0,     0,  -248,     0,     0,     0,     0,
       0,     0,     0,     0,  -248,  -248,     0,  -248,  -248,  -248,
    -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,     0,
    -248,  -248,     0,  -248,  -248,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -248,     0,     0,  -248,  -248,     0,  -248,
    -248,     0,  -248,  -248,  -248,  -248,     0,     0,     0,  -248,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -248,  -248,  -248,     0,  -248,  -248,  -248,  -248,  -248,
    -248,     0,     0,     0,  -248,     0,  -248,  -248,  -248,  -248,
    -248,  -248,  -248,     0,     0,   242,     0,     0,  -248,  -248,
    -248,  -248,  -248,  -248,  -248,     0,     0,  -248,     0,     0,
       0,     0,     0,     0,     0,     0,  -248,  -248,     0,  -248,
    -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,
    -248,     0,  -248,  -248,     0,  -248,  -248,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -248,     0,     0,  -248,  -248,
       0,  -248,  -248,     0,  -248,  -248,  -248,  -248,     0,     0,
       0,  -248,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -502,  -502,  -502,     0,  -502,  -248,  -248,
    -248,  -502,  -502,     0,     0,     0,  -502,     0,  -502,  -502,
    -502,  -502,  -502,  -502,  -502,     0,     0,   245,     0,     0,
    -502,  -502,  -502,  -502,  -502,  -502,  -502,     0,     0,  -502,
       0,     0,     0,     0,     0,     0,     0,     0,  -502,  -502,
       0,  -502,  -502,  -502,  -502,  -502,  -502,  -502,  -502,  -502,
    -502,  -502,  -502,     0,  -502,  -502,     0,  -502,  -502,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -502,     0,     0,
    -502,  -502,     0,  -502,  -502,     0,  -502,  -502,  -502,  -502,
       0,     0,     0,  -502,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -503,  -503,  -503,     0,  -503,
    -502,  -502,  -502,  -503,  -503,     0,     0,     0,  -503,     0,
    -503,  -503,  -503,  -503,  -503,  -503,  -503,     0,     0,  -502,
       0,     0,  -503,  -503,  -503,  -503,  -503,  -503,  -503,     0,
       0,  -503,     0,     0,     0,     0,     0,     0,     0,     0,
    -503,  -503,     0,  -503,  -503,  -503,  -503,  -503,  -503,  -503,
    -503,  -503,  -503,  -503,  -503,     0,  -503,  -503,     0,  -503,
    -503,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -503,
       0,     0,  -503,  -503,     0,  -503,  -503,     0,  -503,  -503,
    -503,  -503,     0,     0,     0,  -503,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,  -503,  -503,  -503,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,    93,    94,    18,    19,     0,
       0,  -503,     0,     0,    95,    96,    97,    23,    24,    25,
      26,     0,     0,    98,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   276,     0,     0,   103,    50,     0,    51,    52,     0,
       0,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,   104,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,    93,    94,    18,    19,     0,
       0,     0,   277,     0,    95,    96,    97,    23,    24,    25,
      26,     0,     0,    98,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   276,     0,     0,   103,    50,     0,    51,    52,     0,
       0,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   104,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   494,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,     0,     0,     0,
     137,   138,   139,   190,   191,   192,   193,   144,   145,   146,
       0,     0,     0,     0,     0,   147,   148,   149,   194,   195,
     152,   196,   154,   283,   284,   197,   285,     0,     0,     0,
       0,     0,   286,     0,     0,     0,     0,   156,   157,   158,
     159,   160,   161,   162,   163,   164,     0,     0,   165,   166,
       0,     0,   167,   168,   169,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,     0,     0,     0,
       0,     0,     0,   287,     0,     0,     0,     0,     0,     0,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
       0,   182,   183,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,     0,     0,     0,
     137,   138,   139,   190,   191,   192,   193,   144,   145,   146,
       0,     0,     0,     0,     0,   147,   148,   149,   194,   195,
     152,   196,   154,   283,   284,   197,   285,     0,     0,     0,
       0,     0,   286,     0,     0,     0,     0,   156,   157,   158,
     159,   160,   161,   162,   163,   164,     0,     0,   165,   166,
       0,     0,   167,   168,   169,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,     0,     0,     0,
       0,     0,     0,   372,     0,     0,     0,     0,     0,     0,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
       0,   182,   183,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,     0,     0,     0,
     137,   138,   139,   190,   191,   192,   193,   144,   145,   146,
       0,     0,     0,     0,     0,   147,   148,   149,   194,   195,
     152,   196,   154,     0,     0,   197,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   156,   157,   158,
     159,   160,   161,   162,   163,   164,     0,     0,   165,   166,
       0,     0,   167,   168,   169,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,     0,     0,   198,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
       0,   182,   183,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,     0,     0,     0,
     137,   138,   139,   190,   191,   192,   193,   144,   145,   146,
       0,     0,     0,     0,     0,   147,   148,   149,   194,   195,
     152,   196,   154,     0,     0,   197,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   156,   157,   158,
     159,   160,   161,   162,   163,   164,     0,     0,   165,   166,
       0,     0,   167,   168,   169,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
       0,   182,   183,     5,     6,     7,     8,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,    29,    30,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,     0,     0,
      49,    50,     0,    51,    52,     0,    53,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     8,     9,     0,     0,     0,    10,    11,
      57,    58,    59,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,     0,    28,     0,    30,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    48,     0,     0,    49,    50,     0,
      51,    52,     0,    53,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    57,    58,    59,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    98,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,   228,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   204,     0,     0,   103,    50,     0,    51,    52,     0,
     229,   230,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    57,   231,    59,    12,     0,    13,
      14,    15,    93,    94,    18,    19,     0,     0,     0,     0,
       0,    95,    96,    97,    23,    24,    25,    26,     0,     0,
      98,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    33,    34,    35,    36,    37,    38,   228,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   204,     0,
       0,   103,    50,     0,    51,    52,     0,   598,   230,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    57,   231,    59,    12,     0,    13,    14,    15,    93,
      94,    18,    19,     0,     0,     0,     0,     0,    95,    96,
      97,    23,    24,    25,    26,     0,     0,    98,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,   228,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   204,     0,     0,   103,    50,
       0,    51,    52,     0,   229,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    57,   231,
      59,    12,     0,    13,    14,    15,    93,    94,    18,    19,
       0,     0,     0,     0,     0,    95,    96,    97,    23,    24,
      25,    26,     0,     0,    98,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    33,    34,    35,    36,
      37,    38,   228,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   204,     0,     0,   103,    50,     0,    51,    52,
       0,     0,   230,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    57,   231,    59,    12,     0,
      13,    14,    15,    93,    94,    18,    19,     0,     0,     0,
       0,     0,    95,    96,    97,    23,    24,    25,    26,     0,
       0,    98,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    33,    34,    35,    36,    37,    38,   228,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   204,
       0,     0,   103,    50,     0,    51,    52,     0,   598,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    57,   231,    59,    12,     0,    13,    14,    15,
      93,    94,    18,    19,     0,     0,     0,     0,     0,    95,
      96,    97,    23,    24,    25,    26,     0,     0,    98,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,   228,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   204,     0,     0,   103,
      50,     0,    51,    52,     0,     0,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    57,
     231,    59,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    98,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   204,     0,     0,   103,    50,     0,    51,
      52,     0,   488,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    57,   231,    59,    12,
       0,    13,    14,    15,    93,    94,    18,    19,     0,     0,
       0,     0,     0,    95,    96,    97,    23,    24,    25,    26,
       0,     0,    98,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     204,     0,     0,   103,    50,     0,    51,    52,     0,   229,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    57,   231,    59,    12,     0,    13,    14,
      15,    93,    94,    18,    19,     0,     0,     0,     0,     0,
      95,    96,    97,    23,    24,    25,    26,     0,     0,    98,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   204,     0,     0,
     103,    50,     0,    51,    52,     0,   488,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      57,   231,    59,    12,     0,    13,    14,    15,    93,    94,
      18,    19,     0,     0,     0,     0,     0,    95,    96,    97,
      23,    24,    25,    26,     0,     0,    98,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   204,     0,     0,   103,    50,     0,
      51,    52,     0,   751,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    57,   231,    59,
      12,     0,    13,    14,    15,    93,    94,    18,    19,     0,
       0,     0,     0,     0,    95,    96,    97,    23,    24,    25,
      26,     0,     0,    98,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   204,     0,     0,   103,    50,     0,    51,    52,     0,
     598,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    57,   231,    59,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    33,    34,    35,    36,    37,    38,     0,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   204,     0,
       0,   103,    50,     0,    51,    52,     0,     0,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    57,    58,    59,    12,     0,    13,    14,    15,    93,
      94,    18,    19,     0,     0,     0,     0,     0,    95,    96,
      97,    23,    24,    25,    26,     0,     0,    98,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   204,     0,     0,   103,    50,
       0,    51,    52,     0,     0,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    57,   231,
      59,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    98,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    33,    34,    35,    36,
      37,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   204,     0,     0,   103,    50,     0,    51,    52,
       0,     0,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    57,   231,    59,    12,     0,
      13,    14,    15,    93,    94,    18,    19,     0,     0,     0,
       0,     0,    95,    96,    97,    23,    24,    25,    26,     0,
       0,    98,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    99,    34,    35,    36,   100,    38,     0,
      39,    40,    41,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   101,     0,     0,   102,
       0,     0,   103,    50,     0,    51,    52,     0,     0,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,     0,     0,
       0,    12,   104,    13,    14,    15,    93,    94,    18,    19,
       0,     0,     0,     0,     0,    95,    96,    97,    23,    24,
      25,    26,     0,     0,    98,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    33,    34,    35,    36,
      37,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   221,     0,     0,    49,    50,     0,    51,    52,
       0,    53,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,   104,    13,    14,    15,    93,
      94,    18,    19,     0,     0,     0,     0,     0,    95,    96,
      97,    23,    24,    25,    26,     0,     0,    98,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   276,     0,     0,   311,    50,
       0,    51,    52,     0,   312,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,   104,    13,
      14,    15,    93,    94,    18,    19,     0,     0,     0,     0,
       0,    95,    96,    97,    23,    24,    25,    26,     0,     0,
      98,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    99,    34,    35,    36,   100,    38,     0,    39,
      40,    41,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   102,     0,
       0,   103,    50,     0,    51,    52,     0,     0,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,     0,     0,
      12,   104,    13,    14,    15,    93,    94,    18,    19,     0,
       0,     0,     0,     0,    95,    96,    97,    23,    24,    25,
      26,     0,     0,    98,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   276,     0,     0,   311,    50,     0,    51,    52,     0,
       0,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,   104,    13,    14,    15,    93,    94,
      18,    19,     0,     0,     0,     0,     0,    95,    96,    97,
      23,    24,    25,    26,     0,     0,    98,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   835,     0,     0,   103,    50,     0,
      51,    52,     0,     0,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,   104,    13,    14,
      15,    93,    94,    18,    19,     0,     0,     0,     0,     0,
      95,    96,    97,    23,    24,    25,    26,     0,     0,    98,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   861,     0,     0,
     103,    50,     0,    51,    52,     0,     0,     0,    54,    55,
       0,     0,     0,    56,     0,   537,   538,     0,     0,   539,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     104,   156,   157,   158,   159,   160,   161,   162,   163,   164,
       0,     0,   165,   166,     0,     0,   167,   168,   169,   170,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     171,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,     0,   182,   183,     0,   558,   530,
       0,     0,   559,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   245,   156,   157,   158,   159,   160,   161,
     162,   163,   164,     0,     0,   165,   166,     0,     0,   167,
     168,   169,   170,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   171,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,     0,   182,   183,
       0,   543,   538,     0,     0,   544,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   245,   156,   157,   158,
     159,   160,   161,   162,   163,   164,     0,     0,   165,   166,
       0,     0,   167,   168,   169,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
       0,   182,   183,     0,   578,   530,     0,     0,   579,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   245,
     156,   157,   158,   159,   160,   161,   162,   163,   164,     0,
       0,   165,   166,     0,     0,   167,   168,   169,   170,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   172,   173,   174,   175,   176,   177,   178,
     179,   180,   181,     0,   182,   183,     0,   581,   538,     0,
       0,   582,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   245,   156,   157,   158,   159,   160,   161,   162,
     163,   164,     0,     0,   165,   166,     0,     0,   167,   168,
     169,   170,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   171,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,     0,   182,   183,     0,
     591,   530,     0,     0,   592,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   245,   156,   157,   158,   159,
     160,   161,   162,   163,   164,     0,     0,   165,   166,     0,
       0,   167,   168,   169,   170,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   171,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   172,
     173,   174,   175,   176,   177,   178,   179,   180,   181,     0,
     182,   183,     0,   594,   538,     0,     0,   595,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   245,   156,
     157,   158,   159,   160,   161,   162,   163,   164,     0,     0,
     165,   166,     0,     0,   167,   168,   169,   170,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   171,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,     0,   182,   183,     0,   619,   530,     0,     0,
     620,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   245,   156,   157,   158,   159,   160,   161,   162,   163,
     164,     0,     0,   165,   166,     0,     0,   167,   168,   169,
     170,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   171,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   181,     0,   182,   183,     0,   622,
     538,     0,     0,   623,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   245,   156,   157,   158,   159,   160,
     161,   162,   163,   164,     0,     0,   165,   166,     0,     0,
     167,   168,   169,   170,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   171,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,     0,   182,
     183,     0,   900,   530,     0,     0,   901,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   245,   156,   157,
     158,   159,   160,   161,   162,   163,   164,     0,     0,   165,
     166,     0,     0,   167,   168,   169,   170,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   171,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   172,   173,   174,   175,   176,   177,   178,   179,   180,
     181,     0,   182,   183,     0,   903,   538,     0,     0,   904,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     245,   156,   157,   158,   159,   160,   161,   162,   163,   164,
       0,     0,   165,   166,     0,     0,   167,   168,   169,   170,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     171,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,     0,   182,   183,     0,   911,   530,
       0,     0,   912,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   245,   156,   157,   158,   159,   160,   161,
     162,   163,   164,     0,     0,   165,   166,     0,     0,   167,
     168,   169,   170,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   171,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,     0,   182,   183,
       0,     0,   315,   316,   317,   318,   319,   320,   321,   322,
     323,   324,   325,   326,   327,     0,   245,   328,   329,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   543,   538,
       0,   330,   544,   331,   332,   333,   334,   335,   336,   337,
     338,   339,   340,     0,   156,   157,   158,   159,   160,   161,
     162,   163,   164,     0,     0,   165,   166,     0,     0,   167,
     168,   169,   170,     0,     0,     0,   249,     0,     0,     0,
       0,     0,     0,   171,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,     0,   182,   183
};

static const yytype_int16 yycheck[] =
{
       2,    79,    27,   212,    12,     8,    49,    28,    58,     5,
       6,    16,    17,    11,    12,    20,    16,    17,    22,    15,
      20,     7,   238,   387,    72,    28,   368,    22,    16,    17,
     405,   392,    20,   102,   282,   277,   270,   639,     4,     2,
     274,     4,   341,   402,   343,   314,   405,   346,   314,    51,
      52,    51,    52,    61,   282,   573,    53,    53,   726,   341,
     103,   343,   437,    51,   346,   583,   445,    61,   367,    55,
     342,    25,    49,   724,   562,   636,    72,     1,   639,    10,
      11,    12,   381,   365,   383,   367,    90,   248,    25,   636,
     651,   651,   580,    97,   366,    90,    25,   396,   106,   381,
      25,   383,   263,   803,    13,   593,   267,   111,    61,    26,
     382,    27,   394,    55,   396,   494,    16,    17,   821,    50,
      20,   259,   370,   395,   423,     0,    86,   546,   547,    86,
      29,   107,    77,   621,    86,   134,    25,   454,    25,    86,
     422,   423,    86,    51,    25,   666,    13,    55,   799,   448,
      88,    52,   673,   107,    86,    13,    25,    13,    15,   107,
      17,    13,   138,    13,   446,    26,   448,   112,    28,   107,
     101,    37,    38,   311,   139,   447,   136,   137,   143,   139,
     137,   849,   882,   743,   138,   137,   140,   111,   891,   143,
     137,    88,    51,   137,    53,    54,    55,    56,   414,    25,
     851,   138,   139,   445,   806,   137,   105,   215,   138,   138,
     253,   459,   198,   138,   139,   557,   259,   134,   142,   143,
     139,   219,   220,   138,   107,   567,   142,   143,   143,   277,
     881,   459,   607,   142,   143,   237,   238,   242,    26,   244,
     245,   139,   242,   245,   244,   806,   407,   606,   607,   138,
     411,   138,   494,   261,   242,   416,   244,   138,   819,   309,
     821,   680,   215,   810,   314,   426,   139,   261,   311,   138,
     431,   138,   819,   134,   792,   142,   143,   143,   139,   138,
     659,   277,   259,   139,   142,   143,   142,   143,   219,   220,
     142,   143,   142,   143,    88,   258,   365,   138,    86,    86,
     350,   351,   300,   301,   302,   303,   572,    25,   261,   626,
     627,    86,   138,   107,    86,   312,   312,   248,   566,   250,
     138,   309,    62,    63,   107,   143,   314,   228,   229,   230,
     891,    55,   493,   299,   311,   378,   299,   342,   566,   343,
     342,   304,   242,   386,   244,    26,   134,   138,   136,   137,
     137,   139,   143,   361,   715,    86,    58,    59,    88,    88,
      17,   366,   137,   367,   366,   137,    26,    88,    88,   300,
     301,   302,   303,   375,   305,   306,   585,   382,   107,   383,
     382,   142,   616,   391,   392,   601,   107,   107,    55,   107,
     395,   660,   396,   395,    25,   443,    88,   445,   419,    26,
     388,    86,   402,   651,   565,    86,   137,   724,   309,   726,
      86,   413,   414,   314,   902,   107,   419,   659,   420,   423,
     138,   796,   140,   142,   143,   143,    86,    89,   792,   138,
      86,   474,   774,   138,   434,    37,    38,   796,   369,    86,
      58,    59,   447,   134,   448,   447,   494,   443,    69,   445,
     140,   136,   137,   134,   143,   136,   137,   420,   139,    86,
     136,   137,   107,   465,   141,   467,   429,   469,    55,   630,
      86,   135,    86,   138,   134,    88,   136,   137,    86,   139,
     136,   137,   799,   800,    69,   487,   138,   388,    69,   136,
     137,    95,   834,   518,   107,    62,    63,    62,   494,    10,
     401,   402,   718,    86,   405,    62,    63,   134,     8,   136,
     137,   536,    13,    51,    10,    53,    54,    55,    56,   138,
     136,   137,   136,   137,    86,   138,   687,   138,   136,   137,
     534,   779,   849,   434,   851,    86,   437,   562,   542,   534,
     542,   545,   140,   545,   546,   547,   554,   542,   138,   570,
     545,    86,   107,   136,   137,   580,   135,   561,   556,   563,
     568,   563,   723,   138,   881,    86,   138,   570,   593,    51,
     572,   573,   781,   575,   136,   137,    51,   876,   787,    14,
      15,   583,    69,   569,   572,   136,   137,   488,    62,    63,
     633,   634,   874,   138,   876,   138,   621,   675,   596,   601,
     138,   136,   137,   875,   647,    88,   606,   632,   890,   652,
     660,   659,   138,   663,   664,   136,   137,   107,   138,   669,
     670,    10,    15,    10,   107,   556,   651,   135,   115,   116,
     117,   118,   119,   138,   677,   678,    69,    51,   135,    53,
      54,    55,    56,     2,   138,     4,     5,     6,    88,   109,
     141,    10,    11,    12,   697,   138,    15,    16,    17,    10,
      88,    20,    10,   659,    89,   596,     9,   107,   711,   712,
     713,   572,    10,    10,   682,   138,    90,   138,   680,   107,
     142,   140,    10,    97,   117,   118,   119,   135,   138,    88,
      49,    50,   138,   695,    53,   138,   698,   598,   138,    58,
     114,   467,   700,   469,   138,   606,   607,   715,   107,    88,
     138,   135,   762,    72,   112,   107,   718,   138,   743,   138,
     138,    10,    51,   624,    53,    54,    55,    56,   107,   138,
     773,   733,   734,   135,   736,    10,   738,   739,    10,   138,
      10,   784,   101,   745,   103,   747,   748,   138,   639,   112,
     138,   642,   135,    10,    55,   656,   138,   138,    55,   138,
     651,    90,    40,    41,    42,    43,    44,    96,    97,   700,
     138,    51,   138,    53,    54,    55,    56,    51,    69,    53,
      54,    55,    56,   138,   780,   114,    10,    10,   117,   139,
     792,   138,    69,    84,    85,   139,   420,   840,    15,   842,
       6,   545,   878,   626,   676,   848,   287,    84,    85,   710,
      90,   882,   877,     7,   143,   779,    96,    97,   819,   636,
     816,   112,   113,   114,   115,   116,   117,   118,   119,   854,
      -1,   189,    -1,    -1,   114,    -1,    -1,   117,   115,   116,
     117,   118,   119,    51,   203,    53,    54,    55,    56,    -1,
     751,    -1,    -1,    -1,    -1,   857,   858,   859,   860,   139,
     219,   220,   864,   865,   866,    -1,   868,   869,    -1,    -1,
     875,    -1,   876,   875,    -1,   877,   878,   902,    -1,    -1,
      -1,    -1,    90,   242,    -1,   244,   245,    -1,    69,   248,
      -1,   250,    -1,    -1,   253,   796,    -1,    -1,    -1,   258,
     259,    -1,    -1,    84,    85,    -1,    -1,    -1,   910,    -1,
      -1,   913,   914,   915,   916,   806,    -1,   808,   277,    -1,
      -1,   812,   924,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     821,    -1,   823,   114,   115,   116,   117,   118,   119,    -1,
     299,   300,   301,   302,   303,   304,   305,   306,    -1,    -1,
     309,    -1,   311,   312,    -1,   314,    -1,    -1,    51,    -1,
      53,    54,    55,    56,    -1,    -1,    -1,   733,   734,    -1,
     736,    -1,   738,   739,    -1,    -1,    -1,    -1,    -1,   745,
      -1,   747,   748,   342,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   350,   351,    -1,   885,    -1,    -1,    90,    -1,    -1,
     891,    -1,   893,    96,    97,   896,    -1,   366,    -1,   368,
     369,    -1,     2,    -1,     4,     5,     6,     7,    -1,   378,
      -1,   114,    -1,   382,   117,    15,    -1,   386,    -1,   920,
      -1,     0,    -1,    -1,    -1,    -1,   395,    -1,    -1,     8,
       9,    10,    -1,    -1,    13,    14,    15,    -1,    17,    -1,
      51,    -1,    53,    54,    55,    56,    -1,    26,    27,    49,
      -1,   420,    51,    53,    53,    54,    55,    56,    37,    38,
     429,    40,    41,    42,    43,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    72,    -1,   443,    -1,   445,    -1,   447,    90,
      -1,   857,   858,   859,   860,    96,    -1,    -1,   864,   865,
     866,    90,   868,   869,    -1,    -1,    51,    96,    53,    54,
      55,    56,    -1,   103,    -1,   474,    51,    86,    53,    54,
      55,    56,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   494,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   910,    90,    -1,   913,   914,   915,
     916,    96,    97,    -1,    -1,    90,    -1,    -1,   924,    -1,
      -1,    96,    97,    -1,    -1,   134,   135,    -1,   137,   114,
     139,   140,   117,   142,   143,    -1,    -1,    -1,    -1,   114,
      -1,    -1,   117,    -1,    -1,    -1,    69,    70,    71,    72,
      73,    74,    75,    76,   139,    78,    79,   556,   557,    -1,
      -1,    84,    85,    -1,    -1,    -1,    -1,    -1,   567,    -1,
      -1,    -1,    -1,   203,    -1,    -1,    -1,    -1,    -1,    16,
      17,    -1,    -1,    20,    -1,    -1,    -1,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   596,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      47,    -1,    -1,    -1,    51,    52,    -1,    -1,    -1,    -1,
      -1,    58,    59,   253,    -1,    -1,    -1,    -1,   258,   259,
      -1,    -1,    -1,    -1,   633,   634,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   277,   647,    -1,
      -1,    -1,    -1,   652,    -1,    -1,    -1,    -1,    -1,    -1,
     659,   660,    -1,    -1,   663,   664,    -1,    -1,    -1,   299,
     669,   670,    -1,    -1,   304,    -1,    -1,    -1,   677,   678,
      -1,   311,   312,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   697,    -1,
      -1,   700,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   711,   712,   713,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     2,    -1,     4,     5,     6,   368,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    15,    -1,   378,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   386,    -1,    -1,    -1,
      -1,    -1,    -1,   762,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   773,   774,    -1,    -1,    -1,    -1,
      49,   780,    -1,    -1,    53,   784,    -1,    -1,    -1,    -1,
     420,   228,   229,   230,   231,    -1,    -1,    -1,    -1,   429,
      -1,    -1,    -1,    72,    -1,   242,    -1,   244,   245,    -1,
      -1,    -1,    -1,   443,    -1,   445,    -1,   816,    -1,     2,
      -1,     4,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    15,    -1,   103,   834,    -1,    -1,    -1,    -1,
      -1,   840,    -1,   842,   474,    -1,    -1,    -1,    -1,   848,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   494,    -1,    49,    -1,    -1,    -1,
      -1,    -1,   309,    -1,    -1,    -1,   875,   314,   315,   316,
     317,   318,   319,   320,   321,   322,   323,   324,   325,   326,
     327,   328,   329,   330,   331,   332,   333,   334,   335,   336,
     337,   338,   339,   340,    -1,   342,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   350,   351,    -1,    -1,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,   557,    -1,   366,
      -1,    -1,    -1,    -1,   203,    -1,    -1,   567,    -1,    -1,
      -1,    -1,    -1,   380,    -1,   382,    -1,   384,   385,    -1,
      -1,   388,    -1,    -1,    -1,    -1,    -1,    -1,   395,    -1,
      -1,    -1,    -1,    -1,   401,   402,    -1,    -1,   405,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   253,    -1,    -1,   424,    -1,   258,
     259,    -1,    -1,    -1,    -1,    -1,    -1,   434,    -1,    -1,
     437,    -1,    -1,   633,   634,    -1,    -1,    -1,   277,    -1,
     447,    -1,    -1,    -1,    -1,    -1,    -1,   647,    -1,    -1,
     203,    -1,   652,    -1,    -1,    -1,    -1,    -1,    -1,   659,
     299,    -1,    -1,    -1,    -1,   304,    -1,    -1,    -1,    -1,
     477,   478,   311,   312,    -1,    -1,    -1,   677,   678,    -1,
      -1,   488,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     2,    -1,     4,   697,    -1,    -1,
     253,    -1,    -1,    -1,    -1,   258,   259,    -1,    -1,    -1,
      -1,   711,   712,   713,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   368,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   378,
      -1,    49,    -1,    -1,    -1,    -1,   299,   386,    -1,    -1,
      -1,   304,    -1,    -1,    -1,    -1,    -1,    -1,   311,    -1,
      -1,   314,    -1,    -1,    -1,   572,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   773,   774,    -1,    -1,    -1,    -1,    -1,
     780,   420,    -1,    -1,   784,    -1,    -1,    -1,    -1,    -1,
     429,   598,    -1,    -1,    -1,   103,    -1,    -1,    -1,   606,
     607,    -1,    -1,    -1,   443,    -1,   445,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   368,   816,   624,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   378,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   386,   834,   474,    -1,    -1,    -1,    -1,
     840,    -1,   842,    -1,    -1,    -1,    -1,    -1,   848,   656,
      -1,    -1,    -1,   660,   661,   494,   663,   664,    -1,    -1,
      -1,    -1,   669,   670,    -1,    -1,    -1,   420,    -1,    -1,
      -1,    -1,   679,    -1,    -1,    -1,   429,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   203,   703,   704,    -1,   706,
     707,    -1,    -1,   710,    -1,    -1,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   557,    -1,
      -1,   474,    -1,    -1,    -1,    -1,    -1,    -1,   567,    -1,
      -1,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,   751,   253,    84,    85,   755,    -1,
     258,   259,    -1,    -1,    -1,   762,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     108,    -1,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   789,    -1,    -1,    -1,    -1,    -1,    -1,   796,
      -1,   299,    -1,    -1,   633,   634,   304,    -1,    -1,    -1,
     138,    -1,    -1,   311,   557,    -1,    -1,    -1,   647,    -1,
      -1,    -1,    -1,   652,   567,    -1,    -1,    -1,    -1,    -1,
     659,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   677,   678,
      -1,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    -1,    -1,    84,    85,   697,    -1,
     368,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   875,    -1,
     378,    -1,   711,   712,   713,    -1,    -1,    -1,   386,    -1,
     633,   634,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,   647,    -1,    -1,    -1,    -1,   652,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   660,    -1,     5,
       6,    -1,   420,    -1,    -1,    -1,    -1,    -1,    -1,    15,
      -1,   429,    -1,    -1,   677,   678,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   773,   774,    -1,    -1,    -1,    -1,
      -1,   780,    -1,    -1,   697,   784,    -1,    -1,    -1,    -1,
      46,    47,    -1,    -1,    -1,    51,    52,    53,   711,   712,
     713,    -1,    -1,    59,    -1,    -1,   474,    -1,    -1,    69,
      70,    71,    72,    73,    74,    75,    72,   816,    78,    79,
      -1,    -1,    -1,    -1,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   834,    -1,    -1,    -1,    -1,
      -1,   840,    -1,   842,    -1,    -1,    -1,    -1,    -1,   848,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     773,   774,    -1,    -1,    -1,    -1,    -1,   780,    -1,    -1,
      -1,   784,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   557,
      -1,    -1,    51,    52,    -1,    -1,    55,    -1,    -1,   567,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    -1,    -1,    78,
      79,   834,    -1,    82,    83,    84,    85,   840,    -1,   842,
      -1,    -1,    -1,    -1,    -1,   848,    -1,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,   121,   122,    44,   633,   634,    -1,    -1,    -1,
      -1,    -1,   228,   229,   230,   231,    -1,    -1,    -1,   647,
     139,    -1,    -1,    -1,   652,    -1,    -1,    -1,    -1,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    -1,    -1,    84,    85,    -1,    -1,    -1,   677,
     678,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   277,    -1,    -1,    -1,    -1,    -1,    -1,   108,   697,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,    -1,   711,   712,   713,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   312,    -1,    -1,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   340,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,   773,   774,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   784,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   380,    -1,    -1,    -1,   384,   385,
      -1,   108,   388,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,   401,   402,    -1,    -1,   405,
      -1,    -1,    -1,    -1,    69,    70,    71,    72,    73,    74,
      75,    -1,    -1,    78,    79,    -1,   834,    -1,   424,    84,
      85,    -1,   840,    -1,   842,    -1,    -1,    -1,   434,    -1,
     848,   437,    -1,    -1,    -1,    -1,    -1,   443,    -1,   445,
      -1,    -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   477,   478,    -1,     0,     1,    -1,     3,     4,     5,
       6,     7,   488,    -1,    -1,    11,    12,    -1,   494,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,   572,    93,    94,    -1,
      96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   598,    -1,   120,   121,   122,    -1,    -1,    -1,
     606,   607,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   142,   143,   624,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     656,     0,    -1,   659,    -1,   661,    -1,    -1,    -1,     8,
       9,    10,    -1,    -1,    -1,    14,    15,    -1,    17,    -1,
      -1,    -1,    -1,   679,    -1,    -1,    -1,    26,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,
      -1,    40,    41,    42,    43,    44,    -1,   703,   704,    -1,
     706,   707,    -1,    -1,   710,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    -1,    -1,    84,    85,    86,    -1,    88,
      -1,    -1,    -1,    -1,    -1,   751,    -1,    -1,    -1,   755,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   107,   108,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,   780,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   789,    -1,   134,   135,   136,   137,   138,
     796,   140,     0,   142,   143,    -1,    -1,    -1,    -1,    -1,
       8,     9,    10,    -1,    -1,    -1,    14,    15,    -1,    17,
     816,    -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    -1,    40,    41,    42,    43,    44,    -1,    -1,    -1,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    -1,    -1,    84,    85,    -1,    -1,    -1,
      -1,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    -1,    -1,    84,    85,    86,   108,
      88,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   107,
     108,    -1,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   135,   136,   137,
     138,    -1,   140,    -1,   142,   143,     1,    -1,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    -1,    -1,
      15,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      45,    -1,    47,    48,    49,    -1,    51,    52,    53,    54,
      55,    56,    -1,    58,    59,    60,    61,    62,    -1,    64,
      65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,
      -1,    96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   120,   121,   122,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,     3,     4,     5,     6,     7,   142,   143,    10,
      11,    12,    -1,    14,    15,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    47,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    -1,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    10,    11,
      12,   142,   143,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    47,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,
     122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    10,    11,    12,
     142,   143,    15,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    47,    48,    49,    -1,    51,    52,
      53,    54,    55,    56,    -1,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,   122,
      -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    11,    12,    -1,   142,
     143,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      45,    46,    47,    48,    49,    -1,    51,    52,    53,    54,
      55,    56,    -1,    58,    59,    60,    61,    62,    -1,    64,
      65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,
      -1,    96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   120,   121,   122,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     135,    -1,    -1,    -1,    -1,    -1,    -1,   142,   143,     1,
      -1,     3,     4,     5,     6,     7,    -1,     9,    10,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    47,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,
     122,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    11,    12,    -1,
     142,   143,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    45,    -1,    47,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    -1,    58,    59,    60,    61,    62,    -1,
      64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
      94,    -1,    96,    -1,    98,    99,    -1,    -1,    -1,   103,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   120,   121,   122,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   140,    -1,   142,   143,
       1,    -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    47,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    -1,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   140,
      -1,   142,   143,     1,    -1,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    -1,    47,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    -1,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,    -1,
      98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   120,   121,   122,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   135,    -1,    -1,
      -1,    -1,    -1,    -1,   142,   143,     1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    10,    11,    12,    -1,    -1,
      -1,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      45,    -1,    47,    48,    49,    -1,    51,    52,    53,    54,
      55,    56,    -1,    58,    59,    60,    61,    62,    -1,    64,
      65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,
      -1,    96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   120,   121,   122,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,    -1,   142,   143,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,
      97,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   120,   121,   122,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,   143,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    -1,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    -1,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,   142,   143,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,
      53,    54,    55,    56,    -1,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    -1,    -1,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,   122,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,
     143,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,
      47,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      -1,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,
      -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   120,   121,   122,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,   142,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    47,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,
     122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     142,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    -1,    -1,    78,    79,    -1,    -1,
      82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,   121,
     122,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,   139,    18,    19,
      20,    21,    22,    23,    24,    -1,    26,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    -1,    64,    65,    -1,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    -1,    93,    94,    -1,    96,    97,    98,    99,
      -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     120,   121,   122,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,   134,    11,    12,    -1,    -1,   139,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      26,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      96,    97,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   120,   121,   122,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,   134,    11,
      12,    -1,    -1,   139,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    26,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    96,    97,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,
     122,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,   134,    11,    12,    -1,    -1,   139,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      88,    -1,    90,    91,    -1,    93,    94,    -1,    96,    97,
      98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,   107,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,   120,   121,   122,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,   139,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    88,    -1,    90,    91,    -1,    93,    94,    -1,
      96,    97,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,   107,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,   120,   121,   122,    11,    12,    -1,
      -1,    -1,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,   139,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    -1,
      64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
      94,    -1,    96,    97,    98,    99,    -1,    -1,    -1,   103,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,   120,   121,   122,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,   139,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    96,    97,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,   120,   121,
     122,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,   139,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    -1,    64,    65,    -1,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    -1,    93,    94,    -1,    96,    97,    98,    99,
      -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
     120,   121,   122,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,   139,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,    97,
      98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,   120,   121,   122,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,   139,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      -1,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,   120,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,   138,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      -1,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   120,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   138,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    -1,    -1,    -1,
      -1,    -1,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    -1,    -1,    78,    79,
      -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,
      -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,   121,   122,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    -1,    -1,    -1,
      -1,    -1,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    -1,    -1,    78,    79,
      -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,
      -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,   121,   122,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,
      50,    51,    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    -1,    -1,    78,    79,
      -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    99,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,   121,   122,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,
      50,    51,    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    -1,    -1,    78,    79,
      -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,   121,   122,     3,     4,     5,     6,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    -1,    58,    59,
      60,    61,    62,    -1,    64,    65,    -1,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    -1,    93,    94,    -1,    96,    -1,    98,    99,
      -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    -1,    11,    12,
     120,   121,   122,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    47,    48,    49,    -1,    51,    52,
      53,    54,    55,    56,    -1,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,   120,   121,   122,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      96,    97,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,   120,   121,   122,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    -1,    93,    94,    -1,    96,    97,    98,
      99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,   120,   121,   122,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,   120,   121,
     122,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    -1,    64,
      65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,
      -1,    -1,    97,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   120,   121,   122,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,    -1,
      98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,   120,   121,   122,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    -1,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,   120,
     121,   122,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    -1,    58,    59,    60,    61,    62,    -1,
      64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
      94,    -1,    96,    -1,    98,    99,    -1,    -1,    -1,   103,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,   120,   121,   122,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      -1,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,
      -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   120,   121,   122,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    -1,    58,    59,
      60,    61,    62,    -1,    64,    65,    -1,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    -1,    93,    94,    -1,    96,    -1,    98,    99,
      -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
     120,   121,   122,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,
      53,    54,    55,    56,    -1,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,   120,   121,   122,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,   120,   121,   122,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    -1,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    -1,    93,    94,    -1,    -1,    -1,    98,
      99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,   120,   121,   122,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    -1,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,   120,   121,
     122,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,    54,
      55,    56,    -1,    58,    59,    60,    61,    62,    -1,    64,
      65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,
      -1,    -1,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   120,   121,   122,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    -1,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    -1,    -1,
      98,    99,    -1,    -1,    -1,   103,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,
      -1,    16,   120,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,    54,
      55,    56,    -1,    58,    59,    60,    61,    62,    -1,    64,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,
      -1,    96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,   120,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,   120,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    -1,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    -1,    93,    94,    -1,    -1,    -1,    98,
      99,    -1,    -1,    -1,   103,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,
      16,   120,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      -1,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,   120,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,
      53,    54,    55,    56,    -1,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    -1,    -1,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,   120,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    -1,    58,    59,
      60,    61,    62,    -1,    64,    65,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    -1,    93,    94,    -1,    -1,    -1,    98,    99,
      -1,    -1,    -1,   103,    -1,    51,    52,    -1,    -1,    55,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     120,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      -1,    -1,    78,    79,    -1,    -1,    82,    83,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,   121,   122,    -1,    51,    52,
      -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   139,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    -1,    -1,    78,    79,    -1,    -1,    82,
      83,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,   121,   122,
      -1,    51,    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   139,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    -1,    -1,    78,    79,
      -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,   121,   122,    -1,    51,    52,    -1,    -1,    55,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   139,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    -1,
      -1,    78,    79,    -1,    -1,    82,    83,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    96,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,   121,   122,    -1,    51,    52,    -1,
      -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   139,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    -1,    -1,    78,    79,    -1,    -1,    82,    83,
      84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,   121,   122,    -1,
      51,    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   139,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    -1,    -1,    78,    79,    -1,
      -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,   122,    -1,    51,    52,    -1,    -1,    55,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   139,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    -1,    -1,
      78,    79,    -1,    -1,    82,    83,    84,    85,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,   121,   122,    -1,    51,    52,    -1,    -1,
      55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   139,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    -1,    -1,    78,    79,    -1,    -1,    82,    83,    84,
      85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,   121,   122,    -1,    51,
      52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   139,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    -1,    -1,    78,    79,    -1,    -1,
      82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,   121,
     122,    -1,    51,    52,    -1,    -1,    55,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   139,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    -1,    -1,    78,
      79,    -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,   121,   122,    -1,    51,    52,    -1,    -1,    55,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     139,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      -1,    -1,    78,    79,    -1,    -1,    82,    83,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,   121,   122,    -1,    51,    52,
      -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   139,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    -1,    -1,    78,    79,    -1,    -1,    82,
      83,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,   121,   122,
      -1,    -1,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    -1,   139,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    51,    52,
      -1,   108,    55,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    -1,    -1,    78,    79,    -1,    -1,    82,
      83,    84,    85,    -1,    -1,    -1,   143,    -1,    -1,    -1,
      -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,   121,   122
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   145,   146,     0,     1,     3,     4,     5,     6,     7,
      11,    12,    16,    18,    19,    20,    21,    22,    23,    24,
      30,    31,    32,    33,    34,    35,    36,    39,    45,    46,
      47,    48,    49,    51,    52,    53,    54,    55,    56,    58,
      59,    60,    61,    62,    64,    65,    67,    68,    87,    90,
      91,    93,    94,    96,    98,    99,   103,   120,   121,   122,
     147,   148,   149,   154,   156,   157,   159,   160,   163,   164,
     166,   167,   168,   170,   171,   181,   194,   212,   231,   232,
     242,   243,   247,   248,   249,   251,   252,   253,   254,   255,
     278,   289,   149,    21,    22,    30,    31,    32,    39,    51,
      55,    84,    87,    90,   120,   172,   173,   194,   212,   252,
     255,   278,   173,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    45,    46,    47,
      48,    49,    50,    51,    52,    55,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    78,    79,    82,    83,    84,
      85,    96,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   121,   122,   139,   174,   179,   180,   254,   273,
      33,    34,    35,    36,    48,    49,    51,    55,    99,   174,
     175,   177,   249,   195,    87,   157,   158,   171,   212,   252,
     253,   255,   158,   142,   143,   158,   282,   287,   288,   199,
     201,    87,   164,   171,   212,   217,   252,   255,    57,    96,
      97,   121,   163,   181,   182,   187,   190,   192,   276,   277,
     187,   187,   139,   188,   189,   139,   184,   188,   139,   143,
     283,   175,   150,   134,   181,   212,   181,    55,     1,    90,
     152,   153,   154,   165,   166,   289,   157,   197,   183,   192,
     276,   289,   182,   275,   276,   289,    87,   138,   170,   212,
     252,   255,   198,    53,    54,    56,    62,   103,   174,   250,
      62,    63,   244,    58,    59,   159,   181,   181,   282,   288,
      40,    41,    42,    43,    44,    37,    38,    28,   229,   107,
     138,    90,    96,   167,   107,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    84,    85,
     108,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    86,   136,   137,    86,   137,   281,    26,   134,   233,
      88,    88,   184,   188,   233,   157,    51,    55,   172,    58,
      59,     1,   111,   256,   287,    86,   136,   137,   208,   274,
     209,   281,   103,   138,   151,   152,    55,    13,   213,   287,
     107,    86,   136,   137,    88,    88,   213,   282,    17,   236,
     142,   158,   158,    55,    86,   136,   137,    25,   182,   182,
     182,    89,   138,   191,   289,   138,   191,   187,   283,   284,
     187,   186,   187,   192,   276,   289,   157,   284,   157,   155,
     134,   152,    86,   137,    88,   154,   165,   140,   282,   288,
     284,   196,   284,   141,   138,   143,   286,   138,   286,   135,
     286,    55,   167,   168,   169,   138,    86,   136,   137,    51,
      53,    54,    55,    56,    90,    96,    97,   114,   117,   139,
     227,   259,   260,   261,   262,   263,   264,   267,   268,   269,
     270,   271,    62,   244,   245,    62,    63,    69,    69,   149,
     158,   158,   158,   158,   154,   157,   157,   230,    96,   159,
     182,   192,   193,   165,   138,   170,   138,   156,   159,   171,
     181,   182,   193,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,    51,
      52,    55,   179,   184,   279,   280,   186,    51,    52,    55,
     179,   184,   279,    51,    55,   279,   235,   234,   159,   181,
     159,   181,    95,   161,   206,   287,   257,   205,    51,    55,
     172,   279,   186,   279,   151,   157,   139,   258,   259,   210,
     178,    10,     8,   238,   289,   152,    13,   181,    51,    55,
     186,    51,    55,   152,   236,   192,    10,    27,   214,   287,
     214,    51,    55,   186,    51,    55,   203,   182,    96,   182,
     190,   276,   277,   284,   140,   284,   138,   138,   284,   175,
     147,   135,   181,   284,   154,   284,   276,   167,   169,    51,
      55,   186,    51,    55,   107,    51,    90,    96,   218,   219,
     220,   261,   259,    29,   105,   228,   138,   272,   289,   138,
     272,    51,   138,   272,    51,    62,   152,   246,   181,   181,
      77,   112,   222,   223,   289,   182,   138,   284,   169,   138,
     107,    44,   283,    88,    88,   184,   188,   283,   285,    88,
      88,   184,   185,   188,   289,   185,   188,   222,   222,    44,
     162,   287,   158,   151,   285,    10,   284,   259,   151,   287,
     174,   175,   176,   182,   193,   239,   289,    15,   216,   289,
      14,   215,   216,    88,    88,   285,    88,    88,   216,    10,
     138,   213,   200,   202,   285,   158,   182,   191,   276,   135,
     286,   285,   182,   220,   138,   261,   138,   284,   224,   283,
     152,   152,   262,   267,   269,   271,   263,   264,   269,   263,
     135,   152,    51,   221,   224,   263,   265,   266,   269,   271,
     152,    96,   182,   169,   181,   109,   159,   181,   159,   181,
     161,   141,    88,   159,   181,   159,   181,   161,   233,   229,
     152,   152,   181,   222,   207,   287,    10,   284,    10,   211,
      89,   240,   289,   152,     9,   241,   289,   158,    10,    88,
      10,   182,   152,   152,   152,   214,   138,   284,   219,   138,
      96,   218,   140,   142,    10,   135,   138,   272,   138,   272,
     138,   272,   138,   272,   272,   135,   107,   224,   112,   138,
     272,   138,   272,   138,   272,    10,   182,   181,   159,   181,
      10,   135,   152,   151,   258,    87,   171,   212,   252,   255,
     213,   152,   213,   216,   236,   237,    10,    10,   204,   138,
     219,   138,   261,    51,   225,   226,   260,   263,   269,   263,
     263,    87,   212,   112,   266,   269,   263,   265,   269,   263,
     135,    10,   151,    55,    86,   136,   137,   152,   152,   152,
     219,   138,   138,   283,   272,   138,   272,   272,   272,    55,
      86,   138,   272,   138,   272,   272,   138,   272,   272,    10,
      51,    55,   186,    51,    55,   238,   215,    10,   219,   226,
     263,    51,    55,   263,   269,   263,   263,   285,   272,   272,
     138,   272,   272,   272,   263,   272
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (p, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, p)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, p); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, parser_state *p)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, p)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    parser_state *p;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (p);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, parser_state *p)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, p)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    parser_state *p;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, p);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, parser_state *p)
#else
static void
yy_reduce_print (yyvsp, yyrule, p)
    YYSTYPE *yyvsp;
    int yyrule;
    parser_state *p;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , p);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, p); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, parser_state *p)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, p)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    parser_state *p;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (p);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (parser_state *p);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */





/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (parser_state *p)
#else
int
yyparse (p)
    parser_state *p;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1455 of yacc.c  */
#line 999 "./parse.y"
    {
		     p->lstate = EXPR_BEG;
		     if (!p->locals) p->locals = cons(0,0);
		   ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 1004 "./parse.y"
    {
		      p->tree = new_scope(p, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 1010 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 1016 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, 0);
		    ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 1020 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, (yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 1024 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), newline_node((yyvsp[(3) - (3)].nd)));
		    ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 1028 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, 0);
		    ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 1035 "./parse.y"
    {
		      if (p->in_def || p->in_single) {
			yyerror(p, "BEGIN in method");
		      }
		      (yyval.nd) = local_switch(p);
		    ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 1042 "./parse.y"
    {
		      p->begin_tree = push(p->begin_tree, (yyvsp[(4) - (5)].nd));
		      local_resume(p, (yyvsp[(2) - (5)].nd));
		      (yyval.nd) = 0;
		    ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 1053 "./parse.y"
    {
		      if ((yyvsp[(2) - (4)].nd)) {
			(yyval.nd) = new_rescue(p, (yyvsp[(1) - (4)].nd), (yyvsp[(2) - (4)].nd), (yyvsp[(3) - (4)].nd));
		      }
		      else if ((yyvsp[(3) - (4)].nd)) {
			yywarn(p, "else without rescue is useless");
			(yyval.nd) = append((yyval.nd), (yyvsp[(3) - (4)].nd));
		      }
		      else {
			(yyval.nd) = (yyvsp[(1) - (4)].nd);
		      }
		      if ((yyvsp[(4) - (4)].nd)) {
			if ((yyval.nd)) {
			  (yyval.nd) = new_ensure(p, (yyval.nd), (yyvsp[(4) - (4)].nd));
			}
			else {
			  (yyval.nd) = push((yyvsp[(4) - (4)].nd), new_nil(p));
			}
		      }
		    ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 1076 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 1082 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, 0);
		    ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 1086 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, (yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 1090 "./parse.y"
    {
			(yyval.nd) = push((yyvsp[(1) - (3)].nd), newline_node((yyvsp[(3) - (3)].nd)));
		    ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 1094 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 1099 "./parse.y"
    {p->lstate = EXPR_FNAME;;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 1100 "./parse.y"
    {
		      (yyval.nd) = new_alias(p, (yyvsp[(2) - (4)].id), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 1104 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 1108 "./parse.y"
    {
			(yyval.nd) = new_if(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd), 0);
		    ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 1112 "./parse.y"
    {
		      (yyval.nd) = new_unless(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd), 0);
		    ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 1116 "./parse.y"
    {
		      (yyval.nd) = new_while(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd));
		    ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 1120 "./parse.y"
    {
		      (yyval.nd) = new_until(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd));
		    ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 1124 "./parse.y"
    {
		      (yyval.nd) = new_rescue(p, (yyvsp[(1) - (3)].nd), list1(list3(0, 0, (yyvsp[(3) - (3)].nd))), 0);
		    ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 1128 "./parse.y"
    {
		      if (p->in_def || p->in_single) {
			yywarn(p, "END in method; use at_exit");
		      }
		      (yyval.nd) = new_postexe(p, (yyvsp[(3) - (4)].nd));
		    ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 1136 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(1) - (3)].nd), list1((yyvsp[(3) - (3)].nd)));
		    ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 1140 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 1144 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].nd), intern("[]"), (yyvsp[(3) - (6)].nd)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].nd));
		    ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 1148 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 1152 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 1156 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.nd) = 0;
		    ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 1161 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 1165 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].nd));
		      (yyval.nd) = new_begin(p, 0);
		    ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 1170 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), new_array(p, (yyvsp[(3) - (3)].nd)));
		    ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 1174 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 1178 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(1) - (3)].nd), new_array(p, (yyvsp[(3) - (3)].nd)));
		    ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 1185 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 1189 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 1197 "./parse.y"
    {
		      (yyval.nd) = new_and(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 1201 "./parse.y"
    {
		      (yyval.nd) = new_or(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 1205 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(3) - (3)].nd)), "!");
		    ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 1209 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(2) - (2)].nd)), "!");
		    ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 1216 "./parse.y"
    {
		      if (!(yyvsp[(1) - (1)].nd)) (yyval.nd) = new_nil(p);
		      else (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 1231 "./parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 1237 "./parse.y"
    {
		      (yyval.nd) = new_block(p, (yyvsp[(3) - (5)].nd), (yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 1244 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 1248 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(2) - (3)].nd), (yyvsp[(3) - (3)].nd));
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (3)].id), (yyvsp[(2) - (3)].nd));
		    ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 1253 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 1257 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].nd), (yyvsp[(5) - (5)].nd));
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		   ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 1262 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 1266 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].nd), (yyvsp[(5) - (5)].nd));
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		    ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 1271 "./parse.y"
    {
		      (yyval.nd) = new_super(p, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 1275 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 1279 "./parse.y"
    {
		      (yyval.nd) = new_return(p, ret_args(p, (yyvsp[(2) - (2)].nd)));
		    ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 1283 "./parse.y"
    {
		      (yyval.nd) = new_break(p, ret_args(p, (yyvsp[(2) - (2)].nd)));
		    ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 1287 "./parse.y"
    {
		      (yyval.nd) = new_next(p, ret_args(p, (yyvsp[(2) - (2)].nd)));
		    ;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 1293 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 1297 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 1304 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(2) - (3)].nd));
		    ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 1310 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 1314 "./parse.y"
    {
		      (yyval.nd) = list1(push((yyvsp[(1) - (2)].nd),(yyvsp[(2) - (2)].nd)));
		    ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 1318 "./parse.y"
    {
		      (yyval.nd) = list2((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 1322 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].nd), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 1326 "./parse.y"
    {
		      (yyval.nd) = list2((yyvsp[(1) - (2)].nd), new_nil(p));
		    ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 1330 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (4)].nd), new_nil(p), (yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 1334 "./parse.y"
    {
		      (yyval.nd) = list2(0, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 1338 "./parse.y"
    {
		      (yyval.nd) = list3(0, (yyvsp[(2) - (4)].nd), (yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 1342 "./parse.y"
    {
		      (yyval.nd) = list2(0, new_nil(p));
		    ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 1346 "./parse.y"
    {
		      (yyval.nd) = list3(0, new_nil(p), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 1353 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    ;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 1359 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (2)].nd));
		    ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 1363 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(2) - (3)].nd));
		    ;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 1369 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 1373 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 1379 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 1383 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), intern("[]"), (yyvsp[(3) - (4)].nd));
		    ;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 1387 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 1391 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 1395 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 1399 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 1405 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 1411 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].nd));
		      (yyval.nd) = 0;
		    ;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 1418 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 1422 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), intern("[]"), (yyvsp[(3) - (4)].nd));
		    ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 1426 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 1430 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 1434 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 1438 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 1444 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 1450 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].nd));
		      (yyval.nd) = 0;
		    ;}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 1457 "./parse.y"
    {
		      yyerror(p, "class/module name must be CONSTANT");
		    ;}
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 1464 "./parse.y"
    {
		      (yyval.nd) = cons((node*)1, (node*)(yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 1468 "./parse.y"
    {
		      (yyval.nd) = cons((node*)0, (node*)(yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 1472 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (3)].nd), (node*)(yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 1481 "./parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 111:

/* Line 1455 of yacc.c  */
#line 1486 "./parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 1497 "./parse.y"
    {
		      (yyval.nd) = new_sym(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 115:

/* Line 1455 of yacc.c  */
#line 1503 "./parse.y"
    {
		      (yyval.nd) = new_undef(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 116:

/* Line 1455 of yacc.c  */
#line 1506 "./parse.y"
    {p->lstate = EXPR_FNAME;;}
    break;

  case 117:

/* Line 1455 of yacc.c  */
#line 1507 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), (node*)(yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 118:

/* Line 1455 of yacc.c  */
#line 1512 "./parse.y"
    { (yyval.id) = intern("|"); ;}
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 1513 "./parse.y"
    { (yyval.id) = intern("^"); ;}
    break;

  case 120:

/* Line 1455 of yacc.c  */
#line 1514 "./parse.y"
    { (yyval.id) = intern("&"); ;}
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 1515 "./parse.y"
    { (yyval.id) = intern("<=>"); ;}
    break;

  case 122:

/* Line 1455 of yacc.c  */
#line 1516 "./parse.y"
    { (yyval.id) = intern("=="); ;}
    break;

  case 123:

/* Line 1455 of yacc.c  */
#line 1517 "./parse.y"
    { (yyval.id) = intern("==="); ;}
    break;

  case 124:

/* Line 1455 of yacc.c  */
#line 1518 "./parse.y"
    { (yyval.id) = intern("=~"); ;}
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 1519 "./parse.y"
    { (yyval.id) = intern("!~"); ;}
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 1520 "./parse.y"
    { (yyval.id) = intern(">"); ;}
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 1521 "./parse.y"
    { (yyval.id) = intern(">="); ;}
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 1522 "./parse.y"
    { (yyval.id) = intern("<"); ;}
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 1523 "./parse.y"
    { (yyval.id) = intern("<="); ;}
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 1524 "./parse.y"
    { (yyval.id) = intern("!="); ;}
    break;

  case 131:

/* Line 1455 of yacc.c  */
#line 1525 "./parse.y"
    { (yyval.id) = intern("<<"); ;}
    break;

  case 132:

/* Line 1455 of yacc.c  */
#line 1526 "./parse.y"
    { (yyval.id) = intern(">>"); ;}
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 1527 "./parse.y"
    { (yyval.id) = intern("+"); ;}
    break;

  case 134:

/* Line 1455 of yacc.c  */
#line 1528 "./parse.y"
    { (yyval.id) = intern("-"); ;}
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 1529 "./parse.y"
    { (yyval.id) = intern("*"); ;}
    break;

  case 136:

/* Line 1455 of yacc.c  */
#line 1530 "./parse.y"
    { (yyval.id) = intern("*"); ;}
    break;

  case 137:

/* Line 1455 of yacc.c  */
#line 1531 "./parse.y"
    { (yyval.id) = intern("/"); ;}
    break;

  case 138:

/* Line 1455 of yacc.c  */
#line 1532 "./parse.y"
    { (yyval.id) = intern("%"); ;}
    break;

  case 139:

/* Line 1455 of yacc.c  */
#line 1533 "./parse.y"
    { (yyval.id) = intern("**"); ;}
    break;

  case 140:

/* Line 1455 of yacc.c  */
#line 1534 "./parse.y"
    { (yyval.id) = intern("!"); ;}
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 1535 "./parse.y"
    { (yyval.id) = intern("~"); ;}
    break;

  case 142:

/* Line 1455 of yacc.c  */
#line 1536 "./parse.y"
    { (yyval.id) = intern("+@"); ;}
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 1537 "./parse.y"
    { (yyval.id) = intern("-@"); ;}
    break;

  case 144:

/* Line 1455 of yacc.c  */
#line 1538 "./parse.y"
    { (yyval.id) = intern("[]"); ;}
    break;

  case 145:

/* Line 1455 of yacc.c  */
#line 1539 "./parse.y"
    { (yyval.id) = intern("[]="); ;}
    break;

  case 186:

/* Line 1455 of yacc.c  */
#line 1557 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 187:

/* Line 1455 of yacc.c  */
#line 1561 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (5)].nd), new_rescue(p, (yyvsp[(3) - (5)].nd), list1(list3(0, 0, (yyvsp[(5) - (5)].nd))), 0));
		    ;}
    break;

  case 188:

/* Line 1455 of yacc.c  */
#line 1565 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 189:

/* Line 1455 of yacc.c  */
#line 1569 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, (yyvsp[(1) - (5)].nd), (yyvsp[(2) - (5)].id), new_rescue(p, (yyvsp[(3) - (5)].nd), list1(list3(0, 0, (yyvsp[(5) - (5)].nd))), 0));
		    ;}
    break;

  case 190:

/* Line 1455 of yacc.c  */
#line 1573 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].nd), intern("[]"), (yyvsp[(3) - (6)].nd)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].nd));
		    ;}
    break;

  case 191:

/* Line 1455 of yacc.c  */
#line 1577 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 192:

/* Line 1455 of yacc.c  */
#line 1581 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 193:

/* Line 1455 of yacc.c  */
#line 1585 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 194:

/* Line 1455 of yacc.c  */
#line 1589 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.nd) = new_begin(p, 0);
		    ;}
    break;

  case 195:

/* Line 1455 of yacc.c  */
#line 1594 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.nd) = new_begin(p, 0);
		    ;}
    break;

  case 196:

/* Line 1455 of yacc.c  */
#line 1599 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].nd));
		      (yyval.nd) = new_begin(p, 0);
		    ;}
    break;

  case 197:

/* Line 1455 of yacc.c  */
#line 1604 "./parse.y"
    {
		      (yyval.nd) = new_dot2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 198:

/* Line 1455 of yacc.c  */
#line 1608 "./parse.y"
    {
		      (yyval.nd) = new_dot3(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 199:

/* Line 1455 of yacc.c  */
#line 1612 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "+", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 200:

/* Line 1455 of yacc.c  */
#line 1616 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "-", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 201:

/* Line 1455 of yacc.c  */
#line 1620 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "*", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 202:

/* Line 1455 of yacc.c  */
#line 1624 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "/", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 203:

/* Line 1455 of yacc.c  */
#line 1628 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "%", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 204:

/* Line 1455 of yacc.c  */
#line 1632 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "**", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 205:

/* Line 1455 of yacc.c  */
#line 1636 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].nd), "**", (yyvsp[(4) - (4)].nd)), "-@");
		    ;}
    break;

  case 206:

/* Line 1455 of yacc.c  */
#line 1640 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].nd), "**", (yyvsp[(4) - (4)].nd)), "-@");
		    ;}
    break;

  case 207:

/* Line 1455 of yacc.c  */
#line 1644 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, (yyvsp[(2) - (2)].nd), "+@");
		    ;}
    break;

  case 208:

/* Line 1455 of yacc.c  */
#line 1648 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, (yyvsp[(2) - (2)].nd), "-@");
		    ;}
    break;

  case 209:

/* Line 1455 of yacc.c  */
#line 1652 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "|", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 210:

/* Line 1455 of yacc.c  */
#line 1656 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "^", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 211:

/* Line 1455 of yacc.c  */
#line 1660 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "&", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 212:

/* Line 1455 of yacc.c  */
#line 1664 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<=>", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 213:

/* Line 1455 of yacc.c  */
#line 1668 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), ">", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 214:

/* Line 1455 of yacc.c  */
#line 1672 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), ">=", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 215:

/* Line 1455 of yacc.c  */
#line 1676 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 216:

/* Line 1455 of yacc.c  */
#line 1680 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<=", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 217:

/* Line 1455 of yacc.c  */
#line 1684 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "==", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 218:

/* Line 1455 of yacc.c  */
#line 1688 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "===", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 219:

/* Line 1455 of yacc.c  */
#line 1692 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "!=", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 220:

/* Line 1455 of yacc.c  */
#line 1696 "./parse.y"
    {
		      (yyval.nd) = match_op(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
#if 0
		      if (nd_type((yyvsp[(1) - (3)].nd)) == NODE_LIT && TYPE((yyvsp[(1) - (3)].nd)->nd_lit) == T_REGEXP) {
			(yyval.nd) = reg_named_capture_assign((yyvsp[(1) - (3)].nd)->nd_lit, (yyval.nd));
		      }
#endif
		    ;}
    break;

  case 221:

/* Line 1455 of yacc.c  */
#line 1705 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "!~", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 222:

/* Line 1455 of yacc.c  */
#line 1709 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(2) - (2)].nd)), "!");
		    ;}
    break;

  case 223:

/* Line 1455 of yacc.c  */
#line 1713 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(2) - (2)].nd)), "~");
		    ;}
    break;

  case 224:

/* Line 1455 of yacc.c  */
#line 1717 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<<", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 225:

/* Line 1455 of yacc.c  */
#line 1721 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), ">>", (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 226:

/* Line 1455 of yacc.c  */
#line 1725 "./parse.y"
    {
		      (yyval.nd) = new_and(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 227:

/* Line 1455 of yacc.c  */
#line 1729 "./parse.y"
    {
		      (yyval.nd) = new_or(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 228:

/* Line 1455 of yacc.c  */
#line 1733 "./parse.y"
    {
		      (yyval.nd) = new_if(p, cond((yyvsp[(1) - (6)].nd)), (yyvsp[(3) - (6)].nd), (yyvsp[(6) - (6)].nd));
		    ;}
    break;

  case 229:

/* Line 1455 of yacc.c  */
#line 1737 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    ;}
    break;

  case 230:

/* Line 1455 of yacc.c  */
#line 1743 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		      if (!(yyval.nd)) (yyval.nd) = new_nil(p);
		    ;}
    break;

  case 232:

/* Line 1455 of yacc.c  */
#line 1751 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    ;}
    break;

  case 233:

/* Line 1455 of yacc.c  */
#line 1755 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), new_hash(p, (yyvsp[(3) - (4)].nd)));
		    ;}
    break;

  case 234:

/* Line 1455 of yacc.c  */
#line 1759 "./parse.y"
    {
		      (yyval.nd) = new_hash(p, (yyvsp[(1) - (2)].nd));
		    ;}
    break;

  case 235:

/* Line 1455 of yacc.c  */
#line 1765 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    ;}
    break;

  case 240:

/* Line 1455 of yacc.c  */
#line 1777 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (2)].nd),0);
		    ;}
    break;

  case 241:

/* Line 1455 of yacc.c  */
#line 1781 "./parse.y"
    {
		      (yyval.nd) = cons(push((yyvsp[(1) - (4)].nd), new_hash(p, (yyvsp[(3) - (4)].nd))), 0);
		    ;}
    break;

  case 242:

/* Line 1455 of yacc.c  */
#line 1785 "./parse.y"
    {
		      (yyval.nd) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].nd))), 0);
		    ;}
    break;

  case 243:

/* Line 1455 of yacc.c  */
#line 1791 "./parse.y"
    {
		      (yyval.nd) = cons(list1((yyvsp[(1) - (1)].nd)), 0);
		    ;}
    break;

  case 244:

/* Line 1455 of yacc.c  */
#line 1795 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 245:

/* Line 1455 of yacc.c  */
#line 1799 "./parse.y"
    {
		      (yyval.nd) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].nd))), (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 246:

/* Line 1455 of yacc.c  */
#line 1803 "./parse.y"
    {
		      (yyval.nd) = cons(push((yyvsp[(1) - (4)].nd), new_hash(p, (yyvsp[(3) - (4)].nd))), (yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 247:

/* Line 1455 of yacc.c  */
#line 1807 "./parse.y"
    {
		      (yyval.nd) = cons(0, (yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 248:

/* Line 1455 of yacc.c  */
#line 1812 "./parse.y"
    {
		      (yyval.stack) = p->cmdarg_stack;
		      CMDARG_PUSH(1);
		    ;}
    break;

  case 249:

/* Line 1455 of yacc.c  */
#line 1817 "./parse.y"
    {
		      p->cmdarg_stack = (yyvsp[(1) - (2)].stack);
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    ;}
    break;

  case 250:

/* Line 1455 of yacc.c  */
#line 1824 "./parse.y"
    {
		      (yyval.nd) = new_block_arg(p, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 251:

/* Line 1455 of yacc.c  */
#line 1830 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    ;}
    break;

  case 252:

/* Line 1455 of yacc.c  */
#line 1834 "./parse.y"
    {
		      (yyval.nd) = 0;
		    ;}
    break;

  case 253:

/* Line 1455 of yacc.c  */
#line 1840 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (1)].nd), 0);
		    ;}
    break;

  case 254:

/* Line 1455 of yacc.c  */
#line 1844 "./parse.y"
    {
		      (yyval.nd) = cons(new_splat(p, (yyvsp[(2) - (2)].nd)), 0);
		    ;}
    break;

  case 255:

/* Line 1455 of yacc.c  */
#line 1848 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 256:

/* Line 1455 of yacc.c  */
#line 1852 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), new_splat(p, (yyvsp[(4) - (4)].nd)));
		    ;}
    break;

  case 257:

/* Line 1455 of yacc.c  */
#line 1858 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 258:

/* Line 1455 of yacc.c  */
#line 1862 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), new_splat(p, (yyvsp[(4) - (4)].nd)));
		    ;}
    break;

  case 259:

/* Line 1455 of yacc.c  */
#line 1866 "./parse.y"
    {
		      (yyval.nd) = list1(new_splat(p, (yyvsp[(2) - (2)].nd)));
		    ;}
    break;

  case 265:

/* Line 1455 of yacc.c  */
#line 1877 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (1)].id), 0);
		    ;}
    break;

  case 266:

/* Line 1455 of yacc.c  */
#line 1881 "./parse.y"
    {
		      (yyvsp[(1) - (1)].stack) = p->cmdarg_stack;
		      p->cmdarg_stack = 0;
		    ;}
    break;

  case 267:

/* Line 1455 of yacc.c  */
#line 1887 "./parse.y"
    {
		      p->cmdarg_stack = (yyvsp[(1) - (4)].stack);
		      (yyval.nd) = (yyvsp[(3) - (4)].nd);
		    ;}
    break;

  case 268:

/* Line 1455 of yacc.c  */
#line 1891 "./parse.y"
    {p->lstate = EXPR_ENDARG;;}
    break;

  case 269:

/* Line 1455 of yacc.c  */
#line 1892 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (4)].nd);
		    ;}
    break;

  case 270:

/* Line 1455 of yacc.c  */
#line 1895 "./parse.y"
    {p->lstate = EXPR_ENDARG;;}
    break;

  case 271:

/* Line 1455 of yacc.c  */
#line 1896 "./parse.y"
    {
		      (yyval.nd) = 0;
		    ;}
    break;

  case 272:

/* Line 1455 of yacc.c  */
#line 1900 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    ;}
    break;

  case 273:

/* Line 1455 of yacc.c  */
#line 1904 "./parse.y"
    {
		      (yyval.nd) = new_colon2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 274:

/* Line 1455 of yacc.c  */
#line 1908 "./parse.y"
    {
		      (yyval.nd) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 275:

/* Line 1455 of yacc.c  */
#line 1912 "./parse.y"
    {
		      (yyval.nd) = new_array(p, (yyvsp[(2) - (3)].nd));
		    ;}
    break;

  case 276:

/* Line 1455 of yacc.c  */
#line 1916 "./parse.y"
    {
		      (yyval.nd) = new_hash(p, (yyvsp[(2) - (3)].nd));
		    ;}
    break;

  case 277:

/* Line 1455 of yacc.c  */
#line 1920 "./parse.y"
    {
		      (yyval.nd) = new_return(p, 0);
		    ;}
    break;

  case 278:

/* Line 1455 of yacc.c  */
#line 1924 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, (yyvsp[(3) - (4)].nd));
		    ;}
    break;

  case 279:

/* Line 1455 of yacc.c  */
#line 1928 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, 0);
		    ;}
    break;

  case 280:

/* Line 1455 of yacc.c  */
#line 1932 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, 0);
		    ;}
    break;

  case 281:

/* Line 1455 of yacc.c  */
#line 1936 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(3) - (4)].nd)), "!");
		    ;}
    break;

  case 282:

/* Line 1455 of yacc.c  */
#line 1940 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, new_nil(p), "!");
		    ;}
    break;

  case 283:

/* Line 1455 of yacc.c  */
#line 1944 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (2)].id), cons(0, (yyvsp[(2) - (2)].nd)));
		    ;}
    break;

  case 285:

/* Line 1455 of yacc.c  */
#line 1949 "./parse.y"
    {
		      call_with_block(p, (yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    ;}
    break;

  case 286:

/* Line 1455 of yacc.c  */
#line 1954 "./parse.y"
    {
		      local_nest(p);
		      (yyval.num) = p->lpar_beg;
		      p->lpar_beg = ++p->paren_nest;
		    ;}
    break;

  case 287:

/* Line 1455 of yacc.c  */
#line 1961 "./parse.y"
    {
		      p->lpar_beg = (yyvsp[(2) - (4)].num);
		      (yyval.nd) = new_lambda(p, (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].nd));
		      local_unnest(p);
		    ;}
    break;

  case 288:

/* Line 1455 of yacc.c  */
#line 1970 "./parse.y"
    {
		      (yyval.nd) = new_if(p, cond((yyvsp[(2) - (6)].nd)), (yyvsp[(4) - (6)].nd), (yyvsp[(5) - (6)].nd));
		    ;}
    break;

  case 289:

/* Line 1455 of yacc.c  */
#line 1977 "./parse.y"
    {
		      (yyval.nd) = new_unless(p, cond((yyvsp[(2) - (6)].nd)), (yyvsp[(4) - (6)].nd), (yyvsp[(5) - (6)].nd));
		    ;}
    break;

  case 290:

/* Line 1455 of yacc.c  */
#line 1980 "./parse.y"
    {COND_PUSH(1);;}
    break;

  case 291:

/* Line 1455 of yacc.c  */
#line 1980 "./parse.y"
    {COND_POP();;}
    break;

  case 292:

/* Line 1455 of yacc.c  */
#line 1983 "./parse.y"
    {
		      (yyval.nd) = new_while(p, cond((yyvsp[(3) - (7)].nd)), (yyvsp[(6) - (7)].nd));
		    ;}
    break;

  case 293:

/* Line 1455 of yacc.c  */
#line 1986 "./parse.y"
    {COND_PUSH(1);;}
    break;

  case 294:

/* Line 1455 of yacc.c  */
#line 1986 "./parse.y"
    {COND_POP();;}
    break;

  case 295:

/* Line 1455 of yacc.c  */
#line 1989 "./parse.y"
    {
		      (yyval.nd) = new_until(p, cond((yyvsp[(3) - (7)].nd)), (yyvsp[(6) - (7)].nd));
		    ;}
    break;

  case 296:

/* Line 1455 of yacc.c  */
#line 1995 "./parse.y"
    {
		      (yyval.nd) = new_case(p, (yyvsp[(2) - (5)].nd), (yyvsp[(4) - (5)].nd));
		    ;}
    break;

  case 297:

/* Line 1455 of yacc.c  */
#line 1999 "./parse.y"
    {
		      (yyval.nd) = new_case(p, 0, (yyvsp[(3) - (4)].nd));
		    ;}
    break;

  case 298:

/* Line 1455 of yacc.c  */
#line 2003 "./parse.y"
    {COND_PUSH(1);;}
    break;

  case 299:

/* Line 1455 of yacc.c  */
#line 2005 "./parse.y"
    {COND_POP();;}
    break;

  case 300:

/* Line 1455 of yacc.c  */
#line 2008 "./parse.y"
    {
		      (yyval.nd) = new_for(p, (yyvsp[(2) - (9)].nd), (yyvsp[(5) - (9)].nd), (yyvsp[(8) - (9)].nd));
		    ;}
    break;

  case 301:

/* Line 1455 of yacc.c  */
#line 2012 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "class definition in method body");
		      (yyval.nd) = local_switch(p);
		    ;}
    break;

  case 302:

/* Line 1455 of yacc.c  */
#line 2019 "./parse.y"
    {
		      (yyval.nd) = new_class(p, (yyvsp[(2) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].nd));
		      local_resume(p, (yyvsp[(4) - (6)].nd));
		    ;}
    break;

  case 303:

/* Line 1455 of yacc.c  */
#line 2024 "./parse.y"
    {
		      (yyval.num) = p->in_def;
		      p->in_def = 0;
		    ;}
    break;

  case 304:

/* Line 1455 of yacc.c  */
#line 2029 "./parse.y"
    {
		      (yyval.nd) = cons(local_switch(p), (node*)(intptr_t)p->in_single);
		      p->in_single = 0;
		    ;}
    break;

  case 305:

/* Line 1455 of yacc.c  */
#line 2035 "./parse.y"
    {
		      (yyval.nd) = new_sclass(p, (yyvsp[(3) - (8)].nd), (yyvsp[(7) - (8)].nd));
		      local_resume(p, (yyvsp[(6) - (8)].nd)->car);
		      p->in_def = (yyvsp[(4) - (8)].num);
		      p->in_single = (int)(intptr_t)(yyvsp[(6) - (8)].nd)->cdr;
		    ;}
    break;

  case 306:

/* Line 1455 of yacc.c  */
#line 2042 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "module definition in method body");
		      (yyval.nd) = local_switch(p);
		    ;}
    break;

  case 307:

/* Line 1455 of yacc.c  */
#line 2049 "./parse.y"
    {
		      (yyval.nd) = new_module(p, (yyvsp[(2) - (5)].nd), (yyvsp[(4) - (5)].nd));
		      local_resume(p, (yyvsp[(3) - (5)].nd));
		    ;}
    break;

  case 308:

/* Line 1455 of yacc.c  */
#line 2054 "./parse.y"
    {
		      p->in_def++;
		      (yyval.nd) = local_switch(p);
		    ;}
    break;

  case 309:

/* Line 1455 of yacc.c  */
#line 2061 "./parse.y"
    {
		      (yyval.nd) = new_def(p, (yyvsp[(2) - (6)].id), (yyvsp[(4) - (6)].nd), (yyvsp[(5) - (6)].nd));
		      local_resume(p, (yyvsp[(3) - (6)].nd));
		      p->in_def--;
		    ;}
    break;

  case 310:

/* Line 1455 of yacc.c  */
#line 2066 "./parse.y"
    {p->lstate = EXPR_FNAME;;}
    break;

  case 311:

/* Line 1455 of yacc.c  */
#line 2067 "./parse.y"
    {
		      p->in_single++;
		      p->lstate = EXPR_ENDFN; /* force for args */
		      (yyval.nd) = local_switch(p);
		    ;}
    break;

  case 312:

/* Line 1455 of yacc.c  */
#line 2075 "./parse.y"
    {
		      (yyval.nd) = new_sdef(p, (yyvsp[(2) - (9)].nd), (yyvsp[(5) - (9)].id), (yyvsp[(7) - (9)].nd), (yyvsp[(8) - (9)].nd));
		      local_resume(p, (yyvsp[(6) - (9)].nd));
		      p->in_single--;
		    ;}
    break;

  case 313:

/* Line 1455 of yacc.c  */
#line 2081 "./parse.y"
    {
		      (yyval.nd) = new_break(p, 0);
		    ;}
    break;

  case 314:

/* Line 1455 of yacc.c  */
#line 2085 "./parse.y"
    {
		      (yyval.nd) = new_next(p, 0);
		    ;}
    break;

  case 315:

/* Line 1455 of yacc.c  */
#line 2089 "./parse.y"
    {
		      (yyval.nd) = new_redo(p);
		    ;}
    break;

  case 316:

/* Line 1455 of yacc.c  */
#line 2093 "./parse.y"
    {
		      (yyval.nd) = new_retry(p);
		    ;}
    break;

  case 317:

/* Line 1455 of yacc.c  */
#line 2099 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		      if (!(yyval.nd)) (yyval.nd) = new_nil(p);
		    ;}
    break;

  case 324:

/* Line 1455 of yacc.c  */
#line 2118 "./parse.y"
    {
		      (yyval.nd) = new_if(p, cond((yyvsp[(2) - (5)].nd)), (yyvsp[(4) - (5)].nd), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 326:

/* Line 1455 of yacc.c  */
#line 2125 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    ;}
    break;

  case 327:

/* Line 1455 of yacc.c  */
#line 2131 "./parse.y"
    {
		      (yyval.nd) = list1(list1((yyvsp[(1) - (1)].nd)));
		    ;}
    break;

  case 329:

/* Line 1455 of yacc.c  */
#line 2138 "./parse.y"
    {
		      (yyval.nd) = new_arg(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 330:

/* Line 1455 of yacc.c  */
#line 2142 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(2) - (3)].nd), 0);
		    ;}
    break;

  case 331:

/* Line 1455 of yacc.c  */
#line 2148 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 332:

/* Line 1455 of yacc.c  */
#line 2152 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 333:

/* Line 1455 of yacc.c  */
#line 2158 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (1)].nd),0,0);
		    ;}
    break;

  case 334:

/* Line 1455 of yacc.c  */
#line 2162 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (4)].nd), new_arg(p, (yyvsp[(4) - (4)].id)), 0);
		    ;}
    break;

  case 335:

/* Line 1455 of yacc.c  */
#line 2166 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (6)].nd), new_arg(p, (yyvsp[(4) - (6)].id)), (yyvsp[(6) - (6)].nd));
		    ;}
    break;

  case 336:

/* Line 1455 of yacc.c  */
#line 2170 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (3)].nd), (node*)-1, 0);
		    ;}
    break;

  case 337:

/* Line 1455 of yacc.c  */
#line 2174 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (5)].nd), (node*)-1, (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 338:

/* Line 1455 of yacc.c  */
#line 2178 "./parse.y"
    {
		      (yyval.nd) = list3(0, new_arg(p, (yyvsp[(2) - (2)].id)), 0);
		    ;}
    break;

  case 339:

/* Line 1455 of yacc.c  */
#line 2182 "./parse.y"
    {
		      (yyval.nd) = list3(0, new_arg(p, (yyvsp[(2) - (4)].id)), (yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 340:

/* Line 1455 of yacc.c  */
#line 2186 "./parse.y"
    {
		      (yyval.nd) = list3(0, (node*)-1, 0);
		    ;}
    break;

  case 341:

/* Line 1455 of yacc.c  */
#line 2190 "./parse.y"
    {
		      (yyval.nd) = list3(0, (node*)-1, (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 342:

/* Line 1455 of yacc.c  */
#line 2196 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 343:

/* Line 1455 of yacc.c  */
#line 2200 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (8)].nd), (yyvsp[(3) - (8)].nd), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].nd), (yyvsp[(8) - (8)].id));
		    ;}
    break;

  case 344:

/* Line 1455 of yacc.c  */
#line 2204 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].nd), 0, 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 345:

/* Line 1455 of yacc.c  */
#line 2208 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), 0, (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 346:

/* Line 1455 of yacc.c  */
#line 2212 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 347:

/* Line 1455 of yacc.c  */
#line 2216 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (2)].nd), 0, 1, 0, 0);
		    ;}
    break;

  case 348:

/* Line 1455 of yacc.c  */
#line 2220 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 349:

/* Line 1455 of yacc.c  */
#line 2224 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (2)].nd), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 350:

/* Line 1455 of yacc.c  */
#line 2228 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 351:

/* Line 1455 of yacc.c  */
#line 2232 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 352:

/* Line 1455 of yacc.c  */
#line 2236 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (2)].nd), 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 353:

/* Line 1455 of yacc.c  */
#line 2240 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 354:

/* Line 1455 of yacc.c  */
#line 2244 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 355:

/* Line 1455 of yacc.c  */
#line 2248 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 356:

/* Line 1455 of yacc.c  */
#line 2252 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 358:

/* Line 1455 of yacc.c  */
#line 2259 "./parse.y"
    {
		      p->cmd_start = TRUE;
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    ;}
    break;

  case 359:

/* Line 1455 of yacc.c  */
#line 2266 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.nd) = 0;
		    ;}
    break;

  case 360:

/* Line 1455 of yacc.c  */
#line 2271 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.nd) = 0;
		    ;}
    break;

  case 361:

/* Line 1455 of yacc.c  */
#line 2276 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (4)].nd);
		    ;}
    break;

  case 362:

/* Line 1455 of yacc.c  */
#line 2283 "./parse.y"
    {
		      (yyval.nd) = 0;
		    ;}
    break;

  case 363:

/* Line 1455 of yacc.c  */
#line 2287 "./parse.y"
    {
		      (yyval.nd) = 0;
		    ;}
    break;

  case 366:

/* Line 1455 of yacc.c  */
#line 2297 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      new_bv(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 368:

/* Line 1455 of yacc.c  */
#line 2305 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (4)].nd);
		    ;}
    break;

  case 369:

/* Line 1455 of yacc.c  */
#line 2309 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    ;}
    break;

  case 370:

/* Line 1455 of yacc.c  */
#line 2315 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    ;}
    break;

  case 371:

/* Line 1455 of yacc.c  */
#line 2319 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    ;}
    break;

  case 372:

/* Line 1455 of yacc.c  */
#line 2325 "./parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 373:

/* Line 1455 of yacc.c  */
#line 2331 "./parse.y"
    {
		      (yyval.nd) = new_block(p,(yyvsp[(3) - (5)].nd),(yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    ;}
    break;

  case 374:

/* Line 1455 of yacc.c  */
#line 2338 "./parse.y"
    {
		      if ((yyvsp[(1) - (2)].nd)->car == (node*)NODE_YIELD) {
			yyerror(p, "block given to yield");
		      }
		      else {
		        call_with_block(p, (yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		      }
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    ;}
    break;

  case 375:

/* Line 1455 of yacc.c  */
#line 2348 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 376:

/* Line 1455 of yacc.c  */
#line 2352 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		      call_with_block(p, (yyval.nd), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 377:

/* Line 1455 of yacc.c  */
#line 2357 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		      call_with_block(p, (yyval.nd), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 378:

/* Line 1455 of yacc.c  */
#line 2364 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 379:

/* Line 1455 of yacc.c  */
#line 2368 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 380:

/* Line 1455 of yacc.c  */
#line 2372 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    ;}
    break;

  case 381:

/* Line 1455 of yacc.c  */
#line 2376 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 382:

/* Line 1455 of yacc.c  */
#line 2380 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), intern("call"), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 383:

/* Line 1455 of yacc.c  */
#line 2384 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), intern("call"), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 384:

/* Line 1455 of yacc.c  */
#line 2388 "./parse.y"
    {
		      (yyval.nd) = new_super(p, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 385:

/* Line 1455 of yacc.c  */
#line 2392 "./parse.y"
    {
		      (yyval.nd) = new_zsuper(p);
		    ;}
    break;

  case 386:

/* Line 1455 of yacc.c  */
#line 2396 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), intern("[]"), (yyvsp[(3) - (4)].nd));
		    ;}
    break;

  case 387:

/* Line 1455 of yacc.c  */
#line 2402 "./parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 388:

/* Line 1455 of yacc.c  */
#line 2407 "./parse.y"
    {
		      (yyval.nd) = new_block(p,(yyvsp[(3) - (5)].nd),(yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    ;}
    break;

  case 389:

/* Line 1455 of yacc.c  */
#line 2412 "./parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 390:

/* Line 1455 of yacc.c  */
#line 2417 "./parse.y"
    {
		      (yyval.nd) = new_block(p,(yyvsp[(3) - (5)].nd),(yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    ;}
    break;

  case 391:

/* Line 1455 of yacc.c  */
#line 2426 "./parse.y"
    {
		      (yyval.nd) = cons(cons((yyvsp[(2) - (5)].nd), (yyvsp[(4) - (5)].nd)), (yyvsp[(5) - (5)].nd));
		    ;}
    break;

  case 392:

/* Line 1455 of yacc.c  */
#line 2432 "./parse.y"
    {
		      if ((yyvsp[(1) - (1)].nd)) {
			(yyval.nd) = cons(cons(0, (yyvsp[(1) - (1)].nd)), 0);
		      }
		      else {
			(yyval.nd) = 0;
		      }
		    ;}
    break;

  case 394:

/* Line 1455 of yacc.c  */
#line 2446 "./parse.y"
    {
		      (yyval.nd) = list1(list3((yyvsp[(2) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].nd)));
		      if ((yyvsp[(6) - (6)].nd)) (yyval.nd) = append((yyval.nd), (yyvsp[(6) - (6)].nd));
		    ;}
    break;

  case 396:

/* Line 1455 of yacc.c  */
#line 2454 "./parse.y"
    {
			(yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 399:

/* Line 1455 of yacc.c  */
#line 2462 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    ;}
    break;

  case 401:

/* Line 1455 of yacc.c  */
#line 2469 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    ;}
    break;

  case 407:

/* Line 1455 of yacc.c  */
#line 2482 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    ;}
    break;

  case 408:

/* Line 1455 of yacc.c  */
#line 2486 "./parse.y"
    {
		      (yyval.nd) = new_dstr(p, push((yyvsp[(2) - (3)].nd), (yyvsp[(3) - (3)].nd)));
		    ;}
    break;

  case 409:

/* Line 1455 of yacc.c  */
#line 2492 "./parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    ;}
    break;

  case 410:

/* Line 1455 of yacc.c  */
#line 2498 "./parse.y"
    {
		      p->sterm = (yyvsp[(2) - (4)].num);
		      (yyval.nd) = list2((yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].nd));
		    ;}
    break;

  case 411:

/* Line 1455 of yacc.c  */
#line 2504 "./parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    ;}
    break;

  case 412:

/* Line 1455 of yacc.c  */
#line 2510 "./parse.y"
    {
		      p->sterm = (yyvsp[(3) - (5)].num);
		      (yyval.nd) = push(push((yyvsp[(1) - (5)].nd), (yyvsp[(2) - (5)].nd)), (yyvsp[(4) - (5)].nd));
		    ;}
    break;

  case 414:

/* Line 1455 of yacc.c  */
#line 2520 "./parse.y"
    {
		      (yyval.nd) = new_sym(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 415:

/* Line 1455 of yacc.c  */
#line 2524 "./parse.y"
    {
		      p->lstate = EXPR_END;
		      (yyval.nd) = new_dsym(p, push((yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].nd)));
		    ;}
    break;

  case 416:

/* Line 1455 of yacc.c  */
#line 2531 "./parse.y"
    {
		      p->lstate = EXPR_END;
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 421:

/* Line 1455 of yacc.c  */
#line 2542 "./parse.y"
    {
		      (yyval.id) = new_strsym(p, (yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 422:

/* Line 1455 of yacc.c  */
#line 2546 "./parse.y"
    {
		      (yyval.id) = new_strsym(p, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 425:

/* Line 1455 of yacc.c  */
#line 2554 "./parse.y"
    {
		      (yyval.nd) = negate_lit(p, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 426:

/* Line 1455 of yacc.c  */
#line 2558 "./parse.y"
    {
		      (yyval.nd) = negate_lit(p, (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 427:

/* Line 1455 of yacc.c  */
#line 2564 "./parse.y"
    {
		      (yyval.nd) = new_lvar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 428:

/* Line 1455 of yacc.c  */
#line 2568 "./parse.y"
    {
		      (yyval.nd) = new_ivar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 429:

/* Line 1455 of yacc.c  */
#line 2572 "./parse.y"
    {
		      (yyval.nd) = new_gvar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 430:

/* Line 1455 of yacc.c  */
#line 2576 "./parse.y"
    {
		      (yyval.nd) = new_cvar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 431:

/* Line 1455 of yacc.c  */
#line 2580 "./parse.y"
    {
		      (yyval.nd) = new_const(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 432:

/* Line 1455 of yacc.c  */
#line 2586 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 433:

/* Line 1455 of yacc.c  */
#line 2592 "./parse.y"
    {
		      (yyval.nd) = var_reference(p, (yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 434:

/* Line 1455 of yacc.c  */
#line 2596 "./parse.y"
    {
		      (yyval.nd) = new_nil(p);
		    ;}
    break;

  case 435:

/* Line 1455 of yacc.c  */
#line 2600 "./parse.y"
    {
		      (yyval.nd) = new_self(p);
   		    ;}
    break;

  case 436:

/* Line 1455 of yacc.c  */
#line 2604 "./parse.y"
    {
		      (yyval.nd) = new_true(p);
   		    ;}
    break;

  case 437:

/* Line 1455 of yacc.c  */
#line 2608 "./parse.y"
    {
		      (yyval.nd) = new_false(p);
   		    ;}
    break;

  case 438:

/* Line 1455 of yacc.c  */
#line 2612 "./parse.y"
    {
		      if (!p->filename) {
			p->filename = "(null)";
		      }
		      (yyval.nd) = new_str(p, p->filename, strlen(p->filename));
		    ;}
    break;

  case 439:

/* Line 1455 of yacc.c  */
#line 2619 "./parse.y"
    {
		      char buf[16];

		      snprintf(buf, sizeof(buf), "%d", p->lineno);
		      (yyval.nd) = new_int(p, buf, 10);
		    ;}
    break;

  case 442:

/* Line 1455 of yacc.c  */
#line 2632 "./parse.y"
    {
		      (yyval.nd) = 0;
		    ;}
    break;

  case 443:

/* Line 1455 of yacc.c  */
#line 2636 "./parse.y"
    {
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    ;}
    break;

  case 444:

/* Line 1455 of yacc.c  */
#line 2641 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(3) - (4)].nd);
		    ;}
    break;

  case 445:

/* Line 1455 of yacc.c  */
#line 2645 "./parse.y"
    {
		      yyerrok;
		      (yyval.nd) = 0;
		    ;}
    break;

  case 446:

/* Line 1455 of yacc.c  */
#line 2652 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    ;}
    break;

  case 447:

/* Line 1455 of yacc.c  */
#line 2658 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    ;}
    break;

  case 448:

/* Line 1455 of yacc.c  */
#line 2664 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 449:

/* Line 1455 of yacc.c  */
#line 2668 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (8)].nd), (yyvsp[(3) - (8)].nd), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].nd), (yyvsp[(8) - (8)].id));
		    ;}
    break;

  case 450:

/* Line 1455 of yacc.c  */
#line 2672 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].nd), 0, 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 451:

/* Line 1455 of yacc.c  */
#line 2676 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), 0, (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 452:

/* Line 1455 of yacc.c  */
#line 2680 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 453:

/* Line 1455 of yacc.c  */
#line 2684 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 454:

/* Line 1455 of yacc.c  */
#line 2688 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (2)].nd), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 455:

/* Line 1455 of yacc.c  */
#line 2692 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 456:

/* Line 1455 of yacc.c  */
#line 2696 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 457:

/* Line 1455 of yacc.c  */
#line 2700 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (2)].nd), 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 458:

/* Line 1455 of yacc.c  */
#line 2704 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 459:

/* Line 1455 of yacc.c  */
#line 2708 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 460:

/* Line 1455 of yacc.c  */
#line 2712 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 461:

/* Line 1455 of yacc.c  */
#line 2716 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 462:

/* Line 1455 of yacc.c  */
#line 2720 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.nd) = new_args(p, 0, 0, 0, 0, 0);
		    ;}
    break;

  case 463:

/* Line 1455 of yacc.c  */
#line 2727 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a constant");
		      (yyval.nd) = 0;
		    ;}
    break;

  case 464:

/* Line 1455 of yacc.c  */
#line 2732 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be an instance variable");
		      (yyval.nd) = 0;
		    ;}
    break;

  case 465:

/* Line 1455 of yacc.c  */
#line 2737 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a global variable");
		      (yyval.nd) = 0;
		    ;}
    break;

  case 466:

/* Line 1455 of yacc.c  */
#line 2742 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a class variable");
		      (yyval.nd) = 0;
		    ;}
    break;

  case 467:

/* Line 1455 of yacc.c  */
#line 2749 "./parse.y"
    {
		      (yyval.id) = 0;
		    ;}
    break;

  case 468:

/* Line 1455 of yacc.c  */
#line 2753 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 469:

/* Line 1455 of yacc.c  */
#line 2760 "./parse.y"
    {
		      (yyval.nd) = new_arg(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 470:

/* Line 1455 of yacc.c  */
#line 2764 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(2) - (3)].nd), 0);
		    ;}
    break;

  case 471:

/* Line 1455 of yacc.c  */
#line 2770 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 472:

/* Line 1455 of yacc.c  */
#line 2774 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 473:

/* Line 1455 of yacc.c  */
#line 2780 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.nd) = cons((node*)(yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 474:

/* Line 1455 of yacc.c  */
#line 2787 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.nd) = cons((node*)(yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 475:

/* Line 1455 of yacc.c  */
#line 2794 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 476:

/* Line 1455 of yacc.c  */
#line 2798 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 477:

/* Line 1455 of yacc.c  */
#line 2804 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 478:

/* Line 1455 of yacc.c  */
#line 2808 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 481:

/* Line 1455 of yacc.c  */
#line 2818 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 482:

/* Line 1455 of yacc.c  */
#line 2823 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.id) = -1;
		    ;}
    break;

  case 485:

/* Line 1455 of yacc.c  */
#line 2834 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 486:

/* Line 1455 of yacc.c  */
#line 2841 "./parse.y"
    {
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 487:

/* Line 1455 of yacc.c  */
#line 2845 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.id) = 0;
		    ;}
    break;

  case 488:

/* Line 1455 of yacc.c  */
#line 2852 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		      if (!(yyval.nd)) (yyval.nd) = new_nil(p);
		    ;}
    break;

  case 489:

/* Line 1455 of yacc.c  */
#line 2856 "./parse.y"
    {p->lstate = EXPR_BEG;;}
    break;

  case 490:

/* Line 1455 of yacc.c  */
#line 2857 "./parse.y"
    {
		      if ((yyvsp[(3) - (4)].nd) == 0) {
			yyerror(p, "can't define singleton method for ().");
		      }
		      else {
			switch ((enum node_type)(yyvsp[(3) - (4)].nd)->car) {
			case NODE_STR:
			case NODE_DSTR:
			case NODE_DREGX:
			case NODE_MATCH:
			case NODE_FLOAT:
			case NODE_ARRAY:
			  yyerror(p, "can't define singleton method for literals");
			default:
			  break;
			}
		      }
		      (yyval.nd) = (yyvsp[(3) - (4)].nd);
		    ;}
    break;

  case 492:

/* Line 1455 of yacc.c  */
#line 2880 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    ;}
    break;

  case 493:

/* Line 1455 of yacc.c  */
#line 2886 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    ;}
    break;

  case 494:

/* Line 1455 of yacc.c  */
#line 2890 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 495:

/* Line 1455 of yacc.c  */
#line 2896 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    ;}
    break;

  case 496:

/* Line 1455 of yacc.c  */
#line 2900 "./parse.y"
    {
		      (yyval.nd) = cons(new_sym(p, (yyvsp[(1) - (2)].id)), (yyvsp[(2) - (2)].nd));
		    ;}
    break;

  case 518:

/* Line 1455 of yacc.c  */
#line 2944 "./parse.y"
    {yyerrok;;}
    break;

  case 521:

/* Line 1455 of yacc.c  */
#line 2949 "./parse.y"
    {yyerrok;;}
    break;

  case 522:

/* Line 1455 of yacc.c  */
#line 2953 "./parse.y"
    {
		      (yyval.nd) = 0;
		    ;}
    break;



/* Line 1455 of yacc.c  */
#line 8697 "./y.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (p, YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (p, yymsg);
	  }
	else
	  {
	    yyerror (p, YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, p);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, p);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (p, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, p);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, p);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 2957 "./parse.y"

#define yylval  (*((YYSTYPE*)(p->ylval)))

static void
yyerror(parser_state *p, const char *s)
{
  char* c;
  size_t n;

  if (! p->capture_errors) {
    if (p->filename) {
      fprintf(stderr, "%s:%d:%d: %s\n", p->filename, p->lineno, p->column, s);
    }
    else {
      fprintf(stderr, "line %d:%d: %s\n", p->lineno, p->column, s);
    }
  }
  else if (p->nerr < sizeof(p->error_buffer) / sizeof(p->error_buffer[0])) {
    n = strlen(s);
    c = parser_palloc(p, n + 1);
    memcpy(c, s, n + 1);
    p->error_buffer[p->nerr].message = c;
    p->error_buffer[p->nerr].lineno = p->lineno;
    p->error_buffer[p->nerr].column = p->column;
  }
  p->nerr++;
}

static void
yyerror_i(parser_state *p, const char *fmt, int i)
{
  char buf[256];

  snprintf(buf, sizeof(buf), fmt, i);
  yyerror(p, buf);
}

static void
yywarn(parser_state *p, const char *s)
{
  char* c;
  size_t n;

  if (! p->capture_errors) {
    if (p->filename) {
      fprintf(stderr, "%s:%d:%d: %s\n", p->filename, p->lineno, p->column, s);
    }
    else {
      fprintf(stderr, "line %d:%d: %s\n", p->lineno, p->column, s);
    }
  }
  else if (p->nerr < sizeof(p->warn_buffer) / sizeof(p->warn_buffer[0])) {
    n = strlen(s);
    c = parser_palloc(p, n + 1);
    memcpy(c, s, n + 1);
    p->warn_buffer[p->nwarn].message = c;
    p->warn_buffer[p->nwarn].lineno = p->lineno;
    p->warn_buffer[p->nwarn].column = p->column;
  }
  p->nwarn++;
}

static void
yywarning(parser_state *p, const char *s)
{
  yywarn(p, s);
}

static void
yywarning_s(parser_state *p, const char *fmt, const char *s)
{
  char buf[256];

  snprintf(buf, sizeof(buf), fmt, s);
  yywarning(p, buf);
}

static void
backref_error(parser_state *p, node *n)
{
  switch ((int)(intptr_t)n->car) {
  case NODE_NTH_REF:
    yyerror_i(p, "can't set variable $%d", (int)(intptr_t)n->cdr);
    break;
  case NODE_BACK_REF:
    yyerror_i(p, "can't set variable $%c", (int)(intptr_t)n->cdr);
    break;
  }
}

static int peeks(parser_state *p, const char *s);
static int skips(parser_state *p, const char *s);

static inline int
nextc(parser_state *p)
{
  int c;

  if (p->pb) {
    node *tmp;

    c = (int)(intptr_t)p->pb->car;
    tmp = p->pb;
    p->pb = p->pb->cdr;
    cons_free(tmp);
  }
  else {
    if (p->f) {
      if (feof(p->f)) return -1;
      c = fgetc(p->f);
      if (c == EOF) return -1;
    }
    else if (!p->s || p->s >= p->send) {
      return -1;
    }
    else {
      c = *p->s++;
    }
    if (c == '\n') {
      // must understand heredoc
    }
  }
  p->column++;
  return c;
}

static void
pushback(parser_state *p, int c)
{
  if (c < 0) return;
  p->column--;
  p->pb = cons((node*)(intptr_t)c, p->pb);
}

static void
skip(parser_state *p, char term)
{
  int c;

  for (;;) {
    c = nextc(p);
    if (c < 0) break;
    if (c == term) break;
  }
}

static int
peek_n(parser_state *p, int c, int n)
{
  node *list = 0;
  int c0;

  do {
    c0 = nextc(p);
    if (c0 < 0) return FALSE;
    list = push(list, (node*)(intptr_t)c0);
  } while(n--);
  if (p->pb) {
    p->pb = push(p->pb, (node*)list);
  }
  else {
    p->pb = list;
  }
  if (c0 == c) return TRUE;
  return FALSE;
}
#define peek(p,c) peek_n((p), (c), 0)

static int
peeks(parser_state *p, const char *s)
{
  int len = strlen(s);

  if (p->f) {
    int n = 0;
    while (*s) {
      if (!peek_n(p, *s++, n++)) return FALSE;
    }
    return TRUE;
  }
  else if (p->s && p->s + len >= p->send) {
    if (memcmp(p->s, s, len) == 0) return TRUE;
  }
  return FALSE;
}

static int
skips(parser_state *p, const char *s)
{
  int c;

  for (;;) {
    // skip until first char
    for (;;) {
      c = nextc(p);
      if (c < 0) return c;
      if (c == *s) break;
    }
    s++;
    if (peeks(p, s)) {
      int len = strlen(s);

      while (len--) {
	nextc(p);
      }
      return TRUE;
    }
  }
  return FALSE;
}

#define STR_FUNC_ESCAPE 0x01
#define STR_FUNC_EXPAND 0x02
#define STR_FUNC_REGEXP 0x04
#define STR_FUNC_QWORDS 0x08
#define STR_FUNC_SYMBOL 0x10
#define STR_FUNC_INDENT 0x20

enum string_type {
    str_squote = (0),
    str_dquote = (STR_FUNC_EXPAND),
    str_xquote = (STR_FUNC_EXPAND),
    str_regexp = (STR_FUNC_REGEXP|STR_FUNC_ESCAPE|STR_FUNC_EXPAND),
    str_sword  = (STR_FUNC_QWORDS),
    str_dword  = (STR_FUNC_QWORDS|STR_FUNC_EXPAND),
    str_ssym   = (STR_FUNC_SYMBOL),
    str_dsym   = (STR_FUNC_SYMBOL|STR_FUNC_EXPAND)
};

static int
newtok(parser_state *p)
{
  p->bidx = 0;
  return p->column - 1;
}

static void
tokadd(parser_state *p, int c)
{
  if (p->bidx < 1024) {
    p->buf[p->bidx++] = c;
  }
}

static int
toklast(parser_state *p)
{
  return p->buf[p->bidx-1];
}

static void
tokfix(parser_state *p)
{
  if (p->bidx >= 1024) {
    yyerror(p, "string too long (truncated)");
  }
  p->buf[p->bidx] = '\0';
}

static const char*
tok(parser_state *p)
{
  return p->buf;
}

static int
toklen(parser_state *p)
{
  return p->bidx;
}

#define IS_ARG() (p->lstate == EXPR_ARG || p->lstate == EXPR_CMDARG)
#define IS_END() (p->lstate == EXPR_END || p->lstate == EXPR_ENDARG || p->lstate == EXPR_ENDFN)
#define IS_BEG() (p->lstate == EXPR_BEG || p->lstate == EXPR_MID || p->lstate == EXPR_VALUE || p->lstate == EXPR_CLASS)
#define IS_SPCARG(c) (IS_ARG() && space_seen && !ISSPACE(c))
#define IS_LABEL_POSSIBLE() ((p->lstate == EXPR_BEG && !cmd_state) || IS_ARG())
#define IS_LABEL_SUFFIX(n) (peek_n(p, ':',(n)) && !peek_n(p, ':', (n)+1))

static unsigned long
scan_oct(const int *start, int len, int *retlen)
{
  const int *s = start;
  unsigned long retval = 0;

  while (len-- && *s >= '0' && *s <= '7') {
    retval <<= 3;
    retval |= *s++ - '0';
  }
  *retlen = s - start;
  return retval;
}

static unsigned long
scan_hex(const int *start, int len, int *retlen)
{
  static const char hexdigit[] = "0123456789abcdef0123456789ABCDEF";
  register const int *s = start;
  register unsigned long retval = 0;
  char *tmp;

  while (len-- && *s && (tmp = strchr(hexdigit, *s))) {
    retval <<= 4;
    retval |= (tmp - hexdigit) & 15;
    s++;
  }
  *retlen = s - start;
  return retval;
}

static int
read_escape(parser_state *p)
{
  int c;

  switch (c = nextc(p)) {
  case '\\':	/* Backslash */
    return c;

  case 'n':	/* newline */
    return '\n';

  case 't':	/* horizontal tab */
    return '\t';

  case 'r':	/* carriage-return */
    return '\r';

  case 'f':	/* form-feed */
    return '\f';

  case 'v':	/* vertical tab */
    return '\13';

  case 'a':	/* alarm(bell) */
    return '\007';

  case 'e':	/* escape */
    return 033;

  case '0': case '1': case '2': case '3': /* octal constant */
  case '4': case '5': case '6': case '7':
    {
       int buf[3];
       int i;

       for (i=0; i<3; i++) {
	 buf[i] = nextc(p);
	 if (buf[i] == -1) goto eof;
	 if (buf[i] < '0' || '7' < buf[i]) {
	   pushback(p, buf[i]);
	   break;
	 }
       }
       c = scan_oct(buf, i+1, &i);
    }
    return c;

  case 'x':	/* hex constant */
    {
      int buf[2];
      int i;

      for (i=0; i<2; i++) {
	buf[i] = nextc(p);
	if (buf[i] == -1) goto eof;
	if (!isxdigit(buf[i])) {
	  pushback(p, buf[i]);
	  break;
	}
      }
      c = scan_hex(buf, i+1, &i);
      if (i == 0) {
	yyerror(p, "Invalid escape character syntax");
	return 0;
      }
    }
    return c;

  case 'b':	/* backspace */
    return '\010';

  case 's':	/* space */
    return ' ';

  case 'M':
    if ((c = nextc(p)) != '-') {
      yyerror(p, "Invalid escape character syntax");
      pushback(p, c);
      return '\0';
    }
    if ((c = nextc(p)) == '\\') {
      return read_escape(p) | 0x80;
    }
    else if (c == -1) goto eof;
    else {
      return ((c & 0xff) | 0x80);
    }

  case 'C':
    if ((c = nextc(p)) != '-') {
      yyerror(p, "Invalid escape character syntax");
      pushback(p, c);
      return '\0';
    }
  case 'c':
    if ((c = nextc(p))== '\\') {
      c = read_escape(p);
    }
    else if (c == '?')
      return 0177;
    else if (c == -1) goto eof;
    return c & 0x9f;

  eof:
  case -1:
    yyerror(p, "Invalid escape character syntax");
    return '\0';

  default:
    return c;
  }
}

static int
parse_string(parser_state *p, int term)
{
  int c;

  newtok(p);

  while ((c = nextc(p)) != term) {
    if (c  == -1) {
      yyerror(p, "unterminated string meets end of file");
      return 0;
    }
    else if (c == '\\') {
      c = nextc(p);
      if (c == term) {
	tokadd(p, c);
      }
      else {
	pushback(p, c);
	tokadd(p, read_escape(p));
      }
      continue;
    }
    if (c == '#') {
      c = nextc(p);
      if (c == '{') {
	tokfix(p);
	p->lstate = EXPR_BEG;
	p->sterm = term;
	p->cmd_start = TRUE;
	yylval.nd = new_str(p, tok(p), toklen(p));
	return tSTRING_PART;
      }
      tokadd(p, '#');
      pushback(p, c);
      continue;
    }
    tokadd(p, c);
  }

  tokfix(p);
  p->lstate = EXPR_END;
  p->sterm = 0;
  yylval.nd = new_str(p, tok(p), toklen(p));
  return tSTRING;
}

static node*
qstring_node(parser_state *p, int term)
{
  int c;

  newtok(p);
  while ((c = nextc(p)) != term) {
    if (c  == -1)  {
      yyerror(p, "unterminated string meets end of file");
      return 0;
    }
    if (c == '\\') {
      c = nextc(p);
      switch (c) {
      case '\n':
	p->lineno++;
	p->column = 0;
	continue;

      case '\\':
	c = '\\';
	break;

      case '\'':
	if (term == '\'') {
	  c = '\'';
	  break;
	}
	/* fall through */
      default:
	tokadd(p, '\\');
      }
    }
    tokadd(p, c);
  }

  tokfix(p);
  p->lstate = EXPR_END;
  return new_str(p, tok(p), toklen(p));
}

static int
parse_qstring(parser_state *p, int term)
{
  node *nd = qstring_node(p, term);

  if (nd) {
    yylval.nd = new_str(p, tok(p), toklen(p));
    return tSTRING;
  }
  return 0;
}

static int
arg_ambiguous(parser_state *p)
{
  yywarning(p, "ambiguous first argument; put parentheses or even spaces");
  return 1;
}

#include "lex.def"

static int
parser_yylex(parser_state *p)
{
  register int c;
  int space_seen = 0;
  int cmd_state;
  enum mrb_lex_state_enum last_state;
  int token_column;

  if (p->sterm) {
    return parse_string(p, p->sterm);
  }
  cmd_state = p->cmd_start;
  p->cmd_start = FALSE;
 retry:
  last_state = p->lstate;
  switch (c = nextc(p)) {
  case '\0':		/* NUL */
  case '\004':		/* ^D */
  case '\032':		/* ^Z */
  case -1:		/* end of script. */
    return 0;

    /* white spaces */
  case ' ': case '\t': case '\f': case '\r':
  case '\13': /* '\v' */
    space_seen = 1;
    goto retry;

  case '#':		/* it's a comment */
    skip(p, '\n');
    /* fall through */
  case '\n':
    p->lineno++;
    p->column = 0;
    switch (p->lstate) {
    case EXPR_BEG:
    case EXPR_FNAME:
    case EXPR_DOT:
    case EXPR_CLASS:
    case EXPR_VALUE:
      goto retry;
    default:
      break;
    }
    while ((c = nextc(p))) {
      switch (c) {
      case ' ': case '\t': case '\f': case '\r':
      case '\13': /* '\v' */
	space_seen = 1;
	break;
      case '.':
	if ((c = nextc(p)) != '.') {
	  pushback(p, c);
	  pushback(p, '.');
	  goto retry;
	}
      case -1:			/* EOF */
	goto normal_newline;
      default:
	pushback(p, c);
	goto normal_newline;
      }
    }
  normal_newline:
    p->cmd_start = TRUE;
    p->lstate = EXPR_BEG;
    return '\n';

  case '*':
    if ((c = nextc(p)) == '*') {
      if ((c = nextc(p)) == '=') {
	yylval.id = intern("**");
	p->lstate = EXPR_BEG;
	return tOP_ASGN;
      }
      pushback(p, c);
      c = tPOW;
    }
    else {
      if (c == '=') {
	yylval.id = intern("*");
	p->lstate = EXPR_BEG;
	return tOP_ASGN;
      }
      pushback(p, c);
      if (IS_SPCARG(c)) {
	yywarning(p, "`*' interpreted as argument prefix");
	c = tSTAR;
      }
      else if (IS_BEG()) {
	c = tSTAR;
      }
      else {
	c = '*';
      }
    }
    switch (p->lstate) {
    case EXPR_FNAME: case EXPR_DOT:
      p->lstate = EXPR_ARG; break;
    default:
      p->lstate = EXPR_BEG; break;
    }
    return c;

  case '!':
    c = nextc(p);
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
      if (c == '@') {
	return '!';
      }
    }
    else {
      p->lstate = EXPR_BEG;
    }
    if (c == '=') {
      return tNEQ;
    }
    if (c == '~') {
      return tNMATCH;
    }
    pushback(p, c);
    return '!';

  case '=':
    if (p->column == 1) {
      if (peeks(p, "begin\n")) {
	skips(p, "\n=end\n");
	goto retry;
      }
    }
    switch (p->lstate) {
    case EXPR_FNAME: case EXPR_DOT:
      p->lstate = EXPR_ARG; break;
    default:
      p->lstate = EXPR_BEG; break;
    }
    if ((c = nextc(p)) == '=') {
      if ((c = nextc(p)) == '=') {
	return tEQQ;
      }
      pushback(p, c);
      return tEQ;
    }
    if (c == '~') {
      return tMATCH;
    }
    else if (c == '>') {
      return tASSOC;
    }
    pushback(p, c);
    return '=';

  case '<':
    last_state = p->lstate;
    c = nextc(p);
#if 0
    // no heredoc supported yet
    if (c == '<' &&
	p->lstate != EXPR_DOT &&
	p->lstate != EXPR_CLASS &&
	!IS_END() &&
	(!IS_ARG() || space_seen)) {
      int token = heredoc_identifier();
      if (token) return token;
    }
#endif
    switch (p->lstate) {
    case EXPR_FNAME: case EXPR_DOT:
      p->lstate = EXPR_ARG; break;
    case EXPR_CLASS:
      p->cmd_start = TRUE;
    default:
      p->lstate = EXPR_BEG; break;
    }
    if (c == '=') {
      if ((c = nextc(p)) == '>') {
	return tCMP;
      }
      pushback(p, c);
      return tLEQ;
    }
    if (c == '<') {
      if ((c = nextc(p)) == '=') {
	yylval.id = intern("<<");
	p->lstate = EXPR_BEG;
	return tOP_ASGN;
      }
      pushback(p, c);
      return tLSHFT;
    }
    pushback(p, c);
    return '<';

  case '>':
    switch (p->lstate) {
    case EXPR_FNAME: case EXPR_DOT:
      p->lstate = EXPR_ARG; break;
    default:
      p->lstate = EXPR_BEG; break;
    }
    if ((c = nextc(p)) == '=') {
      return tGEQ;
    }
    if (c == '>') {
      if ((c = nextc(p)) == '=') {
	yylval.id = intern(">>");
	p->lstate = EXPR_BEG;
	return tOP_ASGN;
      }
      pushback(p, c);
      return tRSHFT;
    }
    pushback(p, c);
    return '>';

  case '"':
    p->sterm = '"';
    return tSTRING_BEG;

  case '\'':
    return parse_qstring(p, c);

  case '?':
    if (IS_END()) {
      p->lstate = EXPR_VALUE;
      return '?';
    }
    c = nextc(p);
    if (c == -1) {
      yyerror(p, "incomplete character syntax");
      return 0;
    }
    if (isspace(c)) {
      if (!IS_ARG()) {
	int c2 = 0;
	switch (c) {
	case ' ':
	  c2 = 's';
	  break;
	case '\n':
	  c2 = 'n';
	  break;
	case '\t':
	  c2 = 't';
	  break;
	case '\v':
	  c2 = 'v';
	  break;
	case '\r':
	  c2 = 'r';
	  break;
	case '\f':
	  c2 = 'f';
	  break;
	}
	if (c2) {
	  char buf[256];
	  snprintf(buf, sizeof(buf), "invalid character syntax; use ?\\%c", c2);
	  yyerror(p, buf);
	}
      }
    ternary:
      pushback(p, c);
      p->lstate = EXPR_VALUE;
      return '?';
    }
    token_column = newtok(p);
    // need support UTF-8 if configured
    if ((isalnum(c) || c == '_')) {
      int c2 = nextc(p);
      pushback(p, c2);
      if ((isalnum(c2) || c2 == '_')) {
	goto ternary;
      }
    }
    if (c == '\\') {
      c = nextc(p);
      if (c == 'u') {
#if 0
	tokadd_utf8(p);
#endif
      }
      else {
	pushback(p, c);
	c = read_escape(p);
	tokadd(p, c);
      }
    }
    else {
      tokadd(p, c);
    }
    tokfix(p);
    yylval.nd = new_str(p, tok(p), toklen(p));
    p->lstate = EXPR_END;
    return tCHAR;

  case '&':
    if ((c = nextc(p)) == '&') {
      p->lstate = EXPR_BEG;
      if ((c = nextc(p)) == '=') {
	yylval.id = intern("&&");
	p->lstate = EXPR_BEG;
	return tOP_ASGN;
      }
      pushback(p, c);
      return tANDOP;
    }
    else if (c == '=') {
      yylval.id = intern("&");
      p->lstate = EXPR_BEG;
      return tOP_ASGN;
    }
    pushback(p, c);
    if (IS_SPCARG(c)) {
      yywarning(p, "`&' interpreted as argument prefix");
      c = tAMPER;
    }
    else if (IS_BEG()) {
      c = tAMPER;
    }
    else {
      c = '&';
    }
    switch (p->lstate) {
    case EXPR_FNAME: case EXPR_DOT:
      p->lstate = EXPR_ARG; break;
    default:
      p->lstate = EXPR_BEG;
    }
    return c;

  case '|':
    if ((c = nextc(p)) == '|') {
      p->lstate = EXPR_BEG;
      if ((c = nextc(p)) == '=') {
	yylval.id = intern("||");
	p->lstate = EXPR_BEG;
	return tOP_ASGN;
      }
      pushback(p, c);
      return tOROP;
    }
    if (c == '=') {
      yylval.id = intern("|");
      p->lstate = EXPR_BEG;
      return tOP_ASGN;
    }
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
    }
    else {
      p->lstate = EXPR_BEG;
    }
    pushback(p, c);
    return '|';

  case '+':
    c = nextc(p);
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
      if (c == '@') {
	return tUPLUS;
      }
      pushback(p, c);
      return '+';
    }
    if (c == '=') {
      yylval.id = intern("+");
      p->lstate = EXPR_BEG;
      return tOP_ASGN;
    }
    if (IS_BEG() || (IS_SPCARG(c) && arg_ambiguous(p))) {
      p->lstate = EXPR_BEG;
      pushback(p, c);
      if (c != -1 && ISDIGIT(c)) {
	c = '+';
	goto start_num;
      }
      return tUPLUS;
    }
    p->lstate = EXPR_BEG;
    pushback(p, c);
    return '+';

  case '-':
    c = nextc(p);
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
      if (c == '@') {
	return tUMINUS;
      }
      pushback(p, c);
      return '-';
    }
    if (c == '=') {
      yylval.id = intern("-");
      p->lstate = EXPR_BEG;
      return tOP_ASGN;
    }
    if (c == '>') {
      p->lstate = EXPR_ENDFN;
      return tLAMBDA;
    }
    if (IS_BEG() || (IS_SPCARG(c) && arg_ambiguous(p))) {
      p->lstate = EXPR_BEG;
      pushback(p, c);
      if (c != -1 && ISDIGIT(c)) {
	return tUMINUS_NUM;
      }
      return tUMINUS;
    }
    p->lstate = EXPR_BEG;
    pushback(p, c);
    return '-';

  case '.':
    p->lstate = EXPR_BEG;
    if ((c = nextc(p)) == '.') {
      if ((c = nextc(p)) == '.') {
	return tDOT3;
      }
      pushback(p, c);
      return tDOT2;
    }
    pushback(p, c);
    if (c != -1 && ISDIGIT(c)) {
      yyerror(p, "no .<digit> floating literal anymore; put 0 before dot");
    }
    p->lstate = EXPR_DOT;
    return '.';

  start_num:
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    {
      int is_float, seen_point, seen_e, nondigit;
      
      is_float = seen_point = seen_e = nondigit = 0;
      p->lstate = EXPR_END;
      token_column = newtok(p);
      if (c == '-' || c == '+') {
	tokadd(p, c);
	c = nextc(p);
      }
      if (c == '0') {
#define no_digits() do {yyerror(p,"numeric literal without digits"); return 0;} while (0)
	int start = toklen(p);
	c = nextc(p);
	if (c == 'x' || c == 'X') {
	  /* hexadecimal */
	  c = nextc(p);
	  if (c != -1 && ISXDIGIT(c)) {
	    do {
	      if (c == '_') {
		if (nondigit) break;
		nondigit = c;
		continue;
	      }
	      if (!ISXDIGIT(c)) break;
	      nondigit = 0;
	      tokadd(p, c);
	    } while ((c = nextc(p)) != -1);
	  }
	  pushback(p, c);
	  tokfix(p);
	  if (toklen(p) == start) {
	    no_digits();
	  }
	  else if (nondigit) goto trailing_uc;
	  yylval.nd = new_int(p, tok(p), 16);
	  return tINTEGER;
	}
	if (c == 'b' || c == 'B') {
	  /* binary */
	  c = nextc(p);
	  if (c == '0' || c == '1') {
	    do {
	      if (c == '_') {
		if (nondigit) break;
		nondigit = c;
		continue;
	      }
	      if (c != '0' && c != '1') break;
	      nondigit = 0;
	      tokadd(p, c);
	    } while ((c = nextc(p)) != -1);
	  }
	  pushback(p, c);
	  tokfix(p);
	  if (toklen(p) == start) {
	    no_digits();
	  }
	  else if (nondigit) goto trailing_uc;
	  yylval.nd = new_int(p, tok(p), 2);
	  return tINTEGER;
	}
	if (c == 'd' || c == 'D') {
	  /* decimal */
	  c = nextc(p);
	  if (c != -1 && ISDIGIT(c)) {
	    do {
	      if (c == '_') {
		if (nondigit) break;
		nondigit = c;
		continue;
	      }
	      if (!ISDIGIT(c)) break;
	      nondigit = 0;
	      tokadd(p, c);
	    } while ((c = nextc(p)) != -1);
	  }
	  pushback(p, c);
	  tokfix(p);
	  if (toklen(p) == start) {
	    no_digits();
	  }
	  else if (nondigit) goto trailing_uc;
	  yylval.nd = new_int(p, tok(p), 10);
	  return tINTEGER;
	}
	if (c == '_') {
	  /* 0_0 */
	  goto octal_number;
	}
	if (c == 'o' || c == 'O') {
	  /* prefixed octal */
	  c = nextc(p);
	  if (c == -1 || c == '_' || !ISDIGIT(c)) {
	    no_digits();
	  }
	}
	if (c >= '0' && c <= '7') {
	  /* octal */
	octal_number:
	  do {
	    if (c == '_') {
	      if (nondigit) break;
	      nondigit = c;
	      continue;
	    }
	    if (c < '0' || c > '9') break;
	    if (c > '7') goto invalid_octal;
	    nondigit = 0;
	    tokadd(p, c);
	  } while ((c = nextc(p)) != -1);

	  if (toklen(p) > start) {
	    pushback(p, c);
	    tokfix(p);
	    if (nondigit) goto trailing_uc;
	    yylval.nd = new_int(p, tok(p), 8);
	    return tINTEGER;
	  }
	  if (nondigit) {
	    pushback(p, c);
	    goto trailing_uc;
	  }
	}
	if (c > '7' && c <= '9') {
	invalid_octal:
	  yyerror(p, "Invalid octal digit");
	}
	else if (c == '.' || c == 'e' || c == 'E') {
	  tokadd(p, '0');
	}
	else {
	  pushback(p, c);
	  yylval.nd = new_int(p, "0", 10);
	  return tINTEGER;
	}
      }

      for (;;) {
	switch (c) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  nondigit = 0;
	  tokadd(p, c);
	  break;

	case '.':
	  if (nondigit) goto trailing_uc;
	  if (seen_point || seen_e) {
	    goto decode_num;
	  }
	  else {
	    int c0 = nextc(p);
	    if (c0 == -1 || !ISDIGIT(c0)) {
	      pushback(p, c0);
	      goto decode_num;
	    }
	    c = c0;
	  }
	  tokadd(p, '.');
	  tokadd(p, c);
	  is_float++;
	  seen_point++;
	  nondigit = 0;
	  break;

	case 'e':
	case 'E':
	  if (nondigit) {
	    pushback(p, c);
	    c = nondigit;
	    goto decode_num;
	  }
	  if (seen_e) {
	    goto decode_num;
	  }
	  tokadd(p, c);
	  seen_e++;
	  is_float++;
	  nondigit = c;
	  c = nextc(p);
	  if (c != '-' && c != '+') continue;
	  tokadd(p, c);
	  nondigit = c;
	  break;

	case '_':	/* `_' in number just ignored */
	  if (nondigit) goto decode_num;
	  nondigit = c;
	  break;
	  
	default:
	  goto decode_num;
	}
	c = nextc(p);
      }

    decode_num:
      pushback(p, c);
      if (nondigit) {
      trailing_uc:
	yyerror_i(p, "trailing `%c' in number", nondigit);
      }
      tokfix(p);
      if (is_float) {
	(void)strtod(tok(p), 0); /* just check if float is within range */
	if (errno == ERANGE) {
	  yywarning_s(p, "float %s out of range", tok(p));
	  errno = 0;
	}
	yylval.nd = new_float(p, tok(p));
	return tFLOAT;
      }
      yylval.nd = new_int(p, tok(p), 10);
      return tINTEGER;
    }

  case ')':
  case ']':
    p->paren_nest--;
  case '}':
    COND_LEXPOP();
    CMDARG_LEXPOP();
    if (c == ')')
      p->lstate = EXPR_ENDFN;
    else
      p->lstate = EXPR_ENDARG;
    return c;

  case ':':
    c = nextc(p);
    if (c == ':') {
      if (IS_BEG() || p->lstate == EXPR_CLASS || IS_SPCARG(-1)) {
	p->lstate = EXPR_BEG;
	return tCOLON3;
      }
      p->lstate = EXPR_DOT;
      return tCOLON2;
    }
    if (IS_END() || ISSPACE(c)) {
      pushback(p, c);
      p->lstate = EXPR_BEG;
      return ':';
    }
    pushback(p, c);
    p->lstate = EXPR_FNAME;
    return tSYMBEG;

  case '/':
    if (IS_BEG()) {
#if 0
      p->lex_strterm = new_strterm(p, str_regexp, '/', 0);
#endif
      return tREGEXP_BEG;
    }
    if ((c = nextc(p)) == '=') {
      yylval.id = intern("/");
      p->lstate = EXPR_BEG;
      return tOP_ASGN;
    }
    pushback(p, c);
    if (IS_SPCARG(c)) {
      arg_ambiguous(p);
#if 0
      p->lex_strterm = new_strterm(p, str_regexp, '/', 0);
#endif
      return tREGEXP_BEG;
    }
    switch (p->lstate) {
    case EXPR_FNAME: case EXPR_DOT:
      p->lstate = EXPR_ARG; break;
    default:
      p->lstate = EXPR_BEG; break;
    }
    return '/';

  case '^':
    if ((c = nextc(p)) == '=') {
      yylval.id = intern("^");
      p->lstate = EXPR_BEG;
      return tOP_ASGN;
    }
    switch (p->lstate) {
    case EXPR_FNAME: case EXPR_DOT:
      p->lstate = EXPR_ARG; break;
    default:
      p->lstate = EXPR_BEG; break;
    }
    pushback(p, c);
    return '^';

  case ';':
    p->lstate = EXPR_BEG;
    return ';';
    
  case ',':
    p->lstate = EXPR_BEG;
    return ',';

  case '~':
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      if ((c = nextc(p)) != '@') {
	pushback(p, c);
      }
      p->lstate = EXPR_ARG;
    }
    else {
      p->lstate = EXPR_BEG;
    }
    return '~';

  case '(':
    if (IS_BEG()) {
      c = tLPAREN;
    }
    else if (IS_SPCARG(-1)) {
      c = tLPAREN_ARG;
    }
    p->paren_nest++;
    COND_PUSH(0);
    CMDARG_PUSH(0);
    p->lstate = EXPR_BEG;
    return c;

  case '[':
    p->paren_nest++;
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
      if ((c = nextc(p)) == ']') {
	if ((c = nextc(p)) == '=') {
	  return tASET;
	}
	pushback(p, c);
	return tAREF;
      }
      pushback(p, c);
      return '[';
    }
    else if (IS_BEG()) {
      c = tLBRACK;
    }
    else if (IS_ARG() && space_seen) {
      c = tLBRACK;
    }
    p->lstate = EXPR_BEG;
    COND_PUSH(0);
    CMDARG_PUSH(0);
    return c;

  case '{':
    if (p->lpar_beg && p->lpar_beg == p->paren_nest) {
      p->lstate = EXPR_BEG;
      p->lpar_beg = 0;
      p->paren_nest--;
      COND_PUSH(0);
      CMDARG_PUSH(0);
      return tLAMBEG;
    }
    if (IS_ARG() || p->lstate == EXPR_END || p->lstate == EXPR_ENDFN)
      c = '{';          /* block (primary) */
    else if (p->lstate == EXPR_ENDARG)
      c = tLBRACE_ARG;  /* block (expr) */
    else
      c = tLBRACE;      /* hash */
    COND_PUSH(0);
    CMDARG_PUSH(0);
    p->lstate = EXPR_BEG;
    return c;

  case '\\':
    c = nextc(p);
    if (c == '\n') {
      p->lineno++;
      p->column = 0;
      space_seen = 1;
      goto retry; /* skip \\n */
    }
    pushback(p, c);
    return '\\';

  case '%':
    if (IS_BEG()) {
      int term;
#if 0
      int paren;
#endif

      c = nextc(p);
    quotation:
      if (c == -1 || !ISALNUM(c)) {
	term = c;
	c = 'Q';
      }
      else {
	term = nextc(p);
	if (isalnum(term)) {
	  yyerror(p, "unknown type of %string");
	  return 0;
	}
      }
      if (c == -1 || term == -1) {
	yyerror(p, "unterminated quoted string meets end of file");
	return 0;
      }
#if 0
      paren = term;
#endif
      if (term == '(') term = ')';
      else if (term == '[') term = ']';
      else if (term == '{') term = '}';
      else if (term == '<') term = '>';
      p->sterm = term;
#if 0
      else paren = 0;
#endif

      switch (c) {
      case 'Q':
#if 0
	p->lex_strterm = new_strterm(p, str_dquote, term, paren);
#endif
	return tSTRING_BEG;

      case 'q':
#if 0
	p->lex_strterm = new_strterm(p, str_squote, term, paren);
#endif
	return tSTRING_BEG;

      case 'W':
#if 0
	p->lex_strterm = new_strterm(p, str_dword, term, paren);
#endif
	do {c = nextc(p);} while (isspace(c));
	pushback(p, c);
	return tWORDS_BEG;

      case 'w':
#if 0
	p->lex_strterm = new_strterm(p, str_sword, term, paren);
#endif
	do {c = nextc(p);} while (isspace(c));
	pushback(p, c);
	return tQWORDS_BEG;

      case 'r':
#if 0
	p->lex_strterm = new_strterm(p, str_regexp, term, paren);
#endif
	return tREGEXP_BEG;

      case 's':
#if 0
	p->lex_strterm = new_strterm(p, str_ssym, term, paren);
#endif
	p->lstate = EXPR_FNAME;
	return tSYMBEG;

      default:
	yyerror(p, "unknown type of %string");
	return 0;
      }
    }
    if ((c = nextc(p)) == '=') {
      yylval.id = intern("%");
      p->lstate = EXPR_BEG;
      return tOP_ASGN;
    }
    if (IS_SPCARG(c)) {
      goto quotation;
    }
    switch (p->lstate) {
    case EXPR_FNAME: case EXPR_DOT:
      p->lstate = EXPR_ARG; break;
    default:
      p->lstate = EXPR_BEG; break;
    }
    pushback(p, c);
    return '%';

  case '$':
    p->lstate = EXPR_END;
    token_column = newtok(p);
    c = nextc(p);
    switch (c) {
    case '_':		     /* $_: last read line string */
      c = nextc(p);
      pushback(p, c);
      c = '_';
      /* fall through */
    case '~':		   /* $~: match-data */
    case '*':		   /* $*: argv */
    case '$':		   /* $$: pid */
    case '?':		   /* $?: last status */
    case '!':		   /* $!: error string */
    case '@':		   /* $@: error position */
    case '/':		   /* $/: input record separator */
    case '\\':		   /* $\: output record separator */
    case ';':		   /* $;: field separator */
    case ',':		   /* $,: output field separator */
    case '.':		   /* $.: last read line number */
    case '=':		   /* $=: ignorecase */
    case ':':		   /* $:: load path */
    case '<':		   /* $<: reading filename */
    case '>':		   /* $>: default output handle */
    case '\"':		   /* $": already loaded files */
      tokadd(p, '$');
      tokadd(p, c);
      tokfix(p);
      yylval.id = intern(tok(p));
      return tGVAR;

    case '-':
      tokadd(p, '$');
      tokadd(p, c);
      c = nextc(p);
      pushback(p, c);
    gvar:
      tokfix(p);
      yylval.id = intern(tok(p));
      return tGVAR;

    case '&':		/* $&: last match */
    case '`':		/* $`: string before last match */
    case '\'':		/* $': string after last match */
    case '+':		/* $+: string matches last paren. */
      if (last_state == EXPR_FNAME) {
	tokadd(p, '$');
	tokadd(p, c);
	goto gvar;
      }
      yylval.nd = new_back_ref(p, c);
      return tBACK_REF;

    case '1': case '2': case '3':
    case '4': case '5': case '6':
    case '7': case '8': case '9':
      do {
	tokadd(p, c);
	c = nextc(p);
      } while (c != -1 && isdigit(c));
      pushback(p, c);
      if (last_state == EXPR_FNAME) goto gvar;
      tokfix(p);
      yylval.nd = new_nth_ref(p, atoi(tok(p))); 
      return tNTH_REF;

    default:
      if (!identchar(c)) {
	pushback(p,  c);
	return '$';
      }
    case '0':
      tokadd(p, '$');
    }
    break;

  case '@':
    c = nextc(p);
    token_column = newtok(p);
    tokadd(p, '@');
    if (c == '@') {
      tokadd(p, '@');
      c = nextc(p);
    }
    if (c != -1 && isdigit(c)) {
      if (p->bidx == 1) {
	yyerror_i(p, "`@%c' is not allowed as an instance variable name", c);
      }
      else {
	yyerror_i(p, "`@@%c' is not allowed as a class variable name", c);
      }
      return 0;
    }
    if (!identchar(c)) {
      pushback(p, c);
      return '@';
    }
    break;

  case '_':
    token_column = newtok(p);
    break;

  default:
    if (!identchar(c)) {
      yyerror_i(p,  "Invalid char `\\x%02X' in expression", c);
      goto retry;
    }

    token_column = newtok(p);
    break;
  }

  do {
    tokadd(p, c);
    c = nextc(p);
    if (c < 0) break;
  } while (identchar(c));
  if (token_column == 0 && toklen(p) == 7 && (c < 0 || c == '\n') &&
      strncmp(tok(p), "__END__", toklen(p)) == 0)
    return -1;

  switch (tok(p)[0]) {
  case '@': case '$':
    pushback(p, c);
    break;
  default:
    if ((c == '!' || c == '?') && !peek(p, '=')) {
      tokadd(p, c);
    }
    else {
      pushback(p, c);
    }
  }
  tokfix(p);
  {
    int result = 0;

    last_state = p->lstate;
    switch (tok(p)[0]) {
    case '$':
      p->lstate = EXPR_END;
      result = tGVAR;
      break;
    case '@':
      p->lstate = EXPR_END;
      if (tok(p)[1] == '@')
	result = tCVAR;
      else
	result = tIVAR;
      break;

    default:
      if (toklast(p) == '!' || toklast(p) == '?') {
	result = tFID;
      }
      else {
	if (p->lstate == EXPR_FNAME) {
	  if ((c = nextc(p)) == '=' && !peek(p, '~') && !peek(p, '>') &&
	      (!peek(p, '=') || (peek_n(p, '>', 1)))) {
	    result = tIDENTIFIER;
	    tokadd(p, c);
	    tokfix(p);
	  }
	  else {
	    pushback(p, c);
	  }
	}
	if (result == 0 && isupper((int)tok(p)[0])) {
	  result = tCONSTANT;
	}
	else {
	  result = tIDENTIFIER;
	}
      }

      if (IS_LABEL_POSSIBLE()) {
	if (IS_LABEL_SUFFIX(0)) {
	  p->lstate = EXPR_BEG;
	  nextc(p);
	  tokfix(p);
	  yylval.id = intern(tok(p));
	  return tLABEL;
	}
      }
      if (p->lstate != EXPR_DOT) {
	const struct kwtable *kw;

	/* See if it is a reserved word.  */
	kw = mrb_reserved_word(tok(p), toklen(p));
	if (kw) {
	  enum mrb_lex_state_enum state = p->lstate;
	  p->lstate = kw->state;
	  if (state == EXPR_FNAME) {
	    yylval.id = intern(kw->name);
	    return kw->id[0];
	  }
	  if (p->lstate == EXPR_BEG) {
	    p->cmd_start = TRUE;
	  }
	  if (kw->id[0] == keyword_do) {
	    if (p->lpar_beg && p->lpar_beg == p->paren_nest) {
	      p->lpar_beg = 0;
	      p->paren_nest--;
	      return keyword_do_LAMBDA;
	    }
	    if (COND_P()) return keyword_do_cond;
	    if (CMDARG_P() && state != EXPR_CMDARG)
	      return keyword_do_block;
	    if (state == EXPR_ENDARG || state == EXPR_BEG)
	      return keyword_do_block;
	    return keyword_do;
	  }
	  if (state == EXPR_BEG || state == EXPR_VALUE)
	    return kw->id[0];
	  else {
	    if (kw->id[0] != kw->id[1])
	      p->lstate = EXPR_BEG;
	    return kw->id[1];
	  }
	}
      }

      if (IS_BEG() ||
	  p->lstate == EXPR_DOT ||
	  IS_ARG()) {
	if (cmd_state) {
	  p->lstate = EXPR_CMDARG;
	}
	else {
	  p->lstate = EXPR_ARG;
	}
      }
      else if (p->lstate == EXPR_FNAME) {
	p->lstate = EXPR_ENDFN;
      }
      else {
	p->lstate = EXPR_END;
      }
    }
    {
      mrb_sym ident = intern(tok(p));

      yylval.id = ident;
#if 0
      if (last_state != EXPR_DOT && islower(tok(p)[0]) && lvar_defined(ident)) {
	p->lstate = EXPR_END;
      }
#endif
    }
    return result;
  }
}

static int
yylex(void *lval, parser_state *p)
{
    int t;

    p->ylval = lval;
    t = parser_yylex(p);

    return t;
}

void
mrb_parser_parse(parser_state *p)
{
  node *tree;

  if (setjmp(p->jmp) != 0) {
    yyerror(p, "memory allocation error");
    p->nerr++;
    p->tree = p->begin_tree = 0;
    return;
  }

  p->cmd_start = TRUE;
  p->in_def = p->in_single = FALSE;
  p->nerr = p->nwarn = 0;
  p->sterm = 0;

  yyparse(p);
  tree = p->tree;
  if (!tree) {
    if (p->begin_tree) {
      tree = p->begin_tree;
    }
    else {
      tree = new_nil(p);
    }
  }
  else {
    if ((intptr_t)tree->car == NODE_SCOPE) {
      p->locals = cons(tree->cdr->car, 0);
    }
    if (p->begin_tree) {
      tree = new_begin(p, p->begin_tree);
      append(tree, p->tree);
    }
  }
}

parser_state*
mrb_parser_new(mrb_state *mrb)
{
  mrb_pool *pool;
  parser_state *p;

  pool = mrb_pool_open(mrb);
  if (!pool) return 0;
  p = mrb_pool_alloc(pool, sizeof(parser_state));
  if (!p) return 0;

  memset(p, 0, sizeof(parser_state));
  p->mrb = mrb;
  p->pool = pool;
  p->in_def = p->in_single = 0;

  p->s = p->send = NULL;
  p->f = NULL;

  p->cmd_start = TRUE;
  p->in_def = p->in_single = FALSE;

  p->capture_errors = 0;

  p->lineno = 1;
  p->column = 0;
#if defined(PARSER_TEST) || defined(PARSER_DEBUG)
  yydebug = 1;
#endif

  return p;
}

const char*
mrb_parser_filename(parser_state *p, const char *s)
{
  if (s) {
    p->filename = strdup(s);
  }
  return p->filename;
}

int
mrb_parser_lineno(struct mrb_parser_state *p, int n)
{
  if (n <= 0) {
    return p->lineno;
  }
  p->column = 0;
  p->lineno = n;
  return n;
}

parser_state*
mrb_parse_file(mrb_state *mrb, FILE *f)
{
  parser_state *p;
 
  p = mrb_parser_new(mrb);
  if (!p) return 0;
  p->s = p->send = NULL;
  p->f = f;

  mrb_parser_parse(p);
  return p;
}

parser_state*
mrb_parse_nstring(mrb_state *mrb, const char *s, size_t len)
{
  parser_state *p;

  p = mrb_parser_new(mrb);
  if (!p) return 0;
  p->s = s;
  p->send = s + len;

  mrb_parser_parse(p);
  return p;
}

parser_state*
mrb_parse_string(mrb_state *mrb, const char *s)
{
  return mrb_parse_nstring(mrb, s, strlen(s));
}

void parser_dump(mrb_state *mrb, node *tree, int offset);

int
mrb_compile_file(mrb_state * mrb, FILE *f)
{
  parser_state *p;
  int n;

  p = mrb_parse_file(mrb, f);
  if (!p) return -1;
  if (!p->tree) return -1;
  if (p->nerr) return -1;
  n = mrb_generate_code(mrb, p->tree);
  mrb_pool_close(p->pool);

  return n;
}

int
mrb_compile_nstring(mrb_state *mrb, char *s, size_t len)
{
  parser_state *p;
  int n;

  p = mrb_parse_nstring(mrb, s, len);
  if (!p) return -1;
  if (!p->tree) return -1;
  if (p->nerr) return -1;
  n = mrb_generate_code(mrb, p->tree);
  mrb_pool_close(p->pool);

  return n;
}

int
mrb_compile_string(mrb_state *mrb, char *s)
{
  return mrb_compile_nstring(mrb, s, strlen(s));
}

static void
dump_prefix(int offset)
{
  while (offset--) {
    putc(' ', stdout);
    putc(' ', stdout);
  }
}

static void
dump_recur(mrb_state *mrb, node *tree, int offset)
{
  while (tree) {
    parser_dump(mrb, tree->car, offset);
    tree = tree->cdr;
  }
}

void
parser_dump(mrb_state *mrb, node *tree, int offset)
{
  int n;

  if (!tree) return;
 again:
  dump_prefix(offset);
  n = (int)(intptr_t)tree->car;
  tree = tree->cdr;
  switch (n) {
  case NODE_BEGIN:
    printf("NODE_BEGIN:\n");
    dump_recur(mrb, tree, offset+1);
    break;

  case NODE_RESCUE:
    printf("NODE_RESCUE:\n");
    if (tree->car) {
      dump_prefix(offset+1);
      printf("body:\n");
      parser_dump(mrb, tree->car, offset+2);
    }
    tree = tree->cdr;
    if (tree->car) {
      node *n2 = tree->car;

      dump_prefix(offset+1);
      printf("rescue:\n");
      while (n2) {
	node *n3 = n2->car;
	if (n3->car) {
	  dump_prefix(offset+2);
	  printf("handle classes:\n");
	  dump_recur(mrb, n3->car, offset+3);
	}
	if (n3->cdr->car) {
	  dump_prefix(offset+2);
	  printf("exc_var:\n");
	  parser_dump(mrb, n3->cdr->car, offset+3);
	}
	if (n3->cdr->cdr->car) {
	  dump_prefix(offset+2);
	  printf("rescue body:\n");
	  parser_dump(mrb, n3->cdr->cdr->car, offset+3);
	}
	n2 = n2->cdr;
      }
    }
    tree = tree->cdr;
    if (tree->car) {
      dump_prefix(offset+1);
      printf("else:\n");
      parser_dump(mrb, tree->car, offset+2);
    }
    break;

  case NODE_ENSURE:
    printf("NODE_ENSURE:\n");
    dump_prefix(offset+1);
    printf("body:\n");
    parser_dump(mrb, tree->car, offset+2);
    dump_prefix(offset+1);
    printf("ensure:\n");
    parser_dump(mrb, tree->cdr->cdr, offset+2);
    break;

  case NODE_LAMBDA:
    printf("NODE_BLOCK:\n");
    goto block;

  case NODE_BLOCK:
  block:
    printf("NODE_BLOCK:\n");
    tree = tree->cdr;
    if (tree->car) {
      node *n = tree->car;

      if (n->car) {
	dump_prefix(offset+1);
	printf("mandatory args:\n");
	dump_recur(mrb, n->car, offset+2);
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("optional args:\n");
	{
	  node *n2 = n->car;

	  while (n2) {
	    dump_prefix(offset+2);
	    printf("%s=", mrb_sym2name(mrb, (mrb_sym)n2->car->car));
	    parser_dump(mrb, n2->car->cdr, 0);
	    n2 = n2->cdr;
	  }
	}
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("rest=*%s\n", mrb_sym2name(mrb, (mrb_sym)n->car));
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("post mandatory args:\n");
	dump_recur(mrb, n->car, offset+2);
      }
      n = n->cdr;
      if (n) {
	dump_prefix(offset+1);
	printf("blk=&%s\n", mrb_sym2name(mrb, (mrb_sym)n));
      }
    }
    dump_prefix(offset+1);
    printf("body:\n");
    parser_dump(mrb, tree->cdr->car, offset+2);
    break;

  case NODE_IF:
    printf("NODE_IF:\n");
    dump_prefix(offset+1);
    printf("cond:\n");
    parser_dump(mrb, tree->car, offset+2);
    dump_prefix(offset+1);
    printf("then:\n");
    parser_dump(mrb, tree->cdr->car, offset+2);
    if (tree->cdr->cdr->car) {
      dump_prefix(offset+1);
      printf("else:\n");
      parser_dump(mrb, tree->cdr->cdr->car, offset+2);
    }
    break;

  case NODE_AND:
    printf("NODE_AND:\n");
    parser_dump(mrb, tree->car, offset+1);
    parser_dump(mrb, tree->cdr, offset+1);
    break;

  case NODE_OR:
    printf("NODE_OR:\n");
    parser_dump(mrb, tree->car, offset+1);
    parser_dump(mrb, tree->cdr, offset+1);
    break;

  case NODE_CASE:
    printf("NODE_CASE:\n");
    if (tree->car) {
      parser_dump(mrb, tree->car, offset+1);
    }
    tree = tree->cdr;
    while (tree) {
      dump_prefix(offset+1);
      printf("case:\n");
      dump_recur(mrb, tree->car->car, offset+2);
      dump_prefix(offset+1);
      printf("body:\n");
      parser_dump(mrb, tree->car->cdr, offset+2);
      tree = tree->cdr;
    }
    break;

  case NODE_WHILE:
    printf("NODE_WHILE:\n");
    dump_prefix(offset+1);
    printf("cond:\n");
    parser_dump(mrb, tree->car, offset+2);
    dump_prefix(offset+1);
    printf("body:\n");
    parser_dump(mrb, tree->cdr, offset+2);
    break;

  case NODE_UNTIL:
    printf("NODE_UNTIL:\n");
    dump_prefix(offset+1);
    printf("cond:\n");
    parser_dump(mrb, tree->car, offset+2);
    dump_prefix(offset+1);
    printf("body:\n");
    parser_dump(mrb, tree->cdr, offset+2);
    break;

  case NODE_FOR:
    printf("NODE_FOR:\n");
    dump_prefix(offset+1);
    printf("var:\n");
    {
      node *n2 = tree->car;

      if (n2->car) {
	dump_prefix(offset+2);
	printf("pre:\n");
	dump_recur(mrb, n2->car, offset+3);
      }
      n2 = n2->cdr;
      if (n2) {
	if (n2->car) {
	  dump_prefix(offset+2);
	  printf("rest:\n");
	  parser_dump(mrb, n2->car, offset+3);
	}
	n2 = n2->cdr;
	if (n2) {
	  if (n2->car) {
	    dump_prefix(offset+2);
	    printf("post:\n");
	    dump_recur(mrb, n2->car, offset+3);
	  }
	}
      }
    }
    tree = tree->cdr;
    dump_prefix(offset+1);
    printf("in:\n");
    parser_dump(mrb, tree->car, offset+2);
    tree = tree->cdr;
    dump_prefix(offset+1);
    printf("do:\n");
    parser_dump(mrb, tree->car, offset+2);
    break;

  case NODE_SCOPE:
    printf("NODE_SCOPE:\n");
    {
      node *n2 = tree->car;

      if (n2  && (n2->car || n2->cdr)) {
	dump_prefix(offset+1);
	printf("local variables:\n");
	while (n2) {
	  if (n2->car) {
	    dump_prefix(offset+2);
	    printf("%s ", mrb_sym2name(mrb, (mrb_sym)n2->car));
	  }
	  n2 = n2->cdr;
	}
	printf("\n");
      }
    }
    tree = tree->cdr;
    offset++;
    goto again;

  case NODE_FCALL:
  case NODE_CALL:
    printf("NODE_CALL:\n");
    parser_dump(mrb, tree->car, offset+1);
    dump_prefix(offset+1);
    printf("method='%s' (%d)\n", 
	   mrb_sym2name(mrb, (mrb_sym)tree->cdr->car),
	   (int)(intptr_t)tree->cdr->car);
    tree = tree->cdr->cdr->car;
    if (tree) {
      dump_prefix(offset+1);
      printf("args:\n");
      dump_recur(mrb, tree->car, offset+2);
      if (tree->cdr) {
	dump_prefix(offset+1);
	printf("block:\n");
	parser_dump(mrb, tree->cdr, offset+2);
      }
    }
    break;

  case NODE_DOT2:
    printf("NODE_DOT2:\n");
    parser_dump(mrb, tree->car, offset+1);
    parser_dump(mrb, tree->cdr, offset+1);
    break;

  case NODE_DOT3:
    printf("NODE_DOT3:\n");
    parser_dump(mrb, tree->car, offset+1);
    parser_dump(mrb, tree->cdr, offset+1);
    break;

  case NODE_COLON2:
    printf("NODE_COLON2:\n");
    parser_dump(mrb, tree->car, offset+1);
    dump_prefix(offset+1);
    printf("::%s\n", mrb_sym2name(mrb, (mrb_sym)tree->cdr));
    break;

  case NODE_COLON3:
    printf("NODE_COLON3:\n");
    dump_prefix(offset+1);
    printf("::%s\n", mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_ARRAY:
    printf("NODE_ARRAY:\n");
    dump_recur(mrb, tree, offset+1);
    break;

  case NODE_HASH:
    printf("NODE_HASH:\n");
    while (tree) {
      dump_prefix(offset+1);
      printf("key:\n");
      parser_dump(mrb, tree->car->car, offset+2);
      dump_prefix(offset+1);
      printf("value:\n");
      parser_dump(mrb, tree->car->cdr, offset+2);
      tree = tree->cdr;
    }
    break;

  case NODE_SPLAT:
    printf("NODE_SPLAT:\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_ASGN:
    printf("NODE_ASGN:\n");
    dump_prefix(offset+1);
    printf("lhs:\n");
    parser_dump(mrb, tree->car, offset+2);
    dump_prefix(offset+1);
    printf("rhs:\n");
    parser_dump(mrb, tree->cdr, offset+2);
    break;

  case NODE_MASGN:
    printf("NODE_MASGN:\n");
    dump_prefix(offset+1);
    printf("mlhs:\n");
    {
      node *n2 = tree->car;

      if (n2->car) {
	dump_prefix(offset+2);
	printf("pre:\n");
	dump_recur(mrb, n2->car, offset+3);
      }
      n2 = n2->cdr;
      if (n2) {
	if (n2->car) {
	  dump_prefix(offset+2);
	  printf("rest:\n");
          if (n2->car == (node*)-1) {
	    dump_prefix(offset+2);
	    printf("(empty)\n");
	  }
          else {
	    parser_dump(mrb, n2->car, offset+3);
	  }
	}
	n2 = n2->cdr;
	if (n2) {
	  if (n2->car) {
	    dump_prefix(offset+2);
	    printf("post:\n");
	    dump_recur(mrb, n2->car, offset+3);
	  }
	}
      }
    }
    dump_prefix(offset+1);
    printf("rhs:\n");
    parser_dump(mrb, tree->cdr, offset+2);
    break;

  case NODE_OP_ASGN:
    printf("NODE_OP_ASGN:\n");
    dump_prefix(offset+1);
    printf("lhs:\n");
    parser_dump(mrb, tree->car, offset+2);
    tree = tree->cdr;
    dump_prefix(offset+1);
    printf("op='%s' (%d)\n", mrb_sym2name(mrb, (mrb_sym)tree->car), (int)(intptr_t)tree->car);
    tree = tree->cdr;
    parser_dump(mrb, tree->car, offset+1);
    break;

  case NODE_SUPER:
    printf("NODE_SUPER:\n");
    if (tree) {
      dump_prefix(offset+1);
      printf("args:\n");
      dump_recur(mrb, tree->car, offset+2);
      if (tree->cdr) {
	dump_prefix(offset+1);
	printf("block:\n");
	parser_dump(mrb, tree->cdr, offset+2);
      }
    }
    break;

  case NODE_ZSUPER:
    printf("NODE_ZSUPER\n");
    break;

  case NODE_RETURN:
    printf("NODE_RETURN:\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_YIELD:
    printf("NODE_YIELD:\n");
    dump_recur(mrb, tree, offset+1);
    break;

  case NODE_BREAK:
    printf("NODE_BREAK:\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_NEXT:
    printf("NODE_NEXT:\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_REDO:
    printf("NODE_REDO\n");
    break;

  case NODE_RETRY:
    printf("NODE_RETRY\n");
    break;

  case NODE_LVAR:
    printf("NODE_LVAR %s\n", mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_GVAR:
    printf("NODE_GVAR %s\n", mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_IVAR:
    printf("NODE_IVAR %s\n", mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_CVAR:
    printf("NODE_CVAR %s\n", mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_CONST:
    printf("NODE_CONST %s\n", mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_BACK_REF:
    printf("NODE_BACK_REF: $%c\n", (int)(intptr_t)tree);
    break;

  case NODE_NTH_REF:
    printf("NODE_NTH_REF: $%d\n", (int)(intptr_t)tree);
    break;

  case NODE_ARG:
    printf("NODE_ARG %s\n", mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_BLOCK_ARG:
    printf("NODE_BLOCK_ARG:\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_INT:
    printf("NODE_INT %s base %d\n", (char*)tree->car, (int)(intptr_t)tree->cdr->car);
    break;

  case NODE_FLOAT:
    printf("NODE_FLOAT %s\n", (char*)tree);
    break;

  case NODE_NEGATE:
    printf("NODE_NEGATE\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_STR:
    printf("NODE_STR \"%s\" len %d\n", (char*)tree->car, (int)(intptr_t)tree->cdr);
    break;

  case NODE_DSTR:
    printf("NODE_DSTR\n");
    dump_recur(mrb, tree, offset+1);
    break;

  case NODE_SYM:
    printf("NODE_SYM :%s\n", mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_SELF:
    printf("NODE_SELF\n");
    break;

  case NODE_NIL:
    printf("NODE_NIL\n");
    break;

  case NODE_TRUE:
    printf("NODE_TRUE\n");
    break;

  case NODE_FALSE:
    printf("NODE_FALSE\n");
    break;

  case NODE_ALIAS:
    printf("NODE_ALIAS %s %s:\n",
	   mrb_sym2name(mrb, (mrb_sym)tree->car),
	   mrb_sym2name(mrb, (mrb_sym)tree->cdr));
    break;

  case NODE_UNDEF:
    printf("NODE_UNDEF %s:\n",
	   mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_CLASS:
    printf("NODE_CLASS:\n");
    if (tree->car->car == (node*)0) {
      dump_prefix(offset+1);
      printf(":%s\n", mrb_sym2name(mrb, (mrb_sym)tree->car->cdr));
    }
    else if (tree->car->car == (node*)1) {
      dump_prefix(offset+1);
      printf("::%s\n", mrb_sym2name(mrb, (mrb_sym)tree->car->cdr));
    }
    else {
      parser_dump(mrb, tree->car->car, offset+1);
      dump_prefix(offset+1);
      printf("::%s\n", mrb_sym2name(mrb, (mrb_sym)tree->car->cdr));
    }
    if (tree->cdr->car) {
      dump_prefix(offset+1);
      printf("super:\n");
      parser_dump(mrb, tree->cdr->car, offset+2);
    }
    dump_prefix(offset+1);
    printf("body:\n");
    parser_dump(mrb, tree->cdr->cdr->car->cdr, offset+2);
    break;

  case NODE_MODULE:
    printf("NODE_MODULE:\n");
    if (tree->car->car == (node*)0) {
      dump_prefix(offset+1);
      printf(":%s\n", mrb_sym2name(mrb, (mrb_sym)tree->car->cdr));
    }
    else if (tree->car->car == (node*)1) {
      dump_prefix(offset+1);
      printf("::%s\n", mrb_sym2name(mrb, (mrb_sym)tree->car->cdr));
    }
    else {
      parser_dump(mrb, tree->car->car, offset+1);
      dump_prefix(offset+1);
      printf("::%s\n", mrb_sym2name(mrb, (mrb_sym)tree->car->cdr));
    }
    dump_prefix(offset+1);
    printf("body:\n");
    parser_dump(mrb, tree->cdr->car->cdr, offset+2);
    break;

  case NODE_SCLASS:
    printf("NODE_SCLASS:\n");
    parser_dump(mrb, tree->car, offset+1);
    dump_prefix(offset+1);
    printf("body:\n");
    parser_dump(mrb, tree->cdr->car->cdr, offset+2);
    break;

  case NODE_DEF:
    printf("NODE_DEF:\n");
    dump_prefix(offset+1);
    printf("%s\n", mrb_sym2name(mrb, (mrb_sym)tree->car));
    tree = tree->cdr;
    {
      node *n2 = tree->car;

      if (n2 && (n2->car || n2->cdr)) {
	dump_prefix(offset+1);
	printf("local variables:\n");

	while (n2) {
	  if (n2->car) {
	    dump_prefix(offset+2);
	    printf("%s ", mrb_sym2name(mrb, (mrb_sym)n2->car));
	  }
	  n2 = n2->cdr;
	}
	printf("\n");
      }
    }
    tree = tree->cdr;
    if (tree->car) {
      node *n = tree->car;

      if (n->car) {
	dump_prefix(offset+1);
	printf("mandatory args:\n");
	dump_recur(mrb, n->car, offset+2);
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("optional args:\n");
	{
	  node *n2 = n->car;

	  while (n2) {
	    dump_prefix(offset+2);
	    printf("%s=", mrb_sym2name(mrb, (mrb_sym)n2->car->car));
	    parser_dump(mrb, n2->car->cdr, 0);
	    n2 = n2->cdr;
	  }
	}
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("rest=*%s\n", mrb_sym2name(mrb, (mrb_sym)n->car));
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("post mandatory args:\n");
	dump_recur(mrb, n->car, offset+2);
      }
      n = n->cdr;
      if (n) {
	dump_prefix(offset+1);
	printf("blk=&%s\n", mrb_sym2name(mrb, (mrb_sym)n));
      }
    }
    parser_dump(mrb, tree->cdr->car, offset+1);
    break;

  case NODE_SDEF:
    printf("NODE_SDEF:\n");
    parser_dump(mrb, tree->car, offset+1);
    tree = tree->cdr;
    dump_prefix(offset+1);
    printf(":%s\n", mrb_sym2name(mrb, (mrb_sym)tree->car));
    tree = tree->cdr->cdr;
    if (tree->car) {
      node *n = tree->car;

      if (n->car) {
	dump_prefix(offset+1);
	printf("mandatory args:\n");
	dump_recur(mrb, n->car, offset+2);
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("optional args:\n");
	{
	  node *n2 = n->car;

	  while (n2) {
	    dump_prefix(offset+2);
	    printf("%s=", mrb_sym2name(mrb, (mrb_sym)n2->car->car));
	    parser_dump(mrb, n2->car->cdr, 0);
	    n2 = n2->cdr;
	  }
	}
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("rest=*%s\n", mrb_sym2name(mrb, (mrb_sym)n->car));
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("post mandatory args:\n");
	dump_recur(mrb, n->car, offset+2);
      }
      n = n->cdr;
      if (n) {
	dump_prefix(offset+1);
	printf("blk=&%s\n", mrb_sym2name(mrb, (mrb_sym)n));
      }
    }
    tree = tree->cdr;
    parser_dump(mrb, tree->car, offset+1);
    break;

  case NODE_POSTEXE:
    printf("NODE_POSTEXE:\n");
    parser_dump(mrb, tree, offset+1);
    break;

  default:
    printf("node type: %d (0x%x)\n", (int)n, (int)n);
    break;
  }
  return;
}

#ifdef PARSER_TEST
int
main()
{
  mrb_state *mrb = mrb_open();
  int n;

  n = mrb_compile_string(mrb, "\
def fib(n)\n\
  if n<2\n\
    n\n\
  else\n\
    fib(n-2)+fib(n-1)\n\
  end\n\
end\n\
print(fib(20), \"\\n\")\n\
");
  printf("ret: %d\n", n);

  return 0;
}
#endif

