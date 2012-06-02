/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
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
#define YYBISON_VERSION "2.5"

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

/* Line 268 of yacc.c  */
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

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

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
  node *n = a->cdr->cdr->cdr;

  if (!n->car) n->car = cons(0, b);
  else {
    args_with_block(p, n->car, b);
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



/* Line 268 of yacc.c  */
#line 887 "./y.tab.c"

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

/* Line 301 of yacc.c  */
#line 826 "./parse.y"

    node *nd;
    mrb_sym id;
    int num;
    unsigned int stack;
    const struct vtable *vars;



/* Line 301 of yacc.c  */
#line 1051 "./y.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 1063 "./y.tab.c"

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
# if defined YYENABLE_NLS && YYENABLE_NLS
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
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

# define YYCOPY_NEEDED 1

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

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
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
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   10209

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  144
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  145
/* YYNRULES -- Number of rules.  */
#define YYNRULES  518
/* YYNRULES -- Number of states.  */
#define YYNSTATES  918

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
    1336,  1341,  1342,  1348,  1350,  1353,  1355,  1357,  1359,  1361,
    1363,  1365,  1368,  1371,  1373,  1375,  1377,  1379,  1381,  1383,
    1385,  1387,  1389,  1391,  1393,  1395,  1397,  1399,  1401,  1403,
    1404,  1409,  1412,  1416,  1419,  1426,  1435,  1440,  1447,  1452,
    1459,  1462,  1467,  1474,  1477,  1482,  1485,  1490,  1492,  1493,
    1495,  1497,  1499,  1501,  1503,  1505,  1507,  1511,  1513,  1517,
    1521,  1525,  1527,  1531,  1533,  1537,  1539,  1541,  1544,  1546,
    1548,  1550,  1553,  1556,  1558,  1560,  1561,  1566,  1568,  1571,
    1573,  1577,  1581,  1584,  1586,  1588,  1590,  1592,  1594,  1596,
    1598,  1600,  1602,  1604,  1606,  1608,  1609,  1611,  1612,  1614,
    1617,  1620,  1621,  1623,  1625,  1627,  1629,  1631,  1634
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     145,     0,    -1,    -1,   146,   147,    -1,   148,   281,    -1,
     288,    -1,   149,    -1,   148,   287,   149,    -1,     1,   149,
      -1,   154,    -1,    -1,    46,   150,   134,   147,   135,    -1,
     152,   238,   216,   241,    -1,   153,   281,    -1,   288,    -1,
     154,    -1,   153,   287,   154,    -1,     1,   154,    -1,    -1,
      45,   175,   155,   175,    -1,     6,   177,    -1,   154,    40,
     158,    -1,   154,    41,   158,    -1,   154,    42,   158,    -1,
     154,    43,   158,    -1,   154,    44,   154,    -1,    47,   134,
     152,   135,    -1,   156,    -1,   164,   107,   159,    -1,   252,
      88,   159,    -1,   212,   136,   186,   284,    88,   159,    -1,
     212,   137,    51,    88,   159,    -1,   212,   137,    55,    88,
     159,    -1,   212,    86,    55,    88,   159,    -1,   212,    86,
      51,    88,   159,    -1,   254,    88,   159,    -1,   171,   107,
     193,    -1,   164,   107,   182,    -1,   164,   107,   193,    -1,
     157,    -1,   171,   107,   159,    -1,   171,   107,   156,    -1,
     159,    -1,   157,    37,   157,    -1,   157,    38,   157,    -1,
      39,   282,   157,    -1,   121,   159,    -1,   181,    -1,   157,
      -1,   163,    -1,   160,    -1,   231,    -1,   231,   280,   278,
     188,    -1,    -1,    95,   162,   222,   152,   135,    -1,   277,
     188,    -1,   277,   188,   161,    -1,   212,   137,   278,   188,
      -1,   212,   137,   278,   188,   161,    -1,   212,    86,   278,
     188,    -1,   212,    86,   278,   188,   161,    -1,    32,   188,
      -1,    31,   188,    -1,    30,   187,    -1,    21,   187,    -1,
      22,   187,    -1,   166,    -1,    90,   165,   283,    -1,   166,
      -1,    90,   165,   283,    -1,   168,    -1,   168,   167,    -1,
     168,    96,   170,    -1,   168,    96,   170,   138,   169,    -1,
     168,    96,    -1,   168,    96,   138,   169,    -1,    96,   170,
      -1,    96,   170,   138,   169,    -1,    96,    -1,    96,   138,
     169,    -1,   170,    -1,    90,   165,   283,    -1,   167,   138,
      -1,   168,   167,   138,    -1,   167,    -1,   168,   167,    -1,
     251,    -1,   212,   136,   186,   284,    -1,   212,   137,    51,
      -1,   212,    86,    51,    -1,   212,   137,    55,    -1,   212,
      86,    55,    -1,    87,    55,    -1,   254,    -1,   251,    -1,
     212,   136,   186,   284,    -1,   212,   137,    51,    -1,   212,
      86,    51,    -1,   212,   137,    55,    -1,   212,    86,    55,
      -1,    87,    55,    -1,   254,    -1,    51,    -1,    55,    -1,
      87,   172,    -1,   172,    -1,   212,    86,   172,    -1,    51,
      -1,    55,    -1,    52,    -1,   179,    -1,   180,    -1,   174,
      -1,   248,    -1,   175,    -1,   175,    -1,    -1,   177,   138,
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
     107,   181,    -1,   171,   107,   181,    44,   181,    -1,   252,
      88,   181,    -1,   252,    88,   181,    44,   181,    -1,   212,
     136,   186,   284,    88,   181,    -1,   212,   137,    51,    88,
     181,    -1,   212,   137,    55,    88,   181,    -1,   212,    86,
      51,    88,   181,    -1,   212,    86,    55,    88,   181,    -1,
      87,    55,    88,   181,    -1,   254,    88,   181,    -1,   181,
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
      -1,   181,   108,   181,   282,   109,   181,    -1,   194,    -1,
     181,    -1,   288,    -1,   192,   285,    -1,   192,   138,   275,
     285,    -1,   275,   285,    -1,   139,   186,   283,    -1,   288,
      -1,   184,    -1,   288,    -1,   187,    -1,   192,   138,    -1,
     192,   138,   275,   138,    -1,   275,   138,    -1,   163,    -1,
     192,   191,    -1,   275,   191,    -1,   192,   138,   275,   191,
      -1,   190,    -1,    -1,   189,   187,    -1,    97,   182,    -1,
     138,   190,    -1,   288,    -1,   182,    -1,    96,   182,    -1,
     192,   138,   182,    -1,   192,   138,    96,   182,    -1,   192,
     138,   182,    -1,   192,   138,    96,   182,    -1,    96,   182,
      -1,   242,    -1,   243,    -1,   247,    -1,   253,    -1,   254,
      -1,    52,    -1,    -1,     7,   195,   151,    10,    -1,    -1,
      91,   157,   196,   283,    -1,    -1,    91,   197,   283,    -1,
      90,   152,   140,    -1,   212,    86,    55,    -1,    87,    55,
      -1,    93,   183,   141,    -1,    94,   274,   135,    -1,    30,
      -1,    31,   139,   187,   283,    -1,    31,   139,   283,    -1,
      31,    -1,    39,   139,   157,   283,    -1,    39,   139,   283,
      -1,   277,   233,    -1,   232,    -1,   232,   233,    -1,    -1,
      98,   198,   227,   228,    -1,    11,   158,   213,   152,   215,
      10,    -1,    12,   158,   213,   152,   216,    10,    -1,    -1,
      -1,    18,   199,   158,   214,   200,   152,    10,    -1,    -1,
      -1,    19,   201,   158,   214,   202,   152,    10,    -1,    16,
     158,   281,   236,    10,    -1,    16,   281,   236,    10,    -1,
      -1,    -1,    20,   217,    25,   203,   158,   214,   204,   152,
      10,    -1,    -1,     3,   173,   255,   205,   151,    10,    -1,
      -1,    -1,     3,    84,   157,   206,   286,   207,   151,    10,
      -1,    -1,     4,   173,   208,   151,    10,    -1,    -1,     5,
     174,   209,   257,   151,    10,    -1,    -1,    -1,     5,   272,
     280,   210,   174,   211,   257,   151,    10,    -1,    21,    -1,
      22,    -1,    23,    -1,    24,    -1,   194,    -1,   286,    -1,
      13,    -1,   286,    13,    -1,   286,    -1,    27,    -1,   216,
      -1,    14,   158,   213,   152,   215,    -1,   288,    -1,    15,
     152,    -1,   171,    -1,   164,    -1,   260,    -1,    90,   220,
     283,    -1,   218,    -1,   219,   138,   218,    -1,   219,    -1,
     219,   138,    96,   260,    -1,   219,   138,    96,   260,   138,
     219,    -1,   219,   138,    96,    -1,   219,   138,    96,   138,
     219,    -1,    96,   260,    -1,    96,   260,   138,   219,    -1,
      96,    -1,    96,   138,   219,    -1,   262,   138,   265,   138,
     268,   271,    -1,   262,   138,   265,   138,   268,   138,   262,
     271,    -1,   262,   138,   265,   271,    -1,   262,   138,   265,
     138,   262,   271,    -1,   262,   138,   268,   271,    -1,   262,
     138,    -1,   262,   138,   268,   138,   262,   271,    -1,   262,
     271,    -1,   265,   138,   268,   271,    -1,   265,   138,   268,
     138,   262,   271,    -1,   265,   271,    -1,   265,   138,   262,
     271,    -1,   268,   271,    -1,   268,   138,   262,   271,    -1,
     270,    -1,   288,    -1,   223,    -1,   112,   224,   112,    -1,
      77,    -1,   112,   221,   224,   112,    -1,   282,    -1,   282,
     142,   225,   282,    -1,   226,    -1,   225,   138,   226,    -1,
      51,    -1,   259,    -1,   139,   258,   224,   140,    -1,   258,
      -1,   105,   152,   135,    -1,    29,   152,    10,    -1,    -1,
      28,   230,   222,   152,    10,    -1,   163,   229,    -1,   231,
     280,   278,   185,    -1,   231,   280,   278,   185,   233,    -1,
     231,   280,   278,   188,   229,    -1,   277,   184,    -1,   212,
     137,   278,   185,    -1,   212,    86,   278,   184,    -1,   212,
      86,   279,    -1,   212,   137,   184,    -1,   212,    86,   184,
      -1,    32,   184,    -1,    32,    -1,   212,   136,   186,   284,
      -1,    -1,   134,   234,   222,   152,   135,    -1,    -1,    26,
     235,   222,   152,    10,    -1,    17,   192,   213,   152,   237,
      -1,   216,    -1,   236,    -1,     8,   239,   240,   213,   152,
     238,    -1,   288,    -1,   182,    -1,   193,    -1,   288,    -1,
      89,   171,    -1,   288,    -1,     9,   152,    -1,   288,    -1,
     250,    -1,   248,    -1,    60,    -1,    62,    -1,   103,    62,
      -1,   103,   244,    62,    -1,    -1,    63,   245,   152,   135,
      -1,    -1,   244,    63,   246,   152,   135,    -1,    61,    -1,
      99,   249,    -1,   174,    -1,    54,    -1,    53,    -1,    56,
      -1,    58,    -1,    59,    -1,   120,    58,    -1,   120,    59,
      -1,    51,    -1,    54,    -1,    53,    -1,    56,    -1,    55,
      -1,   251,    -1,   251,    -1,    34,    -1,    33,    -1,    35,
      -1,    36,    -1,    49,    -1,    48,    -1,    64,    -1,    65,
      -1,   286,    -1,    -1,   111,   256,   158,   286,    -1,     1,
     286,    -1,   139,   258,   283,    -1,   258,   286,    -1,   262,
     138,   266,   138,   268,   271,    -1,   262,   138,   266,   138,
     268,   138,   262,   271,    -1,   262,   138,   266,   271,    -1,
     262,   138,   266,   138,   262,   271,    -1,   262,   138,   268,
     271,    -1,   262,   138,   268,   138,   262,   271,    -1,   262,
     271,    -1,   266,   138,   268,   271,    -1,   266,   138,   268,
     138,   262,   271,    -1,   266,   271,    -1,   266,   138,   262,
     271,    -1,   268,   271,    -1,   268,   138,   262,   271,    -1,
     270,    -1,    -1,    55,    -1,    54,    -1,    53,    -1,    56,
      -1,   259,    -1,    51,    -1,   260,    -1,    90,   220,   283,
      -1,   261,    -1,   262,   138,   261,    -1,    51,   107,   182,
      -1,    51,   107,   212,    -1,   264,    -1,   265,   138,   264,
      -1,   263,    -1,   266,   138,   263,    -1,   117,    -1,    96,
      -1,   267,    51,    -1,   267,    -1,   114,    -1,    97,    -1,
     269,    51,    -1,   138,   270,    -1,   288,    -1,   253,    -1,
      -1,   139,   273,   157,   283,    -1,   288,    -1,   275,   285,
      -1,   276,    -1,   275,   138,   276,    -1,   182,    89,   182,
      -1,    57,   182,    -1,    51,    -1,    55,    -1,    52,    -1,
      51,    -1,    55,    -1,    52,    -1,   179,    -1,    51,    -1,
      52,    -1,   179,    -1,   137,    -1,    86,    -1,    -1,   287,
      -1,    -1,   143,    -1,   282,   140,    -1,   282,   141,    -1,
      -1,   143,    -1,   138,    -1,   142,    -1,   143,    -1,   286,
      -1,   287,   142,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   981,   981,   981,   991,   997,  1001,  1005,  1009,  1015,
    1017,  1016,  1031,  1057,  1063,  1067,  1071,  1075,  1081,  1081,
    1085,  1089,  1093,  1097,  1101,  1105,  1109,  1116,  1117,  1121,
    1125,  1129,  1133,  1137,  1142,  1146,  1151,  1155,  1159,  1163,
    1166,  1170,  1177,  1178,  1182,  1186,  1190,  1194,  1197,  1204,
    1205,  1208,  1209,  1213,  1212,  1225,  1229,  1234,  1238,  1243,
    1247,  1252,  1256,  1260,  1264,  1268,  1274,  1278,  1284,  1285,
    1291,  1295,  1299,  1303,  1307,  1311,  1315,  1319,  1323,  1327,
    1333,  1334,  1340,  1344,  1350,  1354,  1360,  1364,  1368,  1372,
    1376,  1380,  1386,  1392,  1399,  1403,  1407,  1411,  1415,  1419,
    1425,  1431,  1438,  1442,  1445,  1449,  1453,  1459,  1460,  1461,
    1462,  1467,  1474,  1475,  1478,  1484,  1488,  1488,  1494,  1495,
    1496,  1497,  1498,  1499,  1500,  1501,  1502,  1503,  1504,  1505,
    1506,  1507,  1508,  1509,  1510,  1511,  1512,  1513,  1514,  1515,
    1516,  1517,  1518,  1519,  1520,  1521,  1524,  1524,  1524,  1525,
    1525,  1526,  1526,  1526,  1527,  1527,  1527,  1527,  1528,  1528,
    1528,  1529,  1529,  1529,  1530,  1530,  1530,  1530,  1531,  1531,
    1531,  1531,  1532,  1532,  1532,  1532,  1533,  1533,  1533,  1533,
    1534,  1534,  1534,  1534,  1535,  1535,  1538,  1542,  1546,  1550,
    1554,  1558,  1562,  1566,  1570,  1575,  1580,  1585,  1589,  1593,
    1597,  1601,  1605,  1609,  1613,  1617,  1621,  1625,  1629,  1633,
    1637,  1641,  1645,  1649,  1653,  1657,  1661,  1665,  1669,  1673,
    1677,  1686,  1690,  1694,  1698,  1702,  1706,  1710,  1714,  1718,
    1724,  1731,  1732,  1736,  1740,  1746,  1752,  1753,  1756,  1757,
    1758,  1762,  1766,  1772,  1776,  1780,  1784,  1788,  1794,  1794,
    1805,  1811,  1815,  1821,  1825,  1829,  1833,  1839,  1843,  1847,
    1853,  1854,  1855,  1856,  1857,  1858,  1863,  1862,  1873,  1873,
    1877,  1877,  1881,  1885,  1889,  1893,  1897,  1901,  1905,  1909,
    1913,  1917,  1921,  1925,  1929,  1930,  1936,  1935,  1948,  1955,
    1962,  1962,  1962,  1968,  1968,  1968,  1974,  1980,  1985,  1987,
    1984,  1994,  1993,  2006,  2011,  2005,  2024,  2023,  2036,  2035,
    2048,  2049,  2048,  2062,  2066,  2070,  2074,  2080,  2087,  2088,
    2089,  2092,  2093,  2096,  2097,  2105,  2106,  2112,  2116,  2119,
    2123,  2129,  2133,  2139,  2143,  2147,  2151,  2155,  2159,  2163,
    2167,  2171,  2177,  2181,  2185,  2189,  2193,  2197,  2201,  2205,
    2209,  2213,  2217,  2221,  2225,  2229,  2233,  2239,  2240,  2247,
    2252,  2257,  2264,  2268,  2274,  2275,  2278,  2283,  2286,  2290,
    2296,  2300,  2307,  2306,  2319,  2329,  2333,  2338,  2345,  2349,
    2353,  2357,  2361,  2365,  2369,  2373,  2377,  2384,  2383,  2394,
    2393,  2405,  2413,  2422,  2425,  2432,  2435,  2439,  2440,  2443,
    2447,  2450,  2454,  2457,  2458,  2464,  2465,  2466,  2470,  2477,
    2476,  2489,  2487,  2501,  2504,  2511,  2512,  2513,  2514,  2517,
    2518,  2519,  2523,  2529,  2533,  2537,  2541,  2545,  2551,  2557,
    2561,  2565,  2569,  2573,  2577,  2584,  2593,  2594,  2597,  2602,
    2601,  2610,  2617,  2623,  2629,  2633,  2637,  2641,  2645,  2649,
    2653,  2657,  2661,  2665,  2669,  2673,  2677,  2681,  2686,  2692,
    2697,  2702,  2707,  2714,  2718,  2725,  2729,  2735,  2739,  2745,
    2752,  2759,  2763,  2769,  2773,  2779,  2780,  2783,  2788,  2794,
    2795,  2798,  2805,  2809,  2816,  2821,  2821,  2843,  2844,  2850,
    2854,  2860,  2864,  2870,  2871,  2872,  2875,  2876,  2877,  2878,
    2881,  2882,  2883,  2886,  2887,  2890,  2891,  2894,  2895,  2898,
    2901,  2904,  2905,  2906,  2909,  2910,  2913,  2914,  2918
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
  "string_interp", "@27", "@28", "regexp", "symbol", "sym", "numeric",
  "variable", "var_lhs", "var_ref", "backref", "superclass", "$@29",
  "f_arglist", "f_args", "f_bad_arg", "f_norm_arg", "f_arg_item", "f_arg",
  "f_opt", "f_block_opt", "f_block_optarg", "f_optarg", "restarg_mark",
  "f_rest_arg", "blkarg_mark", "f_block_arg", "opt_f_block_arg",
  "singleton", "$@30", "assoc_list", "assocs", "assoc", "operation",
  "operation2", "operation3", "dot_or_colon", "opt_terms", "opt_nl",
  "rparen", "rbracket", "trailer", "term", "terms", "none", 0
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
     244,   246,   244,   247,   248,   249,   249,   249,   249,   250,
     250,   250,   250,   251,   251,   251,   251,   251,   252,   253,
     253,   253,   253,   253,   253,   253,   254,   254,   255,   256,
     255,   255,   257,   257,   258,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   258,   258,   258,   258,   258,   259,
     259,   259,   259,   260,   260,   261,   261,   262,   262,   263,
     264,   265,   265,   266,   266,   267,   267,   268,   268,   269,
     269,   270,   271,   271,   272,   273,   272,   274,   274,   275,
     275,   276,   276,   277,   277,   277,   278,   278,   278,   278,
     279,   279,   279,   280,   280,   281,   281,   282,   282,   283,
     284,   285,   285,   285,   286,   286,   287,   287,   288
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
       4,     0,     5,     1,     2,     1,     1,     1,     1,     1,
       1,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       4,     2,     3,     2,     6,     8,     4,     6,     4,     6,
       2,     4,     6,     2,     4,     2,     4,     1,     0,     1,
       1,     1,     1,     1,     1,     1,     3,     1,     3,     3,
       3,     1,     3,     1,     3,     1,     1,     2,     1,     1,
       1,     2,     2,     1,     1,     0,     4,     1,     2,     1,
       3,     3,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     0,     1,     0,     1,     2,
       2,     0,     1,     1,     1,     1,     1,     2,     0
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,     0,     0,     0,     0,     0,   266,
       0,     0,   505,   290,   293,     0,   313,   314,   315,   316,
     277,   280,   385,   431,   430,   432,   433,   507,     0,    10,
       0,   435,   434,   423,   265,   425,   424,   427,   426,   419,
     420,   405,   413,   406,   436,   437,     0,     0,     0,     0,
     270,   518,   518,    78,   286,     0,     0,     0,     0,     0,
       3,   505,     6,     9,    27,    39,    42,    50,    49,     0,
      66,     0,    70,    80,     0,    47,   229,     0,    51,   284,
     260,   261,   262,   404,   403,   429,     0,   263,   264,   248,
       5,     8,   313,   314,   277,   280,   385,     0,   102,   103,
       0,     0,     0,     0,   105,     0,   317,     0,   429,   264,
       0,   306,   156,   166,   157,   179,   153,   172,   162,   161,
     182,   183,   177,   160,   159,   155,   180,   184,   185,   164,
     154,   167,   171,   173,   165,   158,   174,   181,   176,   175,
     168,   178,   163,   152,   170,   169,   151,   149,   150,   146,
     147,   148,   107,   109,   108,   142,   143,   139,   121,   122,
     123,   130,   127,   129,   124,   125,   144,   145,   131,   132,
     136,   126,   128,   118,   119,   120,   133,   134,   135,   137,
     138,   140,   141,   485,   308,   110,   111,   484,     0,   175,
     168,   178,   163,   146,   147,   107,   108,   112,   115,    20,
     113,     0,     0,    48,     0,     0,     0,   429,     0,   264,
       0,   514,   515,   505,     0,   516,   506,     0,     0,     0,
     328,   327,     0,     0,   429,   264,     0,     0,     0,     0,
     243,   230,   253,    64,   247,   518,   518,   489,    65,    63,
     507,    62,     0,   518,   384,    61,   507,   508,     0,    18,
       0,     0,   207,     0,   208,   274,     0,     0,     0,   505,
      15,   507,    68,    14,   268,   507,     0,   511,   511,   231,
       0,     0,   511,   487,     0,     0,    76,     0,    86,    93,
     458,   417,   416,   418,   415,   414,   407,   409,     0,   421,
     422,    46,   222,   223,     4,   506,     0,     0,     0,     0,
       0,     0,     0,   372,   374,     0,    82,     0,    74,    71,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   518,     0,
     504,   503,     0,   389,   387,   285,     0,     0,   378,    55,
     283,   303,   102,   103,   104,   421,   422,     0,   439,   301,
     438,     0,   518,     0,     0,     0,   458,   310,   116,     0,
     518,   274,   319,     0,   318,     0,     0,   518,     0,     0,
       0,     0,     0,     0,     0,   517,     0,     0,   274,     0,
     518,     0,   298,   492,   254,   250,     0,     0,   244,   252,
       0,   245,   507,     0,   279,   249,   507,   239,   518,   518,
     238,   507,   282,    45,     0,     0,     0,     0,     0,     0,
      17,   507,   272,    13,   506,    67,   507,   271,   275,   513,
     512,   232,   513,   234,   276,   488,    92,    84,     0,    79,
       0,     0,   518,     0,   464,   461,   460,   459,   462,     0,
     476,   480,   479,   475,   458,     0,   369,   463,   465,   467,
     518,   473,   518,   478,   518,     0,   457,     0,   408,   411,
       0,     0,     7,    21,    22,    23,    24,    25,    43,    44,
     518,     0,    28,    37,     0,    38,   507,     0,    72,    83,
      41,    40,     0,   186,   253,    36,   204,   212,   217,   218,
     219,   214,   216,   226,   227,   220,   221,   197,   198,   224,
     225,   507,   213,   215,   209,   210,   211,   199,   200,   201,
     202,   203,   496,   501,   497,   502,   383,   248,   381,   507,
     496,   498,   497,   499,   382,   518,   496,   497,   248,   518,
     518,    29,   188,    35,   196,    53,    56,     0,   441,     0,
       0,   102,   103,   106,     0,   507,   518,     0,   507,   458,
       0,     0,     0,     0,   267,   518,   518,   395,   518,   320,
     186,   500,   273,   507,   496,   497,   518,     0,     0,   297,
     322,   291,   321,   294,   500,   273,   507,   496,   497,     0,
     491,     0,   255,   251,   518,   490,   278,   509,   235,   240,
     242,   281,    19,     0,    26,   195,    69,    16,   269,   511,
      85,    77,    89,    91,   507,   496,   497,     0,   464,     0,
     340,   331,   333,   507,   329,   507,     0,     0,   287,     0,
     450,   483,     0,   453,   477,     0,   455,   481,     0,     0,
     205,   206,   360,   507,     0,   358,   357,   259,     0,    81,
      75,     0,     0,     0,     0,     0,     0,   380,    59,     0,
     386,     0,     0,   237,   379,    57,   236,   375,    52,     0,
       0,     0,   518,   304,     0,     0,   386,   307,   486,   507,
       0,   443,   311,   114,   117,   396,   397,   518,   398,     0,
     518,   325,     0,     0,   323,     0,     0,   386,     0,     0,
       0,   296,     0,     0,     0,     0,   386,     0,   256,   246,
     518,    11,   233,    87,   469,   507,     0,   338,     0,   466,
       0,   362,     0,     0,   468,   518,   518,   482,   518,   474,
     518,   518,   410,     0,   464,   507,     0,   518,   471,   518,
     518,   356,     0,     0,   257,    73,   187,     0,    34,   193,
      33,   194,    60,   510,     0,    31,   191,    32,   192,    58,
     376,   377,     0,     0,   189,     0,     0,   440,   302,   442,
     309,   458,     0,     0,   400,   326,     0,    12,   402,     0,
     288,     0,   289,   255,   518,     0,     0,   299,   241,   330,
     341,     0,   336,   332,   368,     0,   371,   370,     0,   446,
       0,   448,     0,   454,     0,   451,   456,   412,     0,     0,
     359,   347,   349,     0,   352,     0,   354,   373,   258,   228,
      30,   190,   390,   388,     0,     0,     0,     0,   399,     0,
      94,   101,     0,   401,     0,   392,   393,   391,   292,   295,
       0,     0,   339,     0,   334,   366,   507,   364,   367,   518,
     518,   518,   518,     0,   470,   361,   518,   518,   518,   472,
     518,   518,    54,   305,     0,   100,     0,   518,     0,   518,
     518,     0,   337,     0,     0,   363,   447,     0,   444,   449,
     452,   274,     0,     0,   344,     0,   346,   353,     0,   350,
     355,   312,   500,    99,   507,   496,   497,   394,   324,   300,
     335,   365,   518,   500,   273,   518,   518,   518,   518,   386,
     445,   345,     0,   342,   348,   351,   518,   343
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    60,    61,    62,   250,   369,   370,   259,
     260,   414,    64,    65,   204,    66,    67,   546,   672,    68,
      69,   261,    70,    71,    72,   439,    73,   205,   104,   105,
     197,   198,   684,   199,   563,   525,   186,    75,   232,   266,
     526,   664,   406,   407,   241,   242,   234,   398,   408,   485,
      76,   201,   426,   265,   280,   217,   704,   218,   705,   589,
     840,   550,   547,   766,   364,   366,   562,   771,   253,   373,
     581,   693,   694,   223,   621,   622,   623,   735,   644,   645,
     720,   846,   847,   455,   628,   304,   480,    78,    79,   350,
     540,   539,   384,   837,   566,   687,   773,   777,    80,    81,
     288,   467,   639,    82,    83,   285,    84,   207,   208,    87,
     209,   359,   549,   560,   561,   457,   458,   459,   460,   461,
     738,   739,   462,   463,   464,   465,   727,   630,   188,   365,
     271,   409,   237,    89,   554,   528,   342,   214,   403,   404,
     660,   431,   374,   216,   263
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -709
static const yytype_int16 yypact[] =
{
    -709,   131,  2008,  -709,  6722,  8434,  8743,  4950,  6482,  -709,
    8113,  8113,  4431,  -709,  -709,  8537,  6936,  6936,  -709,  -709,
    6936,  5668,  5780,  -709,  -709,  -709,  -709,   153,  6482,  -709,
      10,  -709,  -709,  5072,  5196,  -709,  -709,  5320,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  8220,  8220,    91,  3748,
    8113,  7150,  7471,  6116,  -709,  6362,   100,   261,  8327,  8220,
    -709,   293,  -709,   776,  -709,   156,  -709,  -709,   125,    48,
    -709,    23,  8640,  -709,    92, 10073,    70,   192,    22,    53,
    -709,  -709,  -709,  -709,  -709,    12,   121,  -709,   270,    56,
    -709,  -709,  -709,  -709,  -709,    86,    88,   167,   329,   485,
    8113,   390,  3891,   434,  -709,    54,  -709,   217,  -709,  -709,
      56,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,    32,
      34,    51,    61,  -709,  -709,  -709,  -709,  -709,  -709,   178,
     205,  -709,   209,  -709,   219,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,    22,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,    78,
    -709,  2957,   226,   156,    89,   182,   253,    21,   233,    29,
      89,  -709,  -709,   293,   317,  -709,   220,  8113,  8113,   309,
    -709,  -709,   310,   345,   101,   127,  8220,  8220,  8220,  8220,
    -709, 10073,   291,  -709,  -709,   244,   260,  -709,  -709,  -709,
    4305,  -709,  6936,  6936,  -709,  -709,  4559,  -709,  8113,  -709,
     282,  4034,  -709,   344,   349,   383,  6829,  3748,   298,   293,
     776,   314,   354,  -709,   156,   314,   313,   206,   223,  -709,
     291,   350,   223,  -709,   397,  8846,   326,   347,   365,   460,
     749,  -709,  -709,  -709,  -709,  -709,  -709,  -709,   387,   418,
     420,  -709,  -709,  -709,  -709,  4685,  8113,  8113,  8113,  8113,
    6829,  8113,  8113,  -709,  -709,  7578,  -709,  3748,  6226,   367,
    7578,  8220,  8220,  8220,  8220,  8220,  8220,  8220,  8220,  8220,
    8220,  8220,  8220,  8220,  8220,  8220,  8220,  8220,  8220,  8220,
    8220,  8220,  8220,  8220,  8220,  8220,  8220,  1551,  6936,  9106,
    -709,  -709, 10019,  -709,  -709,  -709,  8327,  8327,  -709,   412,
    -709,   156,  -709,   470,  -709,  -709,  -709,   293,  -709,  -709,
    -709,  9179,  6936,  9252,  2957,  8113,   862,  -709,  -709,   493,
     502,    41,  -709,  3091,   500,  8220,  9325,  6936,  9398,  8220,
    8220,  3349,   317,  7685,   508,  -709,    72,    72,   135,  9471,
    6936,  9544,  -709,  -709,  -709,  -709,  8220,  7043,  -709,  -709,
    7257,  -709,   314,   384,  -709,  -709,   314,  -709,   394,   409,
    -709,    68,  -709,  -709,  6482,  3477,   400,  9325,  9398,  8220,
     776,   314,  -709,  -709,  4810,   411,   314,  -709,  -709,  7364,
    -709,  -709,  7471,  -709,  -709,  -709,   470,    23,  8846,  -709,
    8846,  9617,  6936,  9690,   445,  -709,  -709,  -709,  -709,   606,
    -709,  -709,  -709,  -709,   870,    75,  -709,  -709,  -709,  -709,
     415,  -709,   424,   516,   430,   519,  -709,  4034,  -709,  -709,
    8220,  8220,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
      77,  8220,  -709,   435,   440,  -709,   314,  8846,   446,  -709,
    -709,  -709,   476,  2459,  -709,  -709,   349,  2550,  2550,  2550,
    2550,   680,   680,  2676,  1461,  2550,  2550, 10090, 10090,   642,
     642,  2440,   680,   680,   670,   670,   901,   408,   408,   349,
     349,   349,  2601,  5892,  2813,  6004,  -709,    88,  -709,   314,
     391,  -709,   510,  -709,  -709,  5780,  -709,  -709,  1109,    77,
      77,  -709,  6223,  -709, 10073,  -709,  -709,   293,  -709,  8113,
    2957,   496,    46,  -709,    88,   314,    88,   576,    68,   870,
    2957,   293,  6602,  6482,  -709,  7792,   572,  -709,   501,  -709,
    9965,  5444,  5556,   314,    57,    96,   572,   578,    62,  -709,
    -709,  -709,  -709,  -709,   124,   145,   314,   149,   150,  8113,
    -709,  8220,   291,  -709,   260,  -709,  -709,  -709,  -709,  7043,
    7257,  -709,  -709,   466,  -709, 10073,     0,   776,  -709,   223,
     367,  -709,   496,    46,   314,   115,   142,  8220,  -709,   606,
     419,  -709,   457,   314,  -709,   314,  4177,  4034,  -709,   870,
    -709,  -709,   870,  -709,  -709,   716,  -709,  -709,   469,  4034,
     349,   349,  -709,   666,  4177,  -709,  -709,   467,  7899,  -709,
    -709,  8846,  8327,  8220,   505,  8327,  8327,  -709,   412,   484,
     535,  8327,  8327,  -709,  -709,   412,  -709,    53,   125,  4177,
    4034,  8220,    77,  -709,   293,   610,  -709,  -709,  -709,   314,
     618,  -709,  -709,  -709,  -709,   435,  -709,   540,  -709,  3620,
     625,  -709,  8113,   627,  -709,  8220,  8220,   216,  8220,  8220,
     633,  -709,  8006,  3220,  4177,  4177,   152,    72,  -709,  -709,
     509,  -709,  -709,   319,  -709,   314,   771,   514,   979,  -709,
     513,   524,   646,   523,  -709,   529,   530,  -709,   533,  -709,
     536,   533,  -709,   541,   568,   314,   565,   543,  -709,   548,
     551,  -709,   669,  8220,   553,  -709, 10073,  8220,  -709, 10073,
    -709, 10073,  -709,  -709,  8327,  -709, 10073,  -709, 10073,  -709,
    -709,  -709,   683,   560, 10073,  4034,  2957,  -709,  -709,  -709,
    -709,   862,  8949,    89,  -709,  -709,  4177,  -709,  -709,    89,
    -709,  8220,  -709,  -709,   107,   693,   694,  -709,  7257,  -709,
     562,   771,   489,  -709,  -709,   690,  -709,  -709,   870,  -709,
     716,  -709,   716,  -709,   716,  -709,  -709,  -709,  9052,   595,
    -709,   892,  -709,   892,  -709,   716,  -709,  -709,   571, 10073,
    -709, 10073,  -709,  -709,   579,   703,  2957,   663,  -709,   474,
     365,   460,  2957,  -709,  3091,  -709,  -709,  -709,  -709,  -709,
    4177,   771,   562,   771,   587,  -709,   240,  -709,  -709,   533,
     589,   533,   533,   675,   479,  -709,   593,   594,   533,  -709,
     602,   533,  -709,  -709,   723,   470,  9763,  6936,  9836,   502,
     501,   741,   562,   771,   690,  -709,  -709,   716,  -709,  -709,
    -709,  -709,  9909,   892,  -709,   716,  -709,  -709,   716,  -709,
    -709,  -709,   108,    46,   314,   102,   126,  -709,  -709,  -709,
     562,  -709,   533,   613,   629,   533,   628,   533,   533,   129,
    -709,  -709,   716,  -709,  -709,  -709,   533,  -709
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -709,  -709,  -709,   359,  -709,    35,  -709,  -332,   324,  -709,
      52,  -709,  -285,   123,     2,   -53,  -709,  -535,  -709,    -5,
     763,  -116,     3,   -39,  -239,  -377,    -6,  1583,   -75,   773,
      15,    13,  -709,  -709,  -709,    -4,  -709,  1040,    99,  -709,
     -15,   243,  -317,    69,   -21,  -709,  -357,  -201,    76,  -251,
       4,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,   634,  -194,
    -364,   -80,  -518,  -709,  -628,  -633,   188,  -709,  -442,  -709,
    -549,  -709,   -66,  -709,  -709,   143,  -709,  -709,  -709,   -73,
    -709,  -709,  -355,  -709,   -59,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,     9,  -709,  -709,  1126,  1610,   807,
    1498,  -709,  -709,    58,  -262,  -708,  -441,  -585,  -135,  -586,
    -702,    20,   194,  -709,  -552,  -709,  -252,   311,  -709,  -709,
    -709,    14,  -371,  2166,  -275,  -709,   640,    11,   -25,   -89,
    -502,  -230,     8,    17,    -2
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -519
static const yytype_int16 yytable[] =
{
      90,   245,   248,   185,   185,   291,   345,   244,   624,   106,
     106,   230,   230,   210,   213,   230,   381,   200,   456,   106,
     215,   529,   184,   583,   185,   490,   354,   577,   466,   595,
     236,   236,   557,   309,   236,   401,   438,   200,   433,    91,
     593,   249,   435,   593,   724,   555,   729,   276,   690,   269,
     273,   185,   262,   676,    63,   357,    63,   106,   700,   495,
     573,   595,   527,   611,   535,   268,   272,   538,   349,   215,
     284,   697,   294,   586,   348,   372,   106,   726,   295,   343,
     730,   244,   343,   790,   706,   233,   238,   848,   556,   239,
     793,   740,   235,   235,   736,   348,   235,   669,   670,   580,
    -428,   527,   372,   535,   626,   301,   302,   -67,   340,  -428,
     650,   859,   713,   360,   466,   -96,   556,   380,  -431,   -94,
    -430,   -97,   689,   752,   383,   614,   -94,   267,   -94,   419,
     759,     3,  -273,   203,   203,   203,  -101,  -432,   -81,   -98,
     -88,   421,   -95,   556,   251,   698,   255,  -433,  -100,   -97,
     -86,   270,  -101,   303,   642,   305,  -317,   412,   842,   341,
    -100,   306,   286,   287,   -96,   358,   848,   -90,   556,  -431,
     -99,  -430,   425,   264,   -96,   -98,   427,   -95,   624,   717,
     627,   859,  -273,  -273,   699,  -497,   809,   344,  -432,   643,
     344,   486,   625,   301,   302,   243,   211,   212,  -433,   310,
     702,   438,   466,   -98,   211,   212,  -317,  -317,   401,   346,
     872,   247,   729,   793,   211,   212,   368,   724,   675,   386,
     387,   215,   -88,   351,   382,   240,   724,   243,   680,   595,
     765,   211,   212,   399,   399,   230,   437,   230,   230,   -86,
     900,   410,   593,   593,   -96,   -96,   850,  -496,   438,   -90,
     -97,   -97,   482,   -88,   236,   -88,   236,   491,   -88,   857,
     262,   860,   -89,  -496,  -435,   -93,   835,   215,   -98,   -98,
     423,   -95,   -95,   -92,   745,   624,   424,   624,   337,   106,
     -90,   371,   -90,   -91,  -497,   -90,   553,   -88,   -90,   375,
     -87,  -434,   246,   541,   543,  -423,   247,   679,   473,   474,
     475,   476,   488,   361,   781,  -427,   246,   466,   420,   402,
     262,   405,   106,   596,   686,  -435,   235,   598,   235,   289,
     290,   379,   601,   -95,   534,   393,   394,   395,   338,   339,
     472,   906,   606,   230,   383,   533,   410,   608,   533,   376,
     203,   203,  -434,   787,   429,  -386,  -423,    63,   534,   430,
     624,   844,   477,   362,   363,  -493,  -427,   230,   347,   533,
     410,   432,   385,   534,   388,   548,   430,   490,   567,   411,
     392,   413,   230,   258,   533,   410,   534,  -101,   874,   712,
     396,   484,   397,   247,   703,   230,   484,   533,   410,   377,
     378,   741,   909,   709,   582,   582,   389,   649,   400,   610,
     624,   437,   624,   534,   483,  -386,   399,   399,   -93,   494,
     185,   594,   438,    90,   533,  -423,   415,   595,   311,   203,
     203,   203,   203,   200,   478,   479,   258,   602,   534,   836,
     417,   593,   624,   441,   825,   211,   212,   230,   422,   533,
     410,   352,   106,   609,   106,   353,   390,   391,   437,   468,
     469,  -429,   436,  -386,   428,  -386,  -386,   247,   631,   578,
     631,   -66,   631,  -493,   440,  -423,  -423,    63,  -493,   678,
     618,   419,   445,   446,   447,   448,   607,   311,   646,   661,
     377,   418,   494,   442,   443,   434,   654,   470,   558,   471,
    -100,   106,   355,   356,   864,   590,   592,   728,   -96,   270,
     731,  -429,  -429,   564,   659,   489,   658,   545,   737,   709,
     565,  -494,   657,   569,   665,   692,   689,   668,   579,   466,
     663,   -92,  -500,   663,   597,   334,   335,   336,   592,   -88,
     659,   270,   599,   666,   719,   604,   666,   646,   646,   657,
     618,   663,   445,   446,   447,   448,  -264,   600,   659,   -81,
     894,   674,   617,   629,   666,   673,  -274,   716,   185,   185,
     866,   659,   632,   688,   691,   882,   691,   634,   635,   681,
     637,  -427,   200,  -253,   691,   416,   683,   682,   648,   832,
     647,   258,  -500,   652,   651,   834,   677,   689,   701,   659,
     769,   707,   399,   556,   760,   718,  -264,  -264,   662,   491,
     721,   711,   748,   750,   732,  -254,  -274,  -274,   755,   757,
     867,   868,   437,   710,   747,   362,   363,   -98,   721,  -494,
     768,  -427,  -427,   754,  -494,   753,   789,   843,   770,   772,
    -500,   258,  -500,  -500,   776,  -496,    77,   780,    77,   107,
     107,   484,   -95,   782,   206,   206,   206,   788,   -90,   222,
     206,   206,   791,   794,   206,   106,   796,   618,   797,   445,
     446,   447,   448,   849,   685,   851,   795,   798,   800,   852,
     646,   802,   203,   -87,   804,   808,   807,   810,   858,   817,
     861,   811,   767,    77,   206,   774,   813,   277,   778,   815,
     708,  -255,   206,   822,   779,   823,   619,   568,   592,   270,
     841,   820,   620,   838,   839,   576,   277,   855,   399,  -256,
     721,   311,   203,   863,   862,   582,   714,   734,   865,   445,
     446,   447,   448,   631,   631,   873,   631,   877,   631,   631,
     881,   883,   885,   891,   206,   631,    77,   631,   631,   311,
     888,   845,   902,   445,   446,   447,   448,   744,   905,   311,
     907,   899,  -496,   908,   324,   325,   449,   332,   333,   334,
     335,   336,   450,   451,   324,   325,   912,   618,  -497,   445,
     446,   447,   448,   633,   603,   636,   106,   916,   220,   111,
     452,   667,   691,   453,   331,   332,   333,   334,   335,   336,
     898,   638,   329,   330,   331,   332,   333,   334,   335,   336,
     444,   783,   445,   446,   447,   448,   449,   715,   901,   247,
     897,   761,   106,   451,   187,   203,   296,   297,   298,   299,
     300,   875,   618,   725,   445,   446,   447,   448,   367,   826,
     452,   856,     0,     0,     0,    77,     0,     0,     0,   449,
       0,     0,   818,     0,     0,   450,   451,   631,   631,   631,
     631,   206,   206,   534,   631,   631,   631,     0,   631,   631,
       0,   619,   230,   452,   533,   410,   453,   567,   691,   659,
       0,     0,     0,     0,   206,     0,   206,   206,     0,     0,
     206,     0,   206,     0,     0,    77,     0,   270,   454,     0,
      77,    77,     0,     0,     0,     0,     0,     0,     0,     0,
     631,     0,     0,   631,   631,   631,   631,     0,     0,   277,
       0,     0,     0,   444,   631,   445,   446,   447,   448,     0,
       0,   444,     0,   445,   446,   447,   448,     0,     0,    77,
     206,   206,   206,   206,    77,   206,   206,     0,     0,   206,
       0,    77,   277,   734,   206,   445,   446,   447,   448,     0,
     722,   723,   449,     0,     0,     0,     0,     0,   450,   451,
     449,     0,     0,   733,     0,     0,   450,   451,   742,     0,
     311,     0,   206,     0,     0,     0,   452,     0,     0,   453,
     206,   206,   449,     0,   452,   324,   325,   453,   450,   451,
       0,     0,     0,   762,   763,     0,   206,     0,    77,   206,
       0,   559,     0,     0,     0,     0,   452,    77,     0,   453,
       0,   206,     0,   775,     0,    77,   332,   333,   334,   335,
     336,     0,     0,     0,   206,     0,     0,   784,   785,   786,
     618,     0,   445,   446,   447,   448,   799,   801,     0,   803,
       0,   805,   806,     0,     0,     0,     0,     0,   812,    77,
     814,   816,     0,     0,     0,     0,   231,   231,    77,     0,
     231,     0,     0,     0,     0,     0,     0,     0,     0,   619,
       0,     0,   277,     0,   277,   792,   206,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   252,   254,     0,   824,
       0,   231,   231,     0,     0,     0,     0,     0,   292,   293,
     833,    77,     0,     0,     0,     0,     0,     0,     0,  -518,
       0,     0,     0,     0,     0,     0,     0,  -518,  -518,  -518,
       0,   277,  -518,  -518,  -518,     0,  -518,     0,    85,     0,
      85,   108,   108,   108,     0,  -518,  -518,     0,     0,     0,
       0,   224,     0,     0,     0,     0,  -518,  -518,     0,  -518,
    -518,  -518,  -518,  -518,     0,     0,   869,     0,   870,     0,
     876,   878,   879,   880,   871,     0,     0,   884,   886,   887,
       0,   889,   890,     0,     0,    85,     0,     0,     0,   278,
       0,     0,     0,   206,    77,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    77,  -518,     0,     0,   278,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   910,     0,     0,   911,   913,   914,   915,
       0,     0,     0,   206,     0,     0,     0,   917,    85,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -518,  -518,     0,  -518,     0,   243,  -518,
       0,  -518,  -518,     0,     0,     0,     0,     0,     0,     0,
      77,    77,     0,     0,     0,     0,   231,   231,   231,   292,
       0,     0,     0,    77,     0,     0,     0,     0,    77,     0,
     231,     0,   231,   231,     0,   277,   206,     0,     0,   206,
     206,     0,     0,     0,     0,   206,   206,     0,     0,     0,
       0,     0,     0,    77,    77,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    77,     0,     0,   206,    85,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    77,    77,    77,
       0,     0,     0,     0,     0,   231,     0,     0,     0,     0,
     493,   496,   497,   498,   499,   500,   501,   502,   503,   504,
     505,   506,   507,   508,   509,   510,   511,   512,   513,   514,
     515,   516,   517,   518,   519,   520,   521,    85,   231,     0,
       0,     0,    85,    85,     0,     0,   542,   544,   206,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    77,
      77,   278,   231,     0,     0,     0,   829,     0,     0,     0,
      77,     0,     0,     0,     0,   570,     0,   231,     0,   542,
     544,    85,     0,   231,     0,     0,    85,     0,     0,     0,
     231,     0,     0,    85,   278,     0,   231,   231,     0,     0,
     231,     0,   854,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   605,
      77,     0,     0,     0,     0,     0,    77,     0,    77,   231,
       0,     0,   231,     0,    77,     0,     0,     0,     0,     0,
       0,     0,   231,     0,     0,     0,     0,     0,     0,     0,
      85,     0,     0,     0,     0,     0,     0,     0,     0,    85,
      88,   206,    88,   109,   109,     0,     0,    85,     0,     0,
     640,   641,     0,   225,     0,     0,     0,     0,     0,     0,
       0,   231,     0,     0,     0,     0,     0,     0,     0,     0,
     311,   312,   313,   314,   315,   316,   317,   318,     0,   320,
     321,    85,     0,     0,     0,   324,   325,    88,     0,     0,
      85,   279,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   278,     0,   278,     0,     0,     0,
     279,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,     0,     0,     0,     0,    74,     0,    74,     0,     0,
       0,     0,     0,    85,     0,     0,     0,     0,   221,     0,
      88,     0,   522,   523,     0,   231,   524,     0,     0,     0,
       0,     0,    86,   278,    86,     0,     0,     0,   155,   156,
     157,   158,   159,   160,   161,   162,   163,     0,     0,   164,
     165,   231,    74,   166,   167,   168,   169,     0,     0,   231,
     231,     0,     0,     0,     0,     0,     0,   170,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   231,     0,    86,
       0,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,   181,   182,     0,     0,    85,     0,     0,     0,
       0,     0,     0,     0,     0,    74,    85,     0,   231,     0,
     243,     0,   570,   746,     0,   749,   751,     0,     0,    88,
       0,   756,   758,     0,     0,     0,     0,     0,     0,     0,
       0,   764,    86,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   749,   751,     0,   756,   758,
       0,     0,   231,     0,     0,     0,     0,     0,     0,    88,
       0,     0,    85,    85,    88,    88,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    85,     0,     0,     0,     0,
      85,     0,     0,   279,     0,     0,     0,   278,     0,     0,
       0,     0,     0,   231,    74,     0,     0,   819,     0,     0,
       0,     0,     0,    88,   821,    85,    85,     0,    88,     0,
       0,     0,     0,     0,     0,    88,   279,     0,     0,     0,
       0,    86,     0,     0,     0,    85,     0,     0,     0,     0,
       0,   821,     0,     0,     0,     0,     0,     0,   231,    85,
      85,    85,     0,     0,    74,     0,     0,     0,     0,    74,
      74,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    86,    88,     0,     0,     0,    86,    86,     0,     0,
       0,    88,     0,     0,     0,     0,     0,     0,    74,    88,
       0,     0,     0,    74,     0,     0,     0,     0,     0,     0,
      74,    85,    85,   492,     0,     0,     0,     0,   830,     0,
       0,     0,    85,     0,     0,    86,     0,   231,     0,     0,
      86,     0,     0,    88,     0,     0,     0,    86,     0,     0,
       0,     0,    88,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   108,     0,   279,     0,   279,     0,
       0,     0,     0,     0,     0,     0,     0,    74,     0,     0,
       0,     0,    85,     0,     0,     0,    74,     0,    85,     0,
      85,     0,     0,     0,    74,    88,    85,     0,     0,     0,
       0,     0,     0,     0,    86,     0,     0,     0,     0,     0,
       0,     0,     0,    86,     0,   279,     0,     0,     0,     0,
       0,    86,     0,     0,     0,     0,     0,     0,    74,     0,
       0,     0,     0,     0,     0,     0,     0,    74,  -518,     4,
       0,     5,     6,     7,     8,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,    86,    13,    14,    15,    16,
      17,    18,    19,     0,    86,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,    88,     0,
      74,     0,     0,    28,    29,    30,    31,    32,    88,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,    86,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    48,     0,     0,    49,    50,
       0,    51,    52,     0,    53,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    88,    88,     0,     0,    57,    58,
      59,     0,     0,    74,     0,     0,     0,    88,     0,     0,
       0,     0,    88,    74,     0,     0,     0,     0,     0,   279,
    -518,  -518,     0,     0,     0,     0,     0,     0,     0,     0,
      86,     0,     0,     0,     0,     0,     0,    88,    88,     0,
      86,   110,   110,     0,     0,     0,     0,     0,     0,     0,
       0,   110,     0,     0,     0,     0,     0,    88,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    88,    88,    88,     0,     0,     0,     0,     0,    74,
      74,     0,   110,   110,     0,     0,     0,   110,   110,   110,
       0,     0,    74,     0,     0,   110,     0,    74,     0,     0,
       0,     0,     0,     0,     0,   492,    86,    86,   110,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    86,
       0,     0,    74,    74,    86,     0,     0,     0,     0,     0,
       0,     0,     0,    88,    88,     0,     0,     0,     0,     0,
     831,     0,    74,     0,    88,     0,     0,     0,     0,    86,
      86,     0,     0,     0,     0,     0,    74,    74,    74,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    86,
       0,     0,     0,     0,     0,     0,   109,     0,     0,     0,
       0,     0,     0,    86,    86,    86,     0,     0,     0,     0,
       0,     0,     0,     0,    88,     0,     0,     0,     0,     0,
      88,     0,    88,     0,     0,     0,     0,     0,    88,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    74,    74,
       0,     0,     0,     0,     0,   828,     0,     0,     0,    74,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    86,    86,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    86,     0,     0,     0,
       0,     0,   110,   110,   110,   110,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    74,
       0,     0,     0,     0,     0,    74,     0,    74,     0,     0,
       0,     0,     0,    74,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    86,     0,     0,     0,
       0,   110,    86,     0,    86,     0,     0,     0,     0,     0,
      86,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   110,     0,     0,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   653,     0,     0,     0,     0,     0,   311,
     312,   313,   314,   315,   316,   317,   318,   319,   320,   321,
     322,   323,     0,     0,   324,   325,     0,     0,   311,   312,
     313,   314,   315,   316,   317,   318,   319,   320,   321,   322,
     323,   110,     0,   324,   325,   110,   110,     0,   326,   110,
     327,   328,   329,   330,   331,   332,   333,   334,   335,   336,
       0,     0,   110,   110,     0,     0,   110,   326,     0,   327,
     328,   329,   330,   331,   332,   333,   334,   335,   336,     0,
       0,     0,     0,   247,     0,   110,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   110,     0,  -230,   110,     0,
       0,  -500,     0,     0,   110,     0,   110,     0,     0,  -500,
    -500,  -500,     0,     0,     0,  -500,  -500,     0,  -500,   311,
    -519,  -519,  -519,  -519,   316,   317,     0,  -500,  -519,  -519,
       0,     0,     0,     0,   324,   325,   110,   110,  -500,  -500,
       0,  -500,  -500,  -500,  -500,  -500,     0,   110,     0,     0,
       0,     0,     0,   110,     0,     0,     0,     0,     0,     0,
     327,   328,   329,   330,   331,   332,   333,   334,   335,   336,
    -500,  -500,  -500,  -500,  -500,  -500,  -500,  -500,  -500,  -500,
    -500,  -500,  -500,     0,     0,  -500,  -500,  -500,     0,   655,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   -97,  -500,
       0,  -500,  -500,  -500,  -500,  -500,  -500,  -500,  -500,  -500,
    -500,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   110,     0,     0,     0,  -500,  -500,  -500,  -500,   -89,
       0,  -500,     0,  -500,  -500,   311,   312,   313,   314,   315,
     316,   317,     0,     0,   320,   321,     0,   110,     0,     0,
     324,   325,     0,     0,     0,   110,   110,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   110,     0,     0,   327,   328,   329,   330,
     331,   332,   333,   334,   335,   336,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -273,   110,     0,     0,   110,     0,   110,
       0,  -273,  -273,  -273,     0,     0,     0,  -273,  -273,     0,
    -273,     0,     0,     0,     0,     0,     0,   110,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -273,  -273,     0,  -273,  -273,  -273,  -273,  -273,     0,     0,
       0,   110,   110,     0,   110,   110,     0,     0,   110,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,  -273,     0,     0,  -273,  -273,  -273,
       0,   656,     0,     0,     0,     0,     0,     0,     0,   110,
       0,     0,     0,   110,     0,     0,     0,     0,     0,     0,
     -99,  -273,     0,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,     0,     0,     0,     0,     0,   110,     0,
       0,     0,     0,     0,     0,     0,     0,   110,  -273,  -273,
    -273,   -91,     0,  -273,   110,  -273,  -273,     0,   256,     0,
       5,     6,     7,     8,     9,  -518,  -518,  -518,    10,    11,
       0,     0,  -518,    12,   110,    13,    14,    15,    16,    17,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   256,     0,     5,     6,     7,     8,     9,  -518,
    -518,  -518,    10,    11,     0,  -518,  -518,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,     0,    30,    31,
      32,     0,    33,    34,    35,    36,    37,    38,     0,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,     0,
       0,    49,    50,     0,    51,    52,     0,    53,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    57,    58,    59,     0,     0,     0,     0,     0,     0,
       0,   256,     0,     5,     6,     7,     8,     9,     0,     0,
    -518,    10,    11,  -518,  -518,  -518,    12,  -518,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,     0,    30,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,     0,     0,
      49,    50,     0,    51,    52,     0,    53,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      57,    58,    59,     0,     0,     0,     0,     0,     0,     0,
     256,     0,     5,     6,     7,     8,     9,     0,     0,  -518,
      10,    11,  -518,  -518,  -518,    12,     0,    13,    14,    15,
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
      58,    59,     0,     0,     0,     0,     0,     0,     4,     0,
       5,     6,     7,     8,     9,     0,     0,     0,    10,    11,
       0,  -518,  -518,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,     0,    28,    29,    30,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    48,     0,     0,    49,    50,     0,
      51,    52,     0,    53,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    57,    58,    59,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -518,     0,     0,     0,     0,     0,     0,  -518,
    -518,   256,     0,     5,     6,     7,     8,     9,     0,  -518,
    -518,    10,    11,     0,     0,     0,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,     0,    30,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,     0,     0,
      49,    50,     0,    51,    52,     0,    53,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      57,    58,    59,     0,     0,     0,     0,     0,     0,   256,
       0,     5,     6,     7,     8,     9,     0,     0,     0,    10,
      11,     0,  -518,  -518,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,     0,    30,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    48,     0,     0,   257,    50,
       0,    51,    52,     0,    53,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    57,    58,
      59,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -518,     0,
    -518,  -518,   256,     0,     5,     6,     7,     8,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,     0,    30,    31,
      32,     0,    33,    34,    35,    36,    37,    38,     0,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,     0,
       0,    49,    50,     0,    51,    52,     0,    53,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    57,    58,    59,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -518,     0,  -518,  -518,   256,     0,     5,     6,     7,
       8,     9,     0,     0,     0,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,    28,
       0,    30,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    48,     0,     0,    49,    50,     0,    51,    52,     0,
      53,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    57,    58,    59,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -518,
       0,     0,     0,     0,     0,     0,  -518,  -518,   256,     0,
       5,     6,     7,     8,     9,     0,     0,  -518,    10,    11,
       0,     0,     0,    12,     0,    13,    14,    15,    16,    17,
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
       0,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,     0,  -518,
    -518,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    97,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    33,    34,    35,    36,
      37,    38,   226,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   202,     0,     0,   102,    50,     0,    51,    52,
       0,   227,   228,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    57,   229,    59,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,   247,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    33,    34,    35,    36,    37,    38,     0,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   202,     0,
       0,   102,    50,     0,    51,    52,     0,     0,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    57,    58,    59,     0,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,   211,   212,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   202,     0,     0,   102,
      50,     0,    51,    52,     0,     0,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    57,
      58,    59,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     8,     9,     0,     0,     0,    10,    11,     0,     0,
       0,    12,   247,    13,    14,    15,    16,    17,    18,    19,
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
       0,     0,     0,     5,     6,     7,     8,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,   385,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,     0,    30,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,     0,     0,
      49,    50,     0,    51,    52,     0,    53,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      57,    58,    59,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   385,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,     0,     0,     0,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
       0,     0,     0,     0,     0,   146,   147,   148,   149,   150,
     151,   152,   153,    35,    36,   154,    38,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   155,   156,   157,
     158,   159,   160,   161,   162,   163,     0,     0,   164,   165,
       0,     0,   166,   167,   168,   169,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   170,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
       0,   181,   182,     0,     0,  -493,  -493,  -493,     0,  -493,
       0,     0,     0,  -493,  -493,     0,     0,     0,  -493,   183,
    -493,  -493,  -493,  -493,  -493,  -493,  -493,     0,  -493,     0,
       0,     0,  -493,  -493,  -493,  -493,  -493,  -493,  -493,     0,
       0,  -493,     0,     0,     0,     0,     0,     0,     0,     0,
    -493,  -493,     0,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
    -493,  -493,  -493,  -493,  -493,     0,  -493,  -493,     0,  -493,
    -493,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -493,
       0,     0,  -493,  -493,     0,  -493,  -493,     0,  -493,  -493,
    -493,  -493,     0,     0,     0,  -493,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -493,  -493,  -493,     0,     0,     0,     0,  -495,
    -495,  -495,     0,  -495,     0,     0,  -493,  -495,  -495,     0,
       0,  -493,  -495,     0,  -495,  -495,  -495,  -495,  -495,  -495,
    -495,     0,  -495,     0,     0,     0,  -495,  -495,  -495,  -495,
    -495,  -495,  -495,     0,     0,  -495,     0,     0,     0,     0,
       0,     0,     0,     0,  -495,  -495,     0,  -495,  -495,  -495,
    -495,  -495,  -495,  -495,  -495,  -495,  -495,  -495,  -495,     0,
    -495,  -495,     0,  -495,  -495,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -495,     0,     0,  -495,  -495,     0,  -495,
    -495,     0,  -495,  -495,  -495,  -495,     0,     0,     0,  -495,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -495,  -495,  -495,     0,
       0,     0,     0,  -494,  -494,  -494,     0,  -494,     0,     0,
    -495,  -494,  -494,     0,     0,  -495,  -494,     0,  -494,  -494,
    -494,  -494,  -494,  -494,  -494,     0,  -494,     0,     0,     0,
    -494,  -494,  -494,  -494,  -494,  -494,  -494,     0,     0,  -494,
       0,     0,     0,     0,     0,     0,     0,     0,  -494,  -494,
       0,  -494,  -494,  -494,  -494,  -494,  -494,  -494,  -494,  -494,
    -494,  -494,  -494,     0,  -494,  -494,     0,  -494,  -494,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -494,     0,     0,
    -494,  -494,     0,  -494,  -494,     0,  -494,  -494,  -494,  -494,
       0,     0,     0,  -494,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -494,  -494,  -494,     0,     0,     0,     0,  -496,  -496,  -496,
       0,  -496,     0,     0,  -494,  -496,  -496,     0,     0,  -494,
    -496,     0,  -496,  -496,  -496,  -496,  -496,  -496,  -496,     0,
       0,     0,     0,     0,  -496,  -496,  -496,  -496,  -496,  -496,
    -496,     0,     0,  -496,     0,     0,     0,     0,     0,     0,
       0,     0,  -496,  -496,     0,  -496,  -496,  -496,  -496,  -496,
    -496,  -496,  -496,  -496,  -496,  -496,  -496,     0,  -496,  -496,
       0,  -496,  -496,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -496,   695,     0,  -496,  -496,     0,  -496,  -496,     0,
    -496,  -496,  -496,  -496,     0,     0,     0,  -496,     0,     0,
       0,   -97,     0,     0,     0,     0,     0,     0,     0,  -497,
    -497,  -497,     0,  -497,  -496,  -496,  -496,  -497,  -497,     0,
       0,     0,  -497,     0,  -497,  -497,  -497,  -497,  -497,  -497,
    -497,     0,     0,  -496,     0,     0,  -497,  -497,  -497,  -497,
    -497,  -497,  -497,     0,     0,  -497,     0,     0,     0,     0,
       0,     0,     0,     0,  -497,  -497,     0,  -497,  -497,  -497,
    -497,  -497,  -497,  -497,  -497,  -497,  -497,  -497,  -497,     0,
    -497,  -497,     0,  -497,  -497,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -497,   696,     0,  -497,  -497,     0,  -497,
    -497,     0,  -497,  -497,  -497,  -497,     0,     0,     0,  -497,
       0,     0,     0,   -99,     0,     0,     0,     0,     0,     0,
       0,  -248,  -248,  -248,     0,  -248,  -497,  -497,  -497,  -248,
    -248,     0,     0,     0,  -248,     0,  -248,  -248,  -248,  -248,
    -248,  -248,  -248,     0,     0,  -497,     0,     0,  -248,  -248,
    -248,  -248,  -248,  -248,  -248,     0,     0,  -248,     0,     0,
       0,     0,     0,     0,     0,     0,  -248,  -248,     0,  -248,
    -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,
    -248,     0,  -248,  -248,     0,  -248,  -248,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -248,     0,     0,  -248,  -248,
       0,  -248,  -248,     0,  -248,  -248,  -248,  -248,     0,     0,
       0,  -248,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -248,  -248,  -248,     0,  -248,  -248,  -248,
    -248,  -248,  -248,     0,     0,     0,  -248,     0,  -248,  -248,
    -248,  -248,  -248,  -248,  -248,     0,     0,   240,     0,     0,
    -248,  -248,  -248,  -248,  -248,  -248,  -248,     0,     0,  -248,
       0,     0,     0,     0,     0,     0,     0,     0,  -248,  -248,
       0,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,
    -248,  -248,  -248,     0,  -248,  -248,     0,  -248,  -248,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -248,     0,     0,
    -248,  -248,     0,  -248,  -248,     0,  -248,  -248,  -248,  -248,
       0,     0,     0,  -248,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -498,  -498,  -498,     0,  -498,
    -248,  -248,  -248,  -498,  -498,     0,     0,     0,  -498,     0,
    -498,  -498,  -498,  -498,  -498,  -498,  -498,     0,     0,   243,
       0,     0,  -498,  -498,  -498,  -498,  -498,  -498,  -498,     0,
       0,  -498,     0,     0,     0,     0,     0,     0,     0,     0,
    -498,  -498,     0,  -498,  -498,  -498,  -498,  -498,  -498,  -498,
    -498,  -498,  -498,  -498,  -498,     0,  -498,  -498,     0,  -498,
    -498,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -498,
       0,     0,  -498,  -498,     0,  -498,  -498,     0,  -498,  -498,
    -498,  -498,     0,     0,     0,  -498,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -499,  -499,  -499,
       0,  -499,  -498,  -498,  -498,  -499,  -499,     0,     0,     0,
    -499,     0,  -499,  -499,  -499,  -499,  -499,  -499,  -499,     0,
       0,  -498,     0,     0,  -499,  -499,  -499,  -499,  -499,  -499,
    -499,     0,     0,  -499,     0,     0,     0,     0,     0,     0,
       0,     0,  -499,  -499,     0,  -499,  -499,  -499,  -499,  -499,
    -499,  -499,  -499,  -499,  -499,  -499,  -499,     0,  -499,  -499,
       0,  -499,  -499,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -499,     0,     0,  -499,  -499,     0,  -499,  -499,     0,
    -499,  -499,  -499,  -499,     0,     0,     0,  -499,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,  -499,  -499,  -499,    10,    11,     0,
       0,     0,    12,     0,    13,    14,    15,    92,    93,    18,
      19,     0,     0,  -499,     0,     0,    94,    95,    96,    23,
      24,    25,    26,     0,     0,    97,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   274,     0,     0,   102,    50,     0,    51,
      52,     0,     0,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,   103,    10,    11,     0,
       0,     0,    12,     0,    13,    14,    15,    92,    93,    18,
      19,     0,     0,     0,   275,     0,    94,    95,    96,    23,
      24,    25,    26,     0,     0,    97,     0,   671,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,   311,   312,   313,   314,   315,   316,   317,   318,
     319,   320,   321,   322,   323,     0,     0,   324,   325,     0,
       0,     0,     0,   274,     0,     0,   102,    50,     0,    51,
      52,     0,     0,     0,    54,    55,     0,     0,     0,    56,
       0,   326,     0,   327,   328,   329,   330,   331,   332,   333,
     334,   335,   336,     0,     0,     0,   103,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   487,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,     0,
       0,     0,   136,   137,   138,   189,   190,   191,   192,   143,
     144,   145,     0,     0,     0,     0,     0,   146,   147,   148,
     193,   194,   151,   195,   153,   281,   282,   196,   283,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   155,
     156,   157,   158,   159,   160,   161,   162,   163,     0,     0,
     164,   165,     0,     0,   166,   167,   168,   169,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   170,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,   181,   182,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,     0,
       0,     0,   136,   137,   138,   189,   190,   191,   192,   143,
     144,   145,     0,     0,     0,     0,     0,   146,   147,   148,
     193,   194,   151,   195,   153,     0,     0,   196,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   155,
     156,   157,   158,   159,   160,   161,   162,   163,     0,     0,
     164,   165,     0,     0,   166,   167,   168,   169,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   170,     0,
       0,    55,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,   181,   182,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,     0,
       0,     0,   136,   137,   138,   189,   190,   191,   192,   143,
     144,   145,     0,     0,     0,     0,     0,   146,   147,   148,
     193,   194,   151,   195,   153,     0,     0,   196,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   155,
     156,   157,   158,   159,   160,   161,   162,   163,     0,     0,
     164,   165,     0,     0,   166,   167,   168,   169,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   170,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,   181,   182,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,    29,    30,
      31,    32,     0,    33,    34,    35,    36,    37,    38,     0,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    48,
       0,     0,    49,    50,     0,    51,    52,     0,    53,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     8,     9,     0,     0,     0,
      10,    11,    57,    58,    59,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,     0,    30,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,     0,     0,    49,
      50,     0,    51,    52,     0,    53,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    57,
      58,    59,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    97,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,   226,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   202,     0,     0,   102,    50,     0,    51,
      52,     0,   227,   228,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    57,   229,    59,    12,
       0,    13,    14,    15,    92,    93,    18,    19,     0,     0,
       0,     0,     0,    94,    95,    96,    23,    24,    25,    26,
       0,     0,    97,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
     226,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     202,     0,     0,   102,    50,     0,    51,    52,     0,   591,
     228,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    57,   229,    59,    12,     0,    13,    14,
      15,    92,    93,    18,    19,     0,     0,     0,     0,     0,
      94,    95,    96,    23,    24,    25,    26,     0,     0,    97,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,   226,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   202,     0,     0,
     102,    50,     0,    51,    52,     0,   227,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      57,   229,    59,    12,     0,    13,    14,    15,    92,    93,
      18,    19,     0,     0,     0,     0,     0,    94,    95,    96,
      23,    24,    25,    26,     0,     0,    97,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,   226,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   202,     0,     0,   102,    50,     0,
      51,    52,     0,     0,   228,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    57,   229,    59,
      12,     0,    13,    14,    15,    92,    93,    18,    19,     0,
       0,     0,     0,     0,    94,    95,    96,    23,    24,    25,
      26,     0,     0,    97,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,   226,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   202,     0,     0,   102,    50,     0,    51,    52,     0,
     591,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    57,   229,    59,    12,     0,    13,
      14,    15,    92,    93,    18,    19,     0,     0,     0,     0,
       0,    94,    95,    96,    23,    24,    25,    26,     0,     0,
      97,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    33,    34,    35,    36,    37,    38,   226,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   202,     0,
       0,   102,    50,     0,    51,    52,     0,     0,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    57,   229,    59,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    97,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   202,     0,     0,   102,    50,
       0,    51,    52,     0,   481,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    57,   229,
      59,    12,     0,    13,    14,    15,    92,    93,    18,    19,
       0,     0,     0,     0,     0,    94,    95,    96,    23,    24,
      25,    26,     0,     0,    97,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    33,    34,    35,    36,
      37,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   202,     0,     0,   102,    50,     0,    51,    52,
       0,   227,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    57,   229,    59,    12,     0,
      13,    14,    15,    92,    93,    18,    19,     0,     0,     0,
       0,     0,    94,    95,    96,    23,    24,    25,    26,     0,
       0,    97,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    33,    34,    35,    36,    37,    38,     0,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   202,
       0,     0,   102,    50,     0,    51,    52,     0,   481,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    57,   229,    59,    12,     0,    13,    14,    15,
      92,    93,    18,    19,     0,     0,     0,     0,     0,    94,
      95,    96,    23,    24,    25,    26,     0,     0,    97,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   202,     0,     0,   102,
      50,     0,    51,    52,     0,   743,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    57,
     229,    59,    12,     0,    13,    14,    15,    92,    93,    18,
      19,     0,     0,     0,     0,     0,    94,    95,    96,    23,
      24,    25,    26,     0,     0,    97,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   202,     0,     0,   102,    50,     0,    51,
      52,     0,   591,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    57,   229,    59,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     202,     0,     0,   102,    50,     0,    51,    52,     0,     0,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    57,    58,    59,    12,     0,    13,    14,
      15,    92,    93,    18,    19,     0,     0,     0,     0,     0,
      94,    95,    96,    23,    24,    25,    26,     0,     0,    97,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   202,     0,     0,
     102,    50,     0,    51,    52,     0,     0,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      57,   229,    59,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    97,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   202,     0,     0,   102,    50,     0,
      51,    52,     0,     0,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    57,   229,    59,
      12,     0,    13,    14,    15,    92,    93,    18,    19,     0,
       0,     0,     0,     0,    94,    95,    96,    23,    24,    25,
      26,     0,     0,    97,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    98,    34,    35,    36,    99,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   100,     0,
       0,   101,     0,     0,   102,    50,     0,    51,    52,     0,
       0,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,   103,    13,    14,    15,    92,    93,
      18,    19,     0,     0,     0,     0,     0,    94,    95,    96,
      23,    24,    25,    26,     0,     0,    97,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   219,     0,     0,    49,    50,     0,
      51,    52,     0,    53,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,   103,    13,    14,
      15,    92,    93,    18,    19,     0,     0,     0,     0,     0,
      94,    95,    96,    23,    24,    25,    26,     0,     0,    97,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   274,     0,     0,
     307,    50,     0,    51,    52,     0,   308,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,     0,     0,     0,    12,
     103,    13,    14,    15,    92,    93,    18,    19,     0,     0,
       0,     0,     0,    94,    95,    96,    23,    24,    25,    26,
       0,     0,    97,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    98,    34,    35,    36,    99,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     101,     0,     0,   102,    50,     0,    51,    52,     0,     0,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,     0,
       0,     0,    12,   103,    13,    14,    15,    92,    93,    18,
      19,     0,     0,     0,     0,     0,    94,    95,    96,    23,
      24,    25,    26,     0,     0,    97,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   274,     0,     0,   307,    50,     0,    51,
      52,     0,     0,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,   103,    13,    14,    15,
      92,    93,    18,    19,     0,     0,     0,     0,     0,    94,
      95,    96,    23,    24,    25,    26,     0,     0,    97,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   827,     0,     0,   102,
      50,     0,    51,    52,     0,     0,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,   103,
      13,    14,    15,    92,    93,    18,    19,     0,     0,     0,
       0,     0,    94,    95,    96,    23,    24,    25,    26,     0,
       0,    97,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    33,    34,    35,    36,    37,    38,     0,
      39,    40,    41,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   853,
       0,     0,   102,    50,     0,    51,    52,     0,     0,     0,
      54,    55,     0,     0,     0,    56,     0,   530,   531,     0,
       0,   532,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   103,   155,   156,   157,   158,   159,   160,   161,
     162,   163,     0,     0,   164,   165,     0,     0,   166,   167,
     168,   169,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   170,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,     0,   181,   182,     0,
     551,   523,     0,     0,   552,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   243,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,     0,   536,   531,     0,     0,   537,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   243,   155,
     156,   157,   158,   159,   160,   161,   162,   163,     0,     0,
     164,   165,     0,     0,   166,   167,   168,   169,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   170,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,   181,   182,     0,   571,   523,     0,     0,
     572,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   243,   155,   156,   157,   158,   159,   160,   161,   162,
     163,     0,     0,   164,   165,     0,     0,   166,   167,   168,
     169,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   170,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,     0,   181,   182,     0,   574,
     531,     0,     0,   575,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   243,   155,   156,   157,   158,   159,
     160,   161,   162,   163,     0,     0,   164,   165,     0,     0,
     166,   167,   168,   169,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   170,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,     0,   181,
     182,     0,   584,   523,     0,     0,   585,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   243,   155,   156,
     157,   158,   159,   160,   161,   162,   163,     0,     0,   164,
     165,     0,     0,   166,   167,   168,   169,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   170,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,   181,   182,     0,   587,   531,     0,     0,   588,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     243,   155,   156,   157,   158,   159,   160,   161,   162,   163,
       0,     0,   164,   165,     0,     0,   166,   167,   168,   169,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     170,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,   181,   182,     0,   612,   523,
       0,     0,   613,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   243,   155,   156,   157,   158,   159,   160,
     161,   162,   163,     0,     0,   164,   165,     0,     0,   166,
     167,   168,   169,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   170,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,     0,   181,   182,
       0,   615,   531,     0,     0,   616,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   243,   155,   156,   157,
     158,   159,   160,   161,   162,   163,     0,     0,   164,   165,
       0,     0,   166,   167,   168,   169,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   170,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
       0,   181,   182,     0,   892,   523,     0,     0,   893,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   243,
     155,   156,   157,   158,   159,   160,   161,   162,   163,     0,
       0,   164,   165,     0,     0,   166,   167,   168,   169,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   170,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,   181,   182,     0,   895,   531,     0,
       0,   896,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   243,   155,   156,   157,   158,   159,   160,   161,
     162,   163,     0,     0,   164,   165,     0,     0,   166,   167,
     168,   169,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   170,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,     0,   181,   182,     0,
     903,   523,     0,     0,   904,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   243,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   170,     0,     0,     0,   653,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,     0,     0,   311,   312,   313,   314,   315,   316,
     317,   318,   319,   320,   321,   322,   323,     0,   243,   324,
     325,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     536,   531,     0,   326,   537,   327,   328,   329,   330,   331,
     332,   333,   334,   335,   336,     0,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,   311,   312,   313,   314,   315,   316,   317,   318,
     319,   320,   321,   322,   323,     0,     0,   324,   325,   311,
     312,   313,   314,   315,   316,   317,   318,   319,   320,   321,
    -519,  -519,     0,     0,   324,   325,     0,     0,     0,     0,
       0,   326,     0,   327,   328,   329,   330,   331,   332,   333,
     334,   335,   336,     0,     0,     0,     0,     0,     0,     0,
     327,   328,   329,   330,   331,   332,   333,   334,   335,   336
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-709))

#define yytable_value_is_error(yytable_value) \
  ((yytable_value) == (-519))

static const yytype_int16 yycheck[] =
{
       2,    22,    27,     7,     8,    58,    79,    22,   449,     5,
       6,    16,    17,    11,    12,    20,   210,     8,   280,    15,
      12,   338,     7,   387,    28,   310,   101,   382,   280,   400,
      16,    17,   364,    72,    20,   236,   275,    28,   268,     4,
     397,    28,   272,   400,   629,   362,   632,    53,   566,    51,
      52,    55,    49,   555,     2,     1,     4,    53,   576,   310,
     377,   432,   337,   440,   339,    51,    52,   342,    89,    61,
      55,   573,    61,   390,    89,    13,    72,   629,    61,    26,
     632,    96,    26,   716,   586,    16,    17,   795,   363,    20,
     718,   643,    16,    17,   643,   110,    20,   539,   540,    27,
      88,   376,    13,   378,    29,    37,    38,   107,    86,    88,
     487,   813,   614,   105,   366,    13,   391,    88,    86,   107,
      86,    13,    15,   658,    17,   442,    25,    51,   107,    88,
     665,     0,    86,    10,    11,    12,   107,    86,   138,    13,
      25,   257,    13,   418,   134,    88,    55,    86,   107,    25,
     138,    52,    25,    28,    77,   107,    86,   246,   791,   137,
      25,   138,    62,    63,   107,   111,   874,    25,   443,   137,
      25,   137,   261,    50,    25,    25,   265,    25,   619,   620,
     105,   883,   136,   137,    88,   139,   735,   134,   137,   112,
     134,   307,   454,    37,    38,   139,   142,   143,   137,   107,
     138,   440,   454,   107,   142,   143,   136,   137,   409,    88,
     843,   143,   798,   841,   142,   143,   138,   802,   550,   217,
     218,   213,   107,   100,   213,   139,   811,   139,   560,   600,
     672,   142,   143,   235,   236,   240,   275,   242,   243,   138,
     873,   243,   599,   600,   142,   143,   798,   139,   487,   107,
     142,   143,   305,   138,   240,   140,   242,   310,   143,   811,
     257,   813,   138,   139,    86,   138,   784,   259,   142,   143,
     259,   142,   143,   138,   651,   716,   259,   718,    86,   275,
     138,    55,   140,   138,   139,   143,   361,   138,   138,   107,
     138,    86,   139,   346,   347,    86,   143,   559,   296,   297,
     298,   299,   308,    86,    88,    86,   139,   559,   256,   240,
     307,   242,   308,   402,   565,   137,   240,   406,   242,    58,
      59,    88,   411,   107,   339,   226,   227,   228,   136,   137,
     295,   883,   421,   338,    17,   339,   338,   426,   342,    86,
     217,   218,   137,   707,   138,    26,   137,   295,   363,   143,
     791,   792,   300,   136,   137,    26,   137,   362,    88,   363,
     362,   138,   142,   378,    55,   357,   143,   652,   370,   246,
      25,   248,   377,    49,   378,   377,   391,   107,   138,   609,
      89,   305,   138,   143,   578,   390,   310,   391,   390,   136,
     137,   643,   894,   594,   386,   387,    86,   486,   138,   438,
     841,   440,   843,   418,   305,    86,   408,   409,   138,   310,
     414,   397,   651,   415,   418,    86,   134,   788,    69,   296,
     297,   298,   299,   414,   301,   302,   102,   414,   443,   784,
      86,   788,   873,    86,   766,   142,   143,   442,   140,   443,
     442,    51,   438,   429,   440,    55,   136,   137,   487,    62,
      63,    86,    55,   134,   141,   136,   137,   143,   460,   383,
     462,   107,   464,   134,   138,   136,   137,   415,   139,   558,
      51,    88,    53,    54,    55,    56,   424,    69,   480,    88,
     136,   137,   383,   136,   137,   135,   511,    69,   365,    69,
     107,   487,    58,    59,   826,   396,   397,   632,   107,   400,
     635,   136,   137,    10,   529,   138,   527,    95,   643,   710,
       8,    26,   527,    13,   535,    14,    15,   538,    10,   771,
     535,   138,    26,   538,   140,   117,   118,   119,   429,   138,
     555,   432,   138,   535,   623,   135,   538,   539,   540,   554,
      51,   556,    53,    54,    55,    56,    86,   138,   573,   138,
     867,   549,   107,   138,   556,   547,    86,   138,   562,   563,
      86,   586,   138,   565,   566,    86,   568,    51,   138,   561,
      51,    86,   563,   138,   576,   251,   563,   562,   138,   773,
     481,   257,    86,   107,   138,   779,    10,    15,    10,   614,
     679,   589,   594,   868,   667,   138,   136,   137,    88,   652,
     625,   135,   655,   656,   135,   138,   136,   137,   661,   662,
     136,   137,   651,   599,   109,   136,   137,   107,   643,   134,
      10,   136,   137,    88,   139,   141,   715,   138,    10,    89,
     134,   307,   136,   137,     9,   139,     2,    10,     4,     5,
       6,   565,   107,    10,    10,    11,    12,   138,   138,    15,
      16,    17,   138,   140,    20,   651,    10,    51,   135,    53,
      54,    55,    56,   798,   565,   800,   142,   138,   138,   804,
     672,   138,   549,   138,   138,   107,   135,   112,   813,    10,
     815,   138,   674,    49,    50,   687,   138,    53,   690,   138,
     591,   138,    58,    10,   692,   135,    90,   373,   599,   600,
     138,   754,    96,    10,    10,   381,    72,   112,   710,   138,
     735,    69,   589,    10,   135,   707,   617,    51,    55,    53,
      54,    55,    56,   725,   726,   138,   728,   138,   730,   731,
      55,   138,   138,    10,   100,   737,   102,   739,   740,    69,
     138,    51,   877,    53,    54,    55,    56,   648,   883,    69,
     885,    10,   139,   888,    84,    85,    90,   115,   116,   117,
     118,   119,    96,    97,    84,    85,   138,    51,   139,    53,
      54,    55,    56,   462,   415,   464,   772,   912,    15,     6,
     114,   538,   784,   117,   114,   115,   116,   117,   118,   119,
     870,   467,   112,   113,   114,   115,   116,   117,   118,   119,
      51,   702,    53,    54,    55,    56,    90,   619,   874,   143,
     869,   668,   808,    97,     7,   692,    40,    41,    42,    43,
      44,   846,    51,   629,    53,    54,    55,    56,   188,   771,
     114,   811,    -1,    -1,    -1,   201,    -1,    -1,    -1,    90,
      -1,    -1,   743,    -1,    -1,    96,    97,   849,   850,   851,
     852,   217,   218,   868,   856,   857,   858,    -1,   860,   861,
      -1,    90,   867,   114,   868,   867,   117,   869,   870,   894,
      -1,    -1,    -1,    -1,   240,    -1,   242,   243,    -1,    -1,
     246,    -1,   248,    -1,    -1,   251,    -1,   788,   139,    -1,
     256,   257,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     902,    -1,    -1,   905,   906,   907,   908,    -1,    -1,   275,
      -1,    -1,    -1,    51,   916,    53,    54,    55,    56,    -1,
      -1,    51,    -1,    53,    54,    55,    56,    -1,    -1,   295,
     296,   297,   298,   299,   300,   301,   302,    -1,    -1,   305,
      -1,   307,   308,    51,   310,    53,    54,    55,    56,    -1,
     626,   627,    90,    -1,    -1,    -1,    -1,    -1,    96,    97,
      90,    -1,    -1,   639,    -1,    -1,    96,    97,   644,    -1,
      69,    -1,   338,    -1,    -1,    -1,   114,    -1,    -1,   117,
     346,   347,    90,    -1,   114,    84,    85,   117,    96,    97,
      -1,    -1,    -1,   669,   670,    -1,   362,    -1,   364,   365,
      -1,   139,    -1,    -1,    -1,    -1,   114,   373,    -1,   117,
      -1,   377,    -1,   689,    -1,   381,   115,   116,   117,   118,
     119,    -1,    -1,    -1,   390,    -1,    -1,   703,   704,   705,
      51,    -1,    53,    54,    55,    56,   725,   726,    -1,   728,
      -1,   730,   731,    -1,    -1,    -1,    -1,    -1,   737,   415,
     739,   740,    -1,    -1,    -1,    -1,    16,    17,   424,    -1,
      20,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    90,
      -1,    -1,   438,    -1,   440,    96,   442,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    47,    -1,   765,
      -1,    51,    52,    -1,    -1,    -1,    -1,    -1,    58,    59,
     776,   467,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     0,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     8,     9,    10,
      -1,   487,    13,    14,    15,    -1,    17,    -1,     2,    -1,
       4,     5,     6,     7,    -1,    26,    27,    -1,    -1,    -1,
      -1,    15,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,    -1,   832,    -1,   834,    -1,
     849,   850,   851,   852,   840,    -1,    -1,   856,   857,   858,
      -1,   860,   861,    -1,    -1,    49,    -1,    -1,    -1,    53,
      -1,    -1,    -1,   549,   550,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   560,    86,    -1,    -1,    72,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   902,    -1,    -1,   905,   906,   907,   908,
      -1,    -1,    -1,   589,    -1,    -1,    -1,   916,   102,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   134,   135,    -1,   137,    -1,   139,   140,
      -1,   142,   143,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     626,   627,    -1,    -1,    -1,    -1,   226,   227,   228,   229,
      -1,    -1,    -1,   639,    -1,    -1,    -1,    -1,   644,    -1,
     240,    -1,   242,   243,    -1,   651,   652,    -1,    -1,   655,
     656,    -1,    -1,    -1,    -1,   661,   662,    -1,    -1,    -1,
      -1,    -1,    -1,   669,   670,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   689,    -1,    -1,   692,   201,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   703,   704,   705,
      -1,    -1,    -1,    -1,    -1,   305,    -1,    -1,    -1,    -1,
     310,   311,   312,   313,   314,   315,   316,   317,   318,   319,
     320,   321,   322,   323,   324,   325,   326,   327,   328,   329,
     330,   331,   332,   333,   334,   335,   336,   251,   338,    -1,
      -1,    -1,   256,   257,    -1,    -1,   346,   347,   754,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   765,
     766,   275,   362,    -1,    -1,    -1,   772,    -1,    -1,    -1,
     776,    -1,    -1,    -1,    -1,   375,    -1,   377,    -1,   379,
     380,   295,    -1,   383,    -1,    -1,   300,    -1,    -1,    -1,
     390,    -1,    -1,   307,   308,    -1,   396,   397,    -1,    -1,
     400,    -1,   808,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   419,
     826,    -1,    -1,    -1,    -1,    -1,   832,    -1,   834,   429,
      -1,    -1,   432,    -1,   840,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   442,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     364,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   373,
       2,   867,     4,     5,     6,    -1,    -1,   381,    -1,    -1,
     470,   471,    -1,    15,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   481,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      69,    70,    71,    72,    73,    74,    75,    76,    -1,    78,
      79,   415,    -1,    -1,    -1,    84,    85,    49,    -1,    -1,
     424,    53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   438,    -1,   440,    -1,    -1,    -1,
      72,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,    -1,     2,    -1,     4,    -1,    -1,
      -1,    -1,    -1,   467,    -1,    -1,    -1,    -1,    15,    -1,
     102,    -1,    51,    52,    -1,   565,    55,    -1,    -1,    -1,
      -1,    -1,     2,   487,     4,    -1,    -1,    -1,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    -1,    -1,    78,
      79,   591,    49,    82,    83,    84,    85,    -1,    -1,   599,
     600,    -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   617,    -1,    49,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,   121,   122,    -1,    -1,   550,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   102,   560,    -1,   648,    -1,
     139,    -1,   652,   653,    -1,   655,   656,    -1,    -1,   201,
      -1,   661,   662,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   671,   102,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   695,   696,    -1,   698,   699,
      -1,    -1,   702,    -1,    -1,    -1,    -1,    -1,    -1,   251,
      -1,    -1,   626,   627,   256,   257,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   639,    -1,    -1,    -1,    -1,
     644,    -1,    -1,   275,    -1,    -1,    -1,   651,    -1,    -1,
      -1,    -1,    -1,   743,   201,    -1,    -1,   747,    -1,    -1,
      -1,    -1,    -1,   295,   754,   669,   670,    -1,   300,    -1,
      -1,    -1,    -1,    -1,    -1,   307,   308,    -1,    -1,    -1,
      -1,   201,    -1,    -1,    -1,   689,    -1,    -1,    -1,    -1,
      -1,   781,    -1,    -1,    -1,    -1,    -1,    -1,   788,   703,
     704,   705,    -1,    -1,   251,    -1,    -1,    -1,    -1,   256,
     257,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   251,   364,    -1,    -1,    -1,   256,   257,    -1,    -1,
      -1,   373,    -1,    -1,    -1,    -1,    -1,    -1,   295,   381,
      -1,    -1,    -1,   300,    -1,    -1,    -1,    -1,    -1,    -1,
     307,   765,   766,   310,    -1,    -1,    -1,    -1,   772,    -1,
      -1,    -1,   776,    -1,    -1,   295,    -1,   867,    -1,    -1,
     300,    -1,    -1,   415,    -1,    -1,    -1,   307,    -1,    -1,
      -1,    -1,   424,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   808,    -1,   438,    -1,   440,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   364,    -1,    -1,
      -1,    -1,   826,    -1,    -1,    -1,   373,    -1,   832,    -1,
     834,    -1,    -1,    -1,   381,   467,   840,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   364,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   373,    -1,   487,    -1,    -1,    -1,    -1,
      -1,   381,    -1,    -1,    -1,    -1,    -1,    -1,   415,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   424,     0,     1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,   415,    18,    19,    20,    21,
      22,    23,    24,    -1,   424,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,   550,    -1,
     467,    -1,    -1,    45,    46,    47,    48,    49,   560,    51,
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    67,    68,   467,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   626,   627,    -1,    -1,   120,   121,
     122,    -1,    -1,   550,    -1,    -1,    -1,   639,    -1,    -1,
      -1,    -1,   644,   560,    -1,    -1,    -1,    -1,    -1,   651,
     142,   143,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     550,    -1,    -1,    -1,    -1,    -1,    -1,   669,   670,    -1,
     560,     5,     6,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    15,    -1,    -1,    -1,    -1,    -1,   689,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   703,   704,   705,    -1,    -1,    -1,    -1,    -1,   626,
     627,    -1,    46,    47,    -1,    -1,    -1,    51,    52,    53,
      -1,    -1,   639,    -1,    -1,    59,    -1,   644,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   652,   626,   627,    72,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   639,
      -1,    -1,   669,   670,   644,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   765,   766,    -1,    -1,    -1,    -1,    -1,
     772,    -1,   689,    -1,   776,    -1,    -1,    -1,    -1,   669,
     670,    -1,    -1,    -1,    -1,    -1,   703,   704,   705,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   689,
      -1,    -1,    -1,    -1,    -1,    -1,   808,    -1,    -1,    -1,
      -1,    -1,    -1,   703,   704,   705,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   826,    -1,    -1,    -1,    -1,    -1,
     832,    -1,   834,    -1,    -1,    -1,    -1,    -1,   840,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   765,   766,
      -1,    -1,    -1,    -1,    -1,   772,    -1,    -1,    -1,   776,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   765,   766,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   776,    -1,    -1,    -1,
      -1,    -1,   226,   227,   228,   229,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   826,
      -1,    -1,    -1,    -1,    -1,   832,    -1,   834,    -1,    -1,
      -1,    -1,    -1,   840,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   826,    -1,    -1,    -1,
      -1,   275,   832,    -1,   834,    -1,    -1,    -1,    -1,    -1,
     840,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   308,    -1,    -1,   311,   312,   313,
     314,   315,   316,   317,   318,   319,   320,   321,   322,   323,
     324,   325,   326,   327,   328,   329,   330,   331,   332,   333,
     334,   335,   336,    44,    -1,    -1,    -1,    -1,    -1,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    -1,    -1,    84,    85,    -1,    -1,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,   375,    -1,    84,    85,   379,   380,    -1,   108,   383,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,   396,   397,    -1,    -1,   400,   108,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
      -1,    -1,    -1,   143,    -1,   419,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   429,    -1,   138,   432,    -1,
      -1,     0,    -1,    -1,   438,    -1,   440,    -1,    -1,     8,
       9,    10,    -1,    -1,    -1,    14,    15,    -1,    17,    69,
      70,    71,    72,    73,    74,    75,    -1,    26,    78,    79,
      -1,    -1,    -1,    -1,    84,    85,   470,   471,    37,    38,
      -1,    40,    41,    42,    43,    44,    -1,   481,    -1,    -1,
      -1,    -1,    -1,   487,    -1,    -1,    -1,    -1,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    -1,    -1,    84,    85,    86,    -1,    88,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   107,   108,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   565,    -1,    -1,    -1,   134,   135,   136,   137,   138,
      -1,   140,    -1,   142,   143,    69,    70,    71,    72,    73,
      74,    75,    -1,    -1,    78,    79,    -1,   591,    -1,    -1,
      84,    85,    -1,    -1,    -1,   599,   600,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   617,    -1,    -1,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     0,   648,    -1,    -1,   651,    -1,   653,
      -1,     8,     9,    10,    -1,    -1,    -1,    14,    15,    -1,
      17,    -1,    -1,    -1,    -1,    -1,    -1,   671,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    -1,
      -1,   695,   696,    -1,   698,   699,    -1,    -1,   702,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    -1,    -1,    84,    85,    86,
      -1,    88,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   743,
      -1,    -1,    -1,   747,    -1,    -1,    -1,    -1,    -1,    -1,
     107,   108,    -1,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,    -1,    -1,    -1,   772,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   781,   135,   136,
     137,   138,    -1,   140,   788,   142,   143,    -1,     1,    -1,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      -1,    -1,    15,    16,   808,    18,    19,    20,    21,    22,
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
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     1,    -1,     3,     4,     5,     6,     7,   142,
     143,    10,    11,    12,    -1,    14,    15,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    45,    -1,    47,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    -1,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    -1,    93,    94,    -1,    96,    -1,    98,
      99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   120,   121,   122,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     1,    -1,     3,     4,     5,     6,     7,    -1,    -1,
      10,    11,    12,   142,   143,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    47,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    -1,    58,    59,
      60,    61,    62,    -1,    64,    65,    -1,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    -1,    93,    94,    -1,    96,    -1,    98,    99,
      -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     120,   121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,     3,     4,     5,     6,     7,    -1,    -1,    10,
      11,    12,   142,   143,    15,    16,    -1,    18,    19,    20,
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
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    -1,    11,    12,
      -1,   142,   143,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    45,    46,    47,    48,    49,    -1,    51,    52,
      53,    54,    55,    56,    -1,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,   122,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   135,    -1,    -1,    -1,    -1,    -1,    -1,   142,
     143,     1,    -1,     3,     4,     5,     6,     7,    -1,     9,
      10,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    47,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    -1,    58,    59,
      60,    61,    62,    -1,    64,    65,    -1,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    -1,    93,    94,    -1,    96,    -1,    98,    99,
      -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     120,   121,   122,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,    11,
      12,    -1,   142,   143,    16,    -1,    18,    19,    20,    21,
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
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   140,    -1,
     142,   143,     1,    -1,     3,     4,     5,     6,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    45,    -1,    47,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    -1,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    -1,    93,    94,    -1,    96,    -1,    98,
      99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   120,   121,   122,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   140,    -1,   142,   143,     1,    -1,     3,     4,     5,
       6,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    47,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   120,   121,   122,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   135,
      -1,    -1,    -1,    -1,    -1,    -1,   142,   143,     1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    10,    11,    12,
      -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,    22,
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
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,   142,
     143,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    -1,    64,
      65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,
      -1,    96,    97,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   120,   121,   122,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,   143,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    -1,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    -1,    93,    94,    -1,    -1,    -1,    98,
      99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   120,   121,   122,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,   142,   143,    16,    -1,    18,    19,    20,
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
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,
      -1,    16,   143,    18,    19,    20,    21,    22,    23,    24,
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
      -1,    -1,    -1,     3,     4,     5,     6,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,   142,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    47,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    -1,    58,    59,
      60,    61,    62,    -1,    64,    65,    -1,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    -1,    93,    94,    -1,    96,    -1,    98,    99,
      -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     120,   121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   142,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    -1,    -1,    78,    79,
      -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,   121,   122,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,   139,
      18,    19,    20,    21,    22,    23,    24,    -1,    26,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,    97,
      98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   120,   121,   122,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,   134,    11,    12,    -1,
      -1,   139,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    26,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    -1,
      64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
      94,    -1,    96,    97,    98,    99,    -1,    -1,    -1,   103,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   120,   121,   122,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
     134,    11,    12,    -1,    -1,   139,    16,    -1,    18,    19,
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
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
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
      -1,    -1,    -1,    87,    88,    -1,    90,    91,    -1,    93,
      94,    -1,    96,    97,    98,    99,    -1,    -1,    -1,   103,
      -1,    -1,    -1,   107,    -1,    -1,    -1,    -1,    -1,    -1,
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
      56,    57,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      96,    97,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,   120,   121,   122,    11,    12,    -1,
      -1,    -1,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,   139,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    -1,    58,    59,    60,    61,    62,    -1,
      64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
      94,    -1,    -1,    -1,    98,    99,    -1,    -1,    -1,   103,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,   120,    11,    12,    -1,
      -1,    -1,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,   138,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    -1,    58,    59,    60,    61,    62,    -1,
      64,    65,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    -1,    -1,    84,    85,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
      94,    -1,    -1,    -1,    98,    99,    -1,    -1,    -1,   103,
      -1,   108,    -1,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,    -1,   120,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   138,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    -1,    -1,
      78,    79,    -1,    -1,    82,    83,    84,    85,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,   121,   122,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    -1,    -1,    55,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    -1,    -1,
      78,    79,    -1,    -1,    82,    83,    84,    85,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,
      -1,    99,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,   121,   122,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    -1,    -1,    55,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    -1,    -1,
      78,    79,    -1,    -1,    82,    83,    84,    85,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,   121,   122,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    -1,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,    -1,
      98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,
      11,    12,   120,   121,   122,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    47,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    -1,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,   120,
     121,   122,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    -1,
      64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
      94,    -1,    96,    97,    98,    99,    -1,    -1,    -1,   103,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,   120,   121,   122,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,
      97,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   120,   121,   122,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    57,    58,    59,
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
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    -1,    97,    98,    99,    -1,    -1,    -1,
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
      96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,   120,   121,   122,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    57,    58,
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
      -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,
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
      -1,    96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   120,   121,   122,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    -1,
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
      51,    52,    53,    54,    55,    56,    -1,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,
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
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    -1,
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
      90,    91,    -1,    93,    94,    -1,    -1,    -1,    98,    99,
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
      93,    94,    -1,    -1,    -1,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,   120,   121,   122,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,
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
      93,    94,    -1,    96,    -1,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,   120,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,
      -1,    51,    52,    53,    54,    55,    56,    -1,    58,    59,
      60,    61,    62,    -1,    64,    65,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    -1,    93,    94,    -1,    96,    -1,    98,    99,
      -1,    -1,    -1,   103,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,
     120,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      -1,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    -1,
      -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,
      -1,    -1,    16,   120,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    -1,    58,    59,    60,    61,    62,    -1,
      64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
      94,    -1,    -1,    -1,    98,    99,    -1,    -1,    -1,   103,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,   120,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    -1,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    -1,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,   120,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    -1,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    -1,    -1,
      98,    99,    -1,    -1,    -1,   103,    -1,    51,    52,    -1,
      -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   120,    67,    68,    69,    70,    71,    72,    73,
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
      -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,   122,    -1,    -1,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    -1,   139,    84,
      85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      51,    52,    -1,   108,    55,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    -1,    -1,    78,    79,    -1,
      -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,   122,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    -1,    -1,    84,    85,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    -1,    -1,    84,    85,    -1,    -1,    -1,    -1,
      -1,   108,    -1,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119
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
     242,   243,   247,   248,   250,   251,   252,   253,   254,   277,
     288,   149,    21,    22,    30,    31,    32,    39,    51,    55,
      84,    87,    90,   120,   172,   173,   194,   212,   251,   254,
     277,   173,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    45,    46,    47,    48,
      49,    50,    51,    52,    55,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    78,    79,    82,    83,    84,    85,
      96,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   121,   122,   139,   174,   179,   180,   253,   272,    33,
      34,    35,    36,    48,    49,    51,    55,   174,   175,   177,
     248,   195,    87,   157,   158,   171,   212,   251,   252,   254,
     158,   142,   143,   158,   281,   286,   287,   199,   201,    87,
     164,   171,   212,   217,   251,   254,    57,    96,    97,   121,
     163,   181,   182,   187,   190,   192,   275,   276,   187,   187,
     139,   188,   189,   139,   184,   188,   139,   143,   282,   175,
     150,   134,   181,   212,   181,    55,     1,    90,   152,   153,
     154,   165,   166,   288,   157,   197,   183,   192,   275,   288,
     182,   274,   275,   288,    87,   138,   170,   212,   251,   254,
     198,    53,    54,    56,   174,   249,    62,    63,   244,    58,
      59,   159,   181,   181,   281,   287,    40,    41,    42,    43,
      44,    37,    38,    28,   229,   107,   138,    90,    96,   167,
     107,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    84,    85,   108,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    86,   136,   137,
      86,   137,   280,    26,   134,   233,    88,    88,   184,   188,
     233,   157,    51,    55,   172,    58,    59,     1,   111,   255,
     286,    86,   136,   137,   208,   273,   209,   280,   138,   151,
     152,    55,    13,   213,   286,   107,    86,   136,   137,    88,
      88,   213,   281,    17,   236,   142,   158,   158,    55,    86,
     136,   137,    25,   182,   182,   182,    89,   138,   191,   288,
     138,   191,   187,   282,   283,   187,   186,   187,   192,   275,
     288,   157,   283,   157,   155,   134,   152,    86,   137,    88,
     154,   165,   140,   281,   287,   283,   196,   283,   141,   138,
     143,   285,   138,   285,   135,   285,    55,   167,   168,   169,
     138,    86,   136,   137,    51,    53,    54,    55,    56,    90,
      96,    97,   114,   117,   139,   227,   258,   259,   260,   261,
     262,   263,   266,   267,   268,   269,   270,   245,    62,    63,
      69,    69,   149,   158,   158,   158,   158,   154,   157,   157,
     230,    96,   159,   182,   192,   193,   165,   138,   170,   138,
     156,   159,   171,   181,   182,   193,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,    51,    52,    55,   179,   184,   278,   279,   186,
      51,    52,    55,   179,   184,   278,    51,    55,   278,   235,
     234,   159,   181,   159,   181,    95,   161,   206,   286,   256,
     205,    51,    55,   172,   278,   186,   278,   151,   157,   139,
     257,   258,   210,   178,    10,     8,   238,   288,   152,    13,
     181,    51,    55,   186,    51,    55,   152,   236,   192,    10,
      27,   214,   286,   214,    51,    55,   186,    51,    55,   203,
     182,    96,   182,   190,   275,   276,   283,   140,   283,   138,
     138,   283,   175,   147,   135,   181,   283,   154,   283,   275,
     167,   169,    51,    55,   186,    51,    55,   107,    51,    90,
      96,   218,   219,   220,   260,   258,    29,   105,   228,   138,
     271,   288,   138,   271,    51,   138,   271,    51,   152,   246,
     181,   181,    77,   112,   222,   223,   288,   182,   138,   283,
     169,   138,   107,    44,   282,    88,    88,   184,   188,   282,
     284,    88,    88,   184,   185,   188,   288,   185,   188,   222,
     222,    44,   162,   286,   158,   151,   284,    10,   283,   258,
     151,   286,   174,   175,   176,   182,   193,   239,   288,    15,
     216,   288,    14,   215,   216,    88,    88,   284,    88,    88,
     216,    10,   138,   213,   200,   202,   284,   158,   182,   191,
     275,   135,   285,   284,   182,   220,   138,   260,   138,   283,
     224,   282,   152,   152,   261,   266,   268,   270,   262,   263,
     268,   262,   135,   152,    51,   221,   224,   262,   264,   265,
     268,   270,   152,    96,   182,   169,   181,   109,   159,   181,
     159,   181,   161,   141,    88,   159,   181,   159,   181,   161,
     233,   229,   152,   152,   181,   222,   207,   286,    10,   283,
      10,   211,    89,   240,   288,   152,     9,   241,   288,   158,
      10,    88,    10,   182,   152,   152,   152,   214,   138,   283,
     219,   138,    96,   218,   140,   142,    10,   135,   138,   271,
     138,   271,   138,   271,   138,   271,   271,   135,   107,   224,
     112,   138,   271,   138,   271,   138,   271,    10,   182,   181,
     159,   181,    10,   135,   152,   151,   257,    87,   171,   212,
     251,   254,   213,   152,   213,   216,   236,   237,    10,    10,
     204,   138,   219,   138,   260,    51,   225,   226,   259,   262,
     268,   262,   262,    87,   212,   112,   265,   268,   262,   264,
     268,   262,   135,    10,   151,    55,    86,   136,   137,   152,
     152,   152,   219,   138,   138,   282,   271,   138,   271,   271,
     271,    55,    86,   138,   271,   138,   271,   271,   138,   271,
     271,    10,    51,    55,   186,    51,    55,   238,   215,    10,
     219,   226,   262,    51,    55,   262,   268,   262,   262,   284,
     271,   271,   138,   271,   271,   271,   262,   271
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
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
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


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
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


/*----------.
| yyparse.  |
`----------*/

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
  if (yypact_value_is_default (yyn))
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
      if (yytable_value_is_error (yyn))
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

/* Line 1821 of yacc.c  */
#line 981 "./parse.y"
    {
		     p->lstate = EXPR_BEG;
		     if (!p->locals) p->locals = cons(0,0);
		   }
    break;

  case 3:

/* Line 1821 of yacc.c  */
#line 986 "./parse.y"
    {
		      p->tree = new_scope(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 4:

/* Line 1821 of yacc.c  */
#line 992 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 5:

/* Line 1821 of yacc.c  */
#line 998 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 6:

/* Line 1821 of yacc.c  */
#line 1002 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 7:

/* Line 1821 of yacc.c  */
#line 1006 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), newline_node((yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 8:

/* Line 1821 of yacc.c  */
#line 1010 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 10:

/* Line 1821 of yacc.c  */
#line 1017 "./parse.y"
    {
		      if (p->in_def || p->in_single) {
			yyerror(p, "BEGIN in method");
		      }
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 11:

/* Line 1821 of yacc.c  */
#line 1024 "./parse.y"
    {
		      p->begin_tree = push(p->begin_tree, (yyvsp[(4) - (5)].nd));
		      local_resume(p, (yyvsp[(2) - (5)].nd));
		      (yyval.nd) = 0;
		    }
    break;

  case 12:

/* Line 1821 of yacc.c  */
#line 1035 "./parse.y"
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
		    }
    break;

  case 13:

/* Line 1821 of yacc.c  */
#line 1058 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 14:

/* Line 1821 of yacc.c  */
#line 1064 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 15:

/* Line 1821 of yacc.c  */
#line 1068 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 16:

/* Line 1821 of yacc.c  */
#line 1072 "./parse.y"
    {
			(yyval.nd) = push((yyvsp[(1) - (3)].nd), newline_node((yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 17:

/* Line 1821 of yacc.c  */
#line 1076 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 18:

/* Line 1821 of yacc.c  */
#line 1081 "./parse.y"
    {p->lstate = EXPR_FNAME;}
    break;

  case 19:

/* Line 1821 of yacc.c  */
#line 1082 "./parse.y"
    {
		      (yyval.nd) = new_alias(p, (yyvsp[(2) - (4)].id), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 20:

/* Line 1821 of yacc.c  */
#line 1086 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 21:

/* Line 1821 of yacc.c  */
#line 1090 "./parse.y"
    {
			(yyval.nd) = new_if(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd), 0);
		    }
    break;

  case 22:

/* Line 1821 of yacc.c  */
#line 1094 "./parse.y"
    {
		      (yyval.nd) = new_unless(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd), 0);
		    }
    break;

  case 23:

/* Line 1821 of yacc.c  */
#line 1098 "./parse.y"
    {
		      (yyval.nd) = new_while(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd));
		    }
    break;

  case 24:

/* Line 1821 of yacc.c  */
#line 1102 "./parse.y"
    {
		      (yyval.nd) = new_until(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd));
		    }
    break;

  case 25:

/* Line 1821 of yacc.c  */
#line 1106 "./parse.y"
    {
		      (yyval.nd) = new_rescue(p, (yyvsp[(1) - (3)].nd), list1(list3(0, 0, (yyvsp[(3) - (3)].nd))), 0);
		    }
    break;

  case 26:

/* Line 1821 of yacc.c  */
#line 1110 "./parse.y"
    {
		      if (p->in_def || p->in_single) {
			yywarn(p, "END in method; use at_exit");
		      }
		      (yyval.nd) = new_postexe(p, (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 28:

/* Line 1821 of yacc.c  */
#line 1118 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(1) - (3)].nd), list1((yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 29:

/* Line 1821 of yacc.c  */
#line 1122 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 30:

/* Line 1821 of yacc.c  */
#line 1126 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].nd), intern("[]"), (yyvsp[(3) - (6)].nd)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 31:

/* Line 1821 of yacc.c  */
#line 1130 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 32:

/* Line 1821 of yacc.c  */
#line 1134 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 33:

/* Line 1821 of yacc.c  */
#line 1138 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.nd) = 0;
		    }
    break;

  case 34:

/* Line 1821 of yacc.c  */
#line 1143 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 35:

/* Line 1821 of yacc.c  */
#line 1147 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].nd));
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 36:

/* Line 1821 of yacc.c  */
#line 1152 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), new_array(p, (yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 37:

/* Line 1821 of yacc.c  */
#line 1156 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 38:

/* Line 1821 of yacc.c  */
#line 1160 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(1) - (3)].nd), new_array(p, (yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 40:

/* Line 1821 of yacc.c  */
#line 1167 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 41:

/* Line 1821 of yacc.c  */
#line 1171 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 43:

/* Line 1821 of yacc.c  */
#line 1179 "./parse.y"
    {
		      (yyval.nd) = new_and(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 44:

/* Line 1821 of yacc.c  */
#line 1183 "./parse.y"
    {
		      (yyval.nd) = new_or(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 45:

/* Line 1821 of yacc.c  */
#line 1187 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(3) - (3)].nd)), "!");
		    }
    break;

  case 46:

/* Line 1821 of yacc.c  */
#line 1191 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(2) - (2)].nd)), "!");
		    }
    break;

  case 48:

/* Line 1821 of yacc.c  */
#line 1198 "./parse.y"
    {
		      if (!(yyvsp[(1) - (1)].nd)) (yyval.nd) = new_nil(p);
		      else (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 53:

/* Line 1821 of yacc.c  */
#line 1213 "./parse.y"
    {
		      local_nest(p);
		    }
    break;

  case 54:

/* Line 1821 of yacc.c  */
#line 1219 "./parse.y"
    {
		      (yyval.nd) = new_block(p, (yyvsp[(3) - (5)].nd), (yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    }
    break;

  case 55:

/* Line 1821 of yacc.c  */
#line 1226 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 56:

/* Line 1821 of yacc.c  */
#line 1230 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(2) - (3)].nd), (yyvsp[(3) - (3)].nd));
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (3)].id), (yyvsp[(2) - (3)].nd));
		    }
    break;

  case 57:

/* Line 1821 of yacc.c  */
#line 1235 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 58:

/* Line 1821 of yacc.c  */
#line 1239 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].nd), (yyvsp[(5) - (5)].nd));
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		   }
    break;

  case 59:

/* Line 1821 of yacc.c  */
#line 1244 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 60:

/* Line 1821 of yacc.c  */
#line 1248 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].nd), (yyvsp[(5) - (5)].nd));
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		    }
    break;

  case 61:

/* Line 1821 of yacc.c  */
#line 1253 "./parse.y"
    {
		      (yyval.nd) = new_super(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 62:

/* Line 1821 of yacc.c  */
#line 1257 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 63:

/* Line 1821 of yacc.c  */
#line 1261 "./parse.y"
    {
		      (yyval.nd) = new_return(p, ret_args(p, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 64:

/* Line 1821 of yacc.c  */
#line 1265 "./parse.y"
    {
		      (yyval.nd) = new_break(p, ret_args(p, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 65:

/* Line 1821 of yacc.c  */
#line 1269 "./parse.y"
    {
		      (yyval.nd) = new_next(p, ret_args(p, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 66:

/* Line 1821 of yacc.c  */
#line 1275 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 67:

/* Line 1821 of yacc.c  */
#line 1279 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 69:

/* Line 1821 of yacc.c  */
#line 1286 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(2) - (3)].nd));
		    }
    break;

  case 70:

/* Line 1821 of yacc.c  */
#line 1292 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 71:

/* Line 1821 of yacc.c  */
#line 1296 "./parse.y"
    {
		      (yyval.nd) = list1(push((yyvsp[(1) - (2)].nd),(yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 72:

/* Line 1821 of yacc.c  */
#line 1300 "./parse.y"
    {
		      (yyval.nd) = list2((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 73:

/* Line 1821 of yacc.c  */
#line 1304 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].nd), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 74:

/* Line 1821 of yacc.c  */
#line 1308 "./parse.y"
    {
		      (yyval.nd) = list2((yyvsp[(1) - (2)].nd), new_nil(p));
		    }
    break;

  case 75:

/* Line 1821 of yacc.c  */
#line 1312 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (4)].nd), new_nil(p), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 76:

/* Line 1821 of yacc.c  */
#line 1316 "./parse.y"
    {
		      (yyval.nd) = list2(0, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 77:

/* Line 1821 of yacc.c  */
#line 1320 "./parse.y"
    {
		      (yyval.nd) = list3(0, (yyvsp[(2) - (4)].nd), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 78:

/* Line 1821 of yacc.c  */
#line 1324 "./parse.y"
    {
		      (yyval.nd) = list2(0, new_nil(p));
		    }
    break;

  case 79:

/* Line 1821 of yacc.c  */
#line 1328 "./parse.y"
    {
		      (yyval.nd) = list3(0, new_nil(p), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 81:

/* Line 1821 of yacc.c  */
#line 1335 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 82:

/* Line 1821 of yacc.c  */
#line 1341 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (2)].nd));
		    }
    break;

  case 83:

/* Line 1821 of yacc.c  */
#line 1345 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(2) - (3)].nd));
		    }
    break;

  case 84:

/* Line 1821 of yacc.c  */
#line 1351 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 85:

/* Line 1821 of yacc.c  */
#line 1355 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 86:

/* Line 1821 of yacc.c  */
#line 1361 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 87:

/* Line 1821 of yacc.c  */
#line 1365 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), intern("[]"), (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 88:

/* Line 1821 of yacc.c  */
#line 1369 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 89:

/* Line 1821 of yacc.c  */
#line 1373 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 90:

/* Line 1821 of yacc.c  */
#line 1377 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 91:

/* Line 1821 of yacc.c  */
#line 1381 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 92:

/* Line 1821 of yacc.c  */
#line 1387 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 93:

/* Line 1821 of yacc.c  */
#line 1393 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].nd));
		      (yyval.nd) = 0;
		    }
    break;

  case 94:

/* Line 1821 of yacc.c  */
#line 1400 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 95:

/* Line 1821 of yacc.c  */
#line 1404 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), intern("[]"), (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 96:

/* Line 1821 of yacc.c  */
#line 1408 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 97:

/* Line 1821 of yacc.c  */
#line 1412 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 98:

/* Line 1821 of yacc.c  */
#line 1416 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 99:

/* Line 1821 of yacc.c  */
#line 1420 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 100:

/* Line 1821 of yacc.c  */
#line 1426 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 101:

/* Line 1821 of yacc.c  */
#line 1432 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].nd));
		      (yyval.nd) = 0;
		    }
    break;

  case 102:

/* Line 1821 of yacc.c  */
#line 1439 "./parse.y"
    {
		      yyerror(p, "class/module name must be CONSTANT");
		    }
    break;

  case 104:

/* Line 1821 of yacc.c  */
#line 1446 "./parse.y"
    {
		      (yyval.nd) = cons((node*)1, (node*)(yyvsp[(2) - (2)].id));
		    }
    break;

  case 105:

/* Line 1821 of yacc.c  */
#line 1450 "./parse.y"
    {
		      (yyval.nd) = cons((node*)0, (node*)(yyvsp[(1) - (1)].id));
		    }
    break;

  case 106:

/* Line 1821 of yacc.c  */
#line 1454 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (3)].nd), (node*)(yyvsp[(3) - (3)].id));
		    }
    break;

  case 110:

/* Line 1821 of yacc.c  */
#line 1463 "./parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    }
    break;

  case 111:

/* Line 1821 of yacc.c  */
#line 1468 "./parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    }
    break;

  case 114:

/* Line 1821 of yacc.c  */
#line 1479 "./parse.y"
    {
		      (yyval.nd) = new_sym(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 115:

/* Line 1821 of yacc.c  */
#line 1485 "./parse.y"
    {
		      (yyval.nd) = new_undef(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 116:

/* Line 1821 of yacc.c  */
#line 1488 "./parse.y"
    {p->lstate = EXPR_FNAME;}
    break;

  case 117:

/* Line 1821 of yacc.c  */
#line 1489 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), (node*)(yyvsp[(4) - (4)].nd));
		    }
    break;

  case 118:

/* Line 1821 of yacc.c  */
#line 1494 "./parse.y"
    { (yyval.id) = intern("|"); }
    break;

  case 119:

/* Line 1821 of yacc.c  */
#line 1495 "./parse.y"
    { (yyval.id) = intern("^"); }
    break;

  case 120:

/* Line 1821 of yacc.c  */
#line 1496 "./parse.y"
    { (yyval.id) = intern("&"); }
    break;

  case 121:

/* Line 1821 of yacc.c  */
#line 1497 "./parse.y"
    { (yyval.id) = intern("<=>"); }
    break;

  case 122:

/* Line 1821 of yacc.c  */
#line 1498 "./parse.y"
    { (yyval.id) = intern("=="); }
    break;

  case 123:

/* Line 1821 of yacc.c  */
#line 1499 "./parse.y"
    { (yyval.id) = intern("==="); }
    break;

  case 124:

/* Line 1821 of yacc.c  */
#line 1500 "./parse.y"
    { (yyval.id) = intern("=~"); }
    break;

  case 125:

/* Line 1821 of yacc.c  */
#line 1501 "./parse.y"
    { (yyval.id) = intern("!~"); }
    break;

  case 126:

/* Line 1821 of yacc.c  */
#line 1502 "./parse.y"
    { (yyval.id) = intern(">"); }
    break;

  case 127:

/* Line 1821 of yacc.c  */
#line 1503 "./parse.y"
    { (yyval.id) = intern(">="); }
    break;

  case 128:

/* Line 1821 of yacc.c  */
#line 1504 "./parse.y"
    { (yyval.id) = intern("<"); }
    break;

  case 129:

/* Line 1821 of yacc.c  */
#line 1505 "./parse.y"
    { (yyval.id) = intern(">="); }
    break;

  case 130:

/* Line 1821 of yacc.c  */
#line 1506 "./parse.y"
    { (yyval.id) = intern("!="); }
    break;

  case 131:

/* Line 1821 of yacc.c  */
#line 1507 "./parse.y"
    { (yyval.id) = intern("<<"); }
    break;

  case 132:

/* Line 1821 of yacc.c  */
#line 1508 "./parse.y"
    { (yyval.id) = intern(">>"); }
    break;

  case 133:

/* Line 1821 of yacc.c  */
#line 1509 "./parse.y"
    { (yyval.id) = intern("+"); }
    break;

  case 134:

/* Line 1821 of yacc.c  */
#line 1510 "./parse.y"
    { (yyval.id) = intern("-"); }
    break;

  case 135:

/* Line 1821 of yacc.c  */
#line 1511 "./parse.y"
    { (yyval.id) = intern("*"); }
    break;

  case 136:

/* Line 1821 of yacc.c  */
#line 1512 "./parse.y"
    { (yyval.id) = intern("*"); }
    break;

  case 137:

/* Line 1821 of yacc.c  */
#line 1513 "./parse.y"
    { (yyval.id) = intern("/"); }
    break;

  case 138:

/* Line 1821 of yacc.c  */
#line 1514 "./parse.y"
    { (yyval.id) = intern("%"); }
    break;

  case 139:

/* Line 1821 of yacc.c  */
#line 1515 "./parse.y"
    { (yyval.id) = intern("**"); }
    break;

  case 140:

/* Line 1821 of yacc.c  */
#line 1516 "./parse.y"
    { (yyval.id) = intern("!"); }
    break;

  case 141:

/* Line 1821 of yacc.c  */
#line 1517 "./parse.y"
    { (yyval.id) = intern("~"); }
    break;

  case 142:

/* Line 1821 of yacc.c  */
#line 1518 "./parse.y"
    { (yyval.id) = intern("+@"); }
    break;

  case 143:

/* Line 1821 of yacc.c  */
#line 1519 "./parse.y"
    { (yyval.id) = intern("-@"); }
    break;

  case 144:

/* Line 1821 of yacc.c  */
#line 1520 "./parse.y"
    { (yyval.id) = intern("[]"); }
    break;

  case 145:

/* Line 1821 of yacc.c  */
#line 1521 "./parse.y"
    { (yyval.id) = intern("[]="); }
    break;

  case 186:

/* Line 1821 of yacc.c  */
#line 1539 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 187:

/* Line 1821 of yacc.c  */
#line 1543 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (5)].nd), new_rescue(p, (yyvsp[(3) - (5)].nd), list1(list3(0, 0, (yyvsp[(5) - (5)].nd))), 0));
		    }
    break;

  case 188:

/* Line 1821 of yacc.c  */
#line 1547 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 189:

/* Line 1821 of yacc.c  */
#line 1551 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, (yyvsp[(1) - (5)].nd), (yyvsp[(2) - (5)].id), new_rescue(p, (yyvsp[(3) - (5)].nd), list1(list3(0, 0, (yyvsp[(5) - (5)].nd))), 0));
		    }
    break;

  case 190:

/* Line 1821 of yacc.c  */
#line 1555 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].nd), intern("[]"), (yyvsp[(3) - (6)].nd)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 191:

/* Line 1821 of yacc.c  */
#line 1559 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 192:

/* Line 1821 of yacc.c  */
#line 1563 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 193:

/* Line 1821 of yacc.c  */
#line 1567 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 194:

/* Line 1821 of yacc.c  */
#line 1571 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 195:

/* Line 1821 of yacc.c  */
#line 1576 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 196:

/* Line 1821 of yacc.c  */
#line 1581 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].nd));
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 197:

/* Line 1821 of yacc.c  */
#line 1586 "./parse.y"
    {
		      (yyval.nd) = new_dot2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 198:

/* Line 1821 of yacc.c  */
#line 1590 "./parse.y"
    {
		      (yyval.nd) = new_dot3(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 199:

/* Line 1821 of yacc.c  */
#line 1594 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "+", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 200:

/* Line 1821 of yacc.c  */
#line 1598 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "-", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 201:

/* Line 1821 of yacc.c  */
#line 1602 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "*", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 202:

/* Line 1821 of yacc.c  */
#line 1606 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "/", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 203:

/* Line 1821 of yacc.c  */
#line 1610 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "%", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 204:

/* Line 1821 of yacc.c  */
#line 1614 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "**", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 205:

/* Line 1821 of yacc.c  */
#line 1618 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].nd), "**", (yyvsp[(4) - (4)].nd)), "-@");
		    }
    break;

  case 206:

/* Line 1821 of yacc.c  */
#line 1622 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].nd), "**", (yyvsp[(4) - (4)].nd)), "-@");
		    }
    break;

  case 207:

/* Line 1821 of yacc.c  */
#line 1626 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, (yyvsp[(2) - (2)].nd), "+@");
		    }
    break;

  case 208:

/* Line 1821 of yacc.c  */
#line 1630 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, (yyvsp[(2) - (2)].nd), "-@");
		    }
    break;

  case 209:

/* Line 1821 of yacc.c  */
#line 1634 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "|", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 210:

/* Line 1821 of yacc.c  */
#line 1638 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "^", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 211:

/* Line 1821 of yacc.c  */
#line 1642 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "&", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 212:

/* Line 1821 of yacc.c  */
#line 1646 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<=>", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 213:

/* Line 1821 of yacc.c  */
#line 1650 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), ">", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 214:

/* Line 1821 of yacc.c  */
#line 1654 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), ">=", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 215:

/* Line 1821 of yacc.c  */
#line 1658 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 216:

/* Line 1821 of yacc.c  */
#line 1662 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<=", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 217:

/* Line 1821 of yacc.c  */
#line 1666 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "==", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 218:

/* Line 1821 of yacc.c  */
#line 1670 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "===", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 219:

/* Line 1821 of yacc.c  */
#line 1674 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "!=", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 220:

/* Line 1821 of yacc.c  */
#line 1678 "./parse.y"
    {
		      (yyval.nd) = match_op(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
#if 0
		      if (nd_type((yyvsp[(1) - (3)].nd)) == NODE_LIT && TYPE((yyvsp[(1) - (3)].nd)->nd_lit) == T_REGEXP) {
			(yyval.nd) = reg_named_capture_assign((yyvsp[(1) - (3)].nd)->nd_lit, (yyval.nd));
		      }
#endif
		    }
    break;

  case 221:

/* Line 1821 of yacc.c  */
#line 1687 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "!~", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 222:

/* Line 1821 of yacc.c  */
#line 1691 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(2) - (2)].nd)), "!");
		    }
    break;

  case 223:

/* Line 1821 of yacc.c  */
#line 1695 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(2) - (2)].nd)), "~");
		    }
    break;

  case 224:

/* Line 1821 of yacc.c  */
#line 1699 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<<", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 225:

/* Line 1821 of yacc.c  */
#line 1703 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), ">>", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 226:

/* Line 1821 of yacc.c  */
#line 1707 "./parse.y"
    {
		      (yyval.nd) = new_and(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 227:

/* Line 1821 of yacc.c  */
#line 1711 "./parse.y"
    {
		      (yyval.nd) = new_or(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 228:

/* Line 1821 of yacc.c  */
#line 1715 "./parse.y"
    {
		      (yyval.nd) = new_if(p, cond((yyvsp[(1) - (6)].nd)), (yyvsp[(3) - (6)].nd), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 229:

/* Line 1821 of yacc.c  */
#line 1719 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 230:

/* Line 1821 of yacc.c  */
#line 1725 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		      if (!(yyval.nd)) (yyval.nd) = new_nil(p);
		    }
    break;

  case 232:

/* Line 1821 of yacc.c  */
#line 1733 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 233:

/* Line 1821 of yacc.c  */
#line 1737 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), new_hash(p, (yyvsp[(3) - (4)].nd)));
		    }
    break;

  case 234:

/* Line 1821 of yacc.c  */
#line 1741 "./parse.y"
    {
		      (yyval.nd) = new_hash(p, (yyvsp[(1) - (2)].nd));
		    }
    break;

  case 235:

/* Line 1821 of yacc.c  */
#line 1747 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 240:

/* Line 1821 of yacc.c  */
#line 1759 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (2)].nd),0);
		    }
    break;

  case 241:

/* Line 1821 of yacc.c  */
#line 1763 "./parse.y"
    {
		      (yyval.nd) = cons(push((yyvsp[(1) - (4)].nd), new_hash(p, (yyvsp[(3) - (4)].nd))), 0);
		    }
    break;

  case 242:

/* Line 1821 of yacc.c  */
#line 1767 "./parse.y"
    {
		      (yyval.nd) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].nd))), 0);
		    }
    break;

  case 243:

/* Line 1821 of yacc.c  */
#line 1773 "./parse.y"
    {
		      (yyval.nd) = cons(list1((yyvsp[(1) - (1)].nd)), 0);
		    }
    break;

  case 244:

/* Line 1821 of yacc.c  */
#line 1777 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 245:

/* Line 1821 of yacc.c  */
#line 1781 "./parse.y"
    {
		      (yyval.nd) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].nd))), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 246:

/* Line 1821 of yacc.c  */
#line 1785 "./parse.y"
    {
		      (yyval.nd) = cons(push((yyvsp[(1) - (4)].nd), new_hash(p, (yyvsp[(3) - (4)].nd))), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 247:

/* Line 1821 of yacc.c  */
#line 1789 "./parse.y"
    {
		      (yyval.nd) = cons(0, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 248:

/* Line 1821 of yacc.c  */
#line 1794 "./parse.y"
    {
		      (yyval.stack) = p->cmdarg_stack;
		      CMDARG_PUSH(1);
		    }
    break;

  case 249:

/* Line 1821 of yacc.c  */
#line 1799 "./parse.y"
    {
		      p->cmdarg_stack = (yyvsp[(1) - (2)].stack);
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 250:

/* Line 1821 of yacc.c  */
#line 1806 "./parse.y"
    {
		      (yyval.nd) = new_block_arg(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 251:

/* Line 1821 of yacc.c  */
#line 1812 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 252:

/* Line 1821 of yacc.c  */
#line 1816 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 253:

/* Line 1821 of yacc.c  */
#line 1822 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (1)].nd), 0);
		    }
    break;

  case 254:

/* Line 1821 of yacc.c  */
#line 1826 "./parse.y"
    {
		      (yyval.nd) = cons(new_splat(p, (yyvsp[(2) - (2)].nd)), 0);
		    }
    break;

  case 255:

/* Line 1821 of yacc.c  */
#line 1830 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 256:

/* Line 1821 of yacc.c  */
#line 1834 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), new_splat(p, (yyvsp[(4) - (4)].nd)));
		    }
    break;

  case 257:

/* Line 1821 of yacc.c  */
#line 1840 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 258:

/* Line 1821 of yacc.c  */
#line 1844 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), new_splat(p, (yyvsp[(4) - (4)].nd)));
		    }
    break;

  case 259:

/* Line 1821 of yacc.c  */
#line 1848 "./parse.y"
    {
		      (yyval.nd) = list1(new_splat(p, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 265:

/* Line 1821 of yacc.c  */
#line 1859 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (1)].id), 0);
		    }
    break;

  case 266:

/* Line 1821 of yacc.c  */
#line 1863 "./parse.y"
    {
		      (yyvsp[(1) - (1)].stack) = p->cmdarg_stack;
		      p->cmdarg_stack = 0;
		    }
    break;

  case 267:

/* Line 1821 of yacc.c  */
#line 1869 "./parse.y"
    {
		      p->cmdarg_stack = (yyvsp[(1) - (4)].stack);
		      (yyval.nd) = (yyvsp[(3) - (4)].nd);
		    }
    break;

  case 268:

/* Line 1821 of yacc.c  */
#line 1873 "./parse.y"
    {p->lstate = EXPR_ENDARG;}
    break;

  case 269:

/* Line 1821 of yacc.c  */
#line 1874 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (4)].nd);
		    }
    break;

  case 270:

/* Line 1821 of yacc.c  */
#line 1877 "./parse.y"
    {p->lstate = EXPR_ENDARG;}
    break;

  case 271:

/* Line 1821 of yacc.c  */
#line 1878 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 272:

/* Line 1821 of yacc.c  */
#line 1882 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 273:

/* Line 1821 of yacc.c  */
#line 1886 "./parse.y"
    {
		      (yyval.nd) = new_colon2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 274:

/* Line 1821 of yacc.c  */
#line 1890 "./parse.y"
    {
		      (yyval.nd) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 275:

/* Line 1821 of yacc.c  */
#line 1894 "./parse.y"
    {
		      (yyval.nd) = new_array(p, (yyvsp[(2) - (3)].nd));
		    }
    break;

  case 276:

/* Line 1821 of yacc.c  */
#line 1898 "./parse.y"
    {
		      (yyval.nd) = new_hash(p, (yyvsp[(2) - (3)].nd));
		    }
    break;

  case 277:

/* Line 1821 of yacc.c  */
#line 1902 "./parse.y"
    {
		      (yyval.nd) = new_return(p, 0);
		    }
    break;

  case 278:

/* Line 1821 of yacc.c  */
#line 1906 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 279:

/* Line 1821 of yacc.c  */
#line 1910 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, 0);
		    }
    break;

  case 280:

/* Line 1821 of yacc.c  */
#line 1914 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, 0);
		    }
    break;

  case 281:

/* Line 1821 of yacc.c  */
#line 1918 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(3) - (4)].nd)), "!");
		    }
    break;

  case 282:

/* Line 1821 of yacc.c  */
#line 1922 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, new_nil(p), "!");
		    }
    break;

  case 283:

/* Line 1821 of yacc.c  */
#line 1926 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (2)].id), cons(0, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 285:

/* Line 1821 of yacc.c  */
#line 1931 "./parse.y"
    {
		      call_with_block(p, (yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 286:

/* Line 1821 of yacc.c  */
#line 1936 "./parse.y"
    {
		      local_nest(p);
		      (yyval.num) = p->lpar_beg;
		      p->lpar_beg = ++p->paren_nest;
		    }
    break;

  case 287:

/* Line 1821 of yacc.c  */
#line 1943 "./parse.y"
    {
		      p->lpar_beg = (yyvsp[(2) - (4)].num);
		      (yyval.nd) = new_lambda(p, (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].nd));
		      local_unnest(p);
		    }
    break;

  case 288:

/* Line 1821 of yacc.c  */
#line 1952 "./parse.y"
    {
		      (yyval.nd) = new_if(p, cond((yyvsp[(2) - (6)].nd)), (yyvsp[(4) - (6)].nd), (yyvsp[(5) - (6)].nd));
		    }
    break;

  case 289:

/* Line 1821 of yacc.c  */
#line 1959 "./parse.y"
    {
		      (yyval.nd) = new_unless(p, cond((yyvsp[(2) - (6)].nd)), (yyvsp[(4) - (6)].nd), (yyvsp[(5) - (6)].nd));
		    }
    break;

  case 290:

/* Line 1821 of yacc.c  */
#line 1962 "./parse.y"
    {COND_PUSH(1);}
    break;

  case 291:

/* Line 1821 of yacc.c  */
#line 1962 "./parse.y"
    {COND_POP();}
    break;

  case 292:

/* Line 1821 of yacc.c  */
#line 1965 "./parse.y"
    {
		      (yyval.nd) = new_while(p, cond((yyvsp[(3) - (7)].nd)), (yyvsp[(6) - (7)].nd));
		    }
    break;

  case 293:

/* Line 1821 of yacc.c  */
#line 1968 "./parse.y"
    {COND_PUSH(1);}
    break;

  case 294:

/* Line 1821 of yacc.c  */
#line 1968 "./parse.y"
    {COND_POP();}
    break;

  case 295:

/* Line 1821 of yacc.c  */
#line 1971 "./parse.y"
    {
		      (yyval.nd) = new_until(p, cond((yyvsp[(3) - (7)].nd)), (yyvsp[(6) - (7)].nd));
		    }
    break;

  case 296:

/* Line 1821 of yacc.c  */
#line 1977 "./parse.y"
    {
		      (yyval.nd) = new_case(p, (yyvsp[(2) - (5)].nd), (yyvsp[(4) - (5)].nd));
		    }
    break;

  case 297:

/* Line 1821 of yacc.c  */
#line 1981 "./parse.y"
    {
		      (yyval.nd) = new_case(p, 0, (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 298:

/* Line 1821 of yacc.c  */
#line 1985 "./parse.y"
    {COND_PUSH(1);}
    break;

  case 299:

/* Line 1821 of yacc.c  */
#line 1987 "./parse.y"
    {COND_POP();}
    break;

  case 300:

/* Line 1821 of yacc.c  */
#line 1990 "./parse.y"
    {
		      (yyval.nd) = new_for(p, (yyvsp[(2) - (9)].nd), (yyvsp[(5) - (9)].nd), (yyvsp[(8) - (9)].nd));
		    }
    break;

  case 301:

/* Line 1821 of yacc.c  */
#line 1994 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "class definition in method body");
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 302:

/* Line 1821 of yacc.c  */
#line 2001 "./parse.y"
    {
		      (yyval.nd) = new_class(p, (yyvsp[(2) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].nd));
		      local_resume(p, (yyvsp[(4) - (6)].nd));
		    }
    break;

  case 303:

/* Line 1821 of yacc.c  */
#line 2006 "./parse.y"
    {
		      (yyval.num) = p->in_def;
		      p->in_def = 0;
		    }
    break;

  case 304:

/* Line 1821 of yacc.c  */
#line 2011 "./parse.y"
    {
		      (yyval.nd) = cons(local_switch(p), (node*)(intptr_t)p->in_single);
		      p->in_single = 0;
		    }
    break;

  case 305:

/* Line 1821 of yacc.c  */
#line 2017 "./parse.y"
    {
		      (yyval.nd) = new_sclass(p, (yyvsp[(3) - (8)].nd), (yyvsp[(7) - (8)].nd));
		      local_resume(p, (yyvsp[(6) - (8)].nd)->car);
		      p->in_def = (yyvsp[(4) - (8)].num);
		      p->in_single = (int)(intptr_t)(yyvsp[(6) - (8)].nd)->cdr;
		    }
    break;

  case 306:

/* Line 1821 of yacc.c  */
#line 2024 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "module definition in method body");
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 307:

/* Line 1821 of yacc.c  */
#line 2031 "./parse.y"
    {
		      (yyval.nd) = new_module(p, (yyvsp[(2) - (5)].nd), (yyvsp[(4) - (5)].nd));
		      local_resume(p, (yyvsp[(3) - (5)].nd));
		    }
    break;

  case 308:

/* Line 1821 of yacc.c  */
#line 2036 "./parse.y"
    {
		      p->in_def++;
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 309:

/* Line 1821 of yacc.c  */
#line 2043 "./parse.y"
    {
		      (yyval.nd) = new_def(p, (yyvsp[(2) - (6)].id), (yyvsp[(4) - (6)].nd), (yyvsp[(5) - (6)].nd));
		      local_resume(p, (yyvsp[(3) - (6)].nd));
		      p->in_def--;
		    }
    break;

  case 310:

/* Line 1821 of yacc.c  */
#line 2048 "./parse.y"
    {p->lstate = EXPR_FNAME;}
    break;

  case 311:

/* Line 1821 of yacc.c  */
#line 2049 "./parse.y"
    {
		      p->in_single++;
		      p->lstate = EXPR_ENDFN; /* force for args */
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 312:

/* Line 1821 of yacc.c  */
#line 2057 "./parse.y"
    {
		      (yyval.nd) = new_sdef(p, (yyvsp[(2) - (9)].nd), (yyvsp[(5) - (9)].id), (yyvsp[(7) - (9)].nd), (yyvsp[(8) - (9)].nd));
		      local_resume(p, (yyvsp[(6) - (9)].nd));
		      p->in_single--;
		    }
    break;

  case 313:

/* Line 1821 of yacc.c  */
#line 2063 "./parse.y"
    {
		      (yyval.nd) = new_break(p, 0);
		    }
    break;

  case 314:

/* Line 1821 of yacc.c  */
#line 2067 "./parse.y"
    {
		      (yyval.nd) = new_next(p, 0);
		    }
    break;

  case 315:

/* Line 1821 of yacc.c  */
#line 2071 "./parse.y"
    {
		      (yyval.nd) = new_redo(p);
		    }
    break;

  case 316:

/* Line 1821 of yacc.c  */
#line 2075 "./parse.y"
    {
		      (yyval.nd) = new_retry(p);
		    }
    break;

  case 317:

/* Line 1821 of yacc.c  */
#line 2081 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		      if (!(yyval.nd)) (yyval.nd) = new_nil(p);
		    }
    break;

  case 324:

/* Line 1821 of yacc.c  */
#line 2100 "./parse.y"
    {
		      (yyval.nd) = new_if(p, cond((yyvsp[(2) - (5)].nd)), (yyvsp[(4) - (5)].nd), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 326:

/* Line 1821 of yacc.c  */
#line 2107 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 327:

/* Line 1821 of yacc.c  */
#line 2113 "./parse.y"
    {
		      (yyval.nd) = list1(list1((yyvsp[(1) - (1)].nd)));
		    }
    break;

  case 329:

/* Line 1821 of yacc.c  */
#line 2120 "./parse.y"
    {
		      (yyval.nd) = new_arg(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 330:

/* Line 1821 of yacc.c  */
#line 2124 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(2) - (3)].nd), 0);
		    }
    break;

  case 331:

/* Line 1821 of yacc.c  */
#line 2130 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 332:

/* Line 1821 of yacc.c  */
#line 2134 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 333:

/* Line 1821 of yacc.c  */
#line 2140 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (1)].nd),0,0);
		    }
    break;

  case 334:

/* Line 1821 of yacc.c  */
#line 2144 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (4)].nd), new_arg(p, (yyvsp[(4) - (4)].id)), 0);
		    }
    break;

  case 335:

/* Line 1821 of yacc.c  */
#line 2148 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (6)].nd), new_arg(p, (yyvsp[(4) - (6)].id)), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 336:

/* Line 1821 of yacc.c  */
#line 2152 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (3)].nd), (node*)-1, 0);
		    }
    break;

  case 337:

/* Line 1821 of yacc.c  */
#line 2156 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (5)].nd), (node*)-1, (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 338:

/* Line 1821 of yacc.c  */
#line 2160 "./parse.y"
    {
		      (yyval.nd) = list3(0, new_arg(p, (yyvsp[(2) - (2)].id)), 0);
		    }
    break;

  case 339:

/* Line 1821 of yacc.c  */
#line 2164 "./parse.y"
    {
		      (yyval.nd) = list3(0, new_arg(p, (yyvsp[(2) - (4)].id)), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 340:

/* Line 1821 of yacc.c  */
#line 2168 "./parse.y"
    {
		      (yyval.nd) = list3(0, (node*)-1, 0);
		    }
    break;

  case 341:

/* Line 1821 of yacc.c  */
#line 2172 "./parse.y"
    {
		      (yyval.nd) = list3(0, (node*)-1, (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 342:

/* Line 1821 of yacc.c  */
#line 2178 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    }
    break;

  case 343:

/* Line 1821 of yacc.c  */
#line 2182 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (8)].nd), (yyvsp[(3) - (8)].nd), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].nd), (yyvsp[(8) - (8)].id));
		    }
    break;

  case 344:

/* Line 1821 of yacc.c  */
#line 2186 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].nd), 0, 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 345:

/* Line 1821 of yacc.c  */
#line 2190 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), 0, (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 346:

/* Line 1821 of yacc.c  */
#line 2194 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 347:

/* Line 1821 of yacc.c  */
#line 2198 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (2)].nd), 0, 1, 0, 0);
		    }
    break;

  case 348:

/* Line 1821 of yacc.c  */
#line 2202 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 349:

/* Line 1821 of yacc.c  */
#line 2206 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (2)].nd), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 350:

/* Line 1821 of yacc.c  */
#line 2210 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 351:

/* Line 1821 of yacc.c  */
#line 2214 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 352:

/* Line 1821 of yacc.c  */
#line 2218 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (2)].nd), 0, 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 353:

/* Line 1821 of yacc.c  */
#line 2222 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 354:

/* Line 1821 of yacc.c  */
#line 2226 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 355:

/* Line 1821 of yacc.c  */
#line 2230 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 356:

/* Line 1821 of yacc.c  */
#line 2234 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 358:

/* Line 1821 of yacc.c  */
#line 2241 "./parse.y"
    {
		      p->cmd_start = TRUE;
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 359:

/* Line 1821 of yacc.c  */
#line 2248 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.nd) = 0;
		    }
    break;

  case 360:

/* Line 1821 of yacc.c  */
#line 2253 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.nd) = 0;
		    }
    break;

  case 361:

/* Line 1821 of yacc.c  */
#line 2258 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (4)].nd);
		    }
    break;

  case 362:

/* Line 1821 of yacc.c  */
#line 2265 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 363:

/* Line 1821 of yacc.c  */
#line 2269 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 366:

/* Line 1821 of yacc.c  */
#line 2279 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      new_bv(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 368:

/* Line 1821 of yacc.c  */
#line 2287 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (4)].nd);
		    }
    break;

  case 369:

/* Line 1821 of yacc.c  */
#line 2291 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 370:

/* Line 1821 of yacc.c  */
#line 2297 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 371:

/* Line 1821 of yacc.c  */
#line 2301 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 372:

/* Line 1821 of yacc.c  */
#line 2307 "./parse.y"
    {
		      local_nest(p);
		    }
    break;

  case 373:

/* Line 1821 of yacc.c  */
#line 2313 "./parse.y"
    {
		      (yyval.nd) = new_block(p,(yyvsp[(3) - (5)].nd),(yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    }
    break;

  case 374:

/* Line 1821 of yacc.c  */
#line 2320 "./parse.y"
    {
		      if ((yyvsp[(1) - (2)].nd)->car == (node*)NODE_YIELD) {
			yyerror(p, "block given to yield");
		      }
		      else {
		        call_with_block(p, (yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		      }
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 375:

/* Line 1821 of yacc.c  */
#line 2330 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 376:

/* Line 1821 of yacc.c  */
#line 2334 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		      call_with_block(p, (yyval.nd), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 377:

/* Line 1821 of yacc.c  */
#line 2339 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		      call_with_block(p, (yyval.nd), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 378:

/* Line 1821 of yacc.c  */
#line 2346 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 379:

/* Line 1821 of yacc.c  */
#line 2350 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 380:

/* Line 1821 of yacc.c  */
#line 2354 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 381:

/* Line 1821 of yacc.c  */
#line 2358 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 382:

/* Line 1821 of yacc.c  */
#line 2362 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), intern("call"), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 383:

/* Line 1821 of yacc.c  */
#line 2366 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), intern("call"), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 384:

/* Line 1821 of yacc.c  */
#line 2370 "./parse.y"
    {
		      (yyval.nd) = new_super(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 385:

/* Line 1821 of yacc.c  */
#line 2374 "./parse.y"
    {
		      (yyval.nd) = new_zsuper(p);
		    }
    break;

  case 386:

/* Line 1821 of yacc.c  */
#line 2378 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), intern("[]"), (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 387:

/* Line 1821 of yacc.c  */
#line 2384 "./parse.y"
    {
		      local_nest(p);
		    }
    break;

  case 388:

/* Line 1821 of yacc.c  */
#line 2389 "./parse.y"
    {
		      (yyval.nd) = new_block(p,(yyvsp[(3) - (5)].nd),(yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    }
    break;

  case 389:

/* Line 1821 of yacc.c  */
#line 2394 "./parse.y"
    {
		      local_nest(p);
		    }
    break;

  case 390:

/* Line 1821 of yacc.c  */
#line 2399 "./parse.y"
    {
		      (yyval.nd) = new_block(p,(yyvsp[(3) - (5)].nd),(yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    }
    break;

  case 391:

/* Line 1821 of yacc.c  */
#line 2408 "./parse.y"
    {
		      (yyval.nd) = cons(cons((yyvsp[(2) - (5)].nd), (yyvsp[(4) - (5)].nd)), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 392:

/* Line 1821 of yacc.c  */
#line 2414 "./parse.y"
    {
		      if ((yyvsp[(1) - (1)].nd)) {
			(yyval.nd) = cons(cons(0, (yyvsp[(1) - (1)].nd)), 0);
		      }
		      else {
			(yyval.nd) = 0;
		      }
		    }
    break;

  case 394:

/* Line 1821 of yacc.c  */
#line 2428 "./parse.y"
    {
		      (yyval.nd) = list1(list3((yyvsp[(2) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].nd)));
		      if ((yyvsp[(6) - (6)].nd)) (yyval.nd) = append((yyval.nd), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 396:

/* Line 1821 of yacc.c  */
#line 2436 "./parse.y"
    {
			(yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 399:

/* Line 1821 of yacc.c  */
#line 2444 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 401:

/* Line 1821 of yacc.c  */
#line 2451 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 404:

/* Line 1821 of yacc.c  */
#line 2459 "./parse.y"
    {
		      (yyval.nd) = new_sym(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 407:

/* Line 1821 of yacc.c  */
#line 2467 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 408:

/* Line 1821 of yacc.c  */
#line 2471 "./parse.y"
    {
		      (yyval.nd) = new_dstr(p, push((yyvsp[(2) - (3)].nd), (yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 409:

/* Line 1821 of yacc.c  */
#line 2477 "./parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    }
    break;

  case 410:

/* Line 1821 of yacc.c  */
#line 2483 "./parse.y"
    {
		      p->sterm = (yyvsp[(2) - (4)].num);
		      (yyval.nd) = list2((yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 411:

/* Line 1821 of yacc.c  */
#line 2489 "./parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    }
    break;

  case 412:

/* Line 1821 of yacc.c  */
#line 2495 "./parse.y"
    {
		      p->sterm = (yyvsp[(3) - (5)].num);
		      (yyval.nd) = push(push((yyvsp[(1) - (5)].nd), (yyvsp[(2) - (5)].nd)), (yyvsp[(4) - (5)].nd));
		    }
    break;

  case 414:

/* Line 1821 of yacc.c  */
#line 2505 "./parse.y"
    {
		      p->lstate = EXPR_END;
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    }
    break;

  case 421:

/* Line 1821 of yacc.c  */
#line 2520 "./parse.y"
    {
		      (yyval.nd) = negate_lit(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 422:

/* Line 1821 of yacc.c  */
#line 2524 "./parse.y"
    {
		      (yyval.nd) = negate_lit(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 423:

/* Line 1821 of yacc.c  */
#line 2530 "./parse.y"
    {
		      (yyval.nd) = new_lvar(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 424:

/* Line 1821 of yacc.c  */
#line 2534 "./parse.y"
    {
		      (yyval.nd) = new_ivar(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 425:

/* Line 1821 of yacc.c  */
#line 2538 "./parse.y"
    {
		      (yyval.nd) = new_gvar(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 426:

/* Line 1821 of yacc.c  */
#line 2542 "./parse.y"
    {
		      (yyval.nd) = new_cvar(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 427:

/* Line 1821 of yacc.c  */
#line 2546 "./parse.y"
    {
		      (yyval.nd) = new_const(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 428:

/* Line 1821 of yacc.c  */
#line 2552 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 429:

/* Line 1821 of yacc.c  */
#line 2558 "./parse.y"
    {
		      (yyval.nd) = var_reference(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 430:

/* Line 1821 of yacc.c  */
#line 2562 "./parse.y"
    {
		      (yyval.nd) = new_nil(p);
		    }
    break;

  case 431:

/* Line 1821 of yacc.c  */
#line 2566 "./parse.y"
    {
		      (yyval.nd) = new_self(p);
   		    }
    break;

  case 432:

/* Line 1821 of yacc.c  */
#line 2570 "./parse.y"
    {
		      (yyval.nd) = new_true(p);
   		    }
    break;

  case 433:

/* Line 1821 of yacc.c  */
#line 2574 "./parse.y"
    {
		      (yyval.nd) = new_false(p);
   		    }
    break;

  case 434:

/* Line 1821 of yacc.c  */
#line 2578 "./parse.y"
    {
		      if (!p->filename) {
			p->filename = "(null)";
		      }
		      (yyval.nd) = new_str(p, p->filename, strlen(p->filename));
		    }
    break;

  case 435:

/* Line 1821 of yacc.c  */
#line 2585 "./parse.y"
    {
		      char buf[16];

		      snprintf(buf, 16, "%d", p->lineno);
		      (yyval.nd) = new_int(p, buf, 10);
		    }
    break;

  case 438:

/* Line 1821 of yacc.c  */
#line 2598 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 439:

/* Line 1821 of yacc.c  */
#line 2602 "./parse.y"
    {
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    }
    break;

  case 440:

/* Line 1821 of yacc.c  */
#line 2607 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(3) - (4)].nd);
		    }
    break;

  case 441:

/* Line 1821 of yacc.c  */
#line 2611 "./parse.y"
    {
		      yyerrok;
		      (yyval.nd) = 0;
		    }
    break;

  case 442:

/* Line 1821 of yacc.c  */
#line 2618 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    }
    break;

  case 443:

/* Line 1821 of yacc.c  */
#line 2624 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 444:

/* Line 1821 of yacc.c  */
#line 2630 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    }
    break;

  case 445:

/* Line 1821 of yacc.c  */
#line 2634 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (8)].nd), (yyvsp[(3) - (8)].nd), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].nd), (yyvsp[(8) - (8)].id));
		    }
    break;

  case 446:

/* Line 1821 of yacc.c  */
#line 2638 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].nd), 0, 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 447:

/* Line 1821 of yacc.c  */
#line 2642 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), 0, (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 448:

/* Line 1821 of yacc.c  */
#line 2646 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 449:

/* Line 1821 of yacc.c  */
#line 2650 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 450:

/* Line 1821 of yacc.c  */
#line 2654 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (2)].nd), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 451:

/* Line 1821 of yacc.c  */
#line 2658 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 452:

/* Line 1821 of yacc.c  */
#line 2662 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 453:

/* Line 1821 of yacc.c  */
#line 2666 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (2)].nd), 0, 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 454:

/* Line 1821 of yacc.c  */
#line 2670 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 455:

/* Line 1821 of yacc.c  */
#line 2674 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 456:

/* Line 1821 of yacc.c  */
#line 2678 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 457:

/* Line 1821 of yacc.c  */
#line 2682 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 458:

/* Line 1821 of yacc.c  */
#line 2686 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.nd) = new_args(p, 0, 0, 0, 0, 0);
		    }
    break;

  case 459:

/* Line 1821 of yacc.c  */
#line 2693 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a constant");
		      (yyval.nd) = 0;
		    }
    break;

  case 460:

/* Line 1821 of yacc.c  */
#line 2698 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be an instance variable");
		      (yyval.nd) = 0;
		    }
    break;

  case 461:

/* Line 1821 of yacc.c  */
#line 2703 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a global variable");
		      (yyval.nd) = 0;
		    }
    break;

  case 462:

/* Line 1821 of yacc.c  */
#line 2708 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a class variable");
		      (yyval.nd) = 0;
		    }
    break;

  case 463:

/* Line 1821 of yacc.c  */
#line 2715 "./parse.y"
    {
		      (yyval.id) = 0;
		    }
    break;

  case 464:

/* Line 1821 of yacc.c  */
#line 2719 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    }
    break;

  case 465:

/* Line 1821 of yacc.c  */
#line 2726 "./parse.y"
    {
		      (yyval.nd) = new_arg(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 466:

/* Line 1821 of yacc.c  */
#line 2730 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(2) - (3)].nd), 0);
		    }
    break;

  case 467:

/* Line 1821 of yacc.c  */
#line 2736 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 468:

/* Line 1821 of yacc.c  */
#line 2740 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 469:

/* Line 1821 of yacc.c  */
#line 2746 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.nd) = cons((node*)(yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 470:

/* Line 1821 of yacc.c  */
#line 2753 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.nd) = cons((node*)(yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 471:

/* Line 1821 of yacc.c  */
#line 2760 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 472:

/* Line 1821 of yacc.c  */
#line 2764 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 473:

/* Line 1821 of yacc.c  */
#line 2770 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 474:

/* Line 1821 of yacc.c  */
#line 2774 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 477:

/* Line 1821 of yacc.c  */
#line 2784 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    }
    break;

  case 478:

/* Line 1821 of yacc.c  */
#line 2789 "./parse.y"
    {
		      (yyval.id) = 0;
		    }
    break;

  case 481:

/* Line 1821 of yacc.c  */
#line 2799 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    }
    break;

  case 482:

/* Line 1821 of yacc.c  */
#line 2806 "./parse.y"
    {
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    }
    break;

  case 483:

/* Line 1821 of yacc.c  */
#line 2810 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.id) = 0;
		    }
    break;

  case 484:

/* Line 1821 of yacc.c  */
#line 2817 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		      if (!(yyval.nd)) (yyval.nd) = new_nil(p);
		    }
    break;

  case 485:

/* Line 1821 of yacc.c  */
#line 2821 "./parse.y"
    {p->lstate = EXPR_BEG;}
    break;

  case 486:

/* Line 1821 of yacc.c  */
#line 2822 "./parse.y"
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
		    }
    break;

  case 488:

/* Line 1821 of yacc.c  */
#line 2845 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 489:

/* Line 1821 of yacc.c  */
#line 2851 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 490:

/* Line 1821 of yacc.c  */
#line 2855 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 491:

/* Line 1821 of yacc.c  */
#line 2861 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 492:

/* Line 1821 of yacc.c  */
#line 2865 "./parse.y"
    {
		      (yyval.nd) = cons(new_sym(p, (yyvsp[(1) - (2)].id)), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 514:

/* Line 1821 of yacc.c  */
#line 2909 "./parse.y"
    {yyerrok;}
    break;

  case 517:

/* Line 1821 of yacc.c  */
#line 2914 "./parse.y"
    {yyerrok;}
    break;

  case 518:

/* Line 1821 of yacc.c  */
#line 2918 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;



/* Line 1821 of yacc.c  */
#line 8642 "./y.tab.c"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
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
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (p, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (p, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
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
      if (!yypact_value_is_default (yyn))
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
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, p);
    }
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



/* Line 2067 of yacc.c  */
#line 2922 "./parse.y"

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

  snprintf(buf, 256, fmt, i);
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

  snprintf(buf, 256, fmt, s);
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
scan_oct(const char *start, int len, int *retlen)
{
  const char *s = start;
  unsigned long retval = 0;

  while (len-- && *s >= '0' && *s <= '7') {
    retval <<= 3;
    retval |= *s++ - '0';
  }
  *retlen = s - start;
  return retval;
}

static unsigned long
scan_hex(const char *start, int len, int *retlen)
{
  static const char hexdigit[] = "0123456789abcdef0123456789ABCDEF";
  register const char *s = start;
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
       char buf[3];
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
      char buf[2];
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

static int
parse_qstring(parser_state *p, int term)
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
  yylval.nd = new_str(p, tok(p), toklen(p));
  p->lstate = EXPR_END;
  return tSTRING;
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
	  snprintf(buf, 256, "invalid character syntax; use ?\\%c", c2);
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
    switch (c) {
    case '\'':
#if 0
      p->lex_strterm = new_strterm(p, str_ssym, c, 0);
#endif
      break;
    case '"':
#if 0
      p->lex_strterm = new_strterm(p, str_dsym, c, 0);
#endif
      break;
    default:
      pushback(p, c);
      break;
    }
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
	if (result == 0 && isupper(tok(p)[0])) {
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

#define PARSER_DUMP

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
#ifdef PARSER_DUMP
  parser_dump(mrb, p->tree, 0);
#endif
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
#ifdef PARSER_DUMP
  parser_dump(mrb, p->tree, 0);
#endif
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
    parser_dump(mrb, tree->cdr, offset+2);
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
    dump_prefix(offset+1);
    printf("local variables:\n");
    {
      node *n2 = tree->car;

      while (n2) {
	dump_prefix(offset+2);
	printf("%s\n", mrb_sym2name(mrb, (mrb_sym)n2->car));
	n2 = n2->cdr;
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
    dump_prefix(offset+1);
    printf("local variables:\n");
    {
      node *n2 = tree->car;

      while (n2) {
	dump_prefix(offset+2);
	if (n2->car)
	  printf("%s\n", mrb_sym2name(mrb, (mrb_sym)n2->car));
	n2 = n2->cdr;
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

