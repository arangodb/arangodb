
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
#line 7 "../../src/parse.y"

#undef PARSER_TEST
#undef PARSER_DEBUG

#define YYDEBUG 1
#define YYERROR_VERBOSE 1
#define YYSTACK_USE_ALLOCA 0

#include "mruby.h"
#include "st.h"
#include "compile.h"
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

#define TRUE  1
#define FALSE 0

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
  return list3((node*)NODE_INT, (node*)strdup(s), (node*)base);
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
  return cons((node*)NODE_BACK_REF, (node*)n);
}

// (:nthref . n)
static node*
new_nth_ref(parser_state *p, int n)
{
  return cons((node*)NODE_NTH_REF, (node*)n);
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
  switch ((int)lhs->car) {
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

  switch ((int)lhs->car) {
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
#line 876 "../../src/y.tab.c"

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
#line 813 "../../src/parse.y"

    node *node;
    mrb_sym id;
    int num;
    const struct vtable *vars;



/* Line 214 of yacc.c  */
#line 1039 "../../src/y.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 1051 "../../src/y.tab.c"

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
#define YYLAST   9972

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  144
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  143
/* YYNRULES -- Number of rules.  */
#define YYNRULES  515
/* YYNRULES -- Number of states.  */
#define YYNSTATES  915

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
     811,   813,   815,   817,   819,   821,   823,   827,   828,   833,
     837,   841,   844,   848,   852,   854,   859,   863,   865,   870,
     874,   877,   879,   882,   883,   888,   895,   902,   903,   904,
     912,   913,   914,   922,   928,   933,   934,   935,   945,   946,
     953,   954,   955,   964,   965,   971,   972,   979,   980,   981,
     991,   993,   995,   997,   999,  1001,  1003,  1005,  1008,  1010,
    1012,  1014,  1020,  1022,  1025,  1027,  1029,  1031,  1035,  1037,
    1041,  1043,  1048,  1055,  1059,  1065,  1068,  1073,  1075,  1079,
    1086,  1095,  1100,  1107,  1112,  1115,  1122,  1125,  1130,  1137,
    1140,  1145,  1148,  1153,  1155,  1157,  1159,  1163,  1165,  1170,
    1172,  1177,  1179,  1183,  1185,  1187,  1192,  1194,  1198,  1202,
    1203,  1209,  1212,  1217,  1223,  1229,  1232,  1237,  1242,  1246,
    1250,  1254,  1257,  1259,  1264,  1265,  1271,  1272,  1278,  1284,
    1286,  1288,  1295,  1297,  1299,  1301,  1303,  1306,  1308,  1311,
    1313,  1315,  1317,  1319,  1321,  1324,  1328,  1329,  1334,  1335,
    1341,  1343,  1346,  1348,  1350,  1352,  1354,  1356,  1358,  1361,
    1364,  1366,  1368,  1370,  1372,  1374,  1376,  1378,  1380,  1382,
    1384,  1386,  1388,  1390,  1392,  1394,  1396,  1397,  1402,  1405,
    1409,  1412,  1419,  1428,  1433,  1440,  1445,  1452,  1455,  1460,
    1467,  1470,  1475,  1478,  1483,  1485,  1486,  1488,  1490,  1492,
    1494,  1496,  1498,  1500,  1504,  1506,  1510,  1514,  1518,  1520,
    1524,  1526,  1530,  1532,  1534,  1537,  1539,  1541,  1543,  1546,
    1549,  1551,  1553,  1554,  1559,  1561,  1564,  1566,  1570,  1574,
    1577,  1579,  1581,  1583,  1585,  1587,  1589,  1591,  1593,  1595,
    1597,  1599,  1601,  1602,  1604,  1605,  1607,  1610,  1613,  1614,
    1616,  1618,  1620,  1622,  1624,  1627
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     145,     0,    -1,    -1,   146,   147,    -1,   148,   279,    -1,
     286,    -1,   149,    -1,   148,   285,   149,    -1,     1,   149,
      -1,   154,    -1,    -1,    46,   150,   134,   147,   135,    -1,
     152,   236,   214,   239,    -1,   153,   279,    -1,   286,    -1,
     154,    -1,   153,   285,   154,    -1,     1,   154,    -1,    -1,
      45,   175,   155,   175,    -1,     6,   177,    -1,   154,    40,
     158,    -1,   154,    41,   158,    -1,   154,    42,   158,    -1,
     154,    43,   158,    -1,   154,    44,   154,    -1,    47,   134,
     152,   135,    -1,   156,    -1,   164,   107,   159,    -1,   250,
      88,   159,    -1,   210,   136,   186,   282,    88,   159,    -1,
     210,   137,    51,    88,   159,    -1,   210,   137,    55,    88,
     159,    -1,   210,    86,    55,    88,   159,    -1,   210,    86,
      51,    88,   159,    -1,   252,    88,   159,    -1,   171,   107,
     193,    -1,   164,   107,   182,    -1,   164,   107,   193,    -1,
     157,    -1,   171,   107,   159,    -1,   171,   107,   156,    -1,
     159,    -1,   157,    37,   157,    -1,   157,    38,   157,    -1,
      39,   280,   157,    -1,   121,   159,    -1,   181,    -1,   157,
      -1,   163,    -1,   160,    -1,   229,    -1,   229,   278,   276,
     188,    -1,    -1,    95,   162,   220,   152,   135,    -1,   275,
     188,    -1,   275,   188,   161,    -1,   210,   137,   276,   188,
      -1,   210,   137,   276,   188,   161,    -1,   210,    86,   276,
     188,    -1,   210,    86,   276,   188,   161,    -1,    32,   188,
      -1,    31,   188,    -1,    30,   187,    -1,    21,   187,    -1,
      22,   187,    -1,   166,    -1,    90,   165,   281,    -1,   166,
      -1,    90,   165,   281,    -1,   168,    -1,   168,   167,    -1,
     168,    96,   170,    -1,   168,    96,   170,   138,   169,    -1,
     168,    96,    -1,   168,    96,   138,   169,    -1,    96,   170,
      -1,    96,   170,   138,   169,    -1,    96,    -1,    96,   138,
     169,    -1,   170,    -1,    90,   165,   281,    -1,   167,   138,
      -1,   168,   167,   138,    -1,   167,    -1,   168,   167,    -1,
     249,    -1,   210,   136,   186,   282,    -1,   210,   137,    51,
      -1,   210,    86,    51,    -1,   210,   137,    55,    -1,   210,
      86,    55,    -1,    87,    55,    -1,   252,    -1,   249,    -1,
     210,   136,   186,   282,    -1,   210,   137,    51,    -1,   210,
      86,    51,    -1,   210,   137,    55,    -1,   210,    86,    55,
      -1,    87,    55,    -1,   252,    -1,    51,    -1,    55,    -1,
      87,   172,    -1,   172,    -1,   210,    86,   172,    -1,    51,
      -1,    55,    -1,    52,    -1,   179,    -1,   180,    -1,   174,
      -1,   246,    -1,   175,    -1,   175,    -1,    -1,   177,   138,
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
     107,   181,    -1,   171,   107,   181,    44,   181,    -1,   250,
      88,   181,    -1,   250,    88,   181,    44,   181,    -1,   210,
     136,   186,   282,    88,   181,    -1,   210,   137,    51,    88,
     181,    -1,   210,   137,    55,    88,   181,    -1,   210,    86,
      51,    88,   181,    -1,   210,    86,    55,    88,   181,    -1,
      87,    55,    88,   181,    -1,   252,    88,   181,    -1,   181,
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
      -1,   181,   108,   181,   280,   109,   181,    -1,   194,    -1,
     181,    -1,   286,    -1,   192,   283,    -1,   192,   138,   273,
     283,    -1,   273,   283,    -1,   139,   186,   281,    -1,   286,
      -1,   184,    -1,   286,    -1,   187,    -1,   192,   138,    -1,
     192,   138,   273,   138,    -1,   273,   138,    -1,   163,    -1,
     192,   191,    -1,   273,   191,    -1,   192,   138,   273,   191,
      -1,   190,    -1,    -1,   189,   187,    -1,    97,   182,    -1,
     138,   190,    -1,   286,    -1,   182,    -1,    96,   182,    -1,
     192,   138,   182,    -1,   192,   138,    96,   182,    -1,   192,
     138,   182,    -1,   192,   138,    96,   182,    -1,    96,   182,
      -1,   240,    -1,   241,    -1,   245,    -1,   251,    -1,   252,
      -1,    52,    -1,     7,   151,    10,    -1,    -1,    91,   157,
     195,   281,    -1,    90,   152,   140,    -1,   210,    86,    55,
      -1,    87,    55,    -1,    93,   183,   141,    -1,    94,   272,
     135,    -1,    30,    -1,    31,   139,   187,   281,    -1,    31,
     139,   281,    -1,    31,    -1,    39,   139,   157,   281,    -1,
      39,   139,   281,    -1,   275,   231,    -1,   230,    -1,   230,
     231,    -1,    -1,    98,   196,   225,   226,    -1,    11,   158,
     211,   152,   213,    10,    -1,    12,   158,   211,   152,   214,
      10,    -1,    -1,    -1,    18,   197,   158,   212,   198,   152,
      10,    -1,    -1,    -1,    19,   199,   158,   212,   200,   152,
      10,    -1,    16,   158,   279,   234,    10,    -1,    16,   279,
     234,    10,    -1,    -1,    -1,    20,   215,    25,   201,   158,
     212,   202,   152,    10,    -1,    -1,     3,   173,   253,   203,
     151,    10,    -1,    -1,    -1,     3,    84,   157,   204,   284,
     205,   151,    10,    -1,    -1,     4,   173,   206,   151,    10,
      -1,    -1,     5,   174,   207,   255,   151,    10,    -1,    -1,
      -1,     5,   270,   278,   208,   174,   209,   255,   151,    10,
      -1,    21,    -1,    22,    -1,    23,    -1,    24,    -1,   194,
      -1,   284,    -1,    13,    -1,   284,    13,    -1,   284,    -1,
      27,    -1,   214,    -1,    14,   158,   211,   152,   213,    -1,
     286,    -1,    15,   152,    -1,   171,    -1,   164,    -1,   258,
      -1,    90,   218,   281,    -1,   216,    -1,   217,   138,   216,
      -1,   217,    -1,   217,   138,    96,   258,    -1,   217,   138,
      96,   258,   138,   217,    -1,   217,   138,    96,    -1,   217,
     138,    96,   138,   217,    -1,    96,   258,    -1,    96,   258,
     138,   217,    -1,    96,    -1,    96,   138,   217,    -1,   260,
     138,   263,   138,   266,   269,    -1,   260,   138,   263,   138,
     266,   138,   260,   269,    -1,   260,   138,   263,   269,    -1,
     260,   138,   263,   138,   260,   269,    -1,   260,   138,   266,
     269,    -1,   260,   138,    -1,   260,   138,   266,   138,   260,
     269,    -1,   260,   269,    -1,   263,   138,   266,   269,    -1,
     263,   138,   266,   138,   260,   269,    -1,   263,   269,    -1,
     263,   138,   260,   269,    -1,   266,   269,    -1,   266,   138,
     260,   269,    -1,   268,    -1,   286,    -1,   221,    -1,   112,
     222,   112,    -1,    77,    -1,   112,   219,   222,   112,    -1,
     280,    -1,   280,   142,   223,   280,    -1,   224,    -1,   223,
     138,   224,    -1,    51,    -1,   257,    -1,   139,   256,   222,
     140,    -1,   256,    -1,   105,   152,   135,    -1,    29,   152,
      10,    -1,    -1,    28,   228,   220,   152,    10,    -1,   163,
     227,    -1,   229,   278,   276,   185,    -1,   229,   278,   276,
     185,   231,    -1,   229,   278,   276,   188,   227,    -1,   275,
     184,    -1,   210,   137,   276,   185,    -1,   210,    86,   276,
     184,    -1,   210,    86,   277,    -1,   210,   137,   184,    -1,
     210,    86,   184,    -1,    32,   184,    -1,    32,    -1,   210,
     136,   186,   282,    -1,    -1,   134,   232,   220,   152,   135,
      -1,    -1,    26,   233,   220,   152,    10,    -1,    17,   192,
     211,   152,   235,    -1,   214,    -1,   234,    -1,     8,   237,
     238,   211,   152,   236,    -1,   286,    -1,   182,    -1,   193,
      -1,   286,    -1,    89,   171,    -1,   286,    -1,     9,   152,
      -1,   286,    -1,   248,    -1,   246,    -1,    60,    -1,    62,
      -1,   103,    62,    -1,   103,   242,    62,    -1,    -1,    63,
     243,   152,   135,    -1,    -1,   242,    63,   244,   152,   135,
      -1,    61,    -1,    99,   247,    -1,   174,    -1,    54,    -1,
      53,    -1,    56,    -1,    58,    -1,    59,    -1,   120,    58,
      -1,   120,    59,    -1,    51,    -1,    54,    -1,    53,    -1,
      56,    -1,    55,    -1,   249,    -1,   249,    -1,    34,    -1,
      33,    -1,    35,    -1,    36,    -1,    49,    -1,    48,    -1,
      64,    -1,    65,    -1,   284,    -1,    -1,   111,   254,   158,
     284,    -1,     1,   284,    -1,   139,   256,   281,    -1,   256,
     284,    -1,   260,   138,   264,   138,   266,   269,    -1,   260,
     138,   264,   138,   266,   138,   260,   269,    -1,   260,   138,
     264,   269,    -1,   260,   138,   264,   138,   260,   269,    -1,
     260,   138,   266,   269,    -1,   260,   138,   266,   138,   260,
     269,    -1,   260,   269,    -1,   264,   138,   266,   269,    -1,
     264,   138,   266,   138,   260,   269,    -1,   264,   269,    -1,
     264,   138,   260,   269,    -1,   266,   269,    -1,   266,   138,
     260,   269,    -1,   268,    -1,    -1,    55,    -1,    54,    -1,
      53,    -1,    56,    -1,   257,    -1,    51,    -1,   258,    -1,
      90,   218,   281,    -1,   259,    -1,   260,   138,   259,    -1,
      51,   107,   182,    -1,    51,   107,   210,    -1,   262,    -1,
     263,   138,   262,    -1,   261,    -1,   264,   138,   261,    -1,
     117,    -1,    96,    -1,   265,    51,    -1,   265,    -1,   114,
      -1,    97,    -1,   267,    51,    -1,   138,   268,    -1,   286,
      -1,   251,    -1,    -1,   139,   271,   157,   281,    -1,   286,
      -1,   273,   283,    -1,   274,    -1,   273,   138,   274,    -1,
     182,    89,   182,    -1,    57,   182,    -1,    51,    -1,    55,
      -1,    52,    -1,    51,    -1,    55,    -1,    52,    -1,   179,
      -1,    51,    -1,    52,    -1,   179,    -1,   137,    -1,    86,
      -1,    -1,   285,    -1,    -1,   143,    -1,   280,   140,    -1,
     280,   141,    -1,    -1,   143,    -1,   138,    -1,   142,    -1,
     143,    -1,   284,    -1,   285,   142,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   967,   967,   967,   978,   984,   988,   992,   996,  1002,
    1004,  1003,  1018,  1044,  1050,  1054,  1058,  1062,  1068,  1068,
    1072,  1076,  1080,  1084,  1088,  1092,  1096,  1103,  1104,  1108,
    1112,  1116,  1120,  1124,  1129,  1133,  1138,  1142,  1146,  1150,
    1153,  1157,  1164,  1165,  1169,  1173,  1177,  1181,  1184,  1191,
    1192,  1195,  1196,  1200,  1199,  1212,  1216,  1221,  1225,  1230,
    1234,  1239,  1243,  1247,  1251,  1255,  1261,  1265,  1271,  1272,
    1278,  1282,  1286,  1290,  1294,  1298,  1302,  1306,  1310,  1314,
    1320,  1321,  1327,  1331,  1337,  1341,  1347,  1351,  1355,  1359,
    1363,  1367,  1373,  1379,  1386,  1390,  1394,  1398,  1402,  1406,
    1412,  1418,  1425,  1429,  1432,  1436,  1440,  1446,  1447,  1448,
    1449,  1454,  1461,  1462,  1465,  1471,  1475,  1475,  1481,  1482,
    1483,  1484,  1485,  1486,  1487,  1488,  1489,  1490,  1491,  1492,
    1493,  1494,  1495,  1496,  1497,  1498,  1499,  1500,  1501,  1502,
    1503,  1504,  1505,  1506,  1507,  1508,  1511,  1511,  1511,  1512,
    1512,  1513,  1513,  1513,  1514,  1514,  1514,  1514,  1515,  1515,
    1515,  1516,  1516,  1516,  1517,  1517,  1517,  1517,  1518,  1518,
    1518,  1518,  1519,  1519,  1519,  1519,  1520,  1520,  1520,  1520,
    1521,  1521,  1521,  1521,  1522,  1522,  1525,  1529,  1533,  1537,
    1541,  1545,  1549,  1553,  1557,  1562,  1567,  1572,  1576,  1580,
    1584,  1588,  1592,  1596,  1600,  1604,  1608,  1612,  1616,  1620,
    1624,  1628,  1632,  1636,  1640,  1644,  1648,  1652,  1656,  1660,
    1664,  1673,  1677,  1681,  1685,  1689,  1693,  1697,  1701,  1705,
    1711,  1718,  1719,  1723,  1727,  1733,  1739,  1740,  1743,  1744,
    1745,  1749,  1753,  1759,  1763,  1767,  1771,  1775,  1781,  1781,
    1793,  1799,  1803,  1809,  1813,  1817,  1821,  1827,  1831,  1835,
    1841,  1842,  1843,  1844,  1845,  1846,  1850,  1856,  1856,  1861,
    1865,  1869,  1873,  1877,  1881,  1885,  1889,  1893,  1897,  1901,
    1905,  1909,  1910,  1916,  1915,  1928,  1935,  1942,  1942,  1942,
    1948,  1948,  1948,  1954,  1960,  1965,  1967,  1964,  1974,  1973,
    1986,  1991,  1985,  2004,  2003,  2016,  2015,  2028,  2029,  2028,
    2042,  2046,  2050,  2054,  2060,  2067,  2068,  2069,  2072,  2073,
    2076,  2077,  2085,  2086,  2092,  2096,  2099,  2103,  2109,  2113,
    2119,  2123,  2127,  2131,  2135,  2139,  2143,  2147,  2151,  2157,
    2161,  2165,  2169,  2173,  2177,  2181,  2185,  2189,  2193,  2197,
    2201,  2205,  2209,  2213,  2219,  2220,  2227,  2232,  2237,  2244,
    2248,  2254,  2255,  2258,  2263,  2266,  2270,  2276,  2280,  2287,
    2286,  2299,  2309,  2313,  2318,  2325,  2329,  2333,  2337,  2341,
    2345,  2349,  2353,  2357,  2364,  2363,  2374,  2373,  2385,  2393,
    2402,  2405,  2412,  2415,  2419,  2420,  2423,  2427,  2430,  2434,
    2437,  2438,  2444,  2445,  2446,  2450,  2457,  2456,  2469,  2467,
    2481,  2484,  2491,  2492,  2493,  2494,  2497,  2498,  2499,  2503,
    2509,  2513,  2517,  2521,  2525,  2531,  2537,  2541,  2545,  2549,
    2553,  2557,  2564,  2573,  2574,  2577,  2582,  2581,  2590,  2597,
    2603,  2609,  2613,  2617,  2621,  2625,  2629,  2633,  2637,  2641,
    2645,  2649,  2653,  2657,  2661,  2666,  2672,  2677,  2682,  2687,
    2694,  2698,  2705,  2709,  2715,  2719,  2725,  2732,  2739,  2743,
    2749,  2753,  2759,  2760,  2763,  2768,  2774,  2775,  2778,  2785,
    2789,  2796,  2801,  2801,  2823,  2824,  2830,  2834,  2840,  2844,
    2850,  2851,  2852,  2855,  2856,  2857,  2858,  2861,  2862,  2863,
    2866,  2867,  2870,  2871,  2874,  2875,  2878,  2881,  2884,  2885,
    2886,  2889,  2890,  2893,  2894,  2898
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
  "opt_block_arg", "args", "mrhs", "primary", "$@7", "@8", "$@9", "$@10",
  "$@11", "$@12", "$@13", "$@14", "@15", "@16", "@17", "@18", "@19",
  "$@20", "@21", "primary_value", "then", "do", "if_tail", "opt_else",
  "for_var", "f_marg", "f_marg_list", "f_margs", "block_param",
  "opt_block_param", "block_param_def", "opt_bv_decl", "bv_decls", "bvar",
  "f_larglist", "lambda_body", "do_block", "$@22", "block_call",
  "method_call", "brace_block", "$@23", "$@24", "case_body", "cases",
  "opt_rescue", "exc_list", "exc_var", "opt_ensure", "literal", "string",
  "string_interp", "@25", "@26", "regexp", "symbol", "sym", "numeric",
  "variable", "var_lhs", "var_ref", "backref", "superclass", "$@27",
  "f_arglist", "f_args", "f_bad_arg", "f_norm_arg", "f_arg_item", "f_arg",
  "f_opt", "f_block_opt", "f_block_optarg", "f_optarg", "restarg_mark",
  "f_rest_arg", "blkarg_mark", "f_block_arg", "opt_f_block_arg",
  "singleton", "$@28", "assoc_list", "assocs", "assoc", "operation",
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
     194,   194,   194,   194,   194,   194,   194,   195,   194,   194,
     194,   194,   194,   194,   194,   194,   194,   194,   194,   194,
     194,   194,   194,   196,   194,   194,   194,   197,   198,   194,
     199,   200,   194,   194,   194,   201,   202,   194,   203,   194,
     204,   205,   194,   206,   194,   207,   194,   208,   209,   194,
     194,   194,   194,   194,   210,   211,   211,   211,   212,   212,
     213,   213,   214,   214,   215,   215,   216,   216,   217,   217,
     218,   218,   218,   218,   218,   218,   218,   218,   218,   219,
     219,   219,   219,   219,   219,   219,   219,   219,   219,   219,
     219,   219,   219,   219,   220,   220,   221,   221,   221,   222,
     222,   223,   223,   224,   224,   225,   225,   226,   226,   228,
     227,   229,   229,   229,   229,   230,   230,   230,   230,   230,
     230,   230,   230,   230,   232,   231,   233,   231,   234,   235,
     235,   236,   236,   237,   237,   237,   238,   238,   239,   239,
     240,   240,   241,   241,   241,   241,   243,   242,   244,   242,
     245,   246,   247,   247,   247,   247,   248,   248,   248,   248,
     249,   249,   249,   249,   249,   250,   251,   251,   251,   251,
     251,   251,   251,   252,   252,   253,   254,   253,   253,   255,
     255,   256,   256,   256,   256,   256,   256,   256,   256,   256,
     256,   256,   256,   256,   256,   256,   257,   257,   257,   257,
     258,   258,   259,   259,   260,   260,   261,   262,   263,   263,
     264,   264,   265,   265,   266,   266,   267,   267,   268,   269,
     269,   270,   271,   270,   272,   272,   273,   273,   274,   274,
     275,   275,   275,   276,   276,   276,   276,   277,   277,   277,
     278,   278,   279,   279,   280,   280,   281,   282,   283,   283,
     283,   284,   284,   285,   285,   286
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
       1,     1,     1,     1,     1,     1,     3,     0,     4,     3,
       3,     2,     3,     3,     1,     4,     3,     1,     4,     3,
       2,     1,     2,     0,     4,     6,     6,     0,     0,     7,
       0,     0,     7,     5,     4,     0,     0,     9,     0,     6,
       0,     0,     8,     0,     5,     0,     6,     0,     0,     9,
       1,     1,     1,     1,     1,     1,     1,     2,     1,     1,
       1,     5,     1,     2,     1,     1,     1,     3,     1,     3,
       1,     4,     6,     3,     5,     2,     4,     1,     3,     6,
       8,     4,     6,     4,     2,     6,     2,     4,     6,     2,
       4,     2,     4,     1,     1,     1,     3,     1,     4,     1,
       4,     1,     3,     1,     1,     4,     1,     3,     3,     0,
       5,     2,     4,     5,     5,     2,     4,     4,     3,     3,
       3,     2,     1,     4,     0,     5,     0,     5,     5,     1,
       1,     6,     1,     1,     1,     1,     2,     1,     2,     1,
       1,     1,     1,     1,     2,     3,     0,     4,     0,     5,
       1,     2,     1,     1,     1,     1,     1,     1,     2,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     0,     4,     2,     3,
       2,     6,     8,     4,     6,     4,     6,     2,     4,     6,
       2,     4,     2,     4,     1,     0,     1,     1,     1,     1,
       1,     1,     1,     3,     1,     3,     3,     3,     1,     3,
       1,     3,     1,     1,     2,     1,     1,     1,     2,     2,
       1,     1,     0,     4,     1,     2,     1,     3,     3,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     0,     1,     0,     1,     2,     2,     0,     1,
       1,     1,     1,     1,     2,     0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,     0,     0,     0,     0,     0,     0,
       0,     0,   502,   287,   290,     0,   310,   311,   312,   313,
     274,   277,   382,   428,   427,   429,   430,   504,     0,    10,
       0,   432,   431,   420,   265,   422,   421,   424,   423,   416,
     417,   402,   410,   403,   433,   434,     0,     0,     0,     0,
       0,   515,   515,    78,   283,     0,     0,     0,     0,     0,
       3,   502,     6,     9,    27,    39,    42,    50,    49,     0,
      66,     0,    70,    80,     0,    47,   229,     0,    51,   281,
     260,   261,   262,   401,   400,   426,     0,   263,   264,   248,
       5,     8,   310,   311,   274,   277,   382,     0,   102,   103,
       0,     0,     0,     0,   105,     0,   314,     0,   426,   264,
       0,   303,   156,   166,   157,   179,   153,   172,   162,   161,
     182,   183,   177,   160,   159,   155,   180,   184,   185,   164,
     154,   167,   171,   173,   165,   158,   174,   181,   176,   175,
     168,   178,   163,   152,   170,   169,   151,   149,   150,   146,
     147,   148,   107,   109,   108,   142,   143,   139,   121,   122,
     123,   130,   127,   129,   124,   125,   144,   145,   131,   132,
     136,   126,   128,   118,   119,   120,   133,   134,   135,   137,
     138,   140,   141,   482,   305,   110,   111,   481,     0,   175,
     168,   178,   163,   146,   147,   107,   108,   112,   115,    20,
     113,     0,     0,   515,   502,    15,    14,     0,    48,     0,
       0,     0,   426,     0,   264,     0,   511,   512,   502,     0,
     513,   503,     0,     0,     0,   325,   324,     0,     0,   426,
     264,     0,     0,     0,     0,   243,   230,   253,    64,   247,
     515,   515,   486,    65,    63,   504,    62,     0,   515,   381,
      61,   504,   505,     0,    18,     0,     0,   207,     0,   208,
     271,     0,     0,   504,    68,   267,     0,   508,   508,   231,
       0,     0,   508,   484,     0,     0,    76,     0,    86,    93,
     455,   414,   413,   415,   412,   411,   404,   406,     0,   418,
     419,    46,   222,   223,     4,   503,     0,     0,     0,     0,
       0,     0,     0,   369,   371,     0,    82,     0,    74,    71,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   515,     0,
     501,   500,     0,   386,   384,   282,     0,     0,   375,    55,
     280,   300,   102,   103,   104,   418,   419,     0,   436,   298,
     435,     0,   515,     0,     0,     0,   455,   307,   116,    17,
     266,   515,   515,   392,    13,   503,   271,   316,     0,   315,
       0,     0,   515,     0,     0,     0,     0,     0,     0,     0,
     514,     0,     0,   271,     0,   515,     0,   295,   489,   254,
     250,     0,     0,   244,   252,     0,   245,   504,     0,   276,
     249,   504,   239,   515,   515,   238,   504,   279,    45,     0,
       0,     0,     0,     0,     0,   504,   269,    67,   504,   272,
     510,   509,   232,   510,   234,   273,   485,    92,    84,     0,
      79,     0,     0,   515,     0,   461,   458,   457,   456,   459,
       0,   473,   477,   476,   472,   455,     0,   366,   460,   462,
     464,   515,   470,   515,   475,   515,     0,   454,     0,   405,
     408,     0,     0,     7,    21,    22,    23,    24,    25,    43,
      44,   515,     0,    28,    37,     0,    38,   504,     0,    72,
      83,    41,    40,     0,   186,   253,    36,   204,   212,   217,
     218,   219,   214,   216,   226,   227,   220,   221,   197,   198,
     224,   225,   504,   213,   215,   209,   210,   211,   199,   200,
     201,   202,   203,   493,   498,   494,   499,   380,   248,   378,
     504,   493,   495,   494,   496,   379,   515,   493,   494,   248,
     515,   515,    29,   188,    35,   196,    53,    56,     0,   438,
       0,     0,   102,   103,   106,     0,   504,   515,     0,   504,
     455,     0,     0,     0,     0,   393,   394,   515,   395,     0,
     515,   322,    16,   515,   317,   186,   497,   270,   504,   493,
     494,   515,     0,     0,   294,   319,   288,   318,   291,   497,
     270,   504,   493,   494,     0,   488,     0,   255,   251,   515,
     487,   275,   506,   235,   240,   242,   278,    19,     0,    26,
     195,    69,   268,   508,    85,    77,    89,    91,   504,   493,
     494,     0,   461,     0,   337,   328,   330,   504,   326,   504,
       0,     0,   284,     0,   447,   480,     0,   450,   474,     0,
     452,   478,     0,     0,   205,   206,   357,   504,     0,   355,
     354,   259,     0,    81,    75,     0,     0,     0,     0,     0,
       0,   377,    59,     0,   383,     0,     0,   237,   376,    57,
     236,   372,    52,     0,     0,     0,   515,   301,     0,     0,
     383,   304,   483,   504,     0,   440,   308,   114,   117,     0,
       0,   397,   323,     0,    12,   399,     0,     0,   320,     0,
       0,   383,     0,     0,     0,   293,     0,     0,     0,     0,
     383,     0,   256,   246,   515,    11,   233,    87,   466,   504,
       0,   335,     0,   463,     0,   359,     0,     0,   465,   515,
     515,   479,   515,   471,   515,   515,   407,     0,   461,   504,
       0,   515,   468,   515,   515,   353,     0,     0,   257,    73,
     187,     0,    34,   193,    33,   194,    60,   507,     0,    31,
     191,    32,   192,    58,   373,   374,     0,     0,   189,     0,
       0,   437,   299,   439,   306,   455,     0,   396,     0,    94,
     101,     0,   398,     0,   285,     0,   286,   255,   515,     0,
       0,   296,   241,   327,   338,     0,   333,   329,   365,     0,
     368,   367,     0,   443,     0,   445,     0,   451,     0,   448,
     453,   409,     0,     0,   356,   344,   346,     0,   349,     0,
     351,   370,   258,   228,    30,   190,   387,   385,     0,     0,
       0,   100,     0,   515,     0,   515,     0,   389,   390,   388,
     289,   292,     0,     0,   336,     0,   331,   363,   504,   361,
     364,   515,   515,   515,   515,     0,   467,   358,   515,   515,
     515,   469,   515,   515,    54,   302,     0,   497,    99,   504,
     493,   494,   391,   515,     0,   334,     0,     0,   360,   444,
       0,   441,   446,   449,   271,     0,     0,   341,     0,   343,
     350,     0,   347,   352,   309,   383,   321,   297,   332,   362,
     515,   497,   270,   515,   515,   515,   515,   442,   342,     0,
     339,   345,   348,   515,   340
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    60,    61,    62,   255,   202,   203,   204,
     205,   419,    64,    65,   209,    66,    67,   547,   676,    68,
      69,   263,    70,    71,    72,   440,    73,   210,   104,   105,
     197,   198,   688,   199,   564,   526,   186,    75,   237,   266,
     527,   668,   411,   412,   246,   247,   239,   403,   413,   486,
      76,   428,   280,   222,   708,   223,   709,   594,   842,   551,
     548,   770,   364,   366,   563,   775,   258,   378,   586,   697,
     698,   228,   625,   626,   627,   739,   648,   649,   724,   848,
     849,   456,   632,   304,   481,    78,    79,   350,   541,   540,
     389,   839,   372,   567,   690,   694,    80,    81,   288,   468,
     643,    82,    83,   285,    84,   212,   213,    87,   214,   359,
     550,   561,   562,   458,   459,   460,   461,   462,   742,   743,
     463,   464,   465,   466,   731,   634,   188,   365,   271,   414,
     242,    89,   555,   529,   342,   219,   408,   409,   664,   432,
     379,   221,   206
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -709
static const yytype_int16 yypact[] =
{
    -709,   139,  1944,  -709,  6429,  8141,  8450,  4657,  6189,  2664,
    7820,  7820,  4138,  -709,  -709,  8244,  6643,  6643,  -709,  -709,
    6643,  5375,  5487,  -709,  -709,  -709,  -709,   187,  6189,  -709,
      -6,  -709,  -709,  4779,  4903,  -709,  -709,  5027,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  7927,  7927,   100,  3455,
    7820,  6857,  7178,  5823,  -709,  6069,   326,   471,  8034,  7927,
    -709,   457,  -709,   647,  -709,   602,  -709,  -709,   153,   143,
    -709,   123,  8347,  -709,   184,  5930,    37,    43,    60,    82,
    -709,  -709,  -709,  -709,  -709,   312,   119,  -709,   322,    69,
    -709,  -709,  -709,  -709,  -709,   162,   200,   209,   408,   446,
    7820,   103,  3598,   590,  -709,    78,  -709,    47,  -709,  -709,
      69,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,    65,
     178,   220,   230,  -709,  -709,  -709,  -709,  -709,  -709,   255,
     299,  -709,   317,  -709,   347,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,    60,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,   224,
    -709,  6536,   345,   371,   457,   647,  -709,   328,   602,    85,
     307,    63,    53,   337,    54,    85,  -709,  -709,   457,   429,
    -709,   295,  7820,  7820,   394,  -709,  -709,   156,   449,    74,
      79,  7927,  7927,  7927,  7927,  -709,  5930,   392,  -709,  -709,
     357,   375,  -709,  -709,  -709,  4012,  -709,  6643,  6643,  -709,
    -709,  4266,  -709,  7820,  -709,   356,  3741,  -709,   188,   447,
     411,  3455,   379,   381,   415,   602,   405,    14,   133,  -709,
     392,   398,   133,  -709,   501,  8553,   421,   198,   217,   320,
    1024,  -709,  -709,  -709,  -709,  -709,  -709,  -709,   594,   498,
     509,  -709,  -709,  -709,  -709,  4392,  7820,  7820,  7820,  7820,
    6536,  7820,  7820,  -709,  -709,  7285,  -709,  3455,  5933,   436,
    7285,  7927,  7927,  7927,  7927,  7927,  7927,  7927,  7927,  7927,
    7927,  7927,  7927,  7927,  7927,  7927,  7927,  7927,  7927,  7927,
    7927,  7927,  7927,  7927,  7927,  7927,  7927,  8813,  6643,  8886,
    -709,  -709,  9799,  -709,  -709,  -709,  8034,  8034,  -709,   495,
    -709,   602,  -709,   329,  -709,  -709,  -709,   457,  -709,  -709,
    -709,  8959,  6643,  9032,  2664,  7820,  1072,  -709,  -709,   647,
    -709,  7392,   577,  -709,  -709,  4517,    56,  -709,  2798,   583,
    7927,  9105,  6643,  9178,  7927,  7927,  3056,   429,  7499,   595,
    -709,    88,    88,    91,  9251,  6643,  9324,  -709,  -709,  -709,
    -709,  7927,  6750,  -709,  -709,  6964,  -709,   381,   475,  -709,
    -709,   381,  -709,   480,   483,  -709,   110,  -709,  -709,  6189,
    3184,   499,  9105,  9178,  7927,   381,  -709,   508,   381,  -709,
    7071,  -709,  -709,  7178,  -709,  -709,  -709,   329,   123,  8553,
    -709,  8553,  9397,  6643,  9470,   530,  -709,  -709,  -709,  -709,
     557,  -709,  -709,  -709,  -709,  1150,    77,  -709,  -709,  -709,
    -709,   514,  -709,   516,   624,   538,   628,  -709,  3741,  -709,
    -709,  7927,  7927,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,   193,  7927,  -709,   543,   554,  -709,   381,  8553,   556,
    -709,  -709,  -709,   588,  2034,  -709,  -709,   447,  1456,  1456,
    1456,  1456,  1303,  1303,  8832,  2251,  1456,  1456,  9853,  9853,
     547,   547,  9745,  1303,  1303,    50,    50,   679,   374,   374,
     447,   447,   447,  2302,  5599,  2519,  5711,  -709,   200,  -709,
     381,   453,  -709,   463,  -709,  -709,  5487,  -709,  -709,  1601,
     193,   193,  -709,  2380,  -709,  5930,  -709,  -709,   457,  -709,
    7820,  2664,   450,    51,  -709,   200,   381,   200,   687,   110,
    1150,  2664,   457,  6309,  6189,   543,  -709,   610,  -709,  3327,
     691,  -709,   647,   655,  -709,  2432,  5151,  5263,   381,   414,
     420,   577,   692,   105,  -709,  -709,  -709,  -709,  -709,    66,
     102,   381,   106,   111,  7820,  -709,  7927,   392,  -709,   375,
    -709,  -709,  -709,  -709,  6750,  6964,  -709,  -709,   568,  -709,
    5930,   195,  -709,   133,   436,  -709,   450,    51,   381,    55,
     302,  7927,  -709,   557,   289,  -709,   566,   381,  -709,   381,
    3884,  3741,  -709,  1150,  -709,  -709,  1150,  -709,  -709,  1204,
    -709,  -709,   572,  3741,   447,   447,  -709,   968,  3884,  -709,
    -709,   571,  7606,  -709,  -709,  8553,  8034,  7927,   601,  8034,
    8034,  -709,   495,   570,   481,  8034,  8034,  -709,  -709,   495,
    -709,    82,   153,  3884,  3741,  7927,   193,  -709,   457,   705,
    -709,  -709,  -709,   381,   706,  -709,  -709,  -709,  -709,  8656,
      85,  -709,  -709,  3884,  -709,  -709,  7820,   707,  -709,  7927,
    7927,   469,  7927,  7927,   713,  -709,  7713,  2927,  3884,  3884,
     120,    88,  -709,  -709,   593,  -709,  -709,   546,  -709,   381,
     753,   596,  1062,  -709,   597,   598,   726,   603,  -709,   609,
     611,  -709,   612,  -709,   613,   612,  -709,   618,   648,   381,
     642,   619,  -709,   620,   622,  -709,   746,  7927,   627,  -709,
    5930,  7927,  -709,  5930,  -709,  5930,  -709,  -709,  8034,  -709,
    5930,  -709,  5930,  -709,  -709,  -709,   752,   632,  5930,  3741,
    2664,  -709,  -709,  -709,  -709,  1072,   716,  -709,   352,   217,
     320,  2664,  -709,    85,  -709,  7927,  -709,  -709,   452,   758,
     762,  -709,  6964,  -709,   636,   753,   310,  -709,  -709,   574,
    -709,  -709,  1150,  -709,  1204,  -709,  1204,  -709,  1204,  -709,
    -709,  -709,  8759,   663,  -709,  1183,  -709,  1183,  -709,  1204,
    -709,  -709,   639,  5930,  -709,  5930,  -709,  -709,   643,   769,
    2664,   329,  9543,  6643,  9616,   371,  2798,  -709,  -709,  -709,
    -709,  -709,  3884,   753,   636,   753,   645,  -709,   258,  -709,
    -709,   612,   651,   612,   612,   730,   361,  -709,   652,   653,
     612,  -709,   665,   612,  -709,  -709,   791,    72,    51,   381,
     109,   112,  -709,   655,   801,   636,   753,   574,  -709,  -709,
    1204,  -709,  -709,  -709,  -709,  9689,  1183,  -709,  1204,  -709,
    -709,  1204,  -709,  -709,  -709,   146,  -709,  -709,   636,  -709,
     612,   677,   680,   612,   682,   612,   612,  -709,  -709,  1204,
    -709,  -709,  -709,   612,  -709
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -709,  -709,  -709,   407,  -709,    26,  -709,  -295,   245,  -709,
     128,  -709,  -304,   734,     0,   -15,  -709,  -519,  -709,    35,
     803,  -135,    11,   -57,  -256,  -377,    15,   890,   -61,   819,
       1,   -24,  -709,  -709,  -709,    39,  -709,  1464,   700,  -709,
      81,   291,  -290,   104,   -13,  -709,  -319,  -228,   388,  -200,
      29,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,  -709,  -709,  -709,  -709,    12,  -208,  -355,   -45,
    -362,  -709,  -651,  -639,   213,  -709,  -452,  -709,  -545,  -709,
     -40,  -709,  -709,   160,  -709,  -709,  -709,   -76,  -709,  -709,
    -367,  -709,     4,  -709,  -709,  -709,  -709,  -709,  -709,  -709,
    -709,  -709,    -3,  -709,  -709,   505,   992,   833,   720,  -709,
    -709,    71,  -222,  -702,   164,  -570,   163,  -577,  -708,    32,
     222,  -709,  -540,  -709,  -279,   486,  -709,  -709,  -709,    22,
    -379,  1866,  -306,  -709,   671,    -8,   -25,   410,  -478,  -227,
     267,     5,    -2
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -516
static const yytype_int16 yytable[] =
{
      90,   467,   253,   345,   254,   200,   491,   386,   184,   250,
     570,   215,   218,   406,    77,   309,    77,   107,   107,   439,
     582,    77,   211,   211,   211,   200,   600,   227,   211,   211,
      91,   528,   211,   536,   106,   106,   539,   588,   241,   241,
     354,   434,   241,   291,   106,   436,   185,   185,   530,   269,
     273,   235,   235,   294,   600,   235,   284,   557,   457,   733,
     264,    77,   211,   728,   615,   277,   295,   185,   276,   558,
     211,   797,   556,   268,   272,   528,   349,   536,   680,   357,
     -88,   794,   106,   598,   277,   -97,   598,   467,   673,   674,
     557,   -97,   578,   730,   185,   343,   734,   850,   377,   -94,
     701,   106,   740,   249,  -101,   591,   630,   744,   343,   861,
     496,   654,   211,   710,    77,   585,  -100,   557,   377,   311,
     238,   243,   -96,  -314,   244,   -98,   425,   -99,   256,   337,
      63,   -96,    63,   361,   324,   325,   -98,  -270,   557,     3,
     717,  -425,   385,   756,   424,   -95,   340,   301,   302,   381,
     763,  -428,   430,   618,   352,   260,   844,   431,   353,   -95,
     -94,  -101,   -88,  -100,   331,   332,   333,   334,   335,   336,
     348,   566,   487,  -314,  -314,   850,   467,   249,   861,   338,
     339,   303,   631,   362,   363,   439,   406,  -270,  -270,   358,
    -494,   348,   797,   -88,   813,   -88,   374,   341,   -88,   382,
     383,   373,  -428,   344,   -89,  -493,   875,   346,   248,   375,
     387,  -493,   -86,    77,   -97,   -97,   344,   -93,   438,   704,
     216,   217,   391,   392,   769,   733,   600,   216,   217,   -92,
     216,   217,   439,   629,   211,   211,   728,   898,   404,   404,
     -91,  -494,   394,   706,   -88,   728,   415,   216,   217,   -90,
     305,   -96,   -96,   252,   -98,   -98,   679,   211,   -87,   211,
     211,   306,   852,   211,  -427,   211,   684,   241,    77,   241,
     646,   433,   264,    77,   422,   859,   431,   862,   749,   220,
     235,   467,   235,   235,   442,   598,   598,   277,   -95,   -95,
     483,   310,   395,   396,   262,   492,   474,   475,   476,   477,
     554,   245,   -67,  -426,   106,   647,  -429,    77,   211,   211,
     211,   211,    77,   211,   211,  -427,  -430,   211,   264,    77,
     277,   473,   211,   489,   382,   423,   251,   -90,   220,   369,
     252,   542,   544,   -81,   443,   444,   415,   106,   683,   248,
     622,  -432,   446,   447,   448,   449,   904,   262,   251,   407,
     211,   410,   491,  -426,  -426,   370,   791,  -429,   211,   211,
     415,   622,   368,   446,   447,   448,   449,  -430,   745,   568,
     571,   713,   360,   235,   211,   707,    77,   211,   534,   371,
     415,   534,   614,   376,   438,  -431,   716,    77,   286,   287,
      77,   895,  -432,   415,   211,   607,   877,   235,    77,   439,
    -425,   252,   534,  -420,   240,   240,  -264,   211,   240,   -90,
     347,   404,   404,   600,   380,  -271,   200,   235,    90,   -94,
     535,   838,   534,    63,   599,   384,   837,   720,   478,  -101,
     235,   438,    77,  -424,  -490,   534,  -431,   390,   832,   267,
     -90,   415,   -90,   311,   535,   -90,   388,   885,   845,   393,
     -86,   277,   613,   277,  -420,   211,  -264,  -264,   185,   635,
     -93,   635,   534,   635,   535,  -271,  -271,   569,   106,   388,
     106,   220,  -491,   598,   397,   829,  -497,   535,   235,   650,
      77,   401,   781,   534,  -424,   220,   713,   658,   833,   834,
     420,   334,   335,   336,  -420,   402,   467,   362,   363,   424,
     277,   421,   702,   572,   535,   663,   262,    85,   703,    85,
     108,   108,   108,   405,    85,   662,   311,   106,  -100,   426,
     229,   -96,   -66,   669,   252,   535,   672,   -98,   557,   289,
     290,   663,  -424,   435,   670,   866,  -497,   670,   650,   650,
     687,   665,  -490,   869,  -420,  -420,   429,  -490,    63,   -92,
     678,   666,   262,   663,    85,   670,   437,   785,   278,   441,
     -96,   200,   211,    77,   686,   691,   663,   471,   695,   758,
     -98,   571,  -383,    77,   490,   836,   -95,   278,   472,   571,
    -491,    77,  -424,  -424,  -497,  -491,  -497,  -497,   -95,  -493,
     546,   -88,   569,   663,   711,   764,   574,   404,   438,   216,
     217,   -90,   185,   185,   725,   584,   211,    85,   622,   661,
     446,   447,   448,   449,   628,   602,   311,   667,   604,   -87,
     667,   605,   725,   573,   549,   847,   714,   446,   447,   448,
     449,   581,  -383,   240,   609,   240,   661,   621,   667,   301,
     302,   492,    77,    77,   752,   754,   -81,   623,   355,   356,
     759,   761,   633,   624,   636,    77,   469,   470,   587,   587,
      77,   417,   332,   333,   334,   335,   336,   277,   211,   696,
     569,   211,   211,   427,   650,   638,   639,   211,   211,   641,
    -383,  -253,  -383,  -383,   106,    77,    77,   296,   297,   298,
     299,   300,   652,   485,   655,   656,   783,   681,   485,   689,
     693,   778,   705,   715,   722,    77,    85,   736,   211,  -254,
     751,   757,   404,   642,   725,   772,   774,   784,   106,    77,
      77,    77,    88,   786,    88,   109,   109,   635,   635,    88,
     635,   792,   635,   635,   795,   230,   800,   798,   801,   635,
     799,   635,   635,   824,   208,   208,   208,   802,   311,   804,
     806,   808,   270,   811,   814,   812,   821,   815,   817,   485,
     819,    85,   826,   324,   325,  -255,    85,   827,   840,    88,
     211,   831,   841,   279,   843,   857,   583,  -256,   864,   865,
     278,    77,    77,   876,   265,   884,   571,   628,   721,   880,
     886,   888,   279,    77,   332,   333,   334,   335,   336,   732,
      85,   894,   735,   891,   622,    85,   446,   447,   448,   449,
     741,   897,    85,   278,   692,   677,  -493,   601,   225,  -494,
     909,   603,    88,   878,   856,   111,   606,   608,   896,   685,
     671,   415,   765,   373,   351,   611,   719,   899,   612,   872,
     187,   106,    77,   623,   663,   211,   830,   858,    77,   635,
     635,   635,   635,     0,    77,   729,   635,   635,   635,   367,
     635,   635,     0,     0,     0,     0,     0,     0,   235,    85,
       0,   571,     0,   534,     0,   726,   727,     0,     0,     0,
      85,     0,     0,    85,   628,     0,   628,     0,   737,     0,
       0,    85,    74,   746,    74,     0,     0,   653,   635,    74,
       0,   635,   635,   635,   635,   226,     0,     0,     0,     0,
       0,   635,     0,     0,     0,   535,     0,     0,   766,   767,
       0,    88,     0,     0,     0,    85,     0,     0,     0,     0,
       0,   398,   399,   400,     0,     0,     0,     0,   782,    74,
       0,     0,     0,     0,   278,   771,   278,     0,     0,   637,
       0,   640,   788,   789,   790,     0,   208,   208,     0,   628,
     846,     0,     0,     0,     0,   851,     0,   853,     0,   682,
       0,   854,     0,    85,     0,     0,    88,     0,   587,     0,
     860,    88,   863,     0,     0,   416,     0,   418,     0,     0,
       0,     0,    74,   278,    86,   279,    86,     0,     0,     0,
       0,    86,     0,     0,     0,   484,     0,   628,     0,   628,
     495,     0,     0,     0,   828,    88,     0,     0,     0,   738,
      88,   446,   447,   448,   449,     0,   835,    88,   279,     0,
     208,   208,   208,   208,     0,   479,   480,   723,     0,     0,
     628,    86,     0,   900,     0,     0,     0,     0,     0,   903,
       0,   905,     0,     0,   906,     0,    85,     0,   450,     0,
       0,     0,     0,     0,   451,   452,    85,     0,     0,     0,
       0,   565,   913,     0,    85,   445,     0,   446,   447,   448,
     449,   873,   453,     0,    88,   454,     0,   874,   495,     0,
       0,    74,     0,   773,    86,    88,     0,     0,    88,   559,
       0,   595,   597,     0,     0,   270,    88,     0,     0,     0,
       0,   252,     0,   622,   450,   446,   447,   448,   449,     0,
     451,   452,     0,   445,     0,   446,   447,   448,   449,   793,
     597,     0,     0,   270,     0,    85,    85,     0,   453,     0,
      88,   454,     0,     0,     0,     0,    74,     0,    85,     0,
       0,    74,   623,    85,     0,     0,     0,     0,   796,   279,
     278,   279,   450,   455,     0,     0,     0,     0,   451,   452,
       0,     0,     0,     0,     0,     0,     0,     0,    85,    85,
       0,     0,   651,     0,     0,    74,   453,     0,    88,   454,
      74,     0,     0,    86,   779,     0,     0,    74,    85,     0,
     493,   445,     0,   446,   447,   448,   449,     0,   279,     0,
       0,   560,    85,    85,    85,   803,   805,     0,   807,     0,
     809,   810,     0,     0,     0,     0,     0,   816,     0,   818,
     820,     0,     0,     0,   738,     0,   446,   447,   448,   449,
     450,     0,     0,     0,     0,     0,   451,   452,    86,     0,
       0,     0,     0,    86,    74,   622,     0,   446,   447,   448,
     449,     0,     0,     0,   453,    74,     0,   454,    74,     0,
       0,    88,     0,   450,    85,    85,    74,     0,     0,   451,
     452,    88,     0,     0,   208,     0,    85,    86,     0,    88,
       0,     0,    86,     0,   450,     0,   712,   453,     0,    86,
     454,   452,     0,     0,   597,   270,     0,     0,     0,     0,
      74,     0,     0,     0,     0,     0,     0,   108,   453,     0,
       0,   718,     0,     0,     0,     0,     0,     0,   208,     0,
       0,     0,     0,     0,     0,    85,     0,   879,   881,   882,
     883,    85,     0,     0,   887,   889,   890,    85,   892,   893,
      88,    88,   748,     0,     0,     0,    86,     0,    74,     0,
       0,     0,     0,    88,     0,     0,     0,    86,    88,     0,
      86,     0,   311,     0,     0,   279,     0,     0,    86,     0,
       0,     0,     0,     0,     0,     0,   907,   324,   325,   908,
     910,   911,   912,    88,    88,     0,     0,     0,     0,   914,
       0,     0,     0,     0,     0,     0,   787,     0,     0,   780,
       0,     0,    86,    88,     0,   329,   330,   331,   332,   333,
     334,   335,   336,     0,     0,     0,     0,    88,    88,    88,
     208,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    74,     0,     0,     0,     0,     0,   822,     0,     0,
       0,    74,     0,     0,     0,     0,     0,     0,     0,    74,
      86,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     236,   236,     0,     0,   236,     0,     0,     0,     0,    88,
      88,     0,   270,     0,     0,     0,     0,     0,     0,     0,
       0,    88,     0,     0,     0,     0,     0,     0,     0,     0,
     257,   259,     0,     0,     0,   236,   236,     0,     0,     0,
      74,    74,   292,   293,     0,   311,  -516,  -516,  -516,  -516,
     316,   317,   109,    74,  -516,  -516,     0,     0,    74,     0,
     324,   325,     0,    86,     0,     0,   493,     0,     0,     0,
      88,     0,     0,    86,     0,     0,    88,     0,     0,     0,
       0,    86,    88,    74,    74,     0,   327,   328,   329,   330,
     331,   332,   333,   334,   335,   336,     0,     0,     0,   777,
       0,     0,     0,    74,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    74,    74,    74,
       0,  -515,     0,     0,     0,     0,     0,     0,     0,  -515,
    -515,  -515,     0,     0,  -515,  -515,  -515,     0,  -515,     0,
       0,     0,    86,    86,     0,     0,     0,  -515,  -515,     0,
       0,     0,     0,     0,     0,    86,     0,     0,  -515,  -515,
      86,  -515,  -515,  -515,  -515,  -515,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    74,
      74,     0,     0,     0,     0,    86,    86,     0,     0,     0,
       0,    74,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    86,     0,  -515,     0,     0,
       0,     0,     0,     0,     0,   236,   236,   236,   292,    86,
      86,    86,     0,     0,     0,     0,     0,     0,     0,   236,
       0,   236,   236,     0,     0,     0,     0,     0,     0,     0,
      74,     0,     0,     0,     0,     0,    74,     0,     0,     0,
       0,     0,    74,     0,     0,  -515,  -515,     0,  -515,     0,
     248,  -515,     0,  -515,  -515,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    86,    86,     0,     0,     0,     0,     0,     0,   236,
       0,     0,     0,    86,   494,   497,   498,   499,   500,   501,
     502,   503,   504,   505,   506,   507,   508,   509,   510,   511,
     512,   513,   514,   515,   516,   517,   518,   519,   520,   521,
     522,     0,   236,     0,     0,     0,     0,     0,     0,     0,
     543,   545,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    86,     0,     0,     0,   236,     0,    86,     0,
       0,     0,     0,     0,    86,   236,     0,     0,     0,     0,
       0,     0,     0,     0,   575,     0,   236,     0,   543,   545,
       0,     0,   236,     0,     0,     0,     0,     0,     0,   236,
       0,     0,     0,     0,     0,   236,   236,     0,     0,   236,
       0,   110,   110,     0,     0,     0,     0,     0,     0,     0,
       0,   110,     0,     0,     0,     0,     0,     0,   610,     0,
       0,     0,     0,     0,   236,     0,     0,   236,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   236,     0,     0,
       0,     0,   110,   110,     0,     0,     0,   110,   110,   110,
       0,     0,     0,     0,     0,   110,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   644,   645,     0,   110,     0,
       0,     0,     0,     0,  -515,     4,   236,     5,     6,     7,
       8,     9,     0,     0,     0,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,    28,
      29,    30,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    48,     0,     0,    49,    50,     0,    51,    52,     0,
      53,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     236,     0,     0,     0,    57,    58,    59,     0,   236,   236,
       0,     0,     0,     0,     0,     0,     0,     0,   657,     0,
       0,     0,     0,     0,     0,   236,  -515,  -515,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   110,   110,   110,
     110,     0,     0,   311,   312,   313,   314,   315,   316,   317,
     318,   319,   320,   321,   322,   323,   236,     0,   324,   325,
     575,   750,     0,   753,   755,     0,     0,     0,     0,   760,
     762,     0,     0,     0,     0,     0,     0,     0,     0,   768,
       0,   110,   326,     0,   327,   328,   329,   330,   331,   332,
     333,   334,   335,   336,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   753,   755,     0,   760,   762,     0,     0,
     236,     0,  -230,     0,   110,     0,     0,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,     0,     0,     0,     0,     0,     0,     0,
       0,   236,     0,     0,     0,   823,     0,     0,     0,     0,
       0,     0,   825,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   110,     0,     0,
       0,     0,     0,     0,     0,     0,   110,     0,     0,   825,
     110,   110,     0,     0,   110,     0,   236,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   110,   110,     0,
       0,   110,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     110,     0,     0,     0,     0,     0,   110,   236,     0,   110,
       0,     0,  -497,     0,     0,   110,     0,   110,     0,     0,
    -497,  -497,  -497,     0,     0,     0,  -497,  -497,     0,  -497,
     311,   312,   313,   314,   315,   316,   317,   318,  -497,   320,
     321,     0,     0,     0,     0,   324,   325,   110,   110,  -497,
    -497,     0,  -497,  -497,  -497,  -497,  -497,     0,   110,     0,
       0,     0,     0,     0,   110,     0,     0,     0,     0,     0,
       0,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,  -497,  -497,  -497,  -497,  -497,  -497,  -497,  -497,  -497,
    -497,  -497,  -497,  -497,     0,     0,  -497,  -497,  -497,     0,
     659,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   -97,
    -497,     0,  -497,  -497,  -497,  -497,  -497,  -497,  -497,  -497,
    -497,  -497,     0,     0,   675,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -497,  -497,  -497,  -497,
     -89,     0,  -497,     0,  -497,  -497,     0,     0,     0,   311,
     312,   313,   314,   315,   316,   317,   318,   319,   320,   321,
     322,   323,   110,     0,   324,   325,     0,     0,     0,     0,
     110,   110,     0,     0,     0,     0,   657,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   110,   326,     0,
     327,   328,   329,   330,   331,   332,   333,   334,   335,   336,
       0,   311,   312,   313,   314,   315,   316,   317,   318,   319,
     320,   321,   322,   323,     0,     0,   324,   325,   110,  -270,
       0,   110,     0,   110,     0,     0,     0,  -270,  -270,  -270,
       0,     0,     0,  -270,  -270,     0,  -270,     0,     0,     0,
     326,   110,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,     0,     0,     0,   110,  -270,  -270,     0,  -270,
    -270,  -270,  -270,  -270,     0,   110,   110,     0,   110,   110,
       0,     0,   110,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -270,  -270,
    -270,  -270,  -270,  -270,  -270,  -270,  -270,  -270,  -270,  -270,
    -270,     0,     0,  -270,  -270,  -270,     0,   660,     0,     0,
       0,     0,     0,   110,     0,     0,     0,   110,     0,     0,
       0,     0,     0,     0,     0,     0,   -99,  -270,     0,  -270,
    -270,  -270,  -270,  -270,  -270,  -270,  -270,  -270,  -270,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   110,     0,     0,  -270,  -270,  -270,   -91,   110,  -270,
       0,  -270,  -270,     0,     0,   201,     0,     5,     6,     7,
       8,     9,  -515,  -515,  -515,    10,    11,     0,   110,  -515,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,   201,
       0,     5,     6,     7,     8,     9,  -515,  -515,  -515,    10,
      11,     0,  -515,  -515,    12,     0,    13,    14,    15,    16,
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
      59,     0,     0,     0,     0,     0,     0,     0,   201,     0,
       5,     6,     7,     8,     9,     0,     0,  -515,    10,    11,
    -515,  -515,  -515,    12,  -515,    13,    14,    15,    16,    17,
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
       0,     0,     0,     0,     0,     0,     0,   201,     0,     5,
       6,     7,     8,     9,     0,     0,  -515,    10,    11,  -515,
    -515,  -515,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,     0,    30,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    48,     0,     0,    49,    50,     0,    51,
      52,     0,    53,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    57,    58,    59,     0,
       0,     0,     0,     0,     0,     4,     0,     5,     6,     7,
       8,     9,     0,     0,     0,    10,    11,     0,  -515,  -515,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,    28,
      29,    30,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    48,     0,     0,    49,    50,     0,    51,    52,     0,
      53,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    57,    58,    59,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -515,
       0,     0,     0,     0,     0,     0,  -515,  -515,   201,     0,
       5,     6,     7,     8,     9,     0,  -515,  -515,    10,    11,
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
       0,     0,     0,     0,     0,     0,   201,     0,     5,     6,
       7,     8,     9,     0,     0,     0,    10,    11,     0,  -515,
    -515,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
      28,     0,    30,    31,    32,     0,    33,    34,    35,    36,
      37,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    48,     0,     0,   261,    50,     0,    51,    52,
       0,    53,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    57,    58,    59,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -515,     0,  -515,  -515,   201,
       0,     5,     6,     7,     8,     9,     0,     0,     0,    10,
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
      59,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -515,     0,
    -515,  -515,   201,     0,     5,     6,     7,     8,     9,     0,
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
       0,     0,     0,     0,     0,     0,  -515,     0,     0,     0,
       0,     0,     0,  -515,  -515,   201,     0,     5,     6,     7,
       8,     9,     0,     0,  -515,    10,    11,     0,     0,     0,
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
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,     0,  -515,  -515,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    97,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    33,    34,    35,    36,    37,    38,   231,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   102,    50,     0,    51,    52,     0,   232,   233,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    57,   234,    59,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,   252,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   207,     0,     0,   102,    50,
       0,    51,    52,     0,     0,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    57,    58,
      59,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,     0,
     216,   217,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   207,     0,     0,   102,    50,     0,    51,
      52,     0,     0,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    57,    58,    59,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,   252,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,    29,    30,
      31,    32,     0,    33,    34,    35,    36,    37,    38,     0,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    48,
       0,     0,    49,    50,     0,    51,    52,     0,    53,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    57,    58,    59,     0,     0,     0,     0,     0,
       5,     6,     7,     8,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,   390,    13,    14,    15,    16,    17,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,   390,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,     0,     0,     0,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,     0,     0,     0,
       0,     0,   146,   147,   148,   149,   150,   151,   152,   153,
      35,    36,   154,    38,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   155,   156,   157,   158,   159,   160,
     161,   162,   163,     0,     0,   164,   165,     0,     0,   166,
     167,   168,   169,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   170,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,     0,   181,   182,
       0,     0,  -490,  -490,  -490,     0,  -490,     0,     0,     0,
    -490,  -490,     0,     0,     0,  -490,   183,  -490,  -490,  -490,
    -490,  -490,  -490,  -490,     0,  -490,     0,     0,     0,  -490,
    -490,  -490,  -490,  -490,  -490,  -490,     0,     0,  -490,     0,
       0,     0,     0,     0,     0,     0,     0,  -490,  -490,     0,
    -490,  -490,  -490,  -490,  -490,  -490,  -490,  -490,  -490,  -490,
    -490,  -490,     0,  -490,  -490,     0,  -490,  -490,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -490,     0,     0,  -490,
    -490,     0,  -490,  -490,     0,  -490,  -490,  -490,  -490,     0,
       0,     0,  -490,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -490,
    -490,  -490,     0,     0,     0,     0,  -492,  -492,  -492,     0,
    -492,     0,     0,  -490,  -492,  -492,     0,     0,  -490,  -492,
       0,  -492,  -492,  -492,  -492,  -492,  -492,  -492,     0,  -492,
       0,     0,     0,  -492,  -492,  -492,  -492,  -492,  -492,  -492,
       0,     0,  -492,     0,     0,     0,     0,     0,     0,     0,
       0,  -492,  -492,     0,  -492,  -492,  -492,  -492,  -492,  -492,
    -492,  -492,  -492,  -492,  -492,  -492,     0,  -492,  -492,     0,
    -492,  -492,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -492,     0,     0,  -492,  -492,     0,  -492,  -492,     0,  -492,
    -492,  -492,  -492,     0,     0,     0,  -492,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -492,  -492,  -492,     0,     0,     0,     0,
    -491,  -491,  -491,     0,  -491,     0,     0,  -492,  -491,  -491,
       0,     0,  -492,  -491,     0,  -491,  -491,  -491,  -491,  -491,
    -491,  -491,     0,  -491,     0,     0,     0,  -491,  -491,  -491,
    -491,  -491,  -491,  -491,     0,     0,  -491,     0,     0,     0,
       0,     0,     0,     0,     0,  -491,  -491,     0,  -491,  -491,
    -491,  -491,  -491,  -491,  -491,  -491,  -491,  -491,  -491,  -491,
       0,  -491,  -491,     0,  -491,  -491,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -491,     0,     0,  -491,  -491,     0,
    -491,  -491,     0,  -491,  -491,  -491,  -491,     0,     0,     0,
    -491,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -491,  -491,  -491,
       0,     0,     0,     0,  -493,  -493,  -493,     0,  -493,     0,
       0,  -491,  -493,  -493,     0,     0,  -491,  -493,     0,  -493,
    -493,  -493,  -493,  -493,  -493,  -493,     0,     0,     0,     0,
       0,  -493,  -493,  -493,  -493,  -493,  -493,  -493,     0,     0,
    -493,     0,     0,     0,     0,     0,     0,     0,     0,  -493,
    -493,     0,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
    -493,  -493,  -493,  -493,     0,  -493,  -493,     0,  -493,  -493,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -493,   699,
       0,  -493,  -493,     0,  -493,  -493,     0,  -493,  -493,  -493,
    -493,     0,     0,     0,  -493,     0,     0,     0,   -97,     0,
       0,     0,     0,     0,     0,     0,  -494,  -494,  -494,     0,
    -494,  -493,  -493,  -493,  -494,  -494,     0,     0,     0,  -494,
       0,  -494,  -494,  -494,  -494,  -494,  -494,  -494,     0,     0,
    -493,     0,     0,  -494,  -494,  -494,  -494,  -494,  -494,  -494,
       0,     0,  -494,     0,     0,     0,     0,     0,     0,     0,
       0,  -494,  -494,     0,  -494,  -494,  -494,  -494,  -494,  -494,
    -494,  -494,  -494,  -494,  -494,  -494,     0,  -494,  -494,     0,
    -494,  -494,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -494,   700,     0,  -494,  -494,     0,  -494,  -494,     0,  -494,
    -494,  -494,  -494,     0,     0,     0,  -494,     0,     0,     0,
     -99,     0,     0,     0,     0,     0,     0,     0,  -248,  -248,
    -248,     0,  -248,  -494,  -494,  -494,  -248,  -248,     0,     0,
       0,  -248,     0,  -248,  -248,  -248,  -248,  -248,  -248,  -248,
       0,     0,  -494,     0,     0,  -248,  -248,  -248,  -248,  -248,
    -248,  -248,     0,     0,  -248,     0,     0,     0,     0,     0,
       0,     0,     0,  -248,  -248,     0,  -248,  -248,  -248,  -248,
    -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,     0,  -248,
    -248,     0,  -248,  -248,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -248,     0,     0,  -248,  -248,     0,  -248,  -248,
       0,  -248,  -248,  -248,  -248,     0,     0,     0,  -248,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -248,  -248,  -248,     0,  -248,  -248,  -248,  -248,  -248,  -248,
       0,     0,     0,  -248,     0,  -248,  -248,  -248,  -248,  -248,
    -248,  -248,     0,     0,   245,     0,     0,  -248,  -248,  -248,
    -248,  -248,  -248,  -248,     0,     0,  -248,     0,     0,     0,
       0,     0,     0,     0,     0,  -248,  -248,     0,  -248,  -248,
    -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,
       0,  -248,  -248,     0,  -248,  -248,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -248,     0,     0,  -248,  -248,     0,
    -248,  -248,     0,  -248,  -248,  -248,  -248,     0,     0,     0,
    -248,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -495,  -495,  -495,     0,  -495,  -248,  -248,  -248,
    -495,  -495,     0,     0,     0,  -495,     0,  -495,  -495,  -495,
    -495,  -495,  -495,  -495,     0,     0,   248,     0,     0,  -495,
    -495,  -495,  -495,  -495,  -495,  -495,     0,     0,  -495,     0,
       0,     0,     0,     0,     0,     0,     0,  -495,  -495,     0,
    -495,  -495,  -495,  -495,  -495,  -495,  -495,  -495,  -495,  -495,
    -495,  -495,     0,  -495,  -495,     0,  -495,  -495,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -495,     0,     0,  -495,
    -495,     0,  -495,  -495,     0,  -495,  -495,  -495,  -495,     0,
       0,     0,  -495,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -496,  -496,  -496,     0,  -496,  -495,
    -495,  -495,  -496,  -496,     0,     0,     0,  -496,     0,  -496,
    -496,  -496,  -496,  -496,  -496,  -496,     0,     0,  -495,     0,
       0,  -496,  -496,  -496,  -496,  -496,  -496,  -496,     0,     0,
    -496,     0,     0,     0,     0,     0,     0,     0,     0,  -496,
    -496,     0,  -496,  -496,  -496,  -496,  -496,  -496,  -496,  -496,
    -496,  -496,  -496,  -496,     0,  -496,  -496,     0,  -496,  -496,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -496,     0,
       0,  -496,  -496,     0,  -496,  -496,     0,  -496,  -496,  -496,
    -496,     0,     0,     0,  -496,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,  -496,  -496,  -496,    10,    11,     0,     0,     0,    12,
       0,    13,    14,    15,    92,    93,    18,    19,     0,     0,
    -496,     0,     0,    94,    95,    96,    23,    24,    25,    26,
       0,     0,    97,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     274,     0,     0,   102,    50,     0,    51,    52,     0,     0,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,   103,    10,    11,     0,     0,     0,    12,
       0,    13,    14,    15,    92,    93,    18,    19,     0,     0,
       0,   275,     0,    94,    95,    96,    23,    24,    25,    26,
       0,     0,    97,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,   311,
     312,   313,   314,   315,   316,   317,   318,   319,   320,   321,
     322,   323,     0,     0,   324,   325,     0,     0,     0,     0,
     274,     0,     0,   102,    50,     0,    51,    52,     0,     0,
       0,    54,    55,     0,     0,     0,    56,     0,   326,     0,
     327,   328,   329,   330,   331,   332,   333,   334,   335,   336,
       0,     0,     0,   103,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   488,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,     0,     0,     0,   136,
     137,   138,   189,   190,   191,   192,   143,   144,   145,     0,
       0,     0,     0,     0,   146,   147,   148,   193,   194,   151,
     195,   153,   281,   282,   196,   283,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,     0,     0,     0,   136,
     137,   138,   189,   190,   191,   192,   143,   144,   145,     0,
       0,     0,     0,     0,   146,   147,   148,   193,   194,   151,
     195,   153,     0,     0,   196,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   170,     0,     0,    55,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,     0,     0,     0,   136,
     137,   138,   189,   190,   191,   192,   143,   144,   145,     0,
       0,     0,     0,     0,   146,   147,   148,   193,   194,   151,
     195,   153,     0,     0,   196,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,     5,     6,     7,     8,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,    29,    30,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,     0,     0,    49,
      50,     0,    51,    52,     0,    53,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     8,     9,     0,     0,     0,    10,    11,    57,
      58,    59,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,     0,    30,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    48,     0,     0,    49,    50,     0,    51,
      52,     0,    53,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    57,    58,    59,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    97,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
     231,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     207,     0,     0,   102,    50,     0,    51,    52,     0,   232,
     233,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    57,   234,    59,    12,     0,    13,    14,
      15,    92,    93,    18,    19,     0,     0,     0,     0,     0,
      94,    95,    96,    23,    24,    25,    26,     0,     0,    97,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,   231,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   207,     0,     0,
     102,    50,     0,    51,    52,     0,   596,   233,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      57,   234,    59,    12,     0,    13,    14,    15,    92,    93,
      18,    19,     0,     0,     0,     0,     0,    94,    95,    96,
      23,    24,    25,    26,     0,     0,    97,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,   231,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   207,     0,     0,   102,    50,     0,
      51,    52,     0,   232,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    57,   234,    59,
      12,     0,    13,    14,    15,    92,    93,    18,    19,     0,
       0,     0,     0,     0,    94,    95,    96,    23,    24,    25,
      26,     0,     0,    97,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,   231,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   207,     0,     0,   102,    50,     0,    51,    52,     0,
       0,   233,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    57,   234,    59,    12,     0,    13,
      14,    15,    92,    93,    18,    19,     0,     0,     0,     0,
       0,    94,    95,    96,    23,    24,    25,    26,     0,     0,
      97,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    33,    34,    35,    36,    37,    38,   231,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   207,     0,
       0,   102,    50,     0,    51,    52,     0,   596,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    57,   234,    59,    12,     0,    13,    14,    15,    92,
      93,    18,    19,     0,     0,     0,     0,     0,    94,    95,
      96,    23,    24,    25,    26,     0,     0,    97,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,   231,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   207,     0,     0,   102,    50,
       0,    51,    52,     0,     0,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    57,   234,
      59,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    97,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    33,    34,    35,    36,
      37,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   207,     0,     0,   102,    50,     0,    51,    52,
       0,   482,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    57,   234,    59,    12,     0,
      13,    14,    15,    92,    93,    18,    19,     0,     0,     0,
       0,     0,    94,    95,    96,    23,    24,    25,    26,     0,
       0,    97,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    33,    34,    35,    36,    37,    38,     0,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   102,    50,     0,    51,    52,     0,   482,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    57,   234,    59,    12,     0,    13,    14,    15,
      92,    93,    18,    19,     0,     0,     0,     0,     0,    94,
      95,    96,    23,    24,    25,    26,     0,     0,    97,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   207,     0,     0,   102,
      50,     0,    51,    52,     0,   232,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    57,
     234,    59,    12,     0,    13,    14,    15,    92,    93,    18,
      19,     0,     0,     0,     0,     0,    94,    95,    96,    23,
      24,    25,    26,     0,     0,    97,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   207,     0,     0,   102,    50,     0,    51,
      52,     0,   747,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    57,   234,    59,    12,
       0,    13,    14,    15,    92,    93,    18,    19,     0,     0,
       0,     0,     0,    94,    95,    96,    23,    24,    25,    26,
       0,     0,    97,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     207,     0,     0,   102,    50,     0,    51,    52,     0,   596,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    57,   234,    59,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   207,     0,     0,
     102,    50,     0,    51,    52,     0,     0,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      57,    58,    59,    12,     0,    13,    14,    15,    92,    93,
      18,    19,     0,     0,     0,     0,     0,    94,    95,    96,
      23,    24,    25,    26,     0,     0,    97,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   207,     0,     0,   102,    50,     0,
      51,    52,     0,     0,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    57,   234,    59,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    97,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   207,     0,     0,   102,    50,     0,    51,    52,     0,
       0,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    57,   234,    59,    12,     0,    13,
      14,    15,    92,    93,    18,    19,     0,     0,     0,     0,
       0,    94,    95,    96,    23,    24,    25,    26,     0,     0,
      97,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    98,    34,    35,    36,    99,    38,     0,    39,
      40,    41,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   100,     0,     0,   101,     0,
       0,   102,    50,     0,    51,    52,     0,     0,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,     0,     0,
      12,   103,    13,    14,    15,    92,    93,    18,    19,     0,
       0,     0,     0,     0,    94,    95,    96,    23,    24,    25,
      26,     0,     0,    97,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   224,     0,     0,    49,    50,     0,    51,    52,     0,
      53,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,   103,    13,    14,    15,    92,    93,
      18,    19,     0,     0,     0,     0,     0,    94,    95,    96,
      23,    24,    25,    26,     0,     0,    97,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   274,     0,     0,   307,    50,     0,
      51,    52,     0,   308,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,   103,    13,    14,
      15,    92,    93,    18,    19,     0,     0,     0,     0,     0,
      94,    95,    96,    23,    24,    25,    26,     0,     0,    97,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    98,    34,    35,    36,    99,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   101,     0,     0,
     102,    50,     0,    51,    52,     0,     0,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,     0,     0,     0,    12,
     103,    13,    14,    15,    92,    93,    18,    19,     0,     0,
       0,     0,     0,    94,    95,    96,    23,    24,    25,    26,
       0,     0,    97,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     274,     0,     0,   307,    50,     0,    51,    52,     0,     0,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,     0,
       0,     0,    12,   103,    13,    14,    15,    92,    93,    18,
      19,     0,     0,     0,     0,     0,    94,    95,    96,    23,
      24,    25,    26,     0,     0,    97,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   776,     0,     0,   102,    50,     0,    51,
      52,     0,     0,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,   103,    13,    14,    15,
      92,    93,    18,    19,     0,     0,     0,     0,     0,    94,
      95,    96,    23,    24,    25,    26,     0,     0,    97,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   855,     0,     0,   102,
      50,     0,    51,    52,     0,     0,     0,    54,    55,     0,
       0,     0,    56,     0,   523,   524,     0,     0,   525,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   103,
     155,   156,   157,   158,   159,   160,   161,   162,   163,     0,
       0,   164,   165,     0,     0,   166,   167,   168,   169,     0,
       0,   311,   312,   313,   314,   315,   316,   317,     0,   170,
     320,   321,     0,     0,     0,     0,   324,   325,     0,     0,
       0,     0,     0,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,   181,   182,     0,   531,   532,     0,
       0,   533,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   248,   155,   156,   157,   158,   159,   160,   161,
     162,   163,     0,     0,   164,   165,     0,     0,   166,   167,
     168,   169,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   170,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,     0,   181,   182,     0,
     552,   524,     0,     0,   553,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   248,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,     0,   537,   532,     0,     0,   538,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   248,   155,
     156,   157,   158,   159,   160,   161,   162,   163,     0,     0,
     164,   165,     0,     0,   166,   167,   168,   169,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   170,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,   181,   182,     0,   576,   524,     0,     0,
     577,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   248,   155,   156,   157,   158,   159,   160,   161,   162,
     163,     0,     0,   164,   165,     0,     0,   166,   167,   168,
     169,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   170,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,     0,   181,   182,     0,   579,
     532,     0,     0,   580,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   248,   155,   156,   157,   158,   159,
     160,   161,   162,   163,     0,     0,   164,   165,     0,     0,
     166,   167,   168,   169,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   170,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,     0,   181,
     182,     0,   589,   524,     0,     0,   590,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   248,   155,   156,
     157,   158,   159,   160,   161,   162,   163,     0,     0,   164,
     165,     0,     0,   166,   167,   168,   169,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   170,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,   181,   182,     0,   592,   532,     0,     0,   593,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     248,   155,   156,   157,   158,   159,   160,   161,   162,   163,
       0,     0,   164,   165,     0,     0,   166,   167,   168,   169,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     170,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,   181,   182,     0,   616,   524,
       0,     0,   617,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   248,   155,   156,   157,   158,   159,   160,
     161,   162,   163,     0,     0,   164,   165,     0,     0,   166,
     167,   168,   169,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   170,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,     0,   181,   182,
       0,   619,   532,     0,     0,   620,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   248,   155,   156,   157,
     158,   159,   160,   161,   162,   163,     0,     0,   164,   165,
       0,     0,   166,   167,   168,   169,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   170,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
       0,   181,   182,     0,   867,   524,     0,     0,   868,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   248,
     155,   156,   157,   158,   159,   160,   161,   162,   163,     0,
       0,   164,   165,     0,     0,   166,   167,   168,   169,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   170,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,   181,   182,     0,   870,   532,     0,
       0,   871,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   248,   155,   156,   157,   158,   159,   160,   161,
     162,   163,     0,     0,   164,   165,     0,     0,   166,   167,
     168,   169,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   170,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,     0,   181,   182,     0,
     901,   524,     0,     0,   902,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   248,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,     0,     0,   311,   312,   313,   314,   315,   316,
     317,   318,   319,   320,   321,   322,   323,     0,   248,   324,
     325,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     537,   532,     0,   326,   538,   327,   328,   329,   330,   331,
     332,   333,   334,   335,   336,     0,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,   252,     0,
       0,     0,     0,     0,     0,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,   311,   312,   313,   314,   315,   316,   317,   318,
     319,   320,   321,  -516,  -516,     0,     0,   324,   325,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   327,   328,   329,   330,   331,   332,   333,
     334,   335,   336
};

static const yytype_int16 yycheck[] =
{
       2,   280,    27,    79,    28,     8,   310,   215,     7,    22,
     372,    11,    12,   241,     2,    72,     4,     5,     6,   275,
     387,     9,    10,    11,    12,    28,   405,    15,    16,    17,
       4,   337,    20,   339,     5,     6,   342,   392,    16,    17,
     101,   268,    20,    58,    15,   272,     7,     8,   338,    51,
      52,    16,    17,    61,   433,    20,    55,   363,   280,   636,
      49,    49,    50,   633,   441,    53,    61,    28,    53,   364,
      58,   722,   362,    51,    52,   381,    89,   383,   556,     1,
      25,   720,    53,   402,    72,    13,   405,   366,   540,   541,
     396,    25,   382,   633,    55,    26,   636,   799,    13,    25,
     578,    72,   647,    22,    25,   395,    29,   647,    26,   817,
     310,   488,   100,   591,   102,    27,    25,   423,    13,    69,
      16,    17,    13,    86,    20,    13,   261,    25,   134,    86,
       2,    25,     4,    86,    84,    85,    25,    86,   444,     0,
     618,    88,    88,   662,    88,    25,    86,    37,    38,    86,
     669,    86,   138,   443,    51,    55,   795,   143,    55,    13,
     107,   107,   107,   107,   114,   115,   116,   117,   118,   119,
      89,   371,   307,   136,   137,   877,   455,    96,   886,   136,
     137,    28,   105,   136,   137,   441,   414,   136,   137,   111,
     139,   110,   843,   138,   739,   140,   204,   137,   143,   136,
     137,   203,   137,   134,   138,   139,   845,    88,   139,   204,
     218,   139,   138,   201,   142,   143,   134,   138,   275,   581,
     142,   143,   222,   223,   676,   802,   605,   142,   143,   138,
     142,   143,   488,   455,   222,   223,   806,   876,   240,   241,
     138,   139,    86,   138,   138,   815,   248,   142,   143,   138,
     107,   142,   143,   143,   142,   143,   551,   245,   138,   247,
     248,   138,   802,   251,    86,   253,   561,   245,   256,   247,
      77,   138,   261,   261,    86,   815,   143,   817,   655,    12,
     245,   560,   247,   248,    86,   604,   605,   275,   142,   143,
     305,   107,   136,   137,    49,   310,   296,   297,   298,   299,
     361,   139,   107,    86,   275,   112,    86,   295,   296,   297,
     298,   299,   300,   301,   302,   137,    86,   305,   307,   307,
     308,   295,   310,   308,   136,   137,   139,    25,    61,   201,
     143,   346,   347,   138,   136,   137,   338,   308,   560,   139,
      51,    86,    53,    54,    55,    56,   886,   102,   139,   245,
     338,   247,   656,   136,   137,    10,   711,   137,   346,   347,
     362,    51,   138,    53,    54,    55,    56,   137,   647,   371,
     372,   599,   105,   338,   362,   583,   364,   365,   339,     8,
     382,   342,   439,    55,   441,    86,   613,   375,    62,    63,
     378,   869,   137,   395,   382,   419,   138,   362,   386,   655,
      88,   143,   363,    86,    16,    17,    86,   395,    20,   107,
      88,   413,   414,   792,   107,    86,   419,   382,   420,   107,
     339,   788,   383,   295,   402,    88,   788,   138,   300,   107,
     395,   488,   420,    86,    26,   396,   137,   142,    86,    51,
     138,   443,   140,    69,   363,   143,    17,    86,   138,    55,
     138,   439,   430,   441,   137,   443,   136,   137,   419,   461,
     138,   463,   423,   465,   383,   136,   137,    15,   439,    17,
     441,   204,    26,   792,    25,   770,    26,   396,   443,   481,
     468,    89,   690,   444,   137,   218,   714,   512,   136,   137,
     134,   117,   118,   119,    86,   138,   775,   136,   137,    88,
     488,   256,    88,   375,   423,   530,   261,     2,    88,     4,
       5,     6,     7,   138,     9,   528,    69,   488,   107,   140,
      15,   107,   107,   536,   143,   444,   539,   107,   834,    58,
      59,   556,    86,   135,   536,   830,    86,   539,   540,   541,
     564,    88,   134,   833,   136,   137,   141,   139,   420,   138,
     550,    88,   307,   578,    49,   557,    55,    88,    53,   138,
     107,   564,   550,   551,   563,   567,   591,    69,   570,    88,
     107,   573,    26,   561,   138,   783,   107,    72,    69,   581,
     134,   569,   136,   137,   134,   139,   136,   137,   107,   139,
      95,   138,    15,   618,   594,   671,    13,   599,   655,   142,
     143,   138,   563,   564,   629,    10,   594,   102,    51,   528,
      53,    54,    55,    56,   450,   140,    69,   536,   138,   138,
     539,   138,   647,   378,   357,    51,   604,    53,    54,    55,
      56,   386,    86,   245,   135,   247,   555,   107,   557,    37,
      38,   656,   630,   631,   659,   660,   138,    90,    58,    59,
     665,   666,   138,    96,   138,   643,    62,    63,   391,   392,
     648,   251,   115,   116,   117,   118,   119,   655,   656,    14,
      15,   659,   660,   263,   676,    51,   138,   665,   666,    51,
     134,   138,   136,   137,   655,   673,   674,    40,    41,    42,
      43,    44,   138,   305,   138,   107,   696,    10,   310,    89,
       9,   689,    10,   135,   138,   693,   201,   135,   696,   138,
     109,   141,   714,   468,   739,    10,    10,    10,   689,   707,
     708,   709,     2,    10,     4,     5,     6,   729,   730,     9,
     732,   138,   734,   735,   138,    15,    10,   140,   135,   741,
     142,   743,   744,   758,    10,    11,    12,   138,    69,   138,
     138,   138,    52,   135,   112,   107,    10,   138,   138,   371,
     138,   256,    10,    84,    85,   138,   261,   135,    10,    49,
     758,    55,    10,    53,   138,   112,   388,   138,   135,    10,
     275,   769,   770,   138,    50,    55,   788,   623,   624,   138,
     138,   138,    72,   781,   115,   116,   117,   118,   119,   636,
     295,    10,   639,   138,    51,   300,    53,    54,    55,    56,
     647,    10,   307,   308,   569,   548,   139,   407,    15,   139,
     138,   411,   102,   848,   812,     6,   416,   420,   873,   562,
     539,   833,   672,   835,   100,   425,   623,   877,   428,   835,
       7,   812,   830,    90,   869,   833,   775,   815,   836,   851,
     852,   853,   854,    -1,   842,   633,   858,   859,   860,   188,
     862,   863,    -1,    -1,    -1,    -1,    -1,    -1,   833,   364,
      -1,   873,    -1,   834,    -1,   630,   631,    -1,    -1,    -1,
     375,    -1,    -1,   378,   720,    -1,   722,    -1,   643,    -1,
      -1,   386,     2,   648,     4,    -1,    -1,   487,   900,     9,
      -1,   903,   904,   905,   906,    15,    -1,    -1,    -1,    -1,
      -1,   913,    -1,    -1,    -1,   834,    -1,    -1,   673,   674,
      -1,   201,    -1,    -1,    -1,   420,    -1,    -1,    -1,    -1,
      -1,   231,   232,   233,    -1,    -1,    -1,    -1,   693,    49,
      -1,    -1,    -1,    -1,   439,   678,   441,    -1,    -1,   463,
      -1,   465,   707,   708,   709,    -1,   222,   223,    -1,   795,
     796,    -1,    -1,    -1,    -1,   802,    -1,   804,    -1,   559,
      -1,   808,    -1,   468,    -1,    -1,   256,    -1,   711,    -1,
     817,   261,   819,    -1,    -1,   251,    -1,   253,    -1,    -1,
      -1,    -1,   102,   488,     2,   275,     4,    -1,    -1,    -1,
      -1,     9,    -1,    -1,    -1,   305,    -1,   843,    -1,   845,
     310,    -1,    -1,    -1,   769,   295,    -1,    -1,    -1,    51,
     300,    53,    54,    55,    56,    -1,   781,   307,   308,    -1,
     296,   297,   298,   299,    -1,   301,   302,   627,    -1,    -1,
     876,    49,    -1,   880,    -1,    -1,    -1,    -1,    -1,   886,
      -1,   888,    -1,    -1,   891,    -1,   551,    -1,    90,    -1,
      -1,    -1,    -1,    -1,    96,    97,   561,    -1,    -1,    -1,
      -1,   371,   909,    -1,   569,    51,    -1,    53,    54,    55,
      56,   836,   114,    -1,   364,   117,    -1,   842,   388,    -1,
      -1,   201,    -1,   683,   102,   375,    -1,    -1,   378,   365,
      -1,   401,   402,    -1,    -1,   405,   386,    -1,    -1,    -1,
      -1,   143,    -1,    51,    90,    53,    54,    55,    56,    -1,
      96,    97,    -1,    51,    -1,    53,    54,    55,    56,   719,
     430,    -1,    -1,   433,    -1,   630,   631,    -1,   114,    -1,
     420,   117,    -1,    -1,    -1,    -1,   256,    -1,   643,    -1,
      -1,   261,    90,   648,    -1,    -1,    -1,    -1,    96,   439,
     655,   441,    90,   139,    -1,    -1,    -1,    -1,    96,    97,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   673,   674,
      -1,    -1,   482,    -1,    -1,   295,   114,    -1,   468,   117,
     300,    -1,    -1,   201,   689,    -1,    -1,   307,   693,    -1,
     310,    51,    -1,    53,    54,    55,    56,    -1,   488,    -1,
      -1,   139,   707,   708,   709,   729,   730,    -1,   732,    -1,
     734,   735,    -1,    -1,    -1,    -1,    -1,   741,    -1,   743,
     744,    -1,    -1,    -1,    51,    -1,    53,    54,    55,    56,
      90,    -1,    -1,    -1,    -1,    -1,    96,    97,   256,    -1,
      -1,    -1,    -1,   261,   364,    51,    -1,    53,    54,    55,
      56,    -1,    -1,    -1,   114,   375,    -1,   117,   378,    -1,
      -1,   551,    -1,    90,   769,   770,   386,    -1,    -1,    96,
      97,   561,    -1,    -1,   550,    -1,   781,   295,    -1,   569,
      -1,    -1,   300,    -1,    90,    -1,   596,   114,    -1,   307,
     117,    97,    -1,    -1,   604,   605,    -1,    -1,    -1,    -1,
     420,    -1,    -1,    -1,    -1,    -1,    -1,   812,   114,    -1,
      -1,   621,    -1,    -1,    -1,    -1,    -1,    -1,   594,    -1,
      -1,    -1,    -1,    -1,    -1,   830,    -1,   851,   852,   853,
     854,   836,    -1,    -1,   858,   859,   860,   842,   862,   863,
     630,   631,   652,    -1,    -1,    -1,   364,    -1,   468,    -1,
      -1,    -1,    -1,   643,    -1,    -1,    -1,   375,   648,    -1,
     378,    -1,    69,    -1,    -1,   655,    -1,    -1,   386,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   900,    84,    85,   903,
     904,   905,   906,   673,   674,    -1,    -1,    -1,    -1,   913,
      -1,    -1,    -1,    -1,    -1,    -1,   706,    -1,    -1,   689,
      -1,    -1,   420,   693,    -1,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,    -1,    -1,   707,   708,   709,
     696,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   551,    -1,    -1,    -1,    -1,    -1,   747,    -1,    -1,
      -1,   561,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   569,
     468,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      16,    17,    -1,    -1,    20,    -1,    -1,    -1,    -1,   769,
     770,    -1,   792,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   781,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    47,    -1,    -1,    -1,    51,    52,    -1,    -1,    -1,
     630,   631,    58,    59,    -1,    69,    70,    71,    72,    73,
      74,    75,   812,   643,    78,    79,    -1,    -1,   648,    -1,
      84,    85,    -1,   551,    -1,    -1,   656,    -1,    -1,    -1,
     830,    -1,    -1,   561,    -1,    -1,   836,    -1,    -1,    -1,
      -1,   569,   842,   673,   674,    -1,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,    -1,   689,
      -1,    -1,    -1,   693,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   707,   708,   709,
      -1,     0,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     8,
       9,    10,    -1,    -1,    13,    14,    15,    -1,    17,    -1,
      -1,    -1,   630,   631,    -1,    -1,    -1,    26,    27,    -1,
      -1,    -1,    -1,    -1,    -1,   643,    -1,    -1,    37,    38,
     648,    40,    41,    42,    43,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   769,
     770,    -1,    -1,    -1,    -1,   673,   674,    -1,    -1,    -1,
      -1,   781,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   693,    -1,    86,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   231,   232,   233,   234,   707,
     708,   709,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   245,
      -1,   247,   248,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     830,    -1,    -1,    -1,    -1,    -1,   836,    -1,    -1,    -1,
      -1,    -1,   842,    -1,    -1,   134,   135,    -1,   137,    -1,
     139,   140,    -1,   142,   143,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   769,   770,    -1,    -1,    -1,    -1,    -1,    -1,   305,
      -1,    -1,    -1,   781,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,    -1,   338,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     346,   347,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   830,    -1,    -1,    -1,   362,    -1,   836,    -1,
      -1,    -1,    -1,    -1,   842,   371,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   380,    -1,   382,    -1,   384,   385,
      -1,    -1,   388,    -1,    -1,    -1,    -1,    -1,    -1,   395,
      -1,    -1,    -1,    -1,    -1,   401,   402,    -1,    -1,   405,
      -1,     5,     6,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    15,    -1,    -1,    -1,    -1,    -1,    -1,   424,    -1,
      -1,    -1,    -1,    -1,   430,    -1,    -1,   433,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   443,    -1,    -1,
      -1,    -1,    46,    47,    -1,    -1,    -1,    51,    52,    53,
      -1,    -1,    -1,    -1,    -1,    59,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   471,   472,    -1,    72,    -1,
      -1,    -1,    -1,    -1,     0,     1,   482,     3,     4,     5,
       6,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     596,    -1,    -1,    -1,   120,   121,   122,    -1,   604,   605,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    -1,
      -1,    -1,    -1,    -1,    -1,   621,   142,   143,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   231,   232,   233,
     234,    -1,    -1,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,   652,    -1,    84,    85,
     656,   657,    -1,   659,   660,    -1,    -1,    -1,    -1,   665,
     666,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   675,
      -1,   275,   108,    -1,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   699,   700,    -1,   702,   703,    -1,    -1,
     706,    -1,   138,    -1,   308,    -1,    -1,   311,   312,   313,
     314,   315,   316,   317,   318,   319,   320,   321,   322,   323,
     324,   325,   326,   327,   328,   329,   330,   331,   332,   333,
     334,   335,   336,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   747,    -1,    -1,    -1,   751,    -1,    -1,    -1,    -1,
      -1,    -1,   758,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   371,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   380,    -1,    -1,   785,
     384,   385,    -1,    -1,   388,    -1,   792,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   401,   402,    -1,
      -1,   405,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     424,    -1,    -1,    -1,    -1,    -1,   430,   833,    -1,   433,
      -1,    -1,     0,    -1,    -1,   439,    -1,   441,    -1,    -1,
       8,     9,    10,    -1,    -1,    -1,    14,    15,    -1,    17,
      69,    70,    71,    72,    73,    74,    75,    76,    26,    78,
      79,    -1,    -1,    -1,    -1,    84,    85,   471,   472,    37,
      38,    -1,    40,    41,    42,    43,    44,    -1,   482,    -1,
      -1,    -1,    -1,    -1,   488,    -1,    -1,    -1,    -1,    -1,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    -1,    -1,    84,    85,    86,    -1,
      88,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   107,
     108,    -1,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   134,   135,   136,   137,
     138,    -1,   140,    -1,   142,   143,    -1,    -1,    -1,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,   596,    -1,    84,    85,    -1,    -1,    -1,    -1,
     604,   605,    -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   621,   108,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    -1,    -1,    84,    85,   652,     0,
      -1,   655,    -1,   657,    -1,    -1,    -1,     8,     9,    10,
      -1,    -1,    -1,    14,    15,    -1,    17,    -1,    -1,    -1,
     108,   675,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,    -1,   689,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,   699,   700,    -1,   702,   703,
      -1,    -1,   706,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    -1,    -1,    84,    85,    86,    -1,    88,    -1,    -1,
      -1,    -1,    -1,   747,    -1,    -1,    -1,   751,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   107,   108,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   785,    -1,    -1,   135,   136,   137,   138,   792,   140,
      -1,   142,   143,    -1,    -1,     1,    -1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    -1,   812,    15,
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
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,     3,     4,     5,     6,     7,   142,   143,    10,    11,
      12,    -1,    14,    15,    16,    -1,    18,    19,    20,    21,
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
     142,   143,    15,    16,    17,    18,    19,    20,    21,    22,
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
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,     3,
       4,     5,     6,     7,    -1,    -1,    10,    11,    12,   142,
     143,    15,    16,    -1,    18,    19,    20,    21,    22,    23,
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
      -1,    -1,    -1,    -1,    -1,     1,    -1,     3,     4,     5,
       6,     7,    -1,    -1,    -1,    11,    12,    -1,   142,   143,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   120,   121,   122,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   135,
      -1,    -1,    -1,    -1,    -1,    -1,   142,   143,     1,    -1,
       3,     4,     5,     6,     7,    -1,     9,    10,    11,    12,
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
      -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    11,    12,    -1,   142,
     143,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
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
      -1,    -1,    -1,    -1,    -1,   140,    -1,   142,   143,     1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,    11,
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
      -1,    -1,    -1,    -1,    -1,    -1,   135,    -1,    -1,    -1,
      -1,    -1,    -1,   142,   143,     1,    -1,     3,     4,     5,
       6,     7,    -1,    -1,    10,    11,    12,    -1,    -1,    -1,
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
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,    -1,   142,   143,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,    97,
      98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   120,   121,   122,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,   143,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    -1,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,
     122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,
     142,   143,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    -1,    58,    59,    60,    61,    62,    -1,
      64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
      94,    -1,    -1,    -1,    98,    99,    -1,    -1,    -1,   103,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   120,   121,   122,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,   143,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    -1,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,    -1,
      98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   120,   121,   122,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,   142,    18,    19,    20,    21,    22,
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
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   142,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    -1,    -1,    -1,
      -1,    -1,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    -1,    -1,    78,    79,    -1,    -1,    82,
      83,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,   121,   122,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,   139,    18,    19,    20,
      21,    22,    23,    24,    -1,    26,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    96,    97,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,
     121,   122,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,   134,    11,    12,    -1,    -1,   139,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    26,
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
       3,     4,     5,    -1,     7,    -1,    -1,   134,    11,    12,
      -1,    -1,   139,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    26,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    96,    97,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,   122,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,   134,    11,    12,    -1,    -1,   139,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    88,
      -1,    90,    91,    -1,    93,    94,    -1,    96,    97,    98,
      99,    -1,    -1,    -1,   103,    -1,    -1,    -1,   107,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,   120,   121,   122,    11,    12,    -1,    -1,    -1,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
     139,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    88,    -1,    90,    91,    -1,    93,    94,    -1,    96,
      97,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,
     107,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,   120,   121,   122,    11,    12,    -1,    -1,
      -1,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,   139,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    -1,    64,
      65,    -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,
      -1,    96,    97,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,   120,   121,   122,    11,    12,
      -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,   139,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    96,    97,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,   120,   121,   122,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,   139,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    96,    97,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,   120,
     121,   122,    11,    12,    -1,    -1,    -1,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,   139,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    -1,    93,    94,    -1,    96,    97,    98,
      99,    -1,    -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,   120,   121,   122,    11,    12,    -1,    -1,    -1,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
     139,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      -1,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    -1,
      -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,   120,    11,    12,    -1,    -1,    -1,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,   138,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      -1,    58,    59,    60,    61,    62,    -1,    64,    65,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    -1,    -1,    84,    85,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    -1,
      -1,    98,    99,    -1,    -1,    -1,   103,    -1,   108,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,    -1,   120,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   138,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    -1,    -1,    78,    79,    -1,
      -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,   122,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    50,
      51,    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    -1,    -1,    78,    79,    -1,
      -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    99,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,   122,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    50,
      51,    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    -1,    -1,    78,    79,    -1,
      -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,   122,     3,     4,     5,     6,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    -1,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    11,    12,   120,
     121,   122,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    45,    -1,    47,    48,    49,    -1,    51,    52,    53,
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
      90,    91,    -1,    93,    94,    -1,    96,    97,    98,    99,
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
      -1,    97,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,   120,   121,   122,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    -1,    93,    94,    -1,    96,    -1,    98,
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
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      -1,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,   120,   121,   122,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    -1,    58,
      59,    60,    61,    62,    -1,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    87,    -1,
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
      96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
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
      90,    91,    -1,    93,    94,    -1,    -1,    -1,    98,    99,
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
      -1,    -1,   103,    -1,    51,    52,    -1,    -1,    55,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    -1,
      -1,    78,    79,    -1,    -1,    82,    83,    84,    85,    -1,
      -1,    69,    70,    71,    72,    73,    74,    75,    -1,    96,
      78,    79,    -1,    -1,    -1,    -1,    84,    85,    -1,    -1,
      -1,    -1,    -1,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,   121,   122,    -1,    51,    52,    -1,
      -1,    55,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   139,    67,    68,    69,    70,    71,    72,    73,
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
      -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,   122,    -1,    -1,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    -1,   139,    84,
      85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      51,    52,    -1,   108,    55,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    -1,    -1,    78,    79,    -1,
      -1,    82,    83,    84,    85,    -1,    -1,    -1,   143,    -1,
      -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,   122,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    -1,    -1,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119
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
     166,   167,   168,   170,   171,   181,   194,   210,   229,   230,
     240,   241,   245,   246,   248,   249,   250,   251,   252,   275,
     286,   149,    21,    22,    30,    31,    32,    39,    51,    55,
      84,    87,    90,   120,   172,   173,   194,   210,   249,   252,
     275,   173,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    45,    46,    47,    48,
      49,    50,    51,    52,    55,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    78,    79,    82,    83,    84,    85,
      96,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   121,   122,   139,   174,   179,   180,   251,   270,    33,
      34,    35,    36,    48,    49,    51,    55,   174,   175,   177,
     246,     1,   151,   152,   153,   154,   286,    87,   157,   158,
     171,   210,   249,   250,   252,   158,   142,   143,   158,   279,
     284,   285,   197,   199,    87,   164,   171,   210,   215,   249,
     252,    57,    96,    97,   121,   163,   181,   182,   187,   190,
     192,   273,   274,   187,   187,   139,   188,   189,   139,   184,
     188,   139,   143,   280,   175,   150,   134,   181,   210,   181,
      55,    90,   152,   165,   166,   157,   183,   192,   273,   286,
     182,   272,   273,   286,    87,   138,   170,   210,   249,   252,
     196,    53,    54,    56,   174,   247,    62,    63,   242,    58,
      59,   159,   181,   181,   279,   285,    40,    41,    42,    43,
      44,    37,    38,    28,   227,   107,   138,    90,    96,   167,
     107,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    84,    85,   108,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    86,   136,   137,
      86,   137,   278,    26,   134,   231,    88,    88,   184,   188,
     231,   157,    51,    55,   172,    58,    59,     1,   111,   253,
     284,    86,   136,   137,   206,   271,   207,   278,   138,   154,
      10,     8,   236,   286,   279,   285,    55,    13,   211,   284,
     107,    86,   136,   137,    88,    88,   211,   279,    17,   234,
     142,   158,   158,    55,    86,   136,   137,    25,   182,   182,
     182,    89,   138,   191,   286,   138,   191,   187,   280,   281,
     187,   186,   187,   192,   273,   286,   157,   281,   157,   155,
     134,   152,    86,   137,    88,   165,   140,   281,   195,   141,
     138,   143,   283,   138,   283,   135,   283,    55,   167,   168,
     169,   138,    86,   136,   137,    51,    53,    54,    55,    56,
      90,    96,    97,   114,   117,   139,   225,   256,   257,   258,
     259,   260,   261,   264,   265,   266,   267,   268,   243,    62,
      63,    69,    69,   149,   158,   158,   158,   158,   154,   157,
     157,   228,    96,   159,   182,   192,   193,   165,   138,   170,
     138,   156,   159,   171,   181,   182,   193,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,    51,    52,    55,   179,   184,   276,   277,
     186,    51,    52,    55,   179,   184,   276,    51,    55,   276,
     233,   232,   159,   181,   159,   181,    95,   161,   204,   284,
     254,   203,    51,    55,   172,   276,   186,   276,   151,   157,
     139,   255,   256,   208,   178,   182,   193,   237,   286,    15,
     214,   286,   154,   152,    13,   181,    51,    55,   186,    51,
      55,   152,   234,   192,    10,    27,   212,   284,   212,    51,
      55,   186,    51,    55,   201,   182,    96,   182,   190,   273,
     274,   281,   140,   281,   138,   138,   281,   175,   147,   135,
     181,   281,   281,   273,   167,   169,    51,    55,   186,    51,
      55,   107,    51,    90,    96,   216,   217,   218,   258,   256,
      29,   105,   226,   138,   269,   286,   138,   269,    51,   138,
     269,    51,   152,   244,   181,   181,    77,   112,   220,   221,
     286,   182,   138,   281,   169,   138,   107,    44,   280,    88,
      88,   184,   188,   280,   282,    88,    88,   184,   185,   188,
     286,   185,   188,   220,   220,    44,   162,   284,   158,   151,
     282,    10,   281,   256,   151,   284,   174,   175,   176,    89,
     238,   286,   152,     9,   239,   286,    14,   213,   214,    88,
      88,   282,    88,    88,   214,    10,   138,   211,   198,   200,
     282,   158,   182,   191,   273,   135,   283,   282,   182,   218,
     138,   258,   138,   281,   222,   280,   152,   152,   259,   264,
     266,   268,   260,   261,   266,   260,   135,   152,    51,   219,
     222,   260,   262,   263,   266,   268,   152,    96,   182,   169,
     181,   109,   159,   181,   159,   181,   161,   141,    88,   159,
     181,   159,   181,   161,   231,   227,   152,   152,   181,   220,
     205,   284,    10,   281,    10,   209,    87,   171,   210,   249,
     252,   211,   152,   158,    10,    88,    10,   182,   152,   152,
     152,   212,   138,   281,   217,   138,    96,   216,   140,   142,
      10,   135,   138,   269,   138,   269,   138,   269,   138,   269,
     269,   135,   107,   222,   112,   138,   269,   138,   269,   138,
     269,    10,   182,   181,   159,   181,    10,   135,   152,   151,
     255,    55,    86,   136,   137,   152,   211,   214,   234,   235,
      10,    10,   202,   138,   217,   138,   258,    51,   223,   224,
     257,   260,   266,   260,   260,    87,   210,   112,   263,   266,
     260,   262,   266,   260,   135,    10,   151,    51,    55,   186,
      51,    55,   236,   152,   152,   217,   138,   138,   280,   269,
     138,   269,   269,   269,    55,    86,   138,   269,   138,   269,
     269,   138,   269,   269,    10,   282,   213,    10,   217,   224,
     260,    51,    55,   260,   266,   260,   260,   269,   269,   138,
     269,   269,   269,   260,   269
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
#line 967 "../../src/parse.y"
    {
		     p->lstate = EXPR_BEG;
		     local_nest(p);
		   ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 972 "../../src/parse.y"
    {
		      p->tree = new_scope(p, (yyvsp[(2) - (2)].node));
		     local_unnest(p);
		    ;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 979 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 985 "../../src/parse.y"
    {
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 989 "../../src/parse.y"
    {
		      (yyval.node) = new_begin(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 993 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), newline_node((yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 997 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 1004 "../../src/parse.y"
    {
		      if (p->in_def || p->in_single) {
			yyerror(p, "BEGIN in method");
		      }
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 1011 "../../src/parse.y"
    {
		      p->begin_tree = push(p->begin_tree, (yyvsp[(4) - (5)].node));
		      local_resume(p, (yyvsp[(2) - (5)].node));
		      (yyval.node) = 0;
		    ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 1022 "../../src/parse.y"
    {
		      if ((yyvsp[(2) - (4)].node)) {
			(yyval.node) = new_rescue(p, (yyvsp[(1) - (4)].node), (yyvsp[(2) - (4)].node), (yyvsp[(3) - (4)].node));
		      }
		      else if ((yyvsp[(3) - (4)].node)) {
			yywarn(p, "else without rescue is useless");
			(yyval.node) = append((yyval.node), (yyvsp[(3) - (4)].node));
		      }
		      else {
			(yyval.node) = (yyvsp[(1) - (4)].node);
		      }
		      if ((yyvsp[(4) - (4)].node)) {
			if ((yyval.node)) {
			  (yyval.node) = new_ensure(p, (yyval.node), (yyvsp[(4) - (4)].node));
			}
			else {
			  (yyval.node) = push((yyvsp[(4) - (4)].node), new_nil(p));
			}
		      }
		    ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 1045 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 1051 "../../src/parse.y"
    {
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 1055 "../../src/parse.y"
    {
		      (yyval.node) = new_begin(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 1059 "../../src/parse.y"
    {
			(yyval.node) = push((yyvsp[(1) - (3)].node), newline_node((yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 1063 "../../src/parse.y"
    {
		      (yyval.node) = new_begin(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 1068 "../../src/parse.y"
    {p->lstate = EXPR_FNAME;;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 1069 "../../src/parse.y"
    {
		      (yyval.node) = new_alias(p, (yyvsp[(2) - (4)].id), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 1073 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 1077 "../../src/parse.y"
    {
			(yyval.node) = new_if(p, cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node), 0);
		    ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 1081 "../../src/parse.y"
    {
		      (yyval.node) = new_unless(p, cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node), 0);
		    ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 1085 "../../src/parse.y"
    {
		      (yyval.node) = new_while(p, cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node));
		    ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 1089 "../../src/parse.y"
    {
		      (yyval.node) = new_until(p, cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node));
		    ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 1093 "../../src/parse.y"
    {
		      (yyval.node) = new_rescue(p, (yyvsp[(1) - (3)].node), list1(list3(0, 0, (yyvsp[(3) - (3)].node))), 0);
		    ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 1097 "../../src/parse.y"
    {
		      if (p->in_def || p->in_single) {
			yywarn(p, "END in method; use at_exit");
		      }
		      (yyval.node) = new_postexe(p, (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 1105 "../../src/parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(1) - (3)].node), list1((yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 1109 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 1113 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].node), intern("[]"), (yyvsp[(3) - (6)].node)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 1117 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 1121 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 1125 "../../src/parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.node) = 0;
		    ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 1130 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 1134 "../../src/parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].node));
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 1139 "../../src/parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (3)].node), new_array(p, (yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 1143 "../../src/parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 1147 "../../src/parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(1) - (3)].node), new_array(p, (yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 1154 "../../src/parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 1158 "../../src/parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 1166 "../../src/parse.y"
    {
		      (yyval.node) = new_and(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 1170 "../../src/parse.y"
    {
		      (yyval.node) = new_or(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 1174 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(3) - (3)].node)), "!");
		    ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 1178 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(2) - (2)].node)), "!");
		    ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 1185 "../../src/parse.y"
    {
		      if (!(yyvsp[(1) - (1)].node)) (yyval.node) = new_nil(p);
		      else (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 1200 "../../src/parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 1206 "../../src/parse.y"
    {
		      (yyval.node) = new_block(p, (yyvsp[(3) - (5)].node), (yyvsp[(4) - (5)].node));
		      local_unnest(p);
		    ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 1213 "../../src/parse.y"
    {
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 1217 "../../src/parse.y"
    {
		      args_with_block(p, (yyvsp[(2) - (3)].node), (yyvsp[(3) - (3)].node));
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (3)].id), (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 1222 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 1226 "../../src/parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		      (yyval.node) = new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
		   ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 1231 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 1235 "../../src/parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		      (yyval.node) = new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
		    ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 1240 "../../src/parse.y"
    {
		      (yyval.node) = new_super(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 1244 "../../src/parse.y"
    {
		      (yyval.node) = new_yield(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 1248 "../../src/parse.y"
    {
		      (yyval.node) = new_return(p, ret_args(p, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 1252 "../../src/parse.y"
    {
		      (yyval.node) = new_break(p, ret_args(p, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 1256 "../../src/parse.y"
    {
		      (yyval.node) = new_next(p, ret_args(p, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 1262 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 1266 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 1273 "../../src/parse.y"
    {
		      (yyval.node) = list1((yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 1279 "../../src/parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 1283 "../../src/parse.y"
    {
		      (yyval.node) = list1(push((yyvsp[(1) - (2)].node),(yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 1287 "../../src/parse.y"
    {
		      (yyval.node) = list2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 1291 "../../src/parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 1295 "../../src/parse.y"
    {
		      (yyval.node) = list2((yyvsp[(1) - (2)].node), new_nil(p));
		    ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 1299 "../../src/parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (4)].node), new_nil(p), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 1303 "../../src/parse.y"
    {
		      (yyval.node) = list2(0, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 1307 "../../src/parse.y"
    {
		      (yyval.node) = list3(0, (yyvsp[(2) - (4)].node), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 1311 "../../src/parse.y"
    {
		      (yyval.node) = list2(0, new_nil(p));
		    ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 1315 "../../src/parse.y"
    {
		      (yyval.node) = list3(0, new_nil(p), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 1322 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 1328 "../../src/parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (2)].node));
		    ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 1332 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 1338 "../../src/parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 1342 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 1348 "../../src/parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 1352 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), intern("[]"), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 1356 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 1360 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 1364 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 1368 "../../src/parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.node) = new_colon2(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 1374 "../../src/parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.node) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 1380 "../../src/parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].node));
		      (yyval.node) = 0;
		    ;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 1387 "../../src/parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 1391 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), intern("[]"), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 1395 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 1399 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 1403 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 1407 "../../src/parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.node) = new_colon2(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 1413 "../../src/parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.node) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 1419 "../../src/parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].node));
		      (yyval.node) = 0;
		    ;}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 1426 "../../src/parse.y"
    {
		      yyerror(p, "class/module name must be CONSTANT");
		    ;}
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 1433 "../../src/parse.y"
    {
		      (yyval.node) = cons((node*)1, (node*)(yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 1437 "../../src/parse.y"
    {
		      (yyval.node) = cons((node*)0, (node*)(yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 1441 "../../src/parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (3)].node), (node*)(yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 1450 "../../src/parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 111:

/* Line 1455 of yacc.c  */
#line 1455 "../../src/parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 1466 "../../src/parse.y"
    {
		      (yyval.node) = new_sym(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 115:

/* Line 1455 of yacc.c  */
#line 1472 "../../src/parse.y"
    {
		      (yyval.node) = new_undef(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 116:

/* Line 1455 of yacc.c  */
#line 1475 "../../src/parse.y"
    {p->lstate = EXPR_FNAME;;}
    break;

  case 117:

/* Line 1455 of yacc.c  */
#line 1476 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (4)].node), (node*)(yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 118:

/* Line 1455 of yacc.c  */
#line 1481 "../../src/parse.y"
    { (yyval.id) = intern("|"); ;}
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 1482 "../../src/parse.y"
    { (yyval.id) = intern("^"); ;}
    break;

  case 120:

/* Line 1455 of yacc.c  */
#line 1483 "../../src/parse.y"
    { (yyval.id) = intern("&"); ;}
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 1484 "../../src/parse.y"
    { (yyval.id) = intern("<=>"); ;}
    break;

  case 122:

/* Line 1455 of yacc.c  */
#line 1485 "../../src/parse.y"
    { (yyval.id) = intern("=="); ;}
    break;

  case 123:

/* Line 1455 of yacc.c  */
#line 1486 "../../src/parse.y"
    { (yyval.id) = intern("==="); ;}
    break;

  case 124:

/* Line 1455 of yacc.c  */
#line 1487 "../../src/parse.y"
    { (yyval.id) = intern("=~"); ;}
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 1488 "../../src/parse.y"
    { (yyval.id) = intern("!~"); ;}
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 1489 "../../src/parse.y"
    { (yyval.id) = intern(">"); ;}
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 1490 "../../src/parse.y"
    { (yyval.id) = intern(">="); ;}
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 1491 "../../src/parse.y"
    { (yyval.id) = intern("<"); ;}
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 1492 "../../src/parse.y"
    { (yyval.id) = intern(">="); ;}
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 1493 "../../src/parse.y"
    { (yyval.id) = intern("!="); ;}
    break;

  case 131:

/* Line 1455 of yacc.c  */
#line 1494 "../../src/parse.y"
    { (yyval.id) = intern("<<"); ;}
    break;

  case 132:

/* Line 1455 of yacc.c  */
#line 1495 "../../src/parse.y"
    { (yyval.id) = intern(">>"); ;}
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 1496 "../../src/parse.y"
    { (yyval.id) = intern("+"); ;}
    break;

  case 134:

/* Line 1455 of yacc.c  */
#line 1497 "../../src/parse.y"
    { (yyval.id) = intern("-"); ;}
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 1498 "../../src/parse.y"
    { (yyval.id) = intern("*"); ;}
    break;

  case 136:

/* Line 1455 of yacc.c  */
#line 1499 "../../src/parse.y"
    { (yyval.id) = intern("*"); ;}
    break;

  case 137:

/* Line 1455 of yacc.c  */
#line 1500 "../../src/parse.y"
    { (yyval.id) = intern("/"); ;}
    break;

  case 138:

/* Line 1455 of yacc.c  */
#line 1501 "../../src/parse.y"
    { (yyval.id) = intern("%"); ;}
    break;

  case 139:

/* Line 1455 of yacc.c  */
#line 1502 "../../src/parse.y"
    { (yyval.id) = intern("**"); ;}
    break;

  case 140:

/* Line 1455 of yacc.c  */
#line 1503 "../../src/parse.y"
    { (yyval.id) = intern("!"); ;}
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 1504 "../../src/parse.y"
    { (yyval.id) = intern("~"); ;}
    break;

  case 142:

/* Line 1455 of yacc.c  */
#line 1505 "../../src/parse.y"
    { (yyval.id) = intern("+@"); ;}
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 1506 "../../src/parse.y"
    { (yyval.id) = intern("-@"); ;}
    break;

  case 144:

/* Line 1455 of yacc.c  */
#line 1507 "../../src/parse.y"
    { (yyval.id) = intern("[]"); ;}
    break;

  case 145:

/* Line 1455 of yacc.c  */
#line 1508 "../../src/parse.y"
    { (yyval.id) = intern("[]="); ;}
    break;

  case 186:

/* Line 1455 of yacc.c  */
#line 1526 "../../src/parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 187:

/* Line 1455 of yacc.c  */
#line 1530 "../../src/parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (5)].node), new_rescue(p, (yyvsp[(3) - (5)].node), list1(list3(0, 0, (yyvsp[(5) - (5)].node))), 0));
		    ;}
    break;

  case 188:

/* Line 1455 of yacc.c  */
#line 1534 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 189:

/* Line 1455 of yacc.c  */
#line 1538 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, (yyvsp[(1) - (5)].node), (yyvsp[(2) - (5)].id), new_rescue(p, (yyvsp[(3) - (5)].node), list1(list3(0, 0, (yyvsp[(5) - (5)].node))), 0));
		    ;}
    break;

  case 190:

/* Line 1455 of yacc.c  */
#line 1542 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].node), intern("[]"), (yyvsp[(3) - (6)].node)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 191:

/* Line 1455 of yacc.c  */
#line 1546 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 192:

/* Line 1455 of yacc.c  */
#line 1550 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 193:

/* Line 1455 of yacc.c  */
#line 1554 "../../src/parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 194:

/* Line 1455 of yacc.c  */
#line 1558 "../../src/parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 195:

/* Line 1455 of yacc.c  */
#line 1563 "../../src/parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 196:

/* Line 1455 of yacc.c  */
#line 1568 "../../src/parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].node));
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 197:

/* Line 1455 of yacc.c  */
#line 1573 "../../src/parse.y"
    {
		      (yyval.node) = new_dot2(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 198:

/* Line 1455 of yacc.c  */
#line 1577 "../../src/parse.y"
    {
		      (yyval.node) = new_dot3(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 199:

/* Line 1455 of yacc.c  */
#line 1581 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "+", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 200:

/* Line 1455 of yacc.c  */
#line 1585 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "-", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 201:

/* Line 1455 of yacc.c  */
#line 1589 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "*", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 202:

/* Line 1455 of yacc.c  */
#line 1593 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "/", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 203:

/* Line 1455 of yacc.c  */
#line 1597 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "%", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 204:

/* Line 1455 of yacc.c  */
#line 1601 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "**", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 205:

/* Line 1455 of yacc.c  */
#line 1605 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].node), "**", (yyvsp[(4) - (4)].node)), "-@");
		    ;}
    break;

  case 206:

/* Line 1455 of yacc.c  */
#line 1609 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].node), "**", (yyvsp[(4) - (4)].node)), "-@");
		    ;}
    break;

  case 207:

/* Line 1455 of yacc.c  */
#line 1613 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, (yyvsp[(2) - (2)].node), "+@");
		    ;}
    break;

  case 208:

/* Line 1455 of yacc.c  */
#line 1617 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, (yyvsp[(2) - (2)].node), "-@");
		    ;}
    break;

  case 209:

/* Line 1455 of yacc.c  */
#line 1621 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "|", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 210:

/* Line 1455 of yacc.c  */
#line 1625 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "^", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 211:

/* Line 1455 of yacc.c  */
#line 1629 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "&", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 212:

/* Line 1455 of yacc.c  */
#line 1633 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "<=>", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 213:

/* Line 1455 of yacc.c  */
#line 1637 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), ">", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 214:

/* Line 1455 of yacc.c  */
#line 1641 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), ">=", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 215:

/* Line 1455 of yacc.c  */
#line 1645 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "<", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 216:

/* Line 1455 of yacc.c  */
#line 1649 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "<=", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 217:

/* Line 1455 of yacc.c  */
#line 1653 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "==", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 218:

/* Line 1455 of yacc.c  */
#line 1657 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "===", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 219:

/* Line 1455 of yacc.c  */
#line 1661 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "!=", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 220:

/* Line 1455 of yacc.c  */
#line 1665 "../../src/parse.y"
    {
		      (yyval.node) = match_op(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
#if 0
		      if (nd_type((yyvsp[(1) - (3)].node)) == NODE_LIT && TYPE((yyvsp[(1) - (3)].node)->nd_lit) == T_REGEXP) {
			(yyval.node) = reg_named_capture_assign((yyvsp[(1) - (3)].node)->nd_lit, (yyval.node));
		      }
#endif
		    ;}
    break;

  case 221:

/* Line 1455 of yacc.c  */
#line 1674 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "!~", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 222:

/* Line 1455 of yacc.c  */
#line 1678 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(2) - (2)].node)), "!");
		    ;}
    break;

  case 223:

/* Line 1455 of yacc.c  */
#line 1682 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(2) - (2)].node)), "~");
		    ;}
    break;

  case 224:

/* Line 1455 of yacc.c  */
#line 1686 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "<<", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 225:

/* Line 1455 of yacc.c  */
#line 1690 "../../src/parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), ">>", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 226:

/* Line 1455 of yacc.c  */
#line 1694 "../../src/parse.y"
    {
		      (yyval.node) = new_and(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 227:

/* Line 1455 of yacc.c  */
#line 1698 "../../src/parse.y"
    {
		      (yyval.node) = new_or(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 228:

/* Line 1455 of yacc.c  */
#line 1702 "../../src/parse.y"
    {
		      (yyval.node) = new_if(p, cond((yyvsp[(1) - (6)].node)), (yyvsp[(3) - (6)].node), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 229:

/* Line 1455 of yacc.c  */
#line 1706 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 230:

/* Line 1455 of yacc.c  */
#line 1712 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		      if (!(yyval.node)) (yyval.node) = new_nil(p);
		    ;}
    break;

  case 232:

/* Line 1455 of yacc.c  */
#line 1720 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 233:

/* Line 1455 of yacc.c  */
#line 1724 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (4)].node), new_hash(p, (yyvsp[(3) - (4)].node)));
		    ;}
    break;

  case 234:

/* Line 1455 of yacc.c  */
#line 1728 "../../src/parse.y"
    {
		      (yyval.node) = new_hash(p, (yyvsp[(1) - (2)].node));
		    ;}
    break;

  case 235:

/* Line 1455 of yacc.c  */
#line 1734 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 240:

/* Line 1455 of yacc.c  */
#line 1746 "../../src/parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (2)].node),0);
		    ;}
    break;

  case 241:

/* Line 1455 of yacc.c  */
#line 1750 "../../src/parse.y"
    {
		      (yyval.node) = cons(push((yyvsp[(1) - (4)].node), new_hash(p, (yyvsp[(3) - (4)].node))), 0);
		    ;}
    break;

  case 242:

/* Line 1455 of yacc.c  */
#line 1754 "../../src/parse.y"
    {
		      (yyval.node) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].node))), 0);
		    ;}
    break;

  case 243:

/* Line 1455 of yacc.c  */
#line 1760 "../../src/parse.y"
    {
		      (yyval.node) = cons(list1((yyvsp[(1) - (1)].node)), 0);
		    ;}
    break;

  case 244:

/* Line 1455 of yacc.c  */
#line 1764 "../../src/parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 245:

/* Line 1455 of yacc.c  */
#line 1768 "../../src/parse.y"
    {
		      (yyval.node) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].node))), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 246:

/* Line 1455 of yacc.c  */
#line 1772 "../../src/parse.y"
    {
		      (yyval.node) = cons(push((yyvsp[(1) - (4)].node), new_hash(p, (yyvsp[(3) - (4)].node))), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 247:

/* Line 1455 of yacc.c  */
#line 1776 "../../src/parse.y"
    {
		      (yyval.node) = cons(0, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 248:

/* Line 1455 of yacc.c  */
#line 1781 "../../src/parse.y"
    {
		      (yyval.num) = p->cmdarg_stack;
		      CMDARG_PUSH(1);
		    ;}
    break;

  case 249:

/* Line 1455 of yacc.c  */
#line 1786 "../../src/parse.y"
    {
		      /* CMDARG_POP() */
		      p->cmdarg_stack = (yyvsp[(1) - (2)].num);
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 250:

/* Line 1455 of yacc.c  */
#line 1794 "../../src/parse.y"
    {
		      (yyval.node) = new_block_arg(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 251:

/* Line 1455 of yacc.c  */
#line 1800 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 252:

/* Line 1455 of yacc.c  */
#line 1804 "../../src/parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;

  case 253:

/* Line 1455 of yacc.c  */
#line 1810 "../../src/parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (1)].node), 0);
		    ;}
    break;

  case 254:

/* Line 1455 of yacc.c  */
#line 1814 "../../src/parse.y"
    {
		      (yyval.node) = cons(new_splat(p, (yyvsp[(2) - (2)].node)), 0);
		    ;}
    break;

  case 255:

/* Line 1455 of yacc.c  */
#line 1818 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 256:

/* Line 1455 of yacc.c  */
#line 1822 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (4)].node), new_splat(p, (yyvsp[(4) - (4)].node)));
		    ;}
    break;

  case 257:

/* Line 1455 of yacc.c  */
#line 1828 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 258:

/* Line 1455 of yacc.c  */
#line 1832 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (4)].node), new_splat(p, (yyvsp[(4) - (4)].node)));
		    ;}
    break;

  case 259:

/* Line 1455 of yacc.c  */
#line 1836 "../../src/parse.y"
    {
		      (yyval.node) = list1(new_splat(p, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 265:

/* Line 1455 of yacc.c  */
#line 1847 "../../src/parse.y"
    {
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (1)].id), 0);
		    ;}
    break;

  case 266:

/* Line 1455 of yacc.c  */
#line 1853 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 267:

/* Line 1455 of yacc.c  */
#line 1856 "../../src/parse.y"
    {p->lstate = EXPR_ENDARG;;}
    break;

  case 268:

/* Line 1455 of yacc.c  */
#line 1857 "../../src/parse.y"
    {
		      yywarning(p, "(...) interpreted as grouped expression");
		      (yyval.node) = (yyvsp[(2) - (4)].node);
		    ;}
    break;

  case 269:

/* Line 1455 of yacc.c  */
#line 1862 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 270:

/* Line 1455 of yacc.c  */
#line 1866 "../../src/parse.y"
    {
		      (yyval.node) = new_colon2(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 271:

/* Line 1455 of yacc.c  */
#line 1870 "../../src/parse.y"
    {
		      (yyval.node) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 272:

/* Line 1455 of yacc.c  */
#line 1874 "../../src/parse.y"
    {
		      (yyval.node) = new_array(p, (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 273:

/* Line 1455 of yacc.c  */
#line 1878 "../../src/parse.y"
    {
		      (yyval.node) = new_hash(p, (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 274:

/* Line 1455 of yacc.c  */
#line 1882 "../../src/parse.y"
    {
		      (yyval.node) = new_return(p, 0);
		    ;}
    break;

  case 275:

/* Line 1455 of yacc.c  */
#line 1886 "../../src/parse.y"
    {
		      (yyval.node) = new_yield(p, (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 276:

/* Line 1455 of yacc.c  */
#line 1890 "../../src/parse.y"
    {
		      (yyval.node) = new_yield(p, 0);
		    ;}
    break;

  case 277:

/* Line 1455 of yacc.c  */
#line 1894 "../../src/parse.y"
    {
		      (yyval.node) = new_yield(p, 0);
		    ;}
    break;

  case 278:

/* Line 1455 of yacc.c  */
#line 1898 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(3) - (4)].node)), "!");
		    ;}
    break;

  case 279:

/* Line 1455 of yacc.c  */
#line 1902 "../../src/parse.y"
    {
		      (yyval.node) = call_uni_op(p, new_nil(p), "!");
		    ;}
    break;

  case 280:

/* Line 1455 of yacc.c  */
#line 1906 "../../src/parse.y"
    {
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (2)].id), cons(0, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 282:

/* Line 1455 of yacc.c  */
#line 1911 "../../src/parse.y"
    {
		      call_with_block(p, (yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 283:

/* Line 1455 of yacc.c  */
#line 1916 "../../src/parse.y"
    {
		      local_nest(p);
		      (yyval.num) = p->lpar_beg;
		      p->lpar_beg = ++p->paren_nest;
		    ;}
    break;

  case 284:

/* Line 1455 of yacc.c  */
#line 1923 "../../src/parse.y"
    {
		      p->lpar_beg = (yyvsp[(2) - (4)].num);
		      (yyval.node) = new_lambda(p, (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].node));
		      local_unnest(p);
		    ;}
    break;

  case 285:

/* Line 1455 of yacc.c  */
#line 1932 "../../src/parse.y"
    {
		      (yyval.node) = new_if(p, cond((yyvsp[(2) - (6)].node)), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		    ;}
    break;

  case 286:

/* Line 1455 of yacc.c  */
#line 1939 "../../src/parse.y"
    {
		      (yyval.node) = new_unless(p, cond((yyvsp[(2) - (6)].node)), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		    ;}
    break;

  case 287:

/* Line 1455 of yacc.c  */
#line 1942 "../../src/parse.y"
    {COND_PUSH(1);;}
    break;

  case 288:

/* Line 1455 of yacc.c  */
#line 1942 "../../src/parse.y"
    {COND_POP();;}
    break;

  case 289:

/* Line 1455 of yacc.c  */
#line 1945 "../../src/parse.y"
    {
		      (yyval.node) = new_while(p, cond((yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node));
		    ;}
    break;

  case 290:

/* Line 1455 of yacc.c  */
#line 1948 "../../src/parse.y"
    {COND_PUSH(1);;}
    break;

  case 291:

/* Line 1455 of yacc.c  */
#line 1948 "../../src/parse.y"
    {COND_POP();;}
    break;

  case 292:

/* Line 1455 of yacc.c  */
#line 1951 "../../src/parse.y"
    {
		      (yyval.node) = new_until(p, cond((yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node));
		    ;}
    break;

  case 293:

/* Line 1455 of yacc.c  */
#line 1957 "../../src/parse.y"
    {
		      (yyval.node) = new_case(p, (yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node));
		    ;}
    break;

  case 294:

/* Line 1455 of yacc.c  */
#line 1961 "../../src/parse.y"
    {
		      (yyval.node) = new_case(p, 0, (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 295:

/* Line 1455 of yacc.c  */
#line 1965 "../../src/parse.y"
    {COND_PUSH(1);;}
    break;

  case 296:

/* Line 1455 of yacc.c  */
#line 1967 "../../src/parse.y"
    {COND_POP();;}
    break;

  case 297:

/* Line 1455 of yacc.c  */
#line 1970 "../../src/parse.y"
    {
		      (yyval.node) = new_for(p, (yyvsp[(2) - (9)].node), (yyvsp[(5) - (9)].node), (yyvsp[(8) - (9)].node));
		    ;}
    break;

  case 298:

/* Line 1455 of yacc.c  */
#line 1974 "../../src/parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "class definition in method body");
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 299:

/* Line 1455 of yacc.c  */
#line 1981 "../../src/parse.y"
    {
		      (yyval.node) = new_class(p, (yyvsp[(2) - (6)].node), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].node));
		      local_resume(p, (yyvsp[(4) - (6)].node));
		    ;}
    break;

  case 300:

/* Line 1455 of yacc.c  */
#line 1986 "../../src/parse.y"
    {
		      (yyval.num) = p->in_def;
		      p->in_def = 0;
		    ;}
    break;

  case 301:

/* Line 1455 of yacc.c  */
#line 1991 "../../src/parse.y"
    {
		      (yyval.node) = cons(local_switch(p), (node*)p->in_single);
		      p->in_single = 0;
		    ;}
    break;

  case 302:

/* Line 1455 of yacc.c  */
#line 1997 "../../src/parse.y"
    {
		      (yyval.node) = new_sclass(p, (yyvsp[(3) - (8)].node), (yyvsp[(7) - (8)].node));
		      local_resume(p, (yyvsp[(6) - (8)].node)->car);
		      p->in_def = (yyvsp[(4) - (8)].num);
		      p->in_single = (int)(yyvsp[(6) - (8)].node)->cdr;
		    ;}
    break;

  case 303:

/* Line 1455 of yacc.c  */
#line 2004 "../../src/parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "module definition in method body");
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 304:

/* Line 1455 of yacc.c  */
#line 2011 "../../src/parse.y"
    {
		      (yyval.node) = new_module(p, (yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node));
		      local_resume(p, (yyvsp[(3) - (5)].node));
		    ;}
    break;

  case 305:

/* Line 1455 of yacc.c  */
#line 2016 "../../src/parse.y"
    {
		      p->in_def++;
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 306:

/* Line 1455 of yacc.c  */
#line 2023 "../../src/parse.y"
    {
		      (yyval.node) = new_def(p, (yyvsp[(2) - (6)].id), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		      local_resume(p, (yyvsp[(3) - (6)].node));
		      p->in_def--;
		    ;}
    break;

  case 307:

/* Line 1455 of yacc.c  */
#line 2028 "../../src/parse.y"
    {p->lstate = EXPR_FNAME;;}
    break;

  case 308:

/* Line 1455 of yacc.c  */
#line 2029 "../../src/parse.y"
    {
		      p->in_single++;
		      p->lstate = EXPR_ENDFN; /* force for args */
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 309:

/* Line 1455 of yacc.c  */
#line 2037 "../../src/parse.y"
    {
		      (yyval.node) = new_sdef(p, (yyvsp[(2) - (9)].node), (yyvsp[(5) - (9)].id), (yyvsp[(7) - (9)].node), (yyvsp[(8) - (9)].node));
		      local_resume(p, (yyvsp[(6) - (9)].node));
		      p->in_single--;
		    ;}
    break;

  case 310:

/* Line 1455 of yacc.c  */
#line 2043 "../../src/parse.y"
    {
		      (yyval.node) = new_break(p, 0);
		    ;}
    break;

  case 311:

/* Line 1455 of yacc.c  */
#line 2047 "../../src/parse.y"
    {
		      (yyval.node) = new_next(p, 0);
		    ;}
    break;

  case 312:

/* Line 1455 of yacc.c  */
#line 2051 "../../src/parse.y"
    {
		      (yyval.node) = new_redo(p);
		    ;}
    break;

  case 313:

/* Line 1455 of yacc.c  */
#line 2055 "../../src/parse.y"
    {
		      (yyval.node) = new_retry(p);
		    ;}
    break;

  case 314:

/* Line 1455 of yacc.c  */
#line 2061 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		      if (!(yyval.node)) (yyval.node) = new_nil(p);
		    ;}
    break;

  case 321:

/* Line 1455 of yacc.c  */
#line 2080 "../../src/parse.y"
    {
		      (yyval.node) = new_if(p, cond((yyvsp[(2) - (5)].node)), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 323:

/* Line 1455 of yacc.c  */
#line 2087 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 324:

/* Line 1455 of yacc.c  */
#line 2093 "../../src/parse.y"
    {
		      (yyval.node) = list1(list1((yyvsp[(1) - (1)].node)));
		    ;}
    break;

  case 326:

/* Line 1455 of yacc.c  */
#line 2100 "../../src/parse.y"
    {
		      (yyval.node) = new_arg(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 327:

/* Line 1455 of yacc.c  */
#line 2104 "../../src/parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(2) - (3)].node), 0);
		    ;}
    break;

  case 328:

/* Line 1455 of yacc.c  */
#line 2110 "../../src/parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 329:

/* Line 1455 of yacc.c  */
#line 2114 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 330:

/* Line 1455 of yacc.c  */
#line 2120 "../../src/parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (1)].node),0,0);
		    ;}
    break;

  case 331:

/* Line 1455 of yacc.c  */
#line 2124 "../../src/parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (4)].node), new_arg(p, (yyvsp[(4) - (4)].id)), 0);
		    ;}
    break;

  case 332:

/* Line 1455 of yacc.c  */
#line 2128 "../../src/parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (6)].node), new_arg(p, (yyvsp[(4) - (6)].id)), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 333:

/* Line 1455 of yacc.c  */
#line 2132 "../../src/parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (3)].node), (node*)-1, 0);
		    ;}
    break;

  case 334:

/* Line 1455 of yacc.c  */
#line 2136 "../../src/parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (5)].node), (node*)-1, (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 335:

/* Line 1455 of yacc.c  */
#line 2140 "../../src/parse.y"
    {
		      (yyval.node) = list3(0, new_arg(p, (yyvsp[(2) - (2)].id)), 0);
		    ;}
    break;

  case 336:

/* Line 1455 of yacc.c  */
#line 2144 "../../src/parse.y"
    {
		      (yyval.node) = list3(0, new_arg(p, (yyvsp[(2) - (4)].id)), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 337:

/* Line 1455 of yacc.c  */
#line 2148 "../../src/parse.y"
    {
		      (yyval.node) = list3(0, (node*)-1, 0);
		    ;}
    break;

  case 338:

/* Line 1455 of yacc.c  */
#line 2152 "../../src/parse.y"
    {
		      (yyval.node) = list3(0, (node*)-1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 339:

/* Line 1455 of yacc.c  */
#line 2158 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 340:

/* Line 1455 of yacc.c  */
#line 2162 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (8)].node), (yyvsp[(3) - (8)].node), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].node), (yyvsp[(8) - (8)].id));
		    ;}
    break;

  case 341:

/* Line 1455 of yacc.c  */
#line 2166 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node), 0, 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 342:

/* Line 1455 of yacc.c  */
#line 2170 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].node), 0, (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 343:

/* Line 1455 of yacc.c  */
#line 2174 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (4)].node), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 344:

/* Line 1455 of yacc.c  */
#line 2178 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (2)].node), 0, 1, 0, 0);
		    ;}
    break;

  case 345:

/* Line 1455 of yacc.c  */
#line 2182 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 346:

/* Line 1455 of yacc.c  */
#line 2186 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (2)].node), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 347:

/* Line 1455 of yacc.c  */
#line 2190 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 348:

/* Line 1455 of yacc.c  */
#line 2194 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 349:

/* Line 1455 of yacc.c  */
#line 2198 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (2)].node), 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 350:

/* Line 1455 of yacc.c  */
#line 2202 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (4)].node), 0, (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 351:

/* Line 1455 of yacc.c  */
#line 2206 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 352:

/* Line 1455 of yacc.c  */
#line 2210 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 353:

/* Line 1455 of yacc.c  */
#line 2214 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 355:

/* Line 1455 of yacc.c  */
#line 2221 "../../src/parse.y"
    {
		      p->cmd_start = TRUE;
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 356:

/* Line 1455 of yacc.c  */
#line 2228 "../../src/parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.node) = 0;
		    ;}
    break;

  case 357:

/* Line 1455 of yacc.c  */
#line 2233 "../../src/parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.node) = 0;
		    ;}
    break;

  case 358:

/* Line 1455 of yacc.c  */
#line 2238 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (4)].node);
		    ;}
    break;

  case 359:

/* Line 1455 of yacc.c  */
#line 2245 "../../src/parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;

  case 360:

/* Line 1455 of yacc.c  */
#line 2249 "../../src/parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;

  case 363:

/* Line 1455 of yacc.c  */
#line 2259 "../../src/parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      new_bv(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 365:

/* Line 1455 of yacc.c  */
#line 2267 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (4)].node);
		    ;}
    break;

  case 366:

/* Line 1455 of yacc.c  */
#line 2271 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 367:

/* Line 1455 of yacc.c  */
#line 2277 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 368:

/* Line 1455 of yacc.c  */
#line 2281 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 369:

/* Line 1455 of yacc.c  */
#line 2287 "../../src/parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 370:

/* Line 1455 of yacc.c  */
#line 2293 "../../src/parse.y"
    {
		      (yyval.node) = new_block(p,(yyvsp[(3) - (5)].node),(yyvsp[(4) - (5)].node));
		      local_unnest(p);
		    ;}
    break;

  case 371:

/* Line 1455 of yacc.c  */
#line 2300 "../../src/parse.y"
    {
		      if ((yyvsp[(1) - (2)].node)->car == (node*)NODE_YIELD) {
			yyerror(p, "block given to yield");
		      }
		      else {
		        call_with_block(p, (yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		      }
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 372:

/* Line 1455 of yacc.c  */
#line 2310 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 373:

/* Line 1455 of yacc.c  */
#line 2314 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
		      call_with_block(p, (yyval.node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 374:

/* Line 1455 of yacc.c  */
#line 2319 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
		      call_with_block(p, (yyval.node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 375:

/* Line 1455 of yacc.c  */
#line 2326 "../../src/parse.y"
    {
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 376:

/* Line 1455 of yacc.c  */
#line 2330 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 377:

/* Line 1455 of yacc.c  */
#line 2334 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 378:

/* Line 1455 of yacc.c  */
#line 2338 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 379:

/* Line 1455 of yacc.c  */
#line 2342 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), intern("call"), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 380:

/* Line 1455 of yacc.c  */
#line 2346 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), intern("call"), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 381:

/* Line 1455 of yacc.c  */
#line 2350 "../../src/parse.y"
    {
		      (yyval.node) = new_super(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 382:

/* Line 1455 of yacc.c  */
#line 2354 "../../src/parse.y"
    {
		      (yyval.node) = new_zsuper(p);
		    ;}
    break;

  case 383:

/* Line 1455 of yacc.c  */
#line 2358 "../../src/parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), intern("[]"), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 384:

/* Line 1455 of yacc.c  */
#line 2364 "../../src/parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 385:

/* Line 1455 of yacc.c  */
#line 2369 "../../src/parse.y"
    {
		      (yyval.node) = new_block(p,(yyvsp[(3) - (5)].node),(yyvsp[(4) - (5)].node));
		      local_unnest(p);
		    ;}
    break;

  case 386:

/* Line 1455 of yacc.c  */
#line 2374 "../../src/parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 387:

/* Line 1455 of yacc.c  */
#line 2379 "../../src/parse.y"
    {
		      (yyval.node) = new_block(p,(yyvsp[(3) - (5)].node),(yyvsp[(4) - (5)].node));
		      local_unnest(p);
		    ;}
    break;

  case 388:

/* Line 1455 of yacc.c  */
#line 2388 "../../src/parse.y"
    {
		      (yyval.node) = cons(cons((yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node)), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 389:

/* Line 1455 of yacc.c  */
#line 2394 "../../src/parse.y"
    {
		      if ((yyvsp[(1) - (1)].node)) {
			(yyval.node) = cons(cons(0, (yyvsp[(1) - (1)].node)), 0);
		      }
		      else {
			(yyval.node) = 0;
		      }
		    ;}
    break;

  case 391:

/* Line 1455 of yacc.c  */
#line 2408 "../../src/parse.y"
    {
		      (yyval.node) = list1(list3((yyvsp[(2) - (6)].node), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].node)));
		      if ((yyvsp[(6) - (6)].node)) (yyval.node) = append((yyval.node), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 393:

/* Line 1455 of yacc.c  */
#line 2416 "../../src/parse.y"
    {
			(yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 396:

/* Line 1455 of yacc.c  */
#line 2424 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 398:

/* Line 1455 of yacc.c  */
#line 2431 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 401:

/* Line 1455 of yacc.c  */
#line 2439 "../../src/parse.y"
    {
		      (yyval.node) = new_sym(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 404:

/* Line 1455 of yacc.c  */
#line 2447 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 405:

/* Line 1455 of yacc.c  */
#line 2451 "../../src/parse.y"
    {
		      (yyval.node) = new_dstr(p, push((yyvsp[(2) - (3)].node), (yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 406:

/* Line 1455 of yacc.c  */
#line 2457 "../../src/parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    ;}
    break;

  case 407:

/* Line 1455 of yacc.c  */
#line 2463 "../../src/parse.y"
    {
		      p->sterm = (yyvsp[(2) - (4)].num);
		      (yyval.node) = list2((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 408:

/* Line 1455 of yacc.c  */
#line 2469 "../../src/parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    ;}
    break;

  case 409:

/* Line 1455 of yacc.c  */
#line 2475 "../../src/parse.y"
    {
		      p->sterm = (yyvsp[(3) - (5)].num);
		      (yyval.node) = push(push((yyvsp[(1) - (5)].node), (yyvsp[(2) - (5)].node)), (yyvsp[(4) - (5)].node));
		    ;}
    break;

  case 411:

/* Line 1455 of yacc.c  */
#line 2485 "../../src/parse.y"
    {
		      p->lstate = EXPR_END;
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 418:

/* Line 1455 of yacc.c  */
#line 2500 "../../src/parse.y"
    {
		      (yyval.node) = negate_lit(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 419:

/* Line 1455 of yacc.c  */
#line 2504 "../../src/parse.y"
    {
		      (yyval.node) = negate_lit(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 420:

/* Line 1455 of yacc.c  */
#line 2510 "../../src/parse.y"
    {
		      (yyval.node) = new_lvar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 421:

/* Line 1455 of yacc.c  */
#line 2514 "../../src/parse.y"
    {
		      (yyval.node) = new_ivar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 422:

/* Line 1455 of yacc.c  */
#line 2518 "../../src/parse.y"
    {
		      (yyval.node) = new_gvar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 423:

/* Line 1455 of yacc.c  */
#line 2522 "../../src/parse.y"
    {
		      (yyval.node) = new_cvar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 424:

/* Line 1455 of yacc.c  */
#line 2526 "../../src/parse.y"
    {
		      (yyval.node) = new_const(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 425:

/* Line 1455 of yacc.c  */
#line 2532 "../../src/parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 426:

/* Line 1455 of yacc.c  */
#line 2538 "../../src/parse.y"
    {
		      (yyval.node) = var_reference(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 427:

/* Line 1455 of yacc.c  */
#line 2542 "../../src/parse.y"
    {
		      (yyval.node) = new_nil(p);
		    ;}
    break;

  case 428:

/* Line 1455 of yacc.c  */
#line 2546 "../../src/parse.y"
    {
		      (yyval.node) = new_self(p);
   		    ;}
    break;

  case 429:

/* Line 1455 of yacc.c  */
#line 2550 "../../src/parse.y"
    {
		      (yyval.node) = new_true(p);
   		    ;}
    break;

  case 430:

/* Line 1455 of yacc.c  */
#line 2554 "../../src/parse.y"
    {
		      (yyval.node) = new_false(p);
   		    ;}
    break;

  case 431:

/* Line 1455 of yacc.c  */
#line 2558 "../../src/parse.y"
    {
		      if (!p->filename) {
			p->filename = "(null)";
		      }
		      (yyval.node) = new_str(p, p->filename, strlen(p->filename));
		    ;}
    break;

  case 432:

/* Line 1455 of yacc.c  */
#line 2565 "../../src/parse.y"
    {
		      char buf[16];

		      snprintf(buf, 16, "%d", p->lineno);
		      (yyval.node) = new_int(p, buf, 10);
		    ;}
    break;

  case 435:

/* Line 1455 of yacc.c  */
#line 2578 "../../src/parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;

  case 436:

/* Line 1455 of yacc.c  */
#line 2582 "../../src/parse.y"
    {
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    ;}
    break;

  case 437:

/* Line 1455 of yacc.c  */
#line 2587 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(3) - (4)].node);
		    ;}
    break;

  case 438:

/* Line 1455 of yacc.c  */
#line 2591 "../../src/parse.y"
    {
		      yyerrok;
		      (yyval.node) = 0;
		    ;}
    break;

  case 439:

/* Line 1455 of yacc.c  */
#line 2598 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    ;}
    break;

  case 440:

/* Line 1455 of yacc.c  */
#line 2604 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 441:

/* Line 1455 of yacc.c  */
#line 2610 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 442:

/* Line 1455 of yacc.c  */
#line 2614 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (8)].node), (yyvsp[(3) - (8)].node), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].node), (yyvsp[(8) - (8)].id));
		    ;}
    break;

  case 443:

/* Line 1455 of yacc.c  */
#line 2618 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node), 0, 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 444:

/* Line 1455 of yacc.c  */
#line 2622 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].node), 0, (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 445:

/* Line 1455 of yacc.c  */
#line 2626 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (4)].node), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 446:

/* Line 1455 of yacc.c  */
#line 2630 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 447:

/* Line 1455 of yacc.c  */
#line 2634 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (2)].node), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 448:

/* Line 1455 of yacc.c  */
#line 2638 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 449:

/* Line 1455 of yacc.c  */
#line 2642 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 450:

/* Line 1455 of yacc.c  */
#line 2646 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (2)].node), 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 451:

/* Line 1455 of yacc.c  */
#line 2650 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (4)].node), 0, (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 452:

/* Line 1455 of yacc.c  */
#line 2654 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 453:

/* Line 1455 of yacc.c  */
#line 2658 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 454:

/* Line 1455 of yacc.c  */
#line 2662 "../../src/parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 455:

/* Line 1455 of yacc.c  */
#line 2666 "../../src/parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.node) = new_args(p, 0, 0, 0, 0, 0);
		    ;}
    break;

  case 456:

/* Line 1455 of yacc.c  */
#line 2673 "../../src/parse.y"
    {
		      yyerror(p, "formal argument cannot be a constant");
		      (yyval.node) = 0;
		    ;}
    break;

  case 457:

/* Line 1455 of yacc.c  */
#line 2678 "../../src/parse.y"
    {
		      yyerror(p, "formal argument cannot be an instance variable");
		      (yyval.node) = 0;
		    ;}
    break;

  case 458:

/* Line 1455 of yacc.c  */
#line 2683 "../../src/parse.y"
    {
		      yyerror(p, "formal argument cannot be a global variable");
		      (yyval.node) = 0;
		    ;}
    break;

  case 459:

/* Line 1455 of yacc.c  */
#line 2688 "../../src/parse.y"
    {
		      yyerror(p, "formal argument cannot be a class variable");
		      (yyval.node) = 0;
		    ;}
    break;

  case 460:

/* Line 1455 of yacc.c  */
#line 2695 "../../src/parse.y"
    {
		      (yyval.id) = 0;
		    ;}
    break;

  case 461:

/* Line 1455 of yacc.c  */
#line 2699 "../../src/parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 462:

/* Line 1455 of yacc.c  */
#line 2706 "../../src/parse.y"
    {
		      (yyval.node) = new_arg(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 463:

/* Line 1455 of yacc.c  */
#line 2710 "../../src/parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(2) - (3)].node), 0);
		    ;}
    break;

  case 464:

/* Line 1455 of yacc.c  */
#line 2716 "../../src/parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 465:

/* Line 1455 of yacc.c  */
#line 2720 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 466:

/* Line 1455 of yacc.c  */
#line 2726 "../../src/parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.node) = cons((node*)(yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 467:

/* Line 1455 of yacc.c  */
#line 2733 "../../src/parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.node) = cons((node*)(yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 468:

/* Line 1455 of yacc.c  */
#line 2740 "../../src/parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 469:

/* Line 1455 of yacc.c  */
#line 2744 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 470:

/* Line 1455 of yacc.c  */
#line 2750 "../../src/parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 471:

/* Line 1455 of yacc.c  */
#line 2754 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 474:

/* Line 1455 of yacc.c  */
#line 2764 "../../src/parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 475:

/* Line 1455 of yacc.c  */
#line 2769 "../../src/parse.y"
    {
		      (yyval.id) = 0;
		    ;}
    break;

  case 478:

/* Line 1455 of yacc.c  */
#line 2779 "../../src/parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 479:

/* Line 1455 of yacc.c  */
#line 2786 "../../src/parse.y"
    {
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 480:

/* Line 1455 of yacc.c  */
#line 2790 "../../src/parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.id) = 0;
		    ;}
    break;

  case 481:

/* Line 1455 of yacc.c  */
#line 2797 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		      if (!(yyval.node)) (yyval.node) = new_nil(p);
		    ;}
    break;

  case 482:

/* Line 1455 of yacc.c  */
#line 2801 "../../src/parse.y"
    {p->lstate = EXPR_BEG;;}
    break;

  case 483:

/* Line 1455 of yacc.c  */
#line 2802 "../../src/parse.y"
    {
		      if ((yyvsp[(3) - (4)].node) == 0) {
			yyerror(p, "can't define singleton method for ().");
		      }
		      else {
			switch ((enum node_type)(yyvsp[(3) - (4)].node)->car) {
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
		      (yyval.node) = (yyvsp[(3) - (4)].node);
		    ;}
    break;

  case 485:

/* Line 1455 of yacc.c  */
#line 2825 "../../src/parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 486:

/* Line 1455 of yacc.c  */
#line 2831 "../../src/parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 487:

/* Line 1455 of yacc.c  */
#line 2835 "../../src/parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 488:

/* Line 1455 of yacc.c  */
#line 2841 "../../src/parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 489:

/* Line 1455 of yacc.c  */
#line 2845 "../../src/parse.y"
    {
		      (yyval.node) = cons(new_sym(p, (yyvsp[(1) - (2)].id)), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 511:

/* Line 1455 of yacc.c  */
#line 2889 "../../src/parse.y"
    {yyerrok;;}
    break;

  case 514:

/* Line 1455 of yacc.c  */
#line 2894 "../../src/parse.y"
    {yyerrok;;}
    break;

  case 515:

/* Line 1455 of yacc.c  */
#line 2898 "../../src/parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;



/* Line 1455 of yacc.c  */
#line 8527 "../../src/y.tab.c"
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
#line 2902 "../../src/parse.y"

#define yylval  (*((YYSTYPE*)(p->ylval)))

static void
yyerror(parser_state *p, const char *s)
{
  char* c;
  size_t n;

  if (! p->capture_errors) {
    fputs(s, stderr);
    fputs("\n", stderr);
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
    fputs(s, stderr);
    fputs("\n", stderr);
  }
  else if (p->nerr < sizeof(p->warn_buffer) / sizeof(p->warn_buffer[0])) {
    n = strlen(s);
    c = parser_palloc(p, n + 1);
    memcpy(c, s, n + 1);
    p->error_buffer[p->nwarn].message = c;
    p->error_buffer[p->nwarn].lineno = p->lineno;
    p->error_buffer[p->nwarn].column = p->column;
  }
  p->nwarn++;
}

static void
yywarning(parser_state *p, const char *s)
{
  fputs(s, stderr);
  fputs("\n", stderr);
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
  switch ((int)n->car) {
  case NODE_NTH_REF:
    yyerror_i(p, "can't set variable $%d", (int)n->cdr);
    break;
  case NODE_BACK_REF:
    yyerror_i(p, "can't set variable $%c", (int)n->cdr);
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

    c = (int)p->pb->car;
    tmp = p->pb;
    p->pb = p->pb->cdr;
    cons_free(tmp);
  }
  else if (p->f) {
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
    if (p->column < 0) {
      p->column++; // pushback caused an underflow
    }
    else {
      p->lineno++;
      p->column = 0;
    }
    // must understand heredoc
  }
  else {
    p->column++;
  }
  return c;
}

static void
pushback(parser_state *p, int c)
{
  if (c < 0) return;
  p->column--;
  p->pb = cons((node*)c, p->pb);
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

  n++;				/* must read 1 char */
  while (n--) {
    c0 = nextc(p);
    if (c0 < 0) return FALSE;
    list = push(list, (node*)c0);
  }
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
	yylval.node = new_str(p, tok(p), toklen(p));
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
  yylval.node = new_str(p, tok(p), toklen(p));
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
  yylval.node = new_str(p, tok(p), toklen(p));
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
      }
      goto retry;
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
    yylval.node = new_str(p, tok(p), toklen(p));
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
      p->lstate = EXPR_ARG;
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
	  yylval.node = new_int(p, tok(p), 16);
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
	  yylval.node = new_int(p, tok(p), 2);
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
	  yylval.node = new_int(p, tok(p), 10);
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
	    yylval.node = new_int(p, tok(p), 8);
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
	  yylval.node = new_int(p, "0", 10);
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
	strtod(tok(p), 0);
	if (errno == ERANGE) {
	  yywarning_s(p, "float %s out of range", tok(p));
	  errno = 0;
	}
	yylval.node = new_float(p, tok(p));
	return tFLOAT;
      }
      yylval.node = new_int(p, tok(p), 10);
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
      yylval.node = new_back_ref(p, c);
      return tBACK_REF;

    case '1': case '2': case '3':
    case '4': case '5': case '6':
    case '7': case '8': case '9':
      tokadd(p, '$');
      do {
	tokadd(p, c);
	c = nextc(p);
      } while (c != -1 && isdigit(c));
      pushback(p, c);
      if (last_state == EXPR_FNAME) goto gvar;
      tokfix(p);
      yylval.node = new_nth_ref(p, atoi(tok(p)+1)); 
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

static void
start_parser(parser_state *p)
{
  node *tree;

  if (setjmp(p->jmp) != 0) {
    yyerror(p, "memory allocation error");
    p->nerr++;
    p->tree = p->begin_tree = 0;
    return;
  }
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
  else if (p->begin_tree) {
    tree = new_begin(p, p->begin_tree);
    append(tree, p->tree);
  }
}

static parser_state*
parser_new(mrb_state *mrb)
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

  p->cmd_start = TRUE;
  p->in_def = p->in_single = FALSE;

  p->capture_errors = NULL;

  p->lineno = 1;
#if defined(PARSER_TEST) || defined(PARSER_DEBUG)
  yydebug = 1;
#endif

  return p;
}

parser_state*
mrb_parse_file(mrb_state *mrb, FILE *f)
{
  parser_state *p;
 
  p = parser_new(mrb);
  if (!p) return 0;
  p->s = p->send = NULL;
  p->f = f;

  start_parser(p);
  return p;
}

parser_state*
mrb_parse_nstring(mrb_state *mrb, char *s, size_t len)
{
  parser_state *p;

  p = parser_new(mrb);
  if (!p) return 0;
  p->s = s;
  p->send = s + len;
  p->f = NULL;

  start_parser(p);
  return p;
}

parser_state*
mrb_parse_nstring_ext(mrb_state *mrb, char *s, size_t len)
{
  parser_state *p;

  p = parser_new(mrb);
  if (!p) return 0;
  p->s = s;
  p->send = s + len;
  p->f = NULL;
  p->capture_errors = 1;

  start_parser(p);
  return p;
}

parser_state*
mrb_parse_string(mrb_state *mrb, char *s)
{
  return mrb_parse_nstring(mrb, s, strlen(s));
}

#define PARSER_DUMP

void parser_dump(mrb_state *mrb, node *tree, int offset);
int mrb_generate_code(mrb_state*, mrb_ast_node*);

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
  return p->lineno = n;
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
  n = (int)tree->car;
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
	   (int)tree->cdr->car);
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
    printf("op='%s' (%d)\n", mrb_sym2name(mrb, (mrb_sym)tree->car), (int)tree->car);
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
    printf("NODE_BACK_REF:\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_NTH_REF:
    printf("NODE_NTH_REF:\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_ARG:
    printf("NODE_ARG %s\n", mrb_sym2name(mrb, (mrb_sym)tree));
    break;

  case NODE_BLOCK_ARG:
    printf("NODE_BLOCK_ARG:\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_INT:
    printf("NODE_INT %s base %d\n", (char*)tree->car, (int)tree->cdr->car);
    break;

  case NODE_FLOAT:
    printf("NODE_FLOAT %s\n", (char*)tree);
    break;

  case NODE_NEGATE:
    printf("NODE_NEGATE\n");
    parser_dump(mrb, tree, offset+1);
    break;

  case NODE_STR:
    printf("NODE_STR \"%s\" len %d\n", (char*)tree->car, (int)tree->cdr);
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

