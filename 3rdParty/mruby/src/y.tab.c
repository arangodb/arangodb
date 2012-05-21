
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



/* Line 189 of yacc.c  */
#line 876 "./y.tab.c"

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
#line 813 "./parse.y"

    node *node;
    mrb_sym id;
    int num;
    unsigned int stack;
    const struct vtable *vars;



/* Line 214 of yacc.c  */
#line 1040 "./y.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 1052 "./y.tab.c"

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
#define YYLAST   10113

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  144
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  144
/* YYNRULES -- Number of rules.  */
#define YYNRULES  516
/* YYNRULES -- Number of states.  */
#define YYNSTATES  916

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
     835,   839,   843,   846,   850,   854,   856,   861,   865,   867,
     872,   876,   879,   881,   884,   885,   890,   897,   904,   905,
     906,   914,   915,   916,   924,   930,   935,   936,   937,   947,
     948,   955,   956,   957,   966,   967,   973,   974,   981,   982,
     983,   993,   995,   997,   999,  1001,  1003,  1005,  1007,  1010,
    1012,  1014,  1016,  1022,  1024,  1027,  1029,  1031,  1033,  1037,
    1039,  1043,  1045,  1050,  1057,  1061,  1067,  1070,  1075,  1077,
    1081,  1088,  1097,  1102,  1109,  1114,  1117,  1124,  1127,  1132,
    1139,  1142,  1147,  1150,  1155,  1157,  1159,  1161,  1165,  1167,
    1172,  1174,  1179,  1181,  1185,  1187,  1189,  1194,  1196,  1200,
    1204,  1205,  1211,  1214,  1219,  1225,  1231,  1234,  1239,  1244,
    1248,  1252,  1256,  1259,  1261,  1266,  1267,  1273,  1274,  1280,
    1286,  1288,  1290,  1297,  1299,  1301,  1303,  1305,  1308,  1310,
    1313,  1315,  1317,  1319,  1321,  1323,  1326,  1330,  1331,  1336,
    1337,  1343,  1345,  1348,  1350,  1352,  1354,  1356,  1358,  1360,
    1363,  1366,  1368,  1370,  1372,  1374,  1376,  1378,  1380,  1382,
    1384,  1386,  1388,  1390,  1392,  1394,  1396,  1398,  1399,  1404,
    1407,  1411,  1414,  1421,  1430,  1435,  1442,  1447,  1454,  1457,
    1462,  1469,  1472,  1477,  1480,  1485,  1487,  1488,  1490,  1492,
    1494,  1496,  1498,  1500,  1502,  1506,  1508,  1512,  1516,  1520,
    1522,  1526,  1528,  1532,  1534,  1536,  1539,  1541,  1543,  1545,
    1548,  1551,  1553,  1555,  1556,  1561,  1563,  1566,  1568,  1572,
    1576,  1579,  1581,  1583,  1585,  1587,  1589,  1591,  1593,  1595,
    1597,  1599,  1601,  1603,  1604,  1606,  1607,  1609,  1612,  1615,
    1616,  1618,  1620,  1622,  1624,  1626,  1629
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     145,     0,    -1,    -1,   146,   147,    -1,   148,   280,    -1,
     287,    -1,   149,    -1,   148,   286,   149,    -1,     1,   149,
      -1,   154,    -1,    -1,    46,   150,   134,   147,   135,    -1,
     152,   237,   215,   240,    -1,   153,   280,    -1,   287,    -1,
     154,    -1,   153,   286,   154,    -1,     1,   154,    -1,    -1,
      45,   175,   155,   175,    -1,     6,   177,    -1,   154,    40,
     158,    -1,   154,    41,   158,    -1,   154,    42,   158,    -1,
     154,    43,   158,    -1,   154,    44,   154,    -1,    47,   134,
     152,   135,    -1,   156,    -1,   164,   107,   159,    -1,   251,
      88,   159,    -1,   211,   136,   186,   283,    88,   159,    -1,
     211,   137,    51,    88,   159,    -1,   211,   137,    55,    88,
     159,    -1,   211,    86,    55,    88,   159,    -1,   211,    86,
      51,    88,   159,    -1,   253,    88,   159,    -1,   171,   107,
     193,    -1,   164,   107,   182,    -1,   164,   107,   193,    -1,
     157,    -1,   171,   107,   159,    -1,   171,   107,   156,    -1,
     159,    -1,   157,    37,   157,    -1,   157,    38,   157,    -1,
      39,   281,   157,    -1,   121,   159,    -1,   181,    -1,   157,
      -1,   163,    -1,   160,    -1,   230,    -1,   230,   279,   277,
     188,    -1,    -1,    95,   162,   221,   152,   135,    -1,   276,
     188,    -1,   276,   188,   161,    -1,   211,   137,   277,   188,
      -1,   211,   137,   277,   188,   161,    -1,   211,    86,   277,
     188,    -1,   211,    86,   277,   188,   161,    -1,    32,   188,
      -1,    31,   188,    -1,    30,   187,    -1,    21,   187,    -1,
      22,   187,    -1,   166,    -1,    90,   165,   282,    -1,   166,
      -1,    90,   165,   282,    -1,   168,    -1,   168,   167,    -1,
     168,    96,   170,    -1,   168,    96,   170,   138,   169,    -1,
     168,    96,    -1,   168,    96,   138,   169,    -1,    96,   170,
      -1,    96,   170,   138,   169,    -1,    96,    -1,    96,   138,
     169,    -1,   170,    -1,    90,   165,   282,    -1,   167,   138,
      -1,   168,   167,   138,    -1,   167,    -1,   168,   167,    -1,
     250,    -1,   211,   136,   186,   283,    -1,   211,   137,    51,
      -1,   211,    86,    51,    -1,   211,   137,    55,    -1,   211,
      86,    55,    -1,    87,    55,    -1,   253,    -1,   250,    -1,
     211,   136,   186,   283,    -1,   211,   137,    51,    -1,   211,
      86,    51,    -1,   211,   137,    55,    -1,   211,    86,    55,
      -1,    87,    55,    -1,   253,    -1,    51,    -1,    55,    -1,
      87,   172,    -1,   172,    -1,   211,    86,   172,    -1,    51,
      -1,    55,    -1,    52,    -1,   179,    -1,   180,    -1,   174,
      -1,   247,    -1,   175,    -1,   175,    -1,    -1,   177,   138,
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
     107,   181,    -1,   171,   107,   181,    44,   181,    -1,   251,
      88,   181,    -1,   251,    88,   181,    44,   181,    -1,   211,
     136,   186,   283,    88,   181,    -1,   211,   137,    51,    88,
     181,    -1,   211,   137,    55,    88,   181,    -1,   211,    86,
      51,    88,   181,    -1,   211,    86,    55,    88,   181,    -1,
      87,    55,    88,   181,    -1,   253,    88,   181,    -1,   181,
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
      -1,   181,   108,   181,   281,   109,   181,    -1,   194,    -1,
     181,    -1,   287,    -1,   192,   284,    -1,   192,   138,   274,
     284,    -1,   274,   284,    -1,   139,   186,   282,    -1,   287,
      -1,   184,    -1,   287,    -1,   187,    -1,   192,   138,    -1,
     192,   138,   274,   138,    -1,   274,   138,    -1,   163,    -1,
     192,   191,    -1,   274,   191,    -1,   192,   138,   274,   191,
      -1,   190,    -1,    -1,   189,   187,    -1,    97,   182,    -1,
     138,   190,    -1,   287,    -1,   182,    -1,    96,   182,    -1,
     192,   138,   182,    -1,   192,   138,    96,   182,    -1,   192,
     138,   182,    -1,   192,   138,    96,   182,    -1,    96,   182,
      -1,   241,    -1,   242,    -1,   246,    -1,   252,    -1,   253,
      -1,    52,    -1,    -1,     7,   195,   151,    10,    -1,    -1,
      91,   157,   196,   282,    -1,    90,   152,   140,    -1,   211,
      86,    55,    -1,    87,    55,    -1,    93,   183,   141,    -1,
      94,   273,   135,    -1,    30,    -1,    31,   139,   187,   282,
      -1,    31,   139,   282,    -1,    31,    -1,    39,   139,   157,
     282,    -1,    39,   139,   282,    -1,   276,   232,    -1,   231,
      -1,   231,   232,    -1,    -1,    98,   197,   226,   227,    -1,
      11,   158,   212,   152,   214,    10,    -1,    12,   158,   212,
     152,   215,    10,    -1,    -1,    -1,    18,   198,   158,   213,
     199,   152,    10,    -1,    -1,    -1,    19,   200,   158,   213,
     201,   152,    10,    -1,    16,   158,   280,   235,    10,    -1,
      16,   280,   235,    10,    -1,    -1,    -1,    20,   216,    25,
     202,   158,   213,   203,   152,    10,    -1,    -1,     3,   173,
     254,   204,   151,    10,    -1,    -1,    -1,     3,    84,   157,
     205,   285,   206,   151,    10,    -1,    -1,     4,   173,   207,
     151,    10,    -1,    -1,     5,   174,   208,   256,   151,    10,
      -1,    -1,    -1,     5,   271,   279,   209,   174,   210,   256,
     151,    10,    -1,    21,    -1,    22,    -1,    23,    -1,    24,
      -1,   194,    -1,   285,    -1,    13,    -1,   285,    13,    -1,
     285,    -1,    27,    -1,   215,    -1,    14,   158,   212,   152,
     214,    -1,   287,    -1,    15,   152,    -1,   171,    -1,   164,
      -1,   259,    -1,    90,   219,   282,    -1,   217,    -1,   218,
     138,   217,    -1,   218,    -1,   218,   138,    96,   259,    -1,
     218,   138,    96,   259,   138,   218,    -1,   218,   138,    96,
      -1,   218,   138,    96,   138,   218,    -1,    96,   259,    -1,
      96,   259,   138,   218,    -1,    96,    -1,    96,   138,   218,
      -1,   261,   138,   264,   138,   267,   270,    -1,   261,   138,
     264,   138,   267,   138,   261,   270,    -1,   261,   138,   264,
     270,    -1,   261,   138,   264,   138,   261,   270,    -1,   261,
     138,   267,   270,    -1,   261,   138,    -1,   261,   138,   267,
     138,   261,   270,    -1,   261,   270,    -1,   264,   138,   267,
     270,    -1,   264,   138,   267,   138,   261,   270,    -1,   264,
     270,    -1,   264,   138,   261,   270,    -1,   267,   270,    -1,
     267,   138,   261,   270,    -1,   269,    -1,   287,    -1,   222,
      -1,   112,   223,   112,    -1,    77,    -1,   112,   220,   223,
     112,    -1,   281,    -1,   281,   142,   224,   281,    -1,   225,
      -1,   224,   138,   225,    -1,    51,    -1,   258,    -1,   139,
     257,   223,   140,    -1,   257,    -1,   105,   152,   135,    -1,
      29,   152,    10,    -1,    -1,    28,   229,   221,   152,    10,
      -1,   163,   228,    -1,   230,   279,   277,   185,    -1,   230,
     279,   277,   185,   232,    -1,   230,   279,   277,   188,   228,
      -1,   276,   184,    -1,   211,   137,   277,   185,    -1,   211,
      86,   277,   184,    -1,   211,    86,   278,    -1,   211,   137,
     184,    -1,   211,    86,   184,    -1,    32,   184,    -1,    32,
      -1,   211,   136,   186,   283,    -1,    -1,   134,   233,   221,
     152,   135,    -1,    -1,    26,   234,   221,   152,    10,    -1,
      17,   192,   212,   152,   236,    -1,   215,    -1,   235,    -1,
       8,   238,   239,   212,   152,   237,    -1,   287,    -1,   182,
      -1,   193,    -1,   287,    -1,    89,   171,    -1,   287,    -1,
       9,   152,    -1,   287,    -1,   249,    -1,   247,    -1,    60,
      -1,    62,    -1,   103,    62,    -1,   103,   243,    62,    -1,
      -1,    63,   244,   152,   135,    -1,    -1,   243,    63,   245,
     152,   135,    -1,    61,    -1,    99,   248,    -1,   174,    -1,
      54,    -1,    53,    -1,    56,    -1,    58,    -1,    59,    -1,
     120,    58,    -1,   120,    59,    -1,    51,    -1,    54,    -1,
      53,    -1,    56,    -1,    55,    -1,   250,    -1,   250,    -1,
      34,    -1,    33,    -1,    35,    -1,    36,    -1,    49,    -1,
      48,    -1,    64,    -1,    65,    -1,   285,    -1,    -1,   111,
     255,   158,   285,    -1,     1,   285,    -1,   139,   257,   282,
      -1,   257,   285,    -1,   261,   138,   265,   138,   267,   270,
      -1,   261,   138,   265,   138,   267,   138,   261,   270,    -1,
     261,   138,   265,   270,    -1,   261,   138,   265,   138,   261,
     270,    -1,   261,   138,   267,   270,    -1,   261,   138,   267,
     138,   261,   270,    -1,   261,   270,    -1,   265,   138,   267,
     270,    -1,   265,   138,   267,   138,   261,   270,    -1,   265,
     270,    -1,   265,   138,   261,   270,    -1,   267,   270,    -1,
     267,   138,   261,   270,    -1,   269,    -1,    -1,    55,    -1,
      54,    -1,    53,    -1,    56,    -1,   258,    -1,    51,    -1,
     259,    -1,    90,   219,   282,    -1,   260,    -1,   261,   138,
     260,    -1,    51,   107,   182,    -1,    51,   107,   211,    -1,
     263,    -1,   264,   138,   263,    -1,   262,    -1,   265,   138,
     262,    -1,   117,    -1,    96,    -1,   266,    51,    -1,   266,
      -1,   114,    -1,    97,    -1,   268,    51,    -1,   138,   269,
      -1,   287,    -1,   252,    -1,    -1,   139,   272,   157,   282,
      -1,   287,    -1,   274,   284,    -1,   275,    -1,   274,   138,
     275,    -1,   182,    89,   182,    -1,    57,   182,    -1,    51,
      -1,    55,    -1,    52,    -1,    51,    -1,    55,    -1,    52,
      -1,   179,    -1,    51,    -1,    52,    -1,   179,    -1,   137,
      -1,    86,    -1,    -1,   286,    -1,    -1,   143,    -1,   281,
     140,    -1,   281,   141,    -1,    -1,   143,    -1,   138,    -1,
     142,    -1,   143,    -1,   285,    -1,   286,   142,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   968,   968,   968,   979,   985,   989,   993,   997,  1003,
    1005,  1004,  1019,  1045,  1051,  1055,  1059,  1063,  1069,  1069,
    1073,  1077,  1081,  1085,  1089,  1093,  1097,  1104,  1105,  1109,
    1113,  1117,  1121,  1125,  1130,  1134,  1139,  1143,  1147,  1151,
    1154,  1158,  1165,  1166,  1170,  1174,  1178,  1182,  1185,  1192,
    1193,  1196,  1197,  1201,  1200,  1213,  1217,  1222,  1226,  1231,
    1235,  1240,  1244,  1248,  1252,  1256,  1262,  1266,  1272,  1273,
    1279,  1283,  1287,  1291,  1295,  1299,  1303,  1307,  1311,  1315,
    1321,  1322,  1328,  1332,  1338,  1342,  1348,  1352,  1356,  1360,
    1364,  1368,  1374,  1380,  1387,  1391,  1395,  1399,  1403,  1407,
    1413,  1419,  1426,  1430,  1433,  1437,  1441,  1447,  1448,  1449,
    1450,  1455,  1462,  1463,  1466,  1472,  1476,  1476,  1482,  1483,
    1484,  1485,  1486,  1487,  1488,  1489,  1490,  1491,  1492,  1493,
    1494,  1495,  1496,  1497,  1498,  1499,  1500,  1501,  1502,  1503,
    1504,  1505,  1506,  1507,  1508,  1509,  1512,  1512,  1512,  1513,
    1513,  1514,  1514,  1514,  1515,  1515,  1515,  1515,  1516,  1516,
    1516,  1517,  1517,  1517,  1518,  1518,  1518,  1518,  1519,  1519,
    1519,  1519,  1520,  1520,  1520,  1520,  1521,  1521,  1521,  1521,
    1522,  1522,  1522,  1522,  1523,  1523,  1526,  1530,  1534,  1538,
    1542,  1546,  1550,  1554,  1558,  1563,  1568,  1573,  1577,  1581,
    1585,  1589,  1593,  1597,  1601,  1605,  1609,  1613,  1617,  1621,
    1625,  1629,  1633,  1637,  1641,  1645,  1649,  1653,  1657,  1661,
    1665,  1674,  1678,  1682,  1686,  1690,  1694,  1698,  1702,  1706,
    1712,  1719,  1720,  1724,  1728,  1734,  1740,  1741,  1744,  1745,
    1746,  1750,  1754,  1760,  1764,  1768,  1772,  1776,  1782,  1782,
    1793,  1799,  1803,  1809,  1813,  1817,  1821,  1827,  1831,  1835,
    1841,  1842,  1843,  1844,  1845,  1846,  1851,  1850,  1861,  1861,
    1866,  1870,  1874,  1878,  1882,  1886,  1890,  1894,  1898,  1902,
    1906,  1910,  1914,  1915,  1921,  1920,  1933,  1940,  1947,  1947,
    1947,  1953,  1953,  1953,  1959,  1965,  1970,  1972,  1969,  1979,
    1978,  1991,  1996,  1990,  2009,  2008,  2021,  2020,  2033,  2034,
    2033,  2047,  2051,  2055,  2059,  2065,  2072,  2073,  2074,  2077,
    2078,  2081,  2082,  2090,  2091,  2097,  2101,  2104,  2108,  2114,
    2118,  2124,  2128,  2132,  2136,  2140,  2144,  2148,  2152,  2156,
    2162,  2166,  2170,  2174,  2178,  2182,  2186,  2190,  2194,  2198,
    2202,  2206,  2210,  2214,  2218,  2224,  2225,  2232,  2237,  2242,
    2249,  2253,  2259,  2260,  2263,  2268,  2271,  2275,  2281,  2285,
    2292,  2291,  2304,  2314,  2318,  2323,  2330,  2334,  2338,  2342,
    2346,  2350,  2354,  2358,  2362,  2369,  2368,  2379,  2378,  2390,
    2398,  2407,  2410,  2417,  2420,  2424,  2425,  2428,  2432,  2435,
    2439,  2442,  2443,  2449,  2450,  2451,  2455,  2462,  2461,  2474,
    2472,  2486,  2489,  2496,  2497,  2498,  2499,  2502,  2503,  2504,
    2508,  2514,  2518,  2522,  2526,  2530,  2536,  2542,  2546,  2550,
    2554,  2558,  2562,  2569,  2578,  2579,  2582,  2587,  2586,  2595,
    2602,  2608,  2614,  2618,  2622,  2626,  2630,  2634,  2638,  2642,
    2646,  2650,  2654,  2658,  2662,  2666,  2671,  2677,  2682,  2687,
    2692,  2699,  2703,  2710,  2714,  2720,  2724,  2730,  2737,  2744,
    2748,  2754,  2758,  2764,  2765,  2768,  2773,  2779,  2780,  2783,
    2790,  2794,  2801,  2806,  2806,  2828,  2829,  2835,  2839,  2845,
    2849,  2855,  2856,  2857,  2860,  2861,  2862,  2863,  2866,  2867,
    2868,  2871,  2872,  2875,  2876,  2879,  2880,  2883,  2886,  2889,
    2890,  2891,  2894,  2895,  2898,  2899,  2903
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
  "opt_block_arg", "args", "mrhs", "primary", "$@7", "$@8", "@9", "$@10",
  "$@11", "$@12", "$@13", "$@14", "$@15", "@16", "@17", "@18", "@19",
  "@20", "$@21", "@22", "primary_value", "then", "do", "if_tail",
  "opt_else", "for_var", "f_marg", "f_marg_list", "f_margs", "block_param",
  "opt_block_param", "block_param_def", "opt_bv_decl", "bv_decls", "bvar",
  "f_larglist", "lambda_body", "do_block", "$@23", "block_call",
  "method_call", "brace_block", "$@24", "$@25", "case_body", "cases",
  "opt_rescue", "exc_list", "exc_var", "opt_ensure", "literal", "string",
  "string_interp", "@26", "@27", "regexp", "symbol", "sym", "numeric",
  "variable", "var_lhs", "var_ref", "backref", "superclass", "$@28",
  "f_arglist", "f_args", "f_bad_arg", "f_norm_arg", "f_arg_item", "f_arg",
  "f_opt", "f_block_opt", "f_block_optarg", "f_optarg", "restarg_mark",
  "f_rest_arg", "blkarg_mark", "f_block_arg", "opt_f_block_arg",
  "singleton", "$@29", "assoc_list", "assocs", "assoc", "operation",
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
     194,   194,   194,   194,   194,   194,   194,   194,   194,   194,
     194,   194,   194,   194,   197,   194,   194,   194,   198,   199,
     194,   200,   201,   194,   194,   194,   202,   203,   194,   204,
     194,   205,   206,   194,   207,   194,   208,   194,   209,   210,
     194,   194,   194,   194,   194,   211,   212,   212,   212,   213,
     213,   214,   214,   215,   215,   216,   216,   217,   217,   218,
     218,   219,   219,   219,   219,   219,   219,   219,   219,   219,
     220,   220,   220,   220,   220,   220,   220,   220,   220,   220,
     220,   220,   220,   220,   220,   221,   221,   222,   222,   222,
     223,   223,   224,   224,   225,   225,   226,   226,   227,   227,
     229,   228,   230,   230,   230,   230,   231,   231,   231,   231,
     231,   231,   231,   231,   231,   233,   232,   234,   232,   235,
     236,   236,   237,   237,   238,   238,   238,   239,   239,   240,
     240,   241,   241,   242,   242,   242,   242,   244,   243,   245,
     243,   246,   247,   248,   248,   248,   248,   249,   249,   249,
     249,   250,   250,   250,   250,   250,   251,   252,   252,   252,
     252,   252,   252,   252,   253,   253,   254,   255,   254,   254,
     256,   256,   257,   257,   257,   257,   257,   257,   257,   257,
     257,   257,   257,   257,   257,   257,   257,   258,   258,   258,
     258,   259,   259,   260,   260,   261,   261,   262,   263,   264,
     264,   265,   265,   266,   266,   267,   267,   268,   268,   269,
     270,   270,   271,   272,   271,   273,   273,   274,   274,   275,
     275,   276,   276,   276,   277,   277,   277,   277,   278,   278,
     278,   279,   279,   280,   280,   281,   281,   282,   283,   284,
     284,   284,   285,   285,   286,   286,   287
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
       3,     3,     2,     3,     3,     1,     4,     3,     1,     4,
       3,     2,     1,     2,     0,     4,     6,     6,     0,     0,
       7,     0,     0,     7,     5,     4,     0,     0,     9,     0,
       6,     0,     0,     8,     0,     5,     0,     6,     0,     0,
       9,     1,     1,     1,     1,     1,     1,     1,     2,     1,
       1,     1,     5,     1,     2,     1,     1,     1,     3,     1,
       3,     1,     4,     6,     3,     5,     2,     4,     1,     3,
       6,     8,     4,     6,     4,     2,     6,     2,     4,     6,
       2,     4,     2,     4,     1,     1,     1,     3,     1,     4,
       1,     4,     1,     3,     1,     1,     4,     1,     3,     3,
       0,     5,     2,     4,     5,     5,     2,     4,     4,     3,
       3,     3,     2,     1,     4,     0,     5,     0,     5,     5,
       1,     1,     6,     1,     1,     1,     1,     2,     1,     2,
       1,     1,     1,     1,     1,     2,     3,     0,     4,     0,
       5,     1,     2,     1,     1,     1,     1,     1,     1,     2,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     0,     4,     2,
       3,     2,     6,     8,     4,     6,     4,     6,     2,     4,
       6,     2,     4,     2,     4,     1,     0,     1,     1,     1,
       1,     1,     1,     1,     3,     1,     3,     3,     3,     1,
       3,     1,     3,     1,     1,     2,     1,     1,     1,     2,
       2,     1,     1,     0,     4,     1,     2,     1,     3,     3,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     0,     1,     0,     1,     2,     2,     0,
       1,     1,     1,     1,     1,     2,     0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,     0,     0,     0,     0,     0,   266,
       0,     0,   503,   288,   291,     0,   311,   312,   313,   314,
     275,   278,   383,   429,   428,   430,   431,   505,     0,    10,
       0,   433,   432,   421,   265,   423,   422,   425,   424,   417,
     418,   403,   411,   404,   434,   435,     0,     0,     0,     0,
       0,   516,   516,    78,   284,     0,     0,     0,     0,     0,
       3,   503,     6,     9,    27,    39,    42,    50,    49,     0,
      66,     0,    70,    80,     0,    47,   229,     0,    51,   282,
     260,   261,   262,   402,   401,   427,     0,   263,   264,   248,
       5,     8,   311,   312,   275,   278,   383,     0,   102,   103,
       0,     0,     0,     0,   105,     0,   315,     0,   427,   264,
       0,   304,   156,   166,   157,   179,   153,   172,   162,   161,
     182,   183,   177,   160,   159,   155,   180,   184,   185,   164,
     154,   167,   171,   173,   165,   158,   174,   181,   176,   175,
     168,   178,   163,   152,   170,   169,   151,   149,   150,   146,
     147,   148,   107,   109,   108,   142,   143,   139,   121,   122,
     123,   130,   127,   129,   124,   125,   144,   145,   131,   132,
     136,   126,   128,   118,   119,   120,   133,   134,   135,   137,
     138,   140,   141,   483,   306,   110,   111,   482,     0,   175,
     168,   178,   163,   146,   147,   107,   108,   112,   115,    20,
     113,     0,     0,    48,     0,     0,     0,   427,     0,   264,
       0,   512,   513,   503,     0,   514,   504,     0,     0,     0,
     326,   325,     0,     0,   427,   264,     0,     0,     0,     0,
     243,   230,   253,    64,   247,   516,   516,   487,    65,    63,
     505,    62,     0,   516,   382,    61,   505,   506,     0,    18,
       0,     0,   207,     0,   208,   272,     0,     0,     0,   503,
      15,   505,    68,    14,   268,     0,   509,   509,   231,     0,
       0,   509,   485,     0,     0,    76,     0,    86,    93,   456,
     415,   414,   416,   413,   412,   405,   407,     0,   419,   420,
      46,   222,   223,     4,   504,     0,     0,     0,     0,     0,
       0,     0,   370,   372,     0,    82,     0,    74,    71,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   516,     0,   502,
     501,     0,   387,   385,   283,     0,     0,   376,    55,   281,
     301,   102,   103,   104,   419,   420,     0,   437,   299,   436,
       0,   516,     0,     0,     0,   456,   308,   116,     0,   516,
     272,   317,     0,   316,     0,     0,   516,     0,     0,     0,
       0,     0,     0,     0,   515,     0,     0,   272,     0,   516,
       0,   296,   490,   254,   250,     0,     0,   244,   252,     0,
     245,   505,     0,   277,   249,   505,   239,   516,   516,   238,
     505,   280,    45,     0,     0,     0,     0,     0,     0,    17,
     505,   270,    13,   504,    67,   505,   273,   511,   510,   232,
     511,   234,   274,   486,    92,    84,     0,    79,     0,     0,
     516,     0,   462,   459,   458,   457,   460,     0,   474,   478,
     477,   473,   456,     0,   367,   461,   463,   465,   516,   471,
     516,   476,   516,     0,   455,     0,   406,   409,     0,     0,
       7,    21,    22,    23,    24,    25,    43,    44,   516,     0,
      28,    37,     0,    38,   505,     0,    72,    83,    41,    40,
       0,   186,   253,    36,   204,   212,   217,   218,   219,   214,
     216,   226,   227,   220,   221,   197,   198,   224,   225,   505,
     213,   215,   209,   210,   211,   199,   200,   201,   202,   203,
     494,   499,   495,   500,   381,   248,   379,   505,   494,   496,
     495,   497,   380,   516,   494,   495,   248,   516,   516,    29,
     188,    35,   196,    53,    56,     0,   439,     0,     0,   102,
     103,   106,     0,   505,   516,     0,   505,   456,     0,     0,
       0,     0,   267,   516,   516,   393,   516,   318,   186,   498,
     271,   505,   494,   495,   516,     0,     0,   295,   320,   289,
     319,   292,   498,   271,   505,   494,   495,     0,   489,     0,
     255,   251,   516,   488,   276,   507,   235,   240,   242,   279,
      19,     0,    26,   195,    69,    16,   269,   509,    85,    77,
      89,    91,   505,   494,   495,     0,   462,     0,   338,   329,
     331,   505,   327,   505,     0,     0,   285,     0,   448,   481,
       0,   451,   475,     0,   453,   479,     0,     0,   205,   206,
     358,   505,     0,   356,   355,   259,     0,    81,    75,     0,
       0,     0,     0,     0,     0,   378,    59,     0,   384,     0,
       0,   237,   377,    57,   236,   373,    52,     0,     0,     0,
     516,   302,     0,     0,   384,   305,   484,   505,     0,   441,
     309,   114,   117,   394,   395,   516,   396,     0,   516,   323,
       0,     0,   321,     0,     0,   384,     0,     0,     0,   294,
       0,     0,     0,     0,   384,     0,   256,   246,   516,    11,
     233,    87,   467,   505,     0,   336,     0,   464,     0,   360,
       0,     0,   466,   516,   516,   480,   516,   472,   516,   516,
     408,     0,   462,   505,     0,   516,   469,   516,   516,   354,
       0,     0,   257,    73,   187,     0,    34,   193,    33,   194,
      60,   508,     0,    31,   191,    32,   192,    58,   374,   375,
       0,     0,   189,     0,     0,   438,   300,   440,   307,   456,
       0,     0,   398,   324,     0,    12,   400,     0,   286,     0,
     287,   255,   516,     0,     0,   297,   241,   328,   339,     0,
     334,   330,   366,     0,   369,   368,     0,   444,     0,   446,
       0,   452,     0,   449,   454,   410,     0,     0,   357,   345,
     347,     0,   350,     0,   352,   371,   258,   228,    30,   190,
     388,   386,     0,     0,     0,     0,   397,     0,    94,   101,
       0,   399,     0,   390,   391,   389,   290,   293,     0,     0,
     337,     0,   332,   364,   505,   362,   365,   516,   516,   516,
     516,     0,   468,   359,   516,   516,   516,   470,   516,   516,
      54,   303,     0,   100,     0,   516,     0,   516,   516,     0,
     335,     0,     0,   361,   445,     0,   442,   447,   450,   272,
       0,     0,   342,     0,   344,   351,     0,   348,   353,   310,
     498,    99,   505,   494,   495,   392,   322,   298,   333,   363,
     516,   498,   271,   516,   516,   516,   516,   384,   443,   343,
       0,   340,   346,   349,   516,   341
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    60,    61,    62,   250,   368,   369,   259,
     260,   413,    64,    65,   204,    66,    67,   544,   670,    68,
      69,   261,    70,    71,    72,   437,    73,   205,   104,   105,
     197,   198,   682,   199,   561,   523,   186,    75,   232,   265,
     524,   662,   405,   406,   241,   242,   234,   397,   407,   483,
      76,   201,   425,   279,   217,   702,   218,   703,   587,   838,
     548,   545,   764,   363,   365,   560,   769,   253,   372,   579,
     691,   692,   223,   619,   620,   621,   733,   642,   643,   718,
     844,   845,   453,   626,   303,   478,    78,    79,   349,   538,
     537,   383,   835,   564,   685,   771,   775,    80,    81,   287,
     465,   637,    82,    83,   284,    84,   207,   208,    87,   209,
     358,   547,   558,   559,   455,   456,   457,   458,   459,   736,
     737,   460,   461,   462,   463,   725,   628,   188,   364,   270,
     408,   237,    89,   552,   526,   341,   214,   402,   403,   658,
     429,   373,   216,   263
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -704
static const yytype_int16 yypact[] =
{
    -704,   114,  2469,  -704,  6694,  8406,  8715,  4922,  6454,  -704,
    8085,  8085,  4403,  -704,  -704,  8509,  6908,  6908,  -704,  -704,
    6908,  5640,  5752,  -704,  -704,  -704,  -704,   -17,  6454,  -704,
      -6,  -704,  -704,  5044,  5168,  -704,  -704,  5292,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  8192,  8192,    96,  3720,
    8085,  7122,  7443,  6088,  -704,  6334,   335,   228,  8299,  8192,
    -704,   328,  -704,   659,  -704,   453,  -704,  -704,   109,    75,
    -704,    22,  8612,  -704,    77,  2271,    33,    44,    38,    51,
    -704,  -704,  -704,  -704,  -704,   183,    58,  -704,   437,   163,
    -704,  -704,  -704,  -704,  -704,    80,   137,   157,   346,   369,
    8085,    92,  3863,   416,  -704,    72,  -704,    62,  -704,  -704,
     163,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,    49,
      55,    63,    68,  -704,  -704,  -704,  -704,  -704,  -704,    76,
      90,  -704,   208,  -704,   217,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,    38,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,   160,
    -704,  2929,   133,   453,    88,   205,    81,    35,   232,    57,
      88,  -704,  -704,   328,   313,  -704,   191,  8085,  8085,   296,
    -704,  -704,   172,   332,    71,    82,  8192,  8192,  8192,  8192,
    -704,  2271,   280,  -704,  -704,   243,   247,  -704,  -704,  -704,
    4277,  -704,  6908,  6908,  -704,  -704,  4531,  -704,  8085,  -704,
     257,  4006,  -704,   224,   330,   442,  6801,  3720,   303,   328,
     659,   302,   344,  -704,   453,   320,   212,   293,  -704,   280,
     322,   293,  -704,   410,  8818,   343,   279,   310,   341,   862,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,   464,   418,   423,
    -704,  -704,  -704,  -704,  4657,  8085,  8085,  8085,  8085,  6801,
    8085,  8085,  -704,  -704,  7550,  -704,  3720,  6198,   360,  7550,
    8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
    8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
    8192,  8192,  8192,  8192,  8192,  8192,  2167,  6908,  9078,  -704,
    -704,  9991,  -704,  -704,  -704,  8299,  8299,  -704,   373,  -704,
     453,  -704,   358,  -704,  -704,  -704,   328,  -704,  -704,  -704,
    9151,  6908,  9224,  2929,  8085,  1068,  -704,  -704,   469,   488,
     275,  -704,  3063,   494,  8192,  9297,  6908,  9370,  8192,  8192,
    3321,   313,  7657,   499,  -704,    53,    53,    91,  9443,  6908,
    9516,  -704,  -704,  -704,  -704,  8192,  7015,  -704,  -704,  7229,
    -704,   302,   374,  -704,  -704,   302,  -704,   382,   407,  -704,
     119,  -704,  -704,  6454,  3449,   413,  9297,  9370,  8192,   659,
     302,  -704,  -704,  4782,   417,   302,  -704,  7336,  -704,  -704,
    7443,  -704,  -704,  -704,   358,    22,  8818,  -704,  8818,  9589,
    6908,  9662,   447,  -704,  -704,  -704,  -704,  1003,  -704,  -704,
    -704,  -704,   710,    74,  -704,  -704,  -704,  -704,   420,  -704,
     422,   512,   427,   515,  -704,  4006,  -704,  -704,  8192,  8192,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,    40,  8192,
    -704,   430,   436,  -704,   302,  8818,   446,  -704,  -704,  -704,
     479,  2330,  -704,  -704,   330,  2073,  2073,  2073,  2073,   871,
     871,  9097,  1679,  2073,  2073,  6195,  6195,   622,   622,  9937,
     871,   871,   720,   720,   555,   510,   510,   330,   330,   330,
    2645,  5864,  2786,  5976,  -704,   137,  -704,   302,   463,  -704,
     497,  -704,  -704,  5752,  -704,  -704,  1845,    40,    40,  -704,
    1493,  -704,  2271,  -704,  -704,   328,  -704,  8085,  2929,   524,
      29,  -704,   137,   302,   137,   583,   119,   710,  2929,   328,
    6574,  6454,  -704,  7764,   591,  -704,   518,  -704,  2765,  5416,
    5528,   302,   345,   365,   591,   602,   131,  -704,  -704,  -704,
    -704,  -704,    86,   146,   302,   104,   111,  8085,  -704,  8192,
     280,  -704,   247,  -704,  -704,  -704,  -704,  7015,  7229,  -704,
    -704,   478,  -704,  2271,    20,   659,  -704,   293,   360,  -704,
     524,    29,   302,   125,   198,  8192,  -704,  1003,   487,  -704,
     480,   302,  -704,   302,  4149,  4006,  -704,   710,  -704,  -704,
     710,  -704,  -704,   854,  -704,  -704,   484,  4006,   330,   330,
    -704,   701,  4149,  -704,  -704,   483,  7871,  -704,  -704,  8818,
    8299,  8192,   514,  8299,  8299,  -704,   373,   490,   538,  8299,
    8299,  -704,  -704,   373,  -704,    51,   109,  4149,  4006,  8192,
      40,  -704,   328,   623,  -704,  -704,  -704,   302,   631,  -704,
    -704,  -704,  -704,   430,  -704,   543,  -704,  3592,   637,  -704,
    8085,   641,  -704,  8192,  8192,   381,  8192,  8192,   644,  -704,
    7978,  3192,  4149,  4149,   113,    53,  -704,  -704,   519,  -704,
    -704,   363,  -704,   302,   262,   521,  1020,  -704,   535,   536,
     667,   552,  -704,   550,   551,  -704,   554,  -704,   556,   554,
    -704,   562,   603,   302,   599,   574,  -704,   576,   577,  -704,
     706,  8192,   579,  -704,  2271,  8192,  -704,  2271,  -704,  2271,
    -704,  -704,  8299,  -704,  2271,  -704,  2271,  -704,  -704,  -704,
     708,   584,  2271,  4006,  2929,  -704,  -704,  -704,  -704,  1068,
    8921,    88,  -704,  -704,  4149,  -704,  -704,    88,  -704,  8192,
    -704,  -704,   144,   713,   718,  -704,  7229,  -704,   592,   262,
     541,  -704,  -704,   611,  -704,  -704,   710,  -704,   854,  -704,
     854,  -704,   854,  -704,  -704,  -704,  9024,   617,  -704,  1058,
    -704,  1058,  -704,   854,  -704,  -704,   593,  2271,  -704,  2271,
    -704,  -704,   607,   733,  2929,   689,  -704,   376,   310,   341,
    2929,  -704,  3063,  -704,  -704,  -704,  -704,  -704,  4149,   262,
     592,   262,   608,  -704,   316,  -704,  -704,   554,   610,   554,
     554,   694,   380,  -704,   612,   620,   554,  -704,   621,   554,
    -704,  -704,   750,   358,  9735,  6908,  9808,   488,   518,   757,
     592,   262,   611,  -704,  -704,   854,  -704,  -704,  -704,  -704,
    9881,  1058,  -704,   854,  -704,  -704,   854,  -704,  -704,  -704,
      65,    29,   302,    97,   118,  -704,  -704,  -704,   592,  -704,
     554,   629,   630,   554,   634,   554,   554,   140,  -704,  -704,
     854,  -704,  -704,  -704,   554,  -704
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -704,  -704,  -704,   359,  -704,    28,  -704,  -357,   812,  -704,
      50,  -704,  -226,   122,    30,   -45,  -704,  -538,  -704,    -5,
     760,   -51,    18,   -46,  -241,  -399,   -18,  1291,   -83,   770,
       9,    17,  -704,  -704,  -704,    -4,  -704,  1033,   -16,  -704,
     -15,   242,  -336,    89,   -14,  -704,  -352,  -215,    39,  -272,
       4,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,  -704,  -704,  -704,  -704,   632,  -188,  -359,
     -87,  -502,  -704,  -645,  -668,   166,  -704,  -444,  -704,  -559,
    -704,   -88,  -704,  -704,   120,  -704,  -704,  -704,   -74,  -704,
    -704,  -353,  -704,   -80,  -704,  -704,  -704,  -704,  -704,  -704,
    -704,  -704,  -704,    15,  -704,  -704,  1438,   399,   781,  1530,
    -704,  -704,    21,  -250,  -695,   -94,  -557,   152,  -543,  -703,
     -13,   165,  -704,  -539,  -704,  -265,   291,  -704,  -704,  -704,
      14,  -382,  1998,  -278,  -704,   606,   -23,   -25,  -106,  -492,
    -182,     8,     7,    -2
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -517
static const yytype_int16 yytable[] =
{
      90,   527,   248,   185,   185,   344,   555,   244,   245,   106,
     106,   230,   230,   290,   464,   230,   184,   593,   353,   106,
     215,   400,   380,   200,   185,   553,   308,   581,   575,   454,
     236,   236,    91,   436,   236,   275,   269,   493,   293,   609,
     571,   210,   213,   200,   591,   249,   788,   591,   593,   268,
     272,   185,    63,   584,    63,   235,   235,   106,   525,   235,
     533,   674,   688,   536,   283,   267,   271,   262,   294,   215,
     722,   791,   698,   356,   347,   348,   106,   342,   -97,   695,
     578,   244,   734,   488,   554,   431,   648,   727,   724,   433,
     266,   728,   704,   667,   668,   347,   -94,   525,   846,   533,
     464,   371,   738,   624,   612,   233,   238,  -101,   857,   239,
     -96,   -97,   554,   359,     3,  -271,  -100,   640,   750,  -315,
     711,   840,   246,  -426,   339,   757,   247,   -67,   251,   -96,
     336,   -98,   203,   203,   203,  -429,   -98,   302,   -95,   554,
     411,  -428,   -94,   351,   371,   379,   345,   352,   360,  -430,
     -88,   255,   641,   -95,  -431,   424,   300,   301,   -81,   687,
     305,   382,  -433,   554,  -101,  -271,  -271,   375,  -495,  -315,
    -315,   -99,   264,   870,   807,   340,  -432,   846,   857,   625,
     337,   338,   304,   357,   309,   343,  -429,   464,   370,   342,
     381,   673,  -428,   400,   791,   211,   212,   436,   361,   362,
    -430,   678,   623,   898,  -494,  -431,   420,   -97,   -97,   -86,
     392,   393,   394,  -433,   211,   212,   593,   376,   377,   240,
     -93,   215,   350,   -90,   -89,  -494,   763,  -432,   435,   -92,
     211,   212,   -88,   398,   398,   230,   422,   230,   230,   -96,
     -96,   409,   -88,   722,   436,   591,   591,   385,   386,   -90,
     743,   -87,   722,   727,   236,   484,   236,   848,   388,   480,
     -98,   -98,   247,   -88,   489,   -88,   423,   215,   -88,   700,
     855,  -426,   858,   211,   212,   262,   243,   551,   106,   235,
     833,   235,   -95,   -95,   -91,  -495,   288,   289,   481,   486,
     -94,   684,   464,   492,  -421,   594,   246,   343,   367,   596,
     539,   541,   243,  -425,   599,   -90,   419,   677,   389,   390,
     416,   106,   374,   616,   604,   443,   444,   445,   446,   606,
     378,   -86,   470,   532,   262,   471,   472,   473,   474,   401,
     382,   404,   230,   384,   531,   409,   -90,   531,   -90,   203,
     203,   -90,   904,   482,    63,  -421,   785,   532,   482,   475,
     427,   387,   617,   622,  -425,   428,   230,   391,   531,   409,
     376,   417,   532,   418,   546,   439,   492,   565,   410,   395,
     412,   230,  -491,   531,   409,   532,   739,   707,   647,   588,
     590,   396,  -100,   269,   230,   399,   531,   409,   701,  -384,
     608,   414,   435,   580,   580,  -492,  -427,   285,   286,   310,
     907,    86,   532,    86,   593,   398,   398,   823,   436,   185,
     592,   590,    90,   531,   269,   440,   441,   203,   203,   203,
     203,   576,   476,   477,   488,   710,   532,  -264,   200,   834,
     600,   430,  -421,   696,   591,   230,   428,   531,   409,   435,
     106,   607,   106,   421,  -272,   247,  -427,  -427,    86,  -384,
     676,   -66,   -96,   697,   872,  -425,   629,   432,   629,   247,
     629,   426,   864,   645,    63,   434,   880,   862,   543,   779,
     211,   212,   -98,   605,   354,   355,   644,  -264,  -264,   562,
    -491,   438,  -421,  -421,   652,  -491,   556,   468,   -95,   106,
     300,   301,   469,   707,  -272,  -272,   563,  -384,   487,  -384,
    -384,    86,   657,  -492,   464,  -425,  -425,   567,  -492,   577,
     655,   656,   865,   866,   595,   717,   361,   362,   661,   663,
     597,   661,   666,   622,   715,   346,   466,   467,   657,   892,
     418,   664,   690,   687,   664,   644,   644,   655,   616,   661,
     443,   444,   445,   446,  -101,   598,   657,   683,   602,  -100,
    -498,   659,   664,   671,   615,   -81,   185,   185,   627,   657,
     630,   686,   689,   632,   689,   633,   635,   679,  -253,   680,
     -96,   767,   689,   706,   646,   -93,   200,   672,   681,   310,
     -92,   590,   269,   830,   649,   660,   650,   657,   554,   832,
     398,   758,   616,   675,   443,   444,   445,   446,   719,   712,
      86,   -88,   482,   435,   -98,   489,   687,   787,   746,   748,
    -498,   708,   699,   709,   753,   755,   719,   705,   716,   730,
     622,  -254,   622,   745,   310,   714,   752,   333,   334,   335,
     742,   751,   770,   766,    77,   -90,    77,   107,   107,   323,
     324,   768,   206,   206,   206,   -95,   774,   222,   206,   206,
      86,   778,   206,   106,   780,    86,    86,   786,  -498,   789,
    -498,  -498,   843,  -494,   443,   444,   445,   446,   644,   203,
     331,   332,   333,   334,   335,   792,   -87,   794,   793,   841,
     765,    77,   206,   772,   781,   276,   776,   795,   796,   798,
     206,   310,   800,    86,   802,   622,   842,   805,    86,   295,
     296,   297,   298,   299,   276,    86,   398,   818,   719,   203,
     806,   808,   809,   580,   811,   813,   815,  -255,   820,   821,
     777,   629,   629,   836,   629,   816,   629,   629,   837,   853,
     839,  -256,   206,   629,    77,   629,   629,   331,   332,   333,
     334,   335,   860,   861,   863,   622,   871,   622,   875,   879,
     881,   631,   732,   634,   443,   444,   445,   446,   883,   886,
     889,   442,    86,   443,   444,   445,   446,   897,  -494,  -495,
     269,    86,   910,   601,   106,   220,   111,   622,   665,    86,
     689,   896,   726,   713,   899,   729,   759,   895,   187,   310,
     824,   447,   723,   735,   366,     0,   854,   448,   449,     0,
     447,     0,     0,     0,   323,   324,   448,   449,     0,     0,
     106,     0,   203,    86,     0,   450,     0,     0,   451,   873,
       0,     0,    86,     0,   450,     0,     0,   451,     0,     0,
       0,     0,     0,    77,   330,   331,   332,   333,   334,   335,
       0,     0,     0,     0,   247,   629,   629,   629,   629,   206,
     206,   532,   629,   629,   629,     0,   629,   629,     0,     0,
     230,   258,   531,   409,    86,   565,   689,   657,     0,     0,
       0,     0,   206,     0,   206,   206,     0,     0,   206,     0,
     206,     0,     0,    77,     0,     0,     0,     0,    77,    77,
       0,     0,     0,     0,     0,     0,     0,     0,   629,     0,
       0,   629,   629,   629,   629,   616,   276,   443,   444,   445,
     446,     0,   629,   442,   258,   443,   444,   445,   446,     0,
       0,     0,     0,     0,     0,     0,    77,   206,   206,   206,
     206,    77,   206,   206,     0,     0,   206,     0,    77,   276,
     310,   206,     0,     0,   447,     0,     0,    86,   847,     0,
     849,   449,   447,     0,   850,   323,   324,    86,   448,   449,
       0,     0,     0,   856,     0,   859,     0,     0,   450,   206,
       0,     0,     0,     0,     0,     0,   450,   206,   206,   451,
       0,     0,     0,   328,   329,   330,   331,   332,   333,   334,
     335,     0,     0,   206,     0,    77,   206,     0,     0,     0,
       0,   452,     0,     0,    77,     0,     0,     0,   206,     0,
       0,     0,    77,     0,   797,   799,     0,   801,     0,   803,
     804,   206,     0,    86,    86,     0,   810,   900,   812,   814,
       0,     0,     0,   903,     0,   905,    86,     0,   906,     0,
       0,    86,     0,     0,     0,     0,    77,     0,     0,   231,
     231,     0,     0,   231,   616,    77,   443,   444,   445,   446,
       0,     0,   914,   415,     0,     0,    86,    86,   276,   258,
     276,   616,   206,   443,   444,   445,   446,     0,     0,   252,
     254,     0,     0,     0,   231,   231,    86,     0,     0,     0,
       0,   291,   292,   617,     0,     0,     0,    77,     0,   618,
      86,    86,    86,     0,     0,     0,     0,     0,     0,   732,
     617,   443,   444,   445,   446,     0,   790,   276,   258,   442,
       0,   443,   444,   445,   446,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   874,   876,
     877,   878,     0,     0,     0,   882,   884,   885,   447,   887,
     888,     0,     0,     0,   448,   449,     0,     0,   447,     0,
       0,     0,    86,    86,   448,   449,     0,     0,     0,     0,
       0,     0,   450,    86,     0,   451,     0,     0,     0,   206,
      77,     0,   450,     0,   566,   451,     0,     0,     0,     0,
      77,   908,   574,     0,   909,   911,   912,   913,     0,     0,
       0,     0,     0,     0,     0,   915,     0,   557,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   206,
       0,     0,     0,    86,     0,     0,     0,     0,     0,    86,
       0,    86,     0,     0,     0,     0,     0,    86,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    77,    77,     0,   231,
     231,   231,   291,     0,     0,     0,     0,     0,     0,    77,
       0,     0,     0,   231,    77,   231,   231,   636,     0,     0,
       0,   276,   206,     0,     0,   206,   206,     0,     0,     0,
       0,   206,   206,    74,     0,    74,     0,     0,     0,    77,
      77,     0,     0,     0,     0,     0,   221,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    77,
       0,     0,   206,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    77,    77,    77,     0,   231,     0,     0,
      74,     0,   491,   494,   495,   496,   497,   498,   499,   500,
     501,   502,   503,   504,   505,   506,   507,   508,   509,   510,
     511,   512,   513,   514,   515,   516,   517,   518,   519,     0,
     231,     0,     0,     0,     0,     0,     0,     0,   540,   542,
       0,     0,     0,     0,   206,     0,     0,     0,     0,     0,
       0,     0,     0,    74,   231,    77,    77,     0,     0,     0,
       0,     0,   827,     0,     0,     0,    77,   568,     0,   231,
       0,   540,   542,     0,     0,   231,     0,     0,     0,     0,
       0,     0,   231,     0,     0,     0,     0,     0,   231,   231,
       0,     0,   231,     0,     0,     0,   720,   721,   852,     0,
      85,     0,    85,   108,   108,   108,     0,     0,     0,   731,
       0,   603,     0,   224,   740,     0,    77,     0,     0,     0,
     231,     0,    77,   231,    77,     0,     0,     0,     0,     0,
      77,     0,     0,   231,     0,     0,     0,     0,     0,   760,
     761,     0,     0,     0,     0,     0,     0,    85,     0,     0,
       0,   277,    74,     0,     0,     0,     0,   206,     0,   773,
       0,   638,   639,     0,     0,     0,     0,     0,     0,     0,
     277,     0,   231,   782,   783,   784,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    88,     0,    88,   109,   109,   669,     0,     0,
      85,     0,    74,     0,     0,   225,     0,    74,    74,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   310,   311,   312,   313,   314,   315,   316,   317,
     318,   319,   320,   321,   322,   822,     0,   323,   324,    88,
       0,     0,     0,   278,     0,    74,   831,     0,     0,     0,
      74,     0,     0,     0,     0,     0,   231,    74,     0,     0,
     490,   325,   278,   326,   327,   328,   329,   330,   331,   332,
     333,   334,   335,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   231,     0,     0,     0,     0,     0,     0,     0,
     231,   231,    88,     0,     0,     0,     0,     0,     0,    85,
       0,     0,   867,     0,   868,     0,     0,     0,   231,     0,
     869,     0,     0,     0,    74,     0,     0,     0,     0,     0,
       0,     0,     0,    74,     0,     0,     0,     0,     0,     0,
       0,    74,     0,     0,     0,     0,     0,     0,     0,   231,
       0,     0,     0,   568,   744,     0,   747,   749,     0,    85,
       0,     0,   754,   756,    85,    85,     0,     0,     0,     0,
       0,     0,   762,     0,     0,    74,     0,     0,     0,     0,
       0,     0,   277,     0,    74,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   747,   749,     0,   754,
     756,    88,    85,   231,     0,     0,     0,    85,     0,     0,
       0,     0,     0,     0,    85,   277,     0,     0,   310,   311,
     312,   313,   314,   315,   316,   317,    74,   319,   320,     0,
       0,     0,     0,   323,   324,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   231,     0,     0,     0,   817,     0,
       0,    88,     0,     0,     0,   819,    88,    88,     0,   326,
     327,   328,   329,   330,   331,   332,   333,   334,   335,     0,
       0,    85,     0,     0,   278,     0,     0,     0,     0,     0,
      85,     0,   819,     0,     0,     0,     0,     0,    85,   231,
       0,     0,     0,     0,    88,     0,     0,     0,     0,    88,
       0,     0,     0,     0,     0,     0,    88,   278,     0,    74,
       0,     0,     0,     0,     0,  -516,     0,     0,     0,    74,
       0,     0,    85,  -516,  -516,  -516,     0,     0,  -516,  -516,
    -516,    85,  -516,     0,     0,     0,     0,     0,     0,     0,
       0,  -516,  -516,     0,   277,     0,   277,     0,     0,     0,
       0,     0,  -516,  -516,     0,  -516,  -516,  -516,  -516,  -516,
       0,     0,     0,    88,     0,     0,     0,     0,   231,     0,
       0,     0,    88,    85,     0,     0,     0,     0,     0,     0,
      88,     0,     0,     0,     0,    74,    74,     0,     0,     0,
       0,     0,     0,   277,     0,     0,     0,     0,    74,     0,
       0,  -516,     0,    74,     0,     0,     0,     0,     0,     0,
       0,   490,     0,     0,    88,     0,     0,     0,     0,     0,
       0,     0,     0,    88,     0,     0,     0,     0,    74,    74,
       0,     0,     0,     0,     0,     0,   278,     0,   278,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    74,  -516,
    -516,     0,  -516,     0,   243,  -516,    85,  -516,  -516,     0,
       0,     0,    74,    74,    74,    88,    85,     0,     0,     0,
       0,     0,     0,   110,   110,     0,     0,     0,     0,     0,
       0,     0,     0,   110,     0,   278,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   110,   110,     0,     0,     0,   110,
     110,   110,     0,     0,    74,    74,     0,   110,     0,     0,
       0,   826,    85,    85,     0,    74,     0,     0,     0,     0,
     110,     0,     0,     0,     0,    85,     0,     0,    88,     0,
      85,     0,     0,     0,     0,     0,     0,   277,    88,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    85,    85,     0,     0,     0,
       0,     0,     0,     0,     0,    74,     0,     0,     0,     0,
       0,    74,     0,    74,     0,    85,     0,     0,     0,    74,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    85,
      85,    85,   310,  -517,  -517,  -517,  -517,   315,   316,     0,
       0,  -517,  -517,     0,    88,    88,     0,   323,   324,     0,
       0,     0,     0,     0,     0,     0,     0,    88,     0,     0,
       0,     0,    88,     0,     0,     0,     0,     0,     0,   278,
       0,     0,     0,   326,   327,   328,   329,   330,   331,   332,
     333,   334,   335,     0,     0,     0,     0,    88,    88,     0,
       0,    85,    85,     0,     0,     0,     0,     0,   828,     0,
       0,     0,    85,     0,     0,     0,     0,    88,   520,   521,
       0,     0,   522,     0,   110,   110,   110,   110,     0,     0,
       0,    88,    88,    88,   155,   156,   157,   158,   159,   160,
     161,   162,   163,     0,   108,   164,   165,     0,     0,   166,
     167,   168,   169,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    85,   170,     0,     0,     0,     0,    85,     0,
      85,     0,   110,     0,     0,     0,    85,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,     0,   181,   182,
       0,     0,     0,    88,    88,     0,     0,     0,     0,     0,
     829,     0,     0,     0,    88,   110,   243,     0,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   110,     0,     0,   109,     0,     0,     0,
     310,   311,   312,   313,   314,   315,   316,   317,   318,   319,
     320,   321,   322,     0,    88,   323,   324,     0,     0,     0,
      88,     0,    88,     0,     0,     0,     0,     0,    88,     0,
       0,     0,   110,     0,   651,     0,   110,   110,     0,   325,
     110,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,     0,     0,   110,   110,     0,     0,   110,     0,   310,
     311,   312,   313,   314,   315,   316,   317,   318,   319,   320,
     321,   322,     0,     0,   323,   324,   110,     0,     0,     0,
       0,     0,     0,     0,     0,   110,     0,     0,   110,     0,
       0,     0,     0,     0,   110,     0,   110,     0,   325,     0,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   110,   110,  -230,  -516,
       4,     0,     5,     6,     7,     8,     9,   110,     0,     0,
      10,    11,     0,   110,     0,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,    29,    30,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,     0,     0,    49,
      50,   110,    51,    52,     0,    53,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   110,     0,    57,
      58,    59,     0,     0,     0,   110,   110,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -516,  -516,   110,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   110,  -498,     0,   110,     0,   110,
       0,     0,     0,  -498,  -498,  -498,     0,     0,     0,  -498,
    -498,     0,  -498,     0,     0,     0,     0,   110,     0,     0,
       0,  -498,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -498,  -498,     0,  -498,  -498,  -498,  -498,  -498,
       0,   110,   110,     0,   110,   110,     0,     0,   110,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -498,  -498,  -498,  -498,  -498,  -498,
    -498,  -498,  -498,  -498,  -498,  -498,  -498,     0,     0,  -498,
    -498,  -498,     0,   653,     0,     0,     0,     0,     0,   110,
       0,     0,     0,   110,     0,     0,     0,     0,     0,     0,
       0,     0,   -97,  -498,     0,  -498,  -498,  -498,  -498,  -498,
    -498,  -498,  -498,  -498,  -498,     0,     0,     0,   110,     0,
       0,     0,     0,     0,     0,     0,     0,   110,     0,  -498,
    -498,  -498,  -498,   -89,   110,  -498,  -271,  -498,  -498,     0,
       0,     0,     0,     0,  -271,  -271,  -271,     0,     0,     0,
    -271,  -271,     0,  -271,   110,     0,     0,     0,     0,   651,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -271,  -271,     0,  -271,  -271,  -271,  -271,
    -271,     0,     0,     0,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,     0,     0,   323,
     324,     0,     0,     0,     0,  -271,  -271,  -271,  -271,  -271,
    -271,  -271,  -271,  -271,  -271,  -271,  -271,  -271,     0,     0,
    -271,  -271,  -271,   325,   654,   326,   327,   328,   329,   330,
     331,   332,   333,   334,   335,     0,     0,     0,     0,     0,
       0,     0,     0,   -99,  -271,     0,  -271,  -271,  -271,  -271,
    -271,  -271,  -271,  -271,  -271,  -271,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -271,  -271,  -271,   -91,     0,  -271,     0,  -271,  -271,
     256,     0,     5,     6,     7,     8,     9,  -516,  -516,  -516,
      10,    11,     0,     0,  -516,    12,     0,    13,    14,    15,
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
       0,     0,     0,     0,   256,     0,     5,     6,     7,     8,
       9,  -516,  -516,  -516,    10,    11,     0,  -516,  -516,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,     0,
      30,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      48,     0,     0,    49,    50,     0,    51,    52,     0,    53,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    57,    58,    59,     0,     0,     0,     0,
       0,     0,     0,   256,     0,     5,     6,     7,     8,     9,
       0,     0,  -516,    10,    11,  -516,  -516,  -516,    12,  -516,
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
       0,     0,   256,     0,     5,     6,     7,     8,     9,     0,
       0,  -516,    10,    11,  -516,  -516,  -516,    12,     0,    13,
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
       4,     0,     5,     6,     7,     8,     9,     0,     0,     0,
      10,    11,     0,  -516,  -516,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,    29,    30,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,     0,     0,    49,
      50,     0,    51,    52,     0,    53,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    57,
      58,    59,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -516,     0,     0,     0,     0,     0,
       0,  -516,  -516,   256,     0,     5,     6,     7,     8,     9,
       0,  -516,  -516,    10,    11,     0,     0,     0,    12,     0,
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
       0,   256,     0,     5,     6,     7,     8,     9,     0,     0,
       0,    10,    11,     0,  -516,  -516,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,     0,    30,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,     0,     0,
     257,    50,     0,    51,    52,     0,    53,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      57,    58,    59,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -516,     0,  -516,  -516,   256,     0,     5,     6,     7,     8,
       9,     0,     0,     0,    10,    11,     0,     0,     0,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,     0,
      30,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      48,     0,     0,    49,    50,     0,    51,    52,     0,    53,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    57,    58,    59,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -516,     0,  -516,  -516,   256,     0,     5,
       6,     7,     8,     9,     0,     0,     0,    10,    11,     0,
       0,     0,    12,     0,    13,    14,    15,    16,    17,    18,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -516,     0,     0,     0,     0,     0,     0,  -516,  -516,
     256,     0,     5,     6,     7,     8,     9,     0,     0,  -516,
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
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,  -516,  -516,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    97,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,   226,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   202,     0,     0,   102,    50,     0,
      51,    52,     0,   227,   228,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    57,   229,    59,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,     0,     0,     0,    12,
     247,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     202,     0,     0,   102,    50,     0,    51,    52,     0,     0,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    57,    58,    59,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,   211,   212,    12,     0,    13,
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
       5,     6,     7,     8,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,   247,    13,    14,    15,    16,    17,
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
       0,     0,     0,     0,     0,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,   384,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   384,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,     0,
       0,     0,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,     0,     0,     0,     0,     0,   146,   147,   148,
     149,   150,   151,   152,   153,    35,    36,   154,    38,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   155,
     156,   157,   158,   159,   160,   161,   162,   163,     0,     0,
     164,   165,     0,     0,   166,   167,   168,   169,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   170,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,   181,   182,     0,     0,  -491,  -491,  -491,
       0,  -491,     0,     0,     0,  -491,  -491,     0,     0,     0,
    -491,   183,  -491,  -491,  -491,  -491,  -491,  -491,  -491,     0,
    -491,     0,     0,     0,  -491,  -491,  -491,  -491,  -491,  -491,
    -491,     0,     0,  -491,     0,     0,     0,     0,     0,     0,
       0,     0,  -491,  -491,     0,  -491,  -491,  -491,  -491,  -491,
    -491,  -491,  -491,  -491,  -491,  -491,  -491,     0,  -491,  -491,
       0,  -491,  -491,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -491,     0,     0,  -491,  -491,     0,  -491,  -491,     0,
    -491,  -491,  -491,  -491,     0,     0,     0,  -491,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -491,  -491,  -491,     0,     0,     0,
       0,  -493,  -493,  -493,     0,  -493,     0,     0,  -491,  -493,
    -493,     0,     0,  -491,  -493,     0,  -493,  -493,  -493,  -493,
    -493,  -493,  -493,     0,  -493,     0,     0,     0,  -493,  -493,
    -493,  -493,  -493,  -493,  -493,     0,     0,  -493,     0,     0,
       0,     0,     0,     0,     0,     0,  -493,  -493,     0,  -493,
    -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
    -493,     0,  -493,  -493,     0,  -493,  -493,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -493,     0,     0,  -493,  -493,
       0,  -493,  -493,     0,  -493,  -493,  -493,  -493,     0,     0,
       0,  -493,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -493,  -493,
    -493,     0,     0,     0,     0,  -492,  -492,  -492,     0,  -492,
       0,     0,  -493,  -492,  -492,     0,     0,  -493,  -492,     0,
    -492,  -492,  -492,  -492,  -492,  -492,  -492,     0,  -492,     0,
       0,     0,  -492,  -492,  -492,  -492,  -492,  -492,  -492,     0,
       0,  -492,     0,     0,     0,     0,     0,     0,     0,     0,
    -492,  -492,     0,  -492,  -492,  -492,  -492,  -492,  -492,  -492,
    -492,  -492,  -492,  -492,  -492,     0,  -492,  -492,     0,  -492,
    -492,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -492,
       0,     0,  -492,  -492,     0,  -492,  -492,     0,  -492,  -492,
    -492,  -492,     0,     0,     0,  -492,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -492,  -492,  -492,     0,     0,     0,     0,  -494,
    -494,  -494,     0,  -494,     0,     0,  -492,  -494,  -494,     0,
       0,  -492,  -494,     0,  -494,  -494,  -494,  -494,  -494,  -494,
    -494,     0,     0,     0,     0,     0,  -494,  -494,  -494,  -494,
    -494,  -494,  -494,     0,     0,  -494,     0,     0,     0,     0,
       0,     0,     0,     0,  -494,  -494,     0,  -494,  -494,  -494,
    -494,  -494,  -494,  -494,  -494,  -494,  -494,  -494,  -494,     0,
    -494,  -494,     0,  -494,  -494,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -494,   693,     0,  -494,  -494,     0,  -494,
    -494,     0,  -494,  -494,  -494,  -494,     0,     0,     0,  -494,
       0,     0,     0,   -97,     0,     0,     0,     0,     0,     0,
       0,  -495,  -495,  -495,     0,  -495,  -494,  -494,  -494,  -495,
    -495,     0,     0,     0,  -495,     0,  -495,  -495,  -495,  -495,
    -495,  -495,  -495,     0,     0,  -494,     0,     0,  -495,  -495,
    -495,  -495,  -495,  -495,  -495,     0,     0,  -495,     0,     0,
       0,     0,     0,     0,     0,     0,  -495,  -495,     0,  -495,
    -495,  -495,  -495,  -495,  -495,  -495,  -495,  -495,  -495,  -495,
    -495,     0,  -495,  -495,     0,  -495,  -495,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -495,   694,     0,  -495,  -495,
       0,  -495,  -495,     0,  -495,  -495,  -495,  -495,     0,     0,
       0,  -495,     0,     0,     0,   -99,     0,     0,     0,     0,
       0,     0,     0,  -248,  -248,  -248,     0,  -248,  -495,  -495,
    -495,  -248,  -248,     0,     0,     0,  -248,     0,  -248,  -248,
    -248,  -248,  -248,  -248,  -248,     0,     0,  -495,     0,     0,
    -248,  -248,  -248,  -248,  -248,  -248,  -248,     0,     0,  -248,
       0,     0,     0,     0,     0,     0,     0,     0,  -248,  -248,
       0,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,  -248,
    -248,  -248,  -248,     0,  -248,  -248,     0,  -248,  -248,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -248,     0,     0,
    -248,  -248,     0,  -248,  -248,     0,  -248,  -248,  -248,  -248,
       0,     0,     0,  -248,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -248,  -248,  -248,     0,  -248,
    -248,  -248,  -248,  -248,  -248,     0,     0,     0,  -248,     0,
    -248,  -248,  -248,  -248,  -248,  -248,  -248,     0,     0,   240,
       0,     0,  -248,  -248,  -248,  -248,  -248,  -248,  -248,     0,
       0,  -248,     0,     0,     0,     0,     0,     0,     0,     0,
    -248,  -248,     0,  -248,  -248,  -248,  -248,  -248,  -248,  -248,
    -248,  -248,  -248,  -248,  -248,     0,  -248,  -248,     0,  -248,
    -248,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -248,
       0,     0,  -248,  -248,     0,  -248,  -248,     0,  -248,  -248,
    -248,  -248,     0,     0,     0,  -248,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -496,  -496,  -496,
       0,  -496,  -248,  -248,  -248,  -496,  -496,     0,     0,     0,
    -496,     0,  -496,  -496,  -496,  -496,  -496,  -496,  -496,     0,
       0,   243,     0,     0,  -496,  -496,  -496,  -496,  -496,  -496,
    -496,     0,     0,  -496,     0,     0,     0,     0,     0,     0,
       0,     0,  -496,  -496,     0,  -496,  -496,  -496,  -496,  -496,
    -496,  -496,  -496,  -496,  -496,  -496,  -496,     0,  -496,  -496,
       0,  -496,  -496,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -496,     0,     0,  -496,  -496,     0,  -496,  -496,     0,
    -496,  -496,  -496,  -496,     0,     0,     0,  -496,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -497,
    -497,  -497,     0,  -497,  -496,  -496,  -496,  -497,  -497,     0,
       0,     0,  -497,     0,  -497,  -497,  -497,  -497,  -497,  -497,
    -497,     0,     0,  -496,     0,     0,  -497,  -497,  -497,  -497,
    -497,  -497,  -497,     0,     0,  -497,     0,     0,     0,     0,
       0,     0,     0,     0,  -497,  -497,     0,  -497,  -497,  -497,
    -497,  -497,  -497,  -497,  -497,  -497,  -497,  -497,  -497,     0,
    -497,  -497,     0,  -497,  -497,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -497,     0,     0,  -497,  -497,     0,  -497,
    -497,     0,  -497,  -497,  -497,  -497,     0,     0,     0,  -497,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,  -497,  -497,  -497,    10,
      11,     0,     0,     0,    12,     0,    13,    14,    15,    92,
      93,    18,    19,     0,     0,  -497,     0,     0,    94,    95,
      96,    23,    24,    25,    26,     0,     0,    97,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   273,     0,     0,   102,    50,
       0,    51,    52,     0,     0,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,   103,    10,
      11,     0,     0,     0,    12,     0,    13,    14,    15,    92,
      93,    18,    19,     0,     0,     0,   274,     0,    94,    95,
      96,    23,    24,    25,    26,     0,     0,    97,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,  -517,  -517,     0,     0,   323,
     324,     0,     0,     0,     0,   273,     0,     0,   102,    50,
       0,    51,    52,     0,     0,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,   326,   327,   328,   329,   330,
     331,   332,   333,   334,   335,     0,     0,     0,   103,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   485,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,     0,     0,     0,   136,   137,   138,   189,   190,   191,
     192,   143,   144,   145,     0,     0,     0,     0,     0,   146,
     147,   148,   193,   194,   151,   195,   153,   280,   281,   196,
     282,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   155,   156,   157,   158,   159,   160,   161,   162,   163,
       0,     0,   164,   165,     0,     0,   166,   167,   168,   169,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     170,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,   181,   182,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,     0,     0,     0,   136,   137,   138,   189,   190,   191,
     192,   143,   144,   145,     0,     0,     0,     0,     0,   146,
     147,   148,   193,   194,   151,   195,   153,     0,     0,   196,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   155,   156,   157,   158,   159,   160,   161,   162,   163,
       0,     0,   164,   165,     0,     0,   166,   167,   168,   169,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     170,     0,     0,    55,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,   181,   182,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,     0,     0,     0,   136,   137,   138,   189,   190,   191,
     192,   143,   144,   145,     0,     0,     0,     0,     0,   146,
     147,   148,   193,   194,   151,   195,   153,     0,     0,   196,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   155,   156,   157,   158,   159,   160,   161,   162,   163,
       0,     0,   164,   165,     0,     0,   166,   167,   168,   169,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     170,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,   181,   182,     5,     6,     7,
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
       0,     0,     0,     0,     5,     6,     7,     8,     9,     0,
       0,     0,    10,    11,    57,    58,    59,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,     0,    30,    31,
      32,     0,    33,    34,    35,    36,    37,    38,     0,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,     0,
       0,    49,    50,     0,    51,    52,     0,    53,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    57,    58,    59,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    97,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,   226,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   202,     0,     0,   102,    50,
       0,    51,    52,     0,   227,   228,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    57,   229,
      59,    12,     0,    13,    14,    15,    92,    93,    18,    19,
       0,     0,     0,     0,     0,    94,    95,    96,    23,    24,
      25,    26,     0,     0,    97,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    33,    34,    35,    36,
      37,    38,   226,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   202,     0,     0,   102,    50,     0,    51,    52,
       0,   589,   228,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    57,   229,    59,    12,     0,
      13,    14,    15,    92,    93,    18,    19,     0,     0,     0,
       0,     0,    94,    95,    96,    23,    24,    25,    26,     0,
       0,    97,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    33,    34,    35,    36,    37,    38,   226,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   202,
       0,     0,   102,    50,     0,    51,    52,     0,   227,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    57,   229,    59,    12,     0,    13,    14,    15,
      92,    93,    18,    19,     0,     0,     0,     0,     0,    94,
      95,    96,    23,    24,    25,    26,     0,     0,    97,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,   226,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   202,     0,     0,   102,
      50,     0,    51,    52,     0,     0,   228,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    57,
     229,    59,    12,     0,    13,    14,    15,    92,    93,    18,
      19,     0,     0,     0,     0,     0,    94,    95,    96,    23,
      24,    25,    26,     0,     0,    97,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,   226,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   202,     0,     0,   102,    50,     0,    51,
      52,     0,   589,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    57,   229,    59,    12,
       0,    13,    14,    15,    92,    93,    18,    19,     0,     0,
       0,     0,     0,    94,    95,    96,    23,    24,    25,    26,
       0,     0,    97,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
     226,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     202,     0,     0,   102,    50,     0,    51,    52,     0,     0,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    57,   229,    59,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    97,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   202,     0,     0,
     102,    50,     0,    51,    52,     0,   479,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      57,   229,    59,    12,     0,    13,    14,    15,    92,    93,
      18,    19,     0,     0,     0,     0,     0,    94,    95,    96,
      23,    24,    25,    26,     0,     0,    97,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   202,     0,     0,   102,    50,     0,
      51,    52,     0,   227,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    57,   229,    59,
      12,     0,    13,    14,    15,    92,    93,    18,    19,     0,
       0,     0,     0,     0,    94,    95,    96,    23,    24,    25,
      26,     0,     0,    97,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   202,     0,     0,   102,    50,     0,    51,    52,     0,
     479,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    57,   229,    59,    12,     0,    13,
      14,    15,    92,    93,    18,    19,     0,     0,     0,     0,
       0,    94,    95,    96,    23,    24,    25,    26,     0,     0,
      97,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    33,    34,    35,    36,    37,    38,     0,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   202,     0,
       0,   102,    50,     0,    51,    52,     0,   741,     0,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    57,   229,    59,    12,     0,    13,    14,    15,    92,
      93,    18,    19,     0,     0,     0,     0,     0,    94,    95,
      96,    23,    24,    25,    26,     0,     0,    97,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   202,     0,     0,   102,    50,
       0,    51,    52,     0,   589,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    57,   229,
      59,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    33,    34,    35,    36,
      37,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   202,     0,     0,   102,    50,     0,    51,    52,
       0,     0,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    57,    58,    59,    12,     0,
      13,    14,    15,    92,    93,    18,    19,     0,     0,     0,
       0,     0,    94,    95,    96,    23,    24,    25,    26,     0,
       0,    97,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    33,    34,    35,    36,    37,    38,     0,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   202,
       0,     0,   102,    50,     0,    51,    52,     0,     0,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    57,   229,    59,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    97,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   202,     0,     0,   102,
      50,     0,    51,    52,     0,     0,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    57,
     229,    59,    12,     0,    13,    14,    15,    92,    93,    18,
      19,     0,     0,     0,     0,     0,    94,    95,    96,    23,
      24,    25,    26,     0,     0,    97,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    98,    34,    35,
      36,    99,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     100,     0,     0,   101,     0,     0,   102,    50,     0,    51,
      52,     0,     0,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,   103,    13,    14,    15,
      92,    93,    18,    19,     0,     0,     0,     0,     0,    94,
      95,    96,    23,    24,    25,    26,     0,     0,    97,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,     0,    39,    40,    41,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   219,     0,     0,    49,
      50,     0,    51,    52,     0,    53,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,   103,
      13,    14,    15,    92,    93,    18,    19,     0,     0,     0,
       0,     0,    94,    95,    96,    23,    24,    25,    26,     0,
       0,    97,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    33,    34,    35,    36,    37,    38,     0,
      39,    40,    41,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   273,
       0,     0,   306,    50,     0,    51,    52,     0,   307,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,     0,     0,
       0,    12,   103,    13,    14,    15,    92,    93,    18,    19,
       0,     0,     0,     0,     0,    94,    95,    96,    23,    24,
      25,    26,     0,     0,    97,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    98,    34,    35,    36,
      99,    38,     0,    39,    40,    41,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   101,     0,     0,   102,    50,     0,    51,    52,
       0,     0,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,   103,    13,    14,    15,    92,
      93,    18,    19,     0,     0,     0,     0,     0,    94,    95,
      96,    23,    24,    25,    26,     0,     0,    97,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   273,     0,     0,   306,    50,
       0,    51,    52,     0,     0,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,   103,    13,
      14,    15,    92,    93,    18,    19,     0,     0,     0,     0,
       0,    94,    95,    96,    23,    24,    25,    26,     0,     0,
      97,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    33,    34,    35,    36,    37,    38,     0,    39,
      40,    41,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   825,     0,
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
       0,   851,     0,     0,   102,    50,     0,    51,    52,     0,
       0,     0,    54,    55,     0,     0,     0,    56,     0,   528,
     529,     0,     0,   530,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   103,   155,   156,   157,   158,   159,
     160,   161,   162,   163,     0,     0,   164,   165,     0,     0,
     166,   167,   168,   169,     0,     0,   310,   311,   312,   313,
     314,   315,   316,     0,   170,   319,   320,     0,     0,     0,
       0,   323,   324,     0,     0,     0,     0,     0,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,     0,   181,
     182,     0,   549,   521,     0,     0,   550,   326,   327,   328,
     329,   330,   331,   332,   333,   334,   335,   243,   155,   156,
     157,   158,   159,   160,   161,   162,   163,     0,     0,   164,
     165,     0,     0,   166,   167,   168,   169,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   170,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,   181,   182,     0,   534,   529,     0,     0,   535,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     243,   155,   156,   157,   158,   159,   160,   161,   162,   163,
       0,     0,   164,   165,     0,     0,   166,   167,   168,   169,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     170,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,   181,   182,     0,   569,   521,
       0,     0,   570,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   243,   155,   156,   157,   158,   159,   160,
     161,   162,   163,     0,     0,   164,   165,     0,     0,   166,
     167,   168,   169,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   170,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,     0,   181,   182,
       0,   572,   529,     0,     0,   573,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   243,   155,   156,   157,
     158,   159,   160,   161,   162,   163,     0,     0,   164,   165,
       0,     0,   166,   167,   168,   169,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   170,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
       0,   181,   182,     0,   582,   521,     0,     0,   583,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   243,
     155,   156,   157,   158,   159,   160,   161,   162,   163,     0,
       0,   164,   165,     0,     0,   166,   167,   168,   169,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   170,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,   181,   182,     0,   585,   529,     0,
       0,   586,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   243,   155,   156,   157,   158,   159,   160,   161,
     162,   163,     0,     0,   164,   165,     0,     0,   166,   167,
     168,   169,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   170,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,     0,   181,   182,     0,
     610,   521,     0,     0,   611,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   243,   155,   156,   157,   158,
     159,   160,   161,   162,   163,     0,     0,   164,   165,     0,
       0,   166,   167,   168,   169,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     181,   182,     0,   613,   529,     0,     0,   614,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   243,   155,
     156,   157,   158,   159,   160,   161,   162,   163,     0,     0,
     164,   165,     0,     0,   166,   167,   168,   169,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   170,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,   181,   182,     0,   890,   521,     0,     0,
     891,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   243,   155,   156,   157,   158,   159,   160,   161,   162,
     163,     0,     0,   164,   165,     0,     0,   166,   167,   168,
     169,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   170,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,     0,   181,   182,     0,   893,
     529,     0,     0,   894,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   243,   155,   156,   157,   158,   159,
     160,   161,   162,   163,     0,     0,   164,   165,     0,     0,
     166,   167,   168,   169,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   170,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,     0,   181,
     182,     0,   901,   521,     0,     0,   902,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   243,   155,   156,
     157,   158,   159,   160,   161,   162,   163,     0,     0,   164,
     165,     0,     0,   166,   167,   168,   169,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   170,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,   181,   182,     0,     0,   310,   311,   312,   313,
     314,   315,   316,   317,   318,   319,   320,   321,   322,     0,
     243,   323,   324,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   534,   529,     0,   325,   535,   326,   327,   328,
     329,   330,   331,   332,   333,   334,   335,     0,   155,   156,
     157,   158,   159,   160,   161,   162,   163,     0,     0,   164,
     165,     0,     0,   166,   167,   168,   169,     0,     0,     0,
     247,     0,     0,     0,     0,     0,     0,   170,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,   181,   182
};

static const yytype_int16 yycheck[] =
{
       2,   337,    27,     7,     8,    79,   363,    22,    22,     5,
       6,    16,    17,    58,   279,    20,     7,   399,   101,    15,
      12,   236,   210,     8,    28,   361,    72,   386,   381,   279,
      16,    17,     4,   274,    20,    53,    52,   309,    61,   438,
     376,    11,    12,    28,   396,    28,   714,   399,   430,    51,
      52,    55,     2,   389,     4,    16,    17,    53,   336,    20,
     338,   553,   564,   341,    55,    51,    52,    49,    61,    61,
     627,   716,   574,     1,    89,    89,    72,    26,    13,   571,
      27,    96,   641,   309,   362,   267,   485,   630,   627,   271,
      51,   630,   584,   537,   538,   110,    25,   375,   793,   377,
     365,    13,   641,    29,   440,    16,    17,    25,   811,    20,
      13,    25,   390,   105,     0,    86,    25,    77,   656,    86,
     612,   789,   139,    88,    86,   663,   143,   107,   134,    25,
      86,    13,    10,    11,    12,    86,    25,    28,    25,   417,
     246,    86,   107,    51,    13,    88,    88,    55,    86,    86,
      25,    55,   112,    13,    86,   261,    37,    38,   138,    15,
     138,    17,    86,   441,   107,   136,   137,    86,   139,   136,
     137,    25,    50,   841,   733,   137,    86,   872,   881,   105,
     136,   137,   107,   111,   107,   134,   137,   452,    55,    26,
     213,   548,   137,   408,   839,   142,   143,   438,   136,   137,
     137,   558,   452,   871,   139,   137,   257,   142,   143,   138,
     226,   227,   228,   137,   142,   143,   598,   136,   137,   139,
     138,   213,   100,    25,   138,   139,   670,   137,   274,   138,
     142,   143,   107,   235,   236,   240,   259,   242,   243,   142,
     143,   243,   138,   800,   485,   597,   598,   217,   218,   138,
     649,   138,   809,   796,   240,   306,   242,   796,    86,   304,
     142,   143,   143,   138,   309,   140,   259,   259,   143,   138,
     809,    88,   811,   142,   143,   257,   139,   360,   274,   240,
     782,   242,   142,   143,   138,   139,    58,    59,   304,   307,
     107,   563,   557,   309,    86,   401,   139,   134,   138,   405,
     345,   346,   139,    86,   410,   107,   256,   557,   136,   137,
      86,   307,   107,    51,   420,    53,    54,    55,    56,   425,
      88,   138,   294,   338,   306,   295,   296,   297,   298,   240,
      17,   242,   337,   142,   338,   337,   138,   341,   140,   217,
     218,   143,   881,   304,   294,   137,   705,   362,   309,   299,
     138,    55,    90,   447,   137,   143,   361,    25,   362,   361,
     136,   137,   377,    88,   356,    86,   382,   369,   246,    89,
     248,   376,    26,   377,   376,   390,   641,   592,   484,   395,
     396,   138,   107,   399,   389,   138,   390,   389,   576,    26,
     436,   134,   438,   385,   386,    26,    86,    62,    63,    69,
     892,     2,   417,     4,   786,   407,   408,   764,   649,   413,
     396,   427,   414,   417,   430,   136,   137,   295,   296,   297,
     298,   382,   300,   301,   650,   607,   441,    86,   413,   782,
     413,   138,    86,    88,   786,   440,   143,   441,   440,   485,
     436,   427,   438,   140,    86,   143,   136,   137,    49,    86,
     556,   107,   107,    88,   138,    86,   458,   135,   460,   143,
     462,   141,    86,   479,   414,    55,    86,   824,    95,    88,
     142,   143,   107,   423,    58,    59,   478,   136,   137,    10,
     134,   138,   136,   137,   509,   139,   364,    69,   107,   485,
      37,    38,    69,   708,   136,   137,     8,   134,   138,   136,
     137,   102,   527,   134,   769,   136,   137,    13,   139,    10,
     525,   525,   136,   137,   140,   621,   136,   137,   533,   533,
     138,   536,   536,   617,   618,    88,    62,    63,   553,   865,
      88,   533,    14,    15,   536,   537,   538,   552,    51,   554,
      53,    54,    55,    56,   107,   138,   571,   563,   135,   107,
      26,    88,   554,   545,   107,   138,   560,   561,   138,   584,
     138,   563,   564,    51,   566,   138,    51,   559,   138,   560,
     107,   677,   574,   589,   138,   138,   561,   547,   561,    69,
     138,   597,   598,   771,   138,    88,   107,   612,   866,   777,
     592,   665,    51,    10,    53,    54,    55,    56,   623,   615,
     201,   138,   563,   649,   107,   650,    15,   713,   653,   654,
      86,   597,    10,   135,   659,   660,   641,   587,   138,   135,
     714,   138,   716,   109,    69,   138,    88,   117,   118,   119,
     646,   141,    89,    10,     2,   138,     4,     5,     6,    84,
      85,    10,    10,    11,    12,   107,     9,    15,    16,    17,
     251,    10,    20,   649,    10,   256,   257,   138,   134,   138,
     136,   137,    51,   139,    53,    54,    55,    56,   670,   547,
     115,   116,   117,   118,   119,   140,   138,    10,   142,   138,
     672,    49,    50,   685,   700,    53,   688,   135,   138,   138,
      58,    69,   138,   294,   138,   789,   790,   135,   299,    40,
      41,    42,    43,    44,    72,   306,   708,   752,   733,   587,
     107,   112,   138,   705,   138,   138,    10,   138,    10,   135,
     690,   723,   724,    10,   726,   741,   728,   729,    10,   112,
     138,   138,   100,   735,   102,   737,   738,   115,   116,   117,
     118,   119,   135,    10,    55,   839,   138,   841,   138,    55,
     138,   460,    51,   462,    53,    54,    55,    56,   138,   138,
      10,    51,   363,    53,    54,    55,    56,    10,   139,   139,
     786,   372,   138,   414,   770,    15,     6,   871,   536,   380,
     782,   868,   630,   617,   872,   633,   666,   867,     7,    69,
     769,    90,   627,   641,   188,    -1,   809,    96,    97,    -1,
      90,    -1,    -1,    -1,    84,    85,    96,    97,    -1,    -1,
     806,    -1,   690,   414,    -1,   114,    -1,    -1,   117,   844,
      -1,    -1,   423,    -1,   114,    -1,    -1,   117,    -1,    -1,
      -1,    -1,    -1,   201,   114,   115,   116,   117,   118,   119,
      -1,    -1,    -1,    -1,   143,   847,   848,   849,   850,   217,
     218,   866,   854,   855,   856,    -1,   858,   859,    -1,    -1,
     865,    49,   866,   865,   465,   867,   868,   892,    -1,    -1,
      -1,    -1,   240,    -1,   242,   243,    -1,    -1,   246,    -1,
     248,    -1,    -1,   251,    -1,    -1,    -1,    -1,   256,   257,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   900,    -1,
      -1,   903,   904,   905,   906,    51,   274,    53,    54,    55,
      56,    -1,   914,    51,   102,    53,    54,    55,    56,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   294,   295,   296,   297,
     298,   299,   300,   301,    -1,    -1,   304,    -1,   306,   307,
      69,   309,    -1,    -1,    90,    -1,    -1,   548,   796,    -1,
     798,    97,    90,    -1,   802,    84,    85,   558,    96,    97,
      -1,    -1,    -1,   811,    -1,   813,    -1,    -1,   114,   337,
      -1,    -1,    -1,    -1,    -1,    -1,   114,   345,   346,   117,
      -1,    -1,    -1,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,   361,    -1,   363,   364,    -1,    -1,    -1,
      -1,   139,    -1,    -1,   372,    -1,    -1,    -1,   376,    -1,
      -1,    -1,   380,    -1,   723,   724,    -1,   726,    -1,   728,
     729,   389,    -1,   624,   625,    -1,   735,   875,   737,   738,
      -1,    -1,    -1,   881,    -1,   883,   637,    -1,   886,    -1,
      -1,   642,    -1,    -1,    -1,    -1,   414,    -1,    -1,    16,
      17,    -1,    -1,    20,    51,   423,    53,    54,    55,    56,
      -1,    -1,   910,   251,    -1,    -1,   667,   668,   436,   257,
     438,    51,   440,    53,    54,    55,    56,    -1,    -1,    46,
      47,    -1,    -1,    -1,    51,    52,   687,    -1,    -1,    -1,
      -1,    58,    59,    90,    -1,    -1,    -1,   465,    -1,    96,
     701,   702,   703,    -1,    -1,    -1,    -1,    -1,    -1,    51,
      90,    53,    54,    55,    56,    -1,    96,   485,   306,    51,
      -1,    53,    54,    55,    56,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   847,   848,
     849,   850,    -1,    -1,    -1,   854,   855,   856,    90,   858,
     859,    -1,    -1,    -1,    96,    97,    -1,    -1,    90,    -1,
      -1,    -1,   763,   764,    96,    97,    -1,    -1,    -1,    -1,
      -1,    -1,   114,   774,    -1,   117,    -1,    -1,    -1,   547,
     548,    -1,   114,    -1,   372,   117,    -1,    -1,    -1,    -1,
     558,   900,   380,    -1,   903,   904,   905,   906,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   914,    -1,   139,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   587,
      -1,    -1,    -1,   824,    -1,    -1,    -1,    -1,    -1,   830,
      -1,   832,    -1,    -1,    -1,    -1,    -1,   838,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   624,   625,    -1,   226,
     227,   228,   229,    -1,    -1,    -1,    -1,    -1,    -1,   637,
      -1,    -1,    -1,   240,   642,   242,   243,   465,    -1,    -1,
      -1,   649,   650,    -1,    -1,   653,   654,    -1,    -1,    -1,
      -1,   659,   660,     2,    -1,     4,    -1,    -1,    -1,   667,
     668,    -1,    -1,    -1,    -1,    -1,    15,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   687,
      -1,    -1,   690,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   701,   702,   703,    -1,   304,    -1,    -1,
      49,    -1,   309,   310,   311,   312,   313,   314,   315,   316,
     317,   318,   319,   320,   321,   322,   323,   324,   325,   326,
     327,   328,   329,   330,   331,   332,   333,   334,   335,    -1,
     337,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   345,   346,
      -1,    -1,    -1,    -1,   752,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   102,   361,   763,   764,    -1,    -1,    -1,
      -1,    -1,   770,    -1,    -1,    -1,   774,   374,    -1,   376,
      -1,   378,   379,    -1,    -1,   382,    -1,    -1,    -1,    -1,
      -1,    -1,   389,    -1,    -1,    -1,    -1,    -1,   395,   396,
      -1,    -1,   399,    -1,    -1,    -1,   624,   625,   806,    -1,
       2,    -1,     4,     5,     6,     7,    -1,    -1,    -1,   637,
      -1,   418,    -1,    15,   642,    -1,   824,    -1,    -1,    -1,
     427,    -1,   830,   430,   832,    -1,    -1,    -1,    -1,    -1,
     838,    -1,    -1,   440,    -1,    -1,    -1,    -1,    -1,   667,
     668,    -1,    -1,    -1,    -1,    -1,    -1,    49,    -1,    -1,
      -1,    53,   201,    -1,    -1,    -1,    -1,   865,    -1,   687,
      -1,   468,   469,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      72,    -1,   479,   701,   702,   703,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     2,    -1,     4,     5,     6,    44,    -1,    -1,
     102,    -1,   251,    -1,    -1,    15,    -1,   256,   257,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,   763,    -1,    84,    85,    49,
      -1,    -1,    -1,    53,    -1,   294,   774,    -1,    -1,    -1,
     299,    -1,    -1,    -1,    -1,    -1,   563,   306,    -1,    -1,
     309,   108,    72,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   589,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     597,   598,   102,    -1,    -1,    -1,    -1,    -1,    -1,   201,
      -1,    -1,   830,    -1,   832,    -1,    -1,    -1,   615,    -1,
     838,    -1,    -1,    -1,   363,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   372,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   380,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   646,
      -1,    -1,    -1,   650,   651,    -1,   653,   654,    -1,   251,
      -1,    -1,   659,   660,   256,   257,    -1,    -1,    -1,    -1,
      -1,    -1,   669,    -1,    -1,   414,    -1,    -1,    -1,    -1,
      -1,    -1,   274,    -1,   423,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   693,   694,    -1,   696,
     697,   201,   294,   700,    -1,    -1,    -1,   299,    -1,    -1,
      -1,    -1,    -1,    -1,   306,   307,    -1,    -1,    69,    70,
      71,    72,    73,    74,    75,    76,   465,    78,    79,    -1,
      -1,    -1,    -1,    84,    85,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   741,    -1,    -1,    -1,   745,    -1,
      -1,   251,    -1,    -1,    -1,   752,   256,   257,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
      -1,   363,    -1,    -1,   274,    -1,    -1,    -1,    -1,    -1,
     372,    -1,   779,    -1,    -1,    -1,    -1,    -1,   380,   786,
      -1,    -1,    -1,    -1,   294,    -1,    -1,    -1,    -1,   299,
      -1,    -1,    -1,    -1,    -1,    -1,   306,   307,    -1,   548,
      -1,    -1,    -1,    -1,    -1,     0,    -1,    -1,    -1,   558,
      -1,    -1,   414,     8,     9,    10,    -1,    -1,    13,    14,
      15,   423,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    26,    27,    -1,   436,    -1,   438,    -1,    -1,    -1,
      -1,    -1,    37,    38,    -1,    40,    41,    42,    43,    44,
      -1,    -1,    -1,   363,    -1,    -1,    -1,    -1,   865,    -1,
      -1,    -1,   372,   465,    -1,    -1,    -1,    -1,    -1,    -1,
     380,    -1,    -1,    -1,    -1,   624,   625,    -1,    -1,    -1,
      -1,    -1,    -1,   485,    -1,    -1,    -1,    -1,   637,    -1,
      -1,    86,    -1,   642,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   650,    -1,    -1,   414,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   423,    -1,    -1,    -1,    -1,   667,   668,
      -1,    -1,    -1,    -1,    -1,    -1,   436,    -1,   438,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   687,   134,
     135,    -1,   137,    -1,   139,   140,   548,   142,   143,    -1,
      -1,    -1,   701,   702,   703,   465,   558,    -1,    -1,    -1,
      -1,    -1,    -1,     5,     6,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    15,    -1,   485,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    47,    -1,    -1,    -1,    51,
      52,    53,    -1,    -1,   763,   764,    -1,    59,    -1,    -1,
      -1,   770,   624,   625,    -1,   774,    -1,    -1,    -1,    -1,
      72,    -1,    -1,    -1,    -1,   637,    -1,    -1,   548,    -1,
     642,    -1,    -1,    -1,    -1,    -1,    -1,   649,   558,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   667,   668,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   824,    -1,    -1,    -1,    -1,
      -1,   830,    -1,   832,    -1,   687,    -1,    -1,    -1,   838,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   701,
     702,   703,    69,    70,    71,    72,    73,    74,    75,    -1,
      -1,    78,    79,    -1,   624,   625,    -1,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   637,    -1,    -1,
      -1,    -1,   642,    -1,    -1,    -1,    -1,    -1,    -1,   649,
      -1,    -1,    -1,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,    -1,    -1,   667,   668,    -1,
      -1,   763,   764,    -1,    -1,    -1,    -1,    -1,   770,    -1,
      -1,    -1,   774,    -1,    -1,    -1,    -1,   687,    51,    52,
      -1,    -1,    55,    -1,   226,   227,   228,   229,    -1,    -1,
      -1,   701,   702,   703,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    -1,   806,    78,    79,    -1,    -1,    82,
      83,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   824,    96,    -1,    -1,    -1,    -1,   830,    -1,
     832,    -1,   274,    -1,    -1,    -1,   838,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,   121,   122,
      -1,    -1,    -1,   763,   764,    -1,    -1,    -1,    -1,    -1,
     770,    -1,    -1,    -1,   774,   307,   139,    -1,   310,   311,
     312,   313,   314,   315,   316,   317,   318,   319,   320,   321,
     322,   323,   324,   325,   326,   327,   328,   329,   330,   331,
     332,   333,   334,   335,    -1,    -1,   806,    -1,    -1,    -1,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    -1,   824,    84,    85,    -1,    -1,    -1,
     830,    -1,   832,    -1,    -1,    -1,    -1,    -1,   838,    -1,
      -1,    -1,   374,    -1,    44,    -1,   378,   379,    -1,   108,
     382,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,   395,   396,    -1,    -1,   399,    -1,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    -1,    -1,    84,    85,   418,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   427,    -1,    -1,   430,    -1,
      -1,    -1,    -1,    -1,   436,    -1,   438,    -1,   108,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   468,   469,   138,     0,
       1,    -1,     3,     4,     5,     6,     7,   479,    -1,    -1,
      11,    12,    -1,   485,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    -1,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,   563,    93,    94,    -1,    96,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   589,    -1,   120,
     121,   122,    -1,    -1,    -1,   597,   598,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   142,   143,   615,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   646,     0,    -1,   649,    -1,   651,
      -1,    -1,    -1,     8,     9,    10,    -1,    -1,    -1,    14,
      15,    -1,    17,    -1,    -1,    -1,    -1,   669,    -1,    -1,
      -1,    26,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    37,    38,    -1,    40,    41,    42,    43,    44,
      -1,   693,   694,    -1,   696,   697,    -1,    -1,   700,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    -1,    -1,    84,
      85,    86,    -1,    88,    -1,    -1,    -1,    -1,    -1,   741,
      -1,    -1,    -1,   745,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   107,   108,    -1,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,    -1,    -1,   770,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   779,    -1,   134,
     135,   136,   137,   138,   786,   140,     0,   142,   143,    -1,
      -1,    -1,    -1,    -1,     8,     9,    10,    -1,    -1,    -1,
      14,    15,    -1,    17,   806,    -1,    -1,    -1,    -1,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    -1,    40,    41,    42,    43,
      44,    -1,    -1,    -1,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    -1,    -1,    84,
      85,    -1,    -1,    -1,    -1,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    -1,    -1,
      84,    85,    86,   108,    88,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   107,   108,    -1,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   135,   136,   137,   138,    -1,   140,    -1,   142,   143,
       1,    -1,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    -1,    -1,    15,    16,    -1,    18,    19,    20,
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
      -1,    -1,    -1,    -1,     1,    -1,     3,     4,     5,     6,
       7,   142,   143,    10,    11,    12,    -1,    14,    15,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      47,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      -1,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,
      -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   120,   121,   122,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     1,    -1,     3,     4,     5,     6,     7,
      -1,    -1,    10,    11,    12,   142,   143,    15,    16,    17,
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
      -1,    -1,     1,    -1,     3,     4,     5,     6,     7,    -1,
      -1,    10,    11,    12,   142,   143,    15,    16,    -1,    18,
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
       1,    -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,
      11,    12,    -1,   142,   143,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    -1,
      51,    52,    53,    54,    55,    56,    -1,    58,    59,    60,
      61,    62,    -1,    64,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   135,    -1,    -1,    -1,    -1,    -1,
      -1,   142,   143,     1,    -1,     3,     4,     5,     6,     7,
      -1,     9,    10,    11,    12,    -1,    -1,    -1,    16,    -1,
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
      -1,     1,    -1,     3,     4,     5,     6,     7,    -1,    -1,
      -1,    11,    12,    -1,   142,   143,    16,    -1,    18,    19,
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
     140,    -1,   142,   143,     1,    -1,     3,     4,     5,     6,
       7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      47,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      -1,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,
      -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   120,   121,   122,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   140,    -1,   142,   143,     1,    -1,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    11,    12,    -1,
      -1,    -1,    16,    -1,    18,    19,    20,    21,    22,    23,
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
      -1,   135,    -1,    -1,    -1,    -1,    -1,    -1,   142,   143,
       1,    -1,     3,     4,     5,     6,     7,    -1,    -1,    10,
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
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,   142,   143,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      -1,    64,    65,    -1,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,
      93,    94,    -1,    96,    97,    98,    99,    -1,    -1,    -1,
     103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,   121,   122,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,
     143,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    -1,    51,    52,    53,    54,    55,    56,
      -1,    58,    59,    60,    61,    62,    -1,    64,    65,    -1,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,    -1,
      -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   120,   121,   122,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,   142,   143,    16,    -1,    18,
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
       3,     4,     5,     6,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,   143,    18,    19,    20,    21,    22,
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
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,   142,
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
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   142,     3,     4,     5,     6,     7,
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
     118,   119,    -1,   121,   122,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,
      16,   139,    18,    19,    20,    21,    22,    23,    24,    -1,
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
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
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
      -1,    -1,    -1,    -1,    -1,    87,    88,    -1,    90,    91,
      -1,    93,    94,    -1,    96,    97,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,   107,    -1,    -1,    -1,    -1,
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
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    -1,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,   120,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,   138,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    -1,    -1,    84,
      85,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    -1,    -1,    98,    99,    -1,    -1,
      -1,   103,    -1,    -1,    -1,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,    -1,    -1,   120,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   138,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      -1,    -1,    78,    79,    -1,    -1,    82,    83,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,   121,   122,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    50,    51,    52,    -1,    -1,    55,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      -1,    -1,    78,    79,    -1,    -1,    82,    83,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      96,    -1,    -1,    99,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,   121,   122,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    50,    51,    52,    -1,    -1,    55,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      -1,    -1,    78,    79,    -1,    -1,    82,    83,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,   121,   122,     3,     4,     5,
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
      -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,    -1,
      -1,    -1,    11,    12,   120,   121,   122,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    45,    -1,    47,    48,
      49,    -1,    51,    52,    53,    54,    55,    56,    -1,    58,
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
      -1,    93,    94,    -1,    96,    97,    98,    99,    -1,    -1,
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
      -1,    96,    97,    98,    99,    -1,    -1,    -1,   103,    -1,
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
      91,    -1,    93,    94,    -1,    -1,    97,    98,    99,    -1,
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
      -1,    90,    91,    -1,    93,    94,    -1,    96,    -1,    98,
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
      -1,    -1,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   120,   121,   122,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    -1,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    -1,    -1,
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
      91,    -1,    93,    94,    -1,    -1,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,   120,
     121,   122,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    51,    52,    53,
      54,    55,    56,    -1,    58,    59,    60,    61,    62,    -1,
      64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    -1,    -1,    90,    91,    -1,    93,
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
      91,    -1,    93,    94,    -1,    96,    -1,    98,    99,    -1,
      -1,    -1,   103,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,   120,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    49,    -1,    51,    52,    53,    54,    55,    56,    -1,
      58,    59,    60,    61,    62,    -1,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    -1,    93,    94,    -1,    96,    -1,
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
      -1,    -1,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,   120,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    -1,    51,
      52,    53,    54,    55,    56,    -1,    58,    59,    60,    61,
      62,    -1,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      -1,    93,    94,    -1,    -1,    -1,    98,    99,    -1,    -1,
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
      -1,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    51,
      52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   120,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    -1,    -1,    78,    79,    -1,    -1,
      82,    83,    84,    85,    -1,    -1,    69,    70,    71,    72,
      73,    74,    75,    -1,    96,    78,    79,    -1,    -1,    -1,
      -1,    84,    85,    -1,    -1,    -1,    -1,    -1,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,   121,
     122,    -1,    51,    52,    -1,    -1,    55,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   139,    67,    68,
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
     119,    -1,   121,   122,    -1,    -1,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    -1,
     139,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    51,    52,    -1,   108,    55,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    -1,    -1,    78,
      79,    -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,
     143,    -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,   121,   122
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
     166,   167,   168,   170,   171,   181,   194,   211,   230,   231,
     241,   242,   246,   247,   249,   250,   251,   252,   253,   276,
     287,   149,    21,    22,    30,    31,    32,    39,    51,    55,
      84,    87,    90,   120,   172,   173,   194,   211,   250,   253,
     276,   173,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    45,    46,    47,    48,
      49,    50,    51,    52,    55,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    78,    79,    82,    83,    84,    85,
      96,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   121,   122,   139,   174,   179,   180,   252,   271,    33,
      34,    35,    36,    48,    49,    51,    55,   174,   175,   177,
     247,   195,    87,   157,   158,   171,   211,   250,   251,   253,
     158,   142,   143,   158,   280,   285,   286,   198,   200,    87,
     164,   171,   211,   216,   250,   253,    57,    96,    97,   121,
     163,   181,   182,   187,   190,   192,   274,   275,   187,   187,
     139,   188,   189,   139,   184,   188,   139,   143,   281,   175,
     150,   134,   181,   211,   181,    55,     1,    90,   152,   153,
     154,   165,   166,   287,   157,   183,   192,   274,   287,   182,
     273,   274,   287,    87,   138,   170,   211,   250,   253,   197,
      53,    54,    56,   174,   248,    62,    63,   243,    58,    59,
     159,   181,   181,   280,   286,    40,    41,    42,    43,    44,
      37,    38,    28,   228,   107,   138,    90,    96,   167,   107,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    84,    85,   108,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    86,   136,   137,    86,
     137,   279,    26,   134,   232,    88,    88,   184,   188,   232,
     157,    51,    55,   172,    58,    59,     1,   111,   254,   285,
      86,   136,   137,   207,   272,   208,   279,   138,   151,   152,
      55,    13,   212,   285,   107,    86,   136,   137,    88,    88,
     212,   280,    17,   235,   142,   158,   158,    55,    86,   136,
     137,    25,   182,   182,   182,    89,   138,   191,   287,   138,
     191,   187,   281,   282,   187,   186,   187,   192,   274,   287,
     157,   282,   157,   155,   134,   152,    86,   137,    88,   154,
     165,   140,   280,   286,   282,   196,   141,   138,   143,   284,
     138,   284,   135,   284,    55,   167,   168,   169,   138,    86,
     136,   137,    51,    53,    54,    55,    56,    90,    96,    97,
     114,   117,   139,   226,   257,   258,   259,   260,   261,   262,
     265,   266,   267,   268,   269,   244,    62,    63,    69,    69,
     149,   158,   158,   158,   158,   154,   157,   157,   229,    96,
     159,   182,   192,   193,   165,   138,   170,   138,   156,   159,
     171,   181,   182,   193,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
      51,    52,    55,   179,   184,   277,   278,   186,    51,    52,
      55,   179,   184,   277,    51,    55,   277,   234,   233,   159,
     181,   159,   181,    95,   161,   205,   285,   255,   204,    51,
      55,   172,   277,   186,   277,   151,   157,   139,   256,   257,
     209,   178,    10,     8,   237,   287,   152,    13,   181,    51,
      55,   186,    51,    55,   152,   235,   192,    10,    27,   213,
     285,   213,    51,    55,   186,    51,    55,   202,   182,    96,
     182,   190,   274,   275,   282,   140,   282,   138,   138,   282,
     175,   147,   135,   181,   282,   154,   282,   274,   167,   169,
      51,    55,   186,    51,    55,   107,    51,    90,    96,   217,
     218,   219,   259,   257,    29,   105,   227,   138,   270,   287,
     138,   270,    51,   138,   270,    51,   152,   245,   181,   181,
      77,   112,   221,   222,   287,   182,   138,   282,   169,   138,
     107,    44,   281,    88,    88,   184,   188,   281,   283,    88,
      88,   184,   185,   188,   287,   185,   188,   221,   221,    44,
     162,   285,   158,   151,   283,    10,   282,   257,   151,   285,
     174,   175,   176,   182,   193,   238,   287,    15,   215,   287,
      14,   214,   215,    88,    88,   283,    88,    88,   215,    10,
     138,   212,   199,   201,   283,   158,   182,   191,   274,   135,
     284,   283,   182,   219,   138,   259,   138,   282,   223,   281,
     152,   152,   260,   265,   267,   269,   261,   262,   267,   261,
     135,   152,    51,   220,   223,   261,   263,   264,   267,   269,
     152,    96,   182,   169,   181,   109,   159,   181,   159,   181,
     161,   141,    88,   159,   181,   159,   181,   161,   232,   228,
     152,   152,   181,   221,   206,   285,    10,   282,    10,   210,
      89,   239,   287,   152,     9,   240,   287,   158,    10,    88,
      10,   182,   152,   152,   152,   213,   138,   282,   218,   138,
      96,   217,   140,   142,    10,   135,   138,   270,   138,   270,
     138,   270,   138,   270,   270,   135,   107,   223,   112,   138,
     270,   138,   270,   138,   270,    10,   182,   181,   159,   181,
      10,   135,   152,   151,   256,    87,   171,   211,   250,   253,
     212,   152,   212,   215,   235,   236,    10,    10,   203,   138,
     218,   138,   259,    51,   224,   225,   258,   261,   267,   261,
     261,    87,   211,   112,   264,   267,   261,   263,   267,   261,
     135,    10,   151,    55,    86,   136,   137,   152,   152,   152,
     218,   138,   138,   281,   270,   138,   270,   270,   270,    55,
      86,   138,   270,   138,   270,   270,   138,   270,   270,    10,
      51,    55,   186,    51,    55,   237,   214,    10,   218,   225,
     261,    51,    55,   261,   267,   261,   261,   283,   270,   270,
     138,   270,   270,   270,   261,   270
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
#line 968 "./parse.y"
    {
		     p->lstate = EXPR_BEG;
		     local_nest(p);
		   ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 973 "./parse.y"
    {
		      p->tree = new_scope(p, (yyvsp[(2) - (2)].node));
		     local_unnest(p);
		    ;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 980 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 986 "./parse.y"
    {
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 990 "./parse.y"
    {
		      (yyval.node) = new_begin(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 994 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), newline_node((yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 998 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 1005 "./parse.y"
    {
		      if (p->in_def || p->in_single) {
			yyerror(p, "BEGIN in method");
		      }
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 1012 "./parse.y"
    {
		      p->begin_tree = push(p->begin_tree, (yyvsp[(4) - (5)].node));
		      local_resume(p, (yyvsp[(2) - (5)].node));
		      (yyval.node) = 0;
		    ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 1023 "./parse.y"
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
#line 1046 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 1052 "./parse.y"
    {
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 1056 "./parse.y"
    {
		      (yyval.node) = new_begin(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 1060 "./parse.y"
    {
			(yyval.node) = push((yyvsp[(1) - (3)].node), newline_node((yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 1064 "./parse.y"
    {
		      (yyval.node) = new_begin(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 1069 "./parse.y"
    {p->lstate = EXPR_FNAME;;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 1070 "./parse.y"
    {
		      (yyval.node) = new_alias(p, (yyvsp[(2) - (4)].id), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 1074 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 1078 "./parse.y"
    {
			(yyval.node) = new_if(p, cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node), 0);
		    ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 1082 "./parse.y"
    {
		      (yyval.node) = new_unless(p, cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node), 0);
		    ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 1086 "./parse.y"
    {
		      (yyval.node) = new_while(p, cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node));
		    ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 1090 "./parse.y"
    {
		      (yyval.node) = new_until(p, cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node));
		    ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 1094 "./parse.y"
    {
		      (yyval.node) = new_rescue(p, (yyvsp[(1) - (3)].node), list1(list3(0, 0, (yyvsp[(3) - (3)].node))), 0);
		    ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 1098 "./parse.y"
    {
		      if (p->in_def || p->in_single) {
			yywarn(p, "END in method; use at_exit");
		      }
		      (yyval.node) = new_postexe(p, (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 1106 "./parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(1) - (3)].node), list1((yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 1110 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 1114 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].node), intern("[]"), (yyvsp[(3) - (6)].node)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 1118 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 1122 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 1126 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.node) = 0;
		    ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 1131 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 1135 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].node));
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 1140 "./parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (3)].node), new_array(p, (yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 1144 "./parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 1148 "./parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(1) - (3)].node), new_array(p, (yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 1155 "./parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 1159 "./parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 1167 "./parse.y"
    {
		      (yyval.node) = new_and(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 1171 "./parse.y"
    {
		      (yyval.node) = new_or(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 1175 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(3) - (3)].node)), "!");
		    ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 1179 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(2) - (2)].node)), "!");
		    ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 1186 "./parse.y"
    {
		      if (!(yyvsp[(1) - (1)].node)) (yyval.node) = new_nil(p);
		      else (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 1201 "./parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 1207 "./parse.y"
    {
		      (yyval.node) = new_block(p, (yyvsp[(3) - (5)].node), (yyvsp[(4) - (5)].node));
		      local_unnest(p);
		    ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 1214 "./parse.y"
    {
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 1218 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(2) - (3)].node), (yyvsp[(3) - (3)].node));
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (3)].id), (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 1223 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 1227 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		      (yyval.node) = new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
		   ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 1232 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 1236 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		      (yyval.node) = new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
		    ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 1241 "./parse.y"
    {
		      (yyval.node) = new_super(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 1245 "./parse.y"
    {
		      (yyval.node) = new_yield(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 1249 "./parse.y"
    {
		      (yyval.node) = new_return(p, ret_args(p, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 1253 "./parse.y"
    {
		      (yyval.node) = new_break(p, ret_args(p, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 1257 "./parse.y"
    {
		      (yyval.node) = new_next(p, ret_args(p, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 1263 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 1267 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 1274 "./parse.y"
    {
		      (yyval.node) = list1((yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 1280 "./parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 1284 "./parse.y"
    {
		      (yyval.node) = list1(push((yyvsp[(1) - (2)].node),(yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 1288 "./parse.y"
    {
		      (yyval.node) = list2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 1292 "./parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 1296 "./parse.y"
    {
		      (yyval.node) = list2((yyvsp[(1) - (2)].node), new_nil(p));
		    ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 1300 "./parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (4)].node), new_nil(p), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 1304 "./parse.y"
    {
		      (yyval.node) = list2(0, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 1308 "./parse.y"
    {
		      (yyval.node) = list3(0, (yyvsp[(2) - (4)].node), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 1312 "./parse.y"
    {
		      (yyval.node) = list2(0, new_nil(p));
		    ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 1316 "./parse.y"
    {
		      (yyval.node) = list3(0, new_nil(p), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 1323 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 1329 "./parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (2)].node));
		    ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 1333 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 1339 "./parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 1343 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 1349 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 1353 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), intern("[]"), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 1357 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 1361 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 1365 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 1369 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.node) = new_colon2(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 1375 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.node) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 1381 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].node));
		      (yyval.node) = 0;
		    ;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 1388 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 1392 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), intern("[]"), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 1396 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 1400 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 1404 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 1408 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.node) = new_colon2(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 1414 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.node) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 1420 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].node));
		      (yyval.node) = 0;
		    ;}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 1427 "./parse.y"
    {
		      yyerror(p, "class/module name must be CONSTANT");
		    ;}
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 1434 "./parse.y"
    {
		      (yyval.node) = cons((node*)1, (node*)(yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 1438 "./parse.y"
    {
		      (yyval.node) = cons((node*)0, (node*)(yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 1442 "./parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (3)].node), (node*)(yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 1451 "./parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 111:

/* Line 1455 of yacc.c  */
#line 1456 "./parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 1467 "./parse.y"
    {
		      (yyval.node) = new_sym(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 115:

/* Line 1455 of yacc.c  */
#line 1473 "./parse.y"
    {
		      (yyval.node) = new_undef(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 116:

/* Line 1455 of yacc.c  */
#line 1476 "./parse.y"
    {p->lstate = EXPR_FNAME;;}
    break;

  case 117:

/* Line 1455 of yacc.c  */
#line 1477 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (4)].node), (node*)(yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 118:

/* Line 1455 of yacc.c  */
#line 1482 "./parse.y"
    { (yyval.id) = intern("|"); ;}
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 1483 "./parse.y"
    { (yyval.id) = intern("^"); ;}
    break;

  case 120:

/* Line 1455 of yacc.c  */
#line 1484 "./parse.y"
    { (yyval.id) = intern("&"); ;}
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 1485 "./parse.y"
    { (yyval.id) = intern("<=>"); ;}
    break;

  case 122:

/* Line 1455 of yacc.c  */
#line 1486 "./parse.y"
    { (yyval.id) = intern("=="); ;}
    break;

  case 123:

/* Line 1455 of yacc.c  */
#line 1487 "./parse.y"
    { (yyval.id) = intern("==="); ;}
    break;

  case 124:

/* Line 1455 of yacc.c  */
#line 1488 "./parse.y"
    { (yyval.id) = intern("=~"); ;}
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 1489 "./parse.y"
    { (yyval.id) = intern("!~"); ;}
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 1490 "./parse.y"
    { (yyval.id) = intern(">"); ;}
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 1491 "./parse.y"
    { (yyval.id) = intern(">="); ;}
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 1492 "./parse.y"
    { (yyval.id) = intern("<"); ;}
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 1493 "./parse.y"
    { (yyval.id) = intern(">="); ;}
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 1494 "./parse.y"
    { (yyval.id) = intern("!="); ;}
    break;

  case 131:

/* Line 1455 of yacc.c  */
#line 1495 "./parse.y"
    { (yyval.id) = intern("<<"); ;}
    break;

  case 132:

/* Line 1455 of yacc.c  */
#line 1496 "./parse.y"
    { (yyval.id) = intern(">>"); ;}
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 1497 "./parse.y"
    { (yyval.id) = intern("+"); ;}
    break;

  case 134:

/* Line 1455 of yacc.c  */
#line 1498 "./parse.y"
    { (yyval.id) = intern("-"); ;}
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 1499 "./parse.y"
    { (yyval.id) = intern("*"); ;}
    break;

  case 136:

/* Line 1455 of yacc.c  */
#line 1500 "./parse.y"
    { (yyval.id) = intern("*"); ;}
    break;

  case 137:

/* Line 1455 of yacc.c  */
#line 1501 "./parse.y"
    { (yyval.id) = intern("/"); ;}
    break;

  case 138:

/* Line 1455 of yacc.c  */
#line 1502 "./parse.y"
    { (yyval.id) = intern("%"); ;}
    break;

  case 139:

/* Line 1455 of yacc.c  */
#line 1503 "./parse.y"
    { (yyval.id) = intern("**"); ;}
    break;

  case 140:

/* Line 1455 of yacc.c  */
#line 1504 "./parse.y"
    { (yyval.id) = intern("!"); ;}
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 1505 "./parse.y"
    { (yyval.id) = intern("~"); ;}
    break;

  case 142:

/* Line 1455 of yacc.c  */
#line 1506 "./parse.y"
    { (yyval.id) = intern("+@"); ;}
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 1507 "./parse.y"
    { (yyval.id) = intern("-@"); ;}
    break;

  case 144:

/* Line 1455 of yacc.c  */
#line 1508 "./parse.y"
    { (yyval.id) = intern("[]"); ;}
    break;

  case 145:

/* Line 1455 of yacc.c  */
#line 1509 "./parse.y"
    { (yyval.id) = intern("[]="); ;}
    break;

  case 186:

/* Line 1455 of yacc.c  */
#line 1527 "./parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 187:

/* Line 1455 of yacc.c  */
#line 1531 "./parse.y"
    {
		      (yyval.node) = new_asgn(p, (yyvsp[(1) - (5)].node), new_rescue(p, (yyvsp[(3) - (5)].node), list1(list3(0, 0, (yyvsp[(5) - (5)].node))), 0));
		    ;}
    break;

  case 188:

/* Line 1455 of yacc.c  */
#line 1535 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, (yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 189:

/* Line 1455 of yacc.c  */
#line 1539 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, (yyvsp[(1) - (5)].node), (yyvsp[(2) - (5)].id), new_rescue(p, (yyvsp[(3) - (5)].node), list1(list3(0, 0, (yyvsp[(5) - (5)].node))), 0));
		    ;}
    break;

  case 190:

/* Line 1455 of yacc.c  */
#line 1543 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].node), intern("[]"), (yyvsp[(3) - (6)].node)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 191:

/* Line 1455 of yacc.c  */
#line 1547 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 192:

/* Line 1455 of yacc.c  */
#line 1551 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 193:

/* Line 1455 of yacc.c  */
#line 1555 "./parse.y"
    {
		      (yyval.node) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 194:

/* Line 1455 of yacc.c  */
#line 1559 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 195:

/* Line 1455 of yacc.c  */
#line 1564 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 196:

/* Line 1455 of yacc.c  */
#line 1569 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].node));
		      (yyval.node) = new_begin(p, 0);
		    ;}
    break;

  case 197:

/* Line 1455 of yacc.c  */
#line 1574 "./parse.y"
    {
		      (yyval.node) = new_dot2(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 198:

/* Line 1455 of yacc.c  */
#line 1578 "./parse.y"
    {
		      (yyval.node) = new_dot3(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 199:

/* Line 1455 of yacc.c  */
#line 1582 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "+", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 200:

/* Line 1455 of yacc.c  */
#line 1586 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "-", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 201:

/* Line 1455 of yacc.c  */
#line 1590 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "*", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 202:

/* Line 1455 of yacc.c  */
#line 1594 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "/", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 203:

/* Line 1455 of yacc.c  */
#line 1598 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "%", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 204:

/* Line 1455 of yacc.c  */
#line 1602 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "**", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 205:

/* Line 1455 of yacc.c  */
#line 1606 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].node), "**", (yyvsp[(4) - (4)].node)), "-@");
		    ;}
    break;

  case 206:

/* Line 1455 of yacc.c  */
#line 1610 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].node), "**", (yyvsp[(4) - (4)].node)), "-@");
		    ;}
    break;

  case 207:

/* Line 1455 of yacc.c  */
#line 1614 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, (yyvsp[(2) - (2)].node), "+@");
		    ;}
    break;

  case 208:

/* Line 1455 of yacc.c  */
#line 1618 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, (yyvsp[(2) - (2)].node), "-@");
		    ;}
    break;

  case 209:

/* Line 1455 of yacc.c  */
#line 1622 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "|", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 210:

/* Line 1455 of yacc.c  */
#line 1626 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "^", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 211:

/* Line 1455 of yacc.c  */
#line 1630 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "&", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 212:

/* Line 1455 of yacc.c  */
#line 1634 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "<=>", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 213:

/* Line 1455 of yacc.c  */
#line 1638 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), ">", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 214:

/* Line 1455 of yacc.c  */
#line 1642 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), ">=", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 215:

/* Line 1455 of yacc.c  */
#line 1646 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "<", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 216:

/* Line 1455 of yacc.c  */
#line 1650 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "<=", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 217:

/* Line 1455 of yacc.c  */
#line 1654 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "==", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 218:

/* Line 1455 of yacc.c  */
#line 1658 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "===", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 219:

/* Line 1455 of yacc.c  */
#line 1662 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "!=", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 220:

/* Line 1455 of yacc.c  */
#line 1666 "./parse.y"
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
#line 1675 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "!~", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 222:

/* Line 1455 of yacc.c  */
#line 1679 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(2) - (2)].node)), "!");
		    ;}
    break;

  case 223:

/* Line 1455 of yacc.c  */
#line 1683 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(2) - (2)].node)), "~");
		    ;}
    break;

  case 224:

/* Line 1455 of yacc.c  */
#line 1687 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), "<<", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 225:

/* Line 1455 of yacc.c  */
#line 1691 "./parse.y"
    {
		      (yyval.node) = call_bin_op(p, (yyvsp[(1) - (3)].node), ">>", (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 226:

/* Line 1455 of yacc.c  */
#line 1695 "./parse.y"
    {
		      (yyval.node) = new_and(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 227:

/* Line 1455 of yacc.c  */
#line 1699 "./parse.y"
    {
		      (yyval.node) = new_or(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 228:

/* Line 1455 of yacc.c  */
#line 1703 "./parse.y"
    {
		      (yyval.node) = new_if(p, cond((yyvsp[(1) - (6)].node)), (yyvsp[(3) - (6)].node), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 229:

/* Line 1455 of yacc.c  */
#line 1707 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 230:

/* Line 1455 of yacc.c  */
#line 1713 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		      if (!(yyval.node)) (yyval.node) = new_nil(p);
		    ;}
    break;

  case 232:

/* Line 1455 of yacc.c  */
#line 1721 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 233:

/* Line 1455 of yacc.c  */
#line 1725 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (4)].node), new_hash(p, (yyvsp[(3) - (4)].node)));
		    ;}
    break;

  case 234:

/* Line 1455 of yacc.c  */
#line 1729 "./parse.y"
    {
		      (yyval.node) = new_hash(p, (yyvsp[(1) - (2)].node));
		    ;}
    break;

  case 235:

/* Line 1455 of yacc.c  */
#line 1735 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 240:

/* Line 1455 of yacc.c  */
#line 1747 "./parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (2)].node),0);
		    ;}
    break;

  case 241:

/* Line 1455 of yacc.c  */
#line 1751 "./parse.y"
    {
		      (yyval.node) = cons(push((yyvsp[(1) - (4)].node), new_hash(p, (yyvsp[(3) - (4)].node))), 0);
		    ;}
    break;

  case 242:

/* Line 1455 of yacc.c  */
#line 1755 "./parse.y"
    {
		      (yyval.node) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].node))), 0);
		    ;}
    break;

  case 243:

/* Line 1455 of yacc.c  */
#line 1761 "./parse.y"
    {
		      (yyval.node) = cons(list1((yyvsp[(1) - (1)].node)), 0);
		    ;}
    break;

  case 244:

/* Line 1455 of yacc.c  */
#line 1765 "./parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 245:

/* Line 1455 of yacc.c  */
#line 1769 "./parse.y"
    {
		      (yyval.node) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].node))), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 246:

/* Line 1455 of yacc.c  */
#line 1773 "./parse.y"
    {
		      (yyval.node) = cons(push((yyvsp[(1) - (4)].node), new_hash(p, (yyvsp[(3) - (4)].node))), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 247:

/* Line 1455 of yacc.c  */
#line 1777 "./parse.y"
    {
		      (yyval.node) = cons(0, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 248:

/* Line 1455 of yacc.c  */
#line 1782 "./parse.y"
    {
		      (yyval.stack) = p->cmdarg_stack;
		      CMDARG_PUSH(1);
		    ;}
    break;

  case 249:

/* Line 1455 of yacc.c  */
#line 1787 "./parse.y"
    {
		      p->cmdarg_stack = (yyvsp[(1) - (2)].stack);
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 250:

/* Line 1455 of yacc.c  */
#line 1794 "./parse.y"
    {
		      (yyval.node) = new_block_arg(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 251:

/* Line 1455 of yacc.c  */
#line 1800 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 252:

/* Line 1455 of yacc.c  */
#line 1804 "./parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;

  case 253:

/* Line 1455 of yacc.c  */
#line 1810 "./parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (1)].node), 0);
		    ;}
    break;

  case 254:

/* Line 1455 of yacc.c  */
#line 1814 "./parse.y"
    {
		      (yyval.node) = cons(new_splat(p, (yyvsp[(2) - (2)].node)), 0);
		    ;}
    break;

  case 255:

/* Line 1455 of yacc.c  */
#line 1818 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 256:

/* Line 1455 of yacc.c  */
#line 1822 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (4)].node), new_splat(p, (yyvsp[(4) - (4)].node)));
		    ;}
    break;

  case 257:

/* Line 1455 of yacc.c  */
#line 1828 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 258:

/* Line 1455 of yacc.c  */
#line 1832 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (4)].node), new_splat(p, (yyvsp[(4) - (4)].node)));
		    ;}
    break;

  case 259:

/* Line 1455 of yacc.c  */
#line 1836 "./parse.y"
    {
		      (yyval.node) = list1(new_splat(p, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 265:

/* Line 1455 of yacc.c  */
#line 1847 "./parse.y"
    {
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (1)].id), 0);
		    ;}
    break;

  case 266:

/* Line 1455 of yacc.c  */
#line 1851 "./parse.y"
    {
		      (yyvsp[(1) - (1)].stack) = p->cmdarg_stack;
		      p->cmdarg_stack = 0;
		    ;}
    break;

  case 267:

/* Line 1455 of yacc.c  */
#line 1857 "./parse.y"
    {
		      p->cmdarg_stack = (yyvsp[(1) - (4)].stack);
		      (yyval.node) = (yyvsp[(3) - (4)].node);
		    ;}
    break;

  case 268:

/* Line 1455 of yacc.c  */
#line 1861 "./parse.y"
    {p->lstate = EXPR_ENDARG;;}
    break;

  case 269:

/* Line 1455 of yacc.c  */
#line 1862 "./parse.y"
    {
		      yywarning(p, "(...) interpreted as grouped expression");
		      (yyval.node) = (yyvsp[(2) - (4)].node);
		    ;}
    break;

  case 270:

/* Line 1455 of yacc.c  */
#line 1867 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 271:

/* Line 1455 of yacc.c  */
#line 1871 "./parse.y"
    {
		      (yyval.node) = new_colon2(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 272:

/* Line 1455 of yacc.c  */
#line 1875 "./parse.y"
    {
		      (yyval.node) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 273:

/* Line 1455 of yacc.c  */
#line 1879 "./parse.y"
    {
		      (yyval.node) = new_array(p, (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 274:

/* Line 1455 of yacc.c  */
#line 1883 "./parse.y"
    {
		      (yyval.node) = new_hash(p, (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 275:

/* Line 1455 of yacc.c  */
#line 1887 "./parse.y"
    {
		      (yyval.node) = new_return(p, 0);
		    ;}
    break;

  case 276:

/* Line 1455 of yacc.c  */
#line 1891 "./parse.y"
    {
		      (yyval.node) = new_yield(p, (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 277:

/* Line 1455 of yacc.c  */
#line 1895 "./parse.y"
    {
		      (yyval.node) = new_yield(p, 0);
		    ;}
    break;

  case 278:

/* Line 1455 of yacc.c  */
#line 1899 "./parse.y"
    {
		      (yyval.node) = new_yield(p, 0);
		    ;}
    break;

  case 279:

/* Line 1455 of yacc.c  */
#line 1903 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, cond((yyvsp[(3) - (4)].node)), "!");
		    ;}
    break;

  case 280:

/* Line 1455 of yacc.c  */
#line 1907 "./parse.y"
    {
		      (yyval.node) = call_uni_op(p, new_nil(p), "!");
		    ;}
    break;

  case 281:

/* Line 1455 of yacc.c  */
#line 1911 "./parse.y"
    {
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (2)].id), cons(0, (yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 283:

/* Line 1455 of yacc.c  */
#line 1916 "./parse.y"
    {
		      call_with_block(p, (yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 284:

/* Line 1455 of yacc.c  */
#line 1921 "./parse.y"
    {
		      local_nest(p);
		      (yyval.num) = p->lpar_beg;
		      p->lpar_beg = ++p->paren_nest;
		    ;}
    break;

  case 285:

/* Line 1455 of yacc.c  */
#line 1928 "./parse.y"
    {
		      p->lpar_beg = (yyvsp[(2) - (4)].num);
		      (yyval.node) = new_lambda(p, (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].node));
		      local_unnest(p);
		    ;}
    break;

  case 286:

/* Line 1455 of yacc.c  */
#line 1937 "./parse.y"
    {
		      (yyval.node) = new_if(p, cond((yyvsp[(2) - (6)].node)), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		    ;}
    break;

  case 287:

/* Line 1455 of yacc.c  */
#line 1944 "./parse.y"
    {
		      (yyval.node) = new_unless(p, cond((yyvsp[(2) - (6)].node)), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		    ;}
    break;

  case 288:

/* Line 1455 of yacc.c  */
#line 1947 "./parse.y"
    {COND_PUSH(1);;}
    break;

  case 289:

/* Line 1455 of yacc.c  */
#line 1947 "./parse.y"
    {COND_POP();;}
    break;

  case 290:

/* Line 1455 of yacc.c  */
#line 1950 "./parse.y"
    {
		      (yyval.node) = new_while(p, cond((yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node));
		    ;}
    break;

  case 291:

/* Line 1455 of yacc.c  */
#line 1953 "./parse.y"
    {COND_PUSH(1);;}
    break;

  case 292:

/* Line 1455 of yacc.c  */
#line 1953 "./parse.y"
    {COND_POP();;}
    break;

  case 293:

/* Line 1455 of yacc.c  */
#line 1956 "./parse.y"
    {
		      (yyval.node) = new_until(p, cond((yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node));
		    ;}
    break;

  case 294:

/* Line 1455 of yacc.c  */
#line 1962 "./parse.y"
    {
		      (yyval.node) = new_case(p, (yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node));
		    ;}
    break;

  case 295:

/* Line 1455 of yacc.c  */
#line 1966 "./parse.y"
    {
		      (yyval.node) = new_case(p, 0, (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 296:

/* Line 1455 of yacc.c  */
#line 1970 "./parse.y"
    {COND_PUSH(1);;}
    break;

  case 297:

/* Line 1455 of yacc.c  */
#line 1972 "./parse.y"
    {COND_POP();;}
    break;

  case 298:

/* Line 1455 of yacc.c  */
#line 1975 "./parse.y"
    {
		      (yyval.node) = new_for(p, (yyvsp[(2) - (9)].node), (yyvsp[(5) - (9)].node), (yyvsp[(8) - (9)].node));
		    ;}
    break;

  case 299:

/* Line 1455 of yacc.c  */
#line 1979 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "class definition in method body");
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 300:

/* Line 1455 of yacc.c  */
#line 1986 "./parse.y"
    {
		      (yyval.node) = new_class(p, (yyvsp[(2) - (6)].node), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].node));
		      local_resume(p, (yyvsp[(4) - (6)].node));
		    ;}
    break;

  case 301:

/* Line 1455 of yacc.c  */
#line 1991 "./parse.y"
    {
		      (yyval.num) = p->in_def;
		      p->in_def = 0;
		    ;}
    break;

  case 302:

/* Line 1455 of yacc.c  */
#line 1996 "./parse.y"
    {
		      (yyval.node) = cons(local_switch(p), (node*)(intptr_t)p->in_single);
		      p->in_single = 0;
		    ;}
    break;

  case 303:

/* Line 1455 of yacc.c  */
#line 2002 "./parse.y"
    {
		      (yyval.node) = new_sclass(p, (yyvsp[(3) - (8)].node), (yyvsp[(7) - (8)].node));
		      local_resume(p, (yyvsp[(6) - (8)].node)->car);
		      p->in_def = (yyvsp[(4) - (8)].num);
		      p->in_single = (int)(intptr_t)(yyvsp[(6) - (8)].node)->cdr;
		    ;}
    break;

  case 304:

/* Line 1455 of yacc.c  */
#line 2009 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "module definition in method body");
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 305:

/* Line 1455 of yacc.c  */
#line 2016 "./parse.y"
    {
		      (yyval.node) = new_module(p, (yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node));
		      local_resume(p, (yyvsp[(3) - (5)].node));
		    ;}
    break;

  case 306:

/* Line 1455 of yacc.c  */
#line 2021 "./parse.y"
    {
		      p->in_def++;
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 307:

/* Line 1455 of yacc.c  */
#line 2028 "./parse.y"
    {
		      (yyval.node) = new_def(p, (yyvsp[(2) - (6)].id), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		      local_resume(p, (yyvsp[(3) - (6)].node));
		      p->in_def--;
		    ;}
    break;

  case 308:

/* Line 1455 of yacc.c  */
#line 2033 "./parse.y"
    {p->lstate = EXPR_FNAME;;}
    break;

  case 309:

/* Line 1455 of yacc.c  */
#line 2034 "./parse.y"
    {
		      p->in_single++;
		      p->lstate = EXPR_ENDFN; /* force for args */
		      (yyval.node) = local_switch(p);
		    ;}
    break;

  case 310:

/* Line 1455 of yacc.c  */
#line 2042 "./parse.y"
    {
		      (yyval.node) = new_sdef(p, (yyvsp[(2) - (9)].node), (yyvsp[(5) - (9)].id), (yyvsp[(7) - (9)].node), (yyvsp[(8) - (9)].node));
		      local_resume(p, (yyvsp[(6) - (9)].node));
		      p->in_single--;
		    ;}
    break;

  case 311:

/* Line 1455 of yacc.c  */
#line 2048 "./parse.y"
    {
		      (yyval.node) = new_break(p, 0);
		    ;}
    break;

  case 312:

/* Line 1455 of yacc.c  */
#line 2052 "./parse.y"
    {
		      (yyval.node) = new_next(p, 0);
		    ;}
    break;

  case 313:

/* Line 1455 of yacc.c  */
#line 2056 "./parse.y"
    {
		      (yyval.node) = new_redo(p);
		    ;}
    break;

  case 314:

/* Line 1455 of yacc.c  */
#line 2060 "./parse.y"
    {
		      (yyval.node) = new_retry(p);
		    ;}
    break;

  case 315:

/* Line 1455 of yacc.c  */
#line 2066 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		      if (!(yyval.node)) (yyval.node) = new_nil(p);
		    ;}
    break;

  case 322:

/* Line 1455 of yacc.c  */
#line 2085 "./parse.y"
    {
		      (yyval.node) = new_if(p, cond((yyvsp[(2) - (5)].node)), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 324:

/* Line 1455 of yacc.c  */
#line 2092 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 325:

/* Line 1455 of yacc.c  */
#line 2098 "./parse.y"
    {
		      (yyval.node) = list1(list1((yyvsp[(1) - (1)].node)));
		    ;}
    break;

  case 327:

/* Line 1455 of yacc.c  */
#line 2105 "./parse.y"
    {
		      (yyval.node) = new_arg(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 328:

/* Line 1455 of yacc.c  */
#line 2109 "./parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(2) - (3)].node), 0);
		    ;}
    break;

  case 329:

/* Line 1455 of yacc.c  */
#line 2115 "./parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 330:

/* Line 1455 of yacc.c  */
#line 2119 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 331:

/* Line 1455 of yacc.c  */
#line 2125 "./parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (1)].node),0,0);
		    ;}
    break;

  case 332:

/* Line 1455 of yacc.c  */
#line 2129 "./parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (4)].node), new_arg(p, (yyvsp[(4) - (4)].id)), 0);
		    ;}
    break;

  case 333:

/* Line 1455 of yacc.c  */
#line 2133 "./parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (6)].node), new_arg(p, (yyvsp[(4) - (6)].id)), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 334:

/* Line 1455 of yacc.c  */
#line 2137 "./parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (3)].node), (node*)-1, 0);
		    ;}
    break;

  case 335:

/* Line 1455 of yacc.c  */
#line 2141 "./parse.y"
    {
		      (yyval.node) = list3((yyvsp[(1) - (5)].node), (node*)-1, (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 336:

/* Line 1455 of yacc.c  */
#line 2145 "./parse.y"
    {
		      (yyval.node) = list3(0, new_arg(p, (yyvsp[(2) - (2)].id)), 0);
		    ;}
    break;

  case 337:

/* Line 1455 of yacc.c  */
#line 2149 "./parse.y"
    {
		      (yyval.node) = list3(0, new_arg(p, (yyvsp[(2) - (4)].id)), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 338:

/* Line 1455 of yacc.c  */
#line 2153 "./parse.y"
    {
		      (yyval.node) = list3(0, (node*)-1, 0);
		    ;}
    break;

  case 339:

/* Line 1455 of yacc.c  */
#line 2157 "./parse.y"
    {
		      (yyval.node) = list3(0, (node*)-1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 340:

/* Line 1455 of yacc.c  */
#line 2163 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 341:

/* Line 1455 of yacc.c  */
#line 2167 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (8)].node), (yyvsp[(3) - (8)].node), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].node), (yyvsp[(8) - (8)].id));
		    ;}
    break;

  case 342:

/* Line 1455 of yacc.c  */
#line 2171 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node), 0, 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 343:

/* Line 1455 of yacc.c  */
#line 2175 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].node), 0, (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 344:

/* Line 1455 of yacc.c  */
#line 2179 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (4)].node), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 345:

/* Line 1455 of yacc.c  */
#line 2183 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (2)].node), 0, 1, 0, 0);
		    ;}
    break;

  case 346:

/* Line 1455 of yacc.c  */
#line 2187 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 347:

/* Line 1455 of yacc.c  */
#line 2191 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (2)].node), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 348:

/* Line 1455 of yacc.c  */
#line 2195 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 349:

/* Line 1455 of yacc.c  */
#line 2199 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 350:

/* Line 1455 of yacc.c  */
#line 2203 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (2)].node), 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 351:

/* Line 1455 of yacc.c  */
#line 2207 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (4)].node), 0, (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 352:

/* Line 1455 of yacc.c  */
#line 2211 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 353:

/* Line 1455 of yacc.c  */
#line 2215 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 354:

/* Line 1455 of yacc.c  */
#line 2219 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 356:

/* Line 1455 of yacc.c  */
#line 2226 "./parse.y"
    {
		      p->cmd_start = TRUE;
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 357:

/* Line 1455 of yacc.c  */
#line 2233 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.node) = 0;
		    ;}
    break;

  case 358:

/* Line 1455 of yacc.c  */
#line 2238 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.node) = 0;
		    ;}
    break;

  case 359:

/* Line 1455 of yacc.c  */
#line 2243 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (4)].node);
		    ;}
    break;

  case 360:

/* Line 1455 of yacc.c  */
#line 2250 "./parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;

  case 361:

/* Line 1455 of yacc.c  */
#line 2254 "./parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;

  case 364:

/* Line 1455 of yacc.c  */
#line 2264 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      new_bv(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 366:

/* Line 1455 of yacc.c  */
#line 2272 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (4)].node);
		    ;}
    break;

  case 367:

/* Line 1455 of yacc.c  */
#line 2276 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 368:

/* Line 1455 of yacc.c  */
#line 2282 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 369:

/* Line 1455 of yacc.c  */
#line 2286 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 370:

/* Line 1455 of yacc.c  */
#line 2292 "./parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 371:

/* Line 1455 of yacc.c  */
#line 2298 "./parse.y"
    {
		      (yyval.node) = new_block(p,(yyvsp[(3) - (5)].node),(yyvsp[(4) - (5)].node));
		      local_unnest(p);
		    ;}
    break;

  case 372:

/* Line 1455 of yacc.c  */
#line 2305 "./parse.y"
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

  case 373:

/* Line 1455 of yacc.c  */
#line 2315 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 374:

/* Line 1455 of yacc.c  */
#line 2319 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
		      call_with_block(p, (yyval.node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 375:

/* Line 1455 of yacc.c  */
#line 2324 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
		      call_with_block(p, (yyval.node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 376:

/* Line 1455 of yacc.c  */
#line 2331 "./parse.y"
    {
		      (yyval.node) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 377:

/* Line 1455 of yacc.c  */
#line 2335 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 378:

/* Line 1455 of yacc.c  */
#line 2339 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 379:

/* Line 1455 of yacc.c  */
#line 2343 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 380:

/* Line 1455 of yacc.c  */
#line 2347 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), intern("call"), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 381:

/* Line 1455 of yacc.c  */
#line 2351 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (3)].node), intern("call"), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 382:

/* Line 1455 of yacc.c  */
#line 2355 "./parse.y"
    {
		      (yyval.node) = new_super(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 383:

/* Line 1455 of yacc.c  */
#line 2359 "./parse.y"
    {
		      (yyval.node) = new_zsuper(p);
		    ;}
    break;

  case 384:

/* Line 1455 of yacc.c  */
#line 2363 "./parse.y"
    {
		      (yyval.node) = new_call(p, (yyvsp[(1) - (4)].node), intern("[]"), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 385:

/* Line 1455 of yacc.c  */
#line 2369 "./parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 386:

/* Line 1455 of yacc.c  */
#line 2374 "./parse.y"
    {
		      (yyval.node) = new_block(p,(yyvsp[(3) - (5)].node),(yyvsp[(4) - (5)].node));
		      local_unnest(p);
		    ;}
    break;

  case 387:

/* Line 1455 of yacc.c  */
#line 2379 "./parse.y"
    {
		      local_nest(p);
		    ;}
    break;

  case 388:

/* Line 1455 of yacc.c  */
#line 2384 "./parse.y"
    {
		      (yyval.node) = new_block(p,(yyvsp[(3) - (5)].node),(yyvsp[(4) - (5)].node));
		      local_unnest(p);
		    ;}
    break;

  case 389:

/* Line 1455 of yacc.c  */
#line 2393 "./parse.y"
    {
		      (yyval.node) = cons(cons((yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node)), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 390:

/* Line 1455 of yacc.c  */
#line 2399 "./parse.y"
    {
		      if ((yyvsp[(1) - (1)].node)) {
			(yyval.node) = cons(cons(0, (yyvsp[(1) - (1)].node)), 0);
		      }
		      else {
			(yyval.node) = 0;
		      }
		    ;}
    break;

  case 392:

/* Line 1455 of yacc.c  */
#line 2413 "./parse.y"
    {
		      (yyval.node) = list1(list3((yyvsp[(2) - (6)].node), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].node)));
		      if ((yyvsp[(6) - (6)].node)) (yyval.node) = append((yyval.node), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 394:

/* Line 1455 of yacc.c  */
#line 2421 "./parse.y"
    {
			(yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 397:

/* Line 1455 of yacc.c  */
#line 2429 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 399:

/* Line 1455 of yacc.c  */
#line 2436 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 402:

/* Line 1455 of yacc.c  */
#line 2444 "./parse.y"
    {
		      (yyval.node) = new_sym(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 405:

/* Line 1455 of yacc.c  */
#line 2452 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 406:

/* Line 1455 of yacc.c  */
#line 2456 "./parse.y"
    {
		      (yyval.node) = new_dstr(p, push((yyvsp[(2) - (3)].node), (yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 407:

/* Line 1455 of yacc.c  */
#line 2462 "./parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    ;}
    break;

  case 408:

/* Line 1455 of yacc.c  */
#line 2468 "./parse.y"
    {
		      p->sterm = (yyvsp[(2) - (4)].num);
		      (yyval.node) = list2((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 409:

/* Line 1455 of yacc.c  */
#line 2474 "./parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    ;}
    break;

  case 410:

/* Line 1455 of yacc.c  */
#line 2480 "./parse.y"
    {
		      p->sterm = (yyvsp[(3) - (5)].num);
		      (yyval.node) = push(push((yyvsp[(1) - (5)].node), (yyvsp[(2) - (5)].node)), (yyvsp[(4) - (5)].node));
		    ;}
    break;

  case 412:

/* Line 1455 of yacc.c  */
#line 2490 "./parse.y"
    {
		      p->lstate = EXPR_END;
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 419:

/* Line 1455 of yacc.c  */
#line 2505 "./parse.y"
    {
		      (yyval.node) = negate_lit(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 420:

/* Line 1455 of yacc.c  */
#line 2509 "./parse.y"
    {
		      (yyval.node) = negate_lit(p, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 421:

/* Line 1455 of yacc.c  */
#line 2515 "./parse.y"
    {
		      (yyval.node) = new_lvar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 422:

/* Line 1455 of yacc.c  */
#line 2519 "./parse.y"
    {
		      (yyval.node) = new_ivar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 423:

/* Line 1455 of yacc.c  */
#line 2523 "./parse.y"
    {
		      (yyval.node) = new_gvar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 424:

/* Line 1455 of yacc.c  */
#line 2527 "./parse.y"
    {
		      (yyval.node) = new_cvar(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 425:

/* Line 1455 of yacc.c  */
#line 2531 "./parse.y"
    {
		      (yyval.node) = new_const(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 426:

/* Line 1455 of yacc.c  */
#line 2537 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 427:

/* Line 1455 of yacc.c  */
#line 2543 "./parse.y"
    {
		      (yyval.node) = var_reference(p, (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 428:

/* Line 1455 of yacc.c  */
#line 2547 "./parse.y"
    {
		      (yyval.node) = new_nil(p);
		    ;}
    break;

  case 429:

/* Line 1455 of yacc.c  */
#line 2551 "./parse.y"
    {
		      (yyval.node) = new_self(p);
   		    ;}
    break;

  case 430:

/* Line 1455 of yacc.c  */
#line 2555 "./parse.y"
    {
		      (yyval.node) = new_true(p);
   		    ;}
    break;

  case 431:

/* Line 1455 of yacc.c  */
#line 2559 "./parse.y"
    {
		      (yyval.node) = new_false(p);
   		    ;}
    break;

  case 432:

/* Line 1455 of yacc.c  */
#line 2563 "./parse.y"
    {
		      if (!p->filename) {
			p->filename = "(null)";
		      }
		      (yyval.node) = new_str(p, p->filename, strlen(p->filename));
		    ;}
    break;

  case 433:

/* Line 1455 of yacc.c  */
#line 2570 "./parse.y"
    {
		      char buf[16];

		      snprintf(buf, 16, "%d", p->lineno);
		      (yyval.node) = new_int(p, buf, 10);
		    ;}
    break;

  case 436:

/* Line 1455 of yacc.c  */
#line 2583 "./parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;

  case 437:

/* Line 1455 of yacc.c  */
#line 2587 "./parse.y"
    {
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    ;}
    break;

  case 438:

/* Line 1455 of yacc.c  */
#line 2592 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(3) - (4)].node);
		    ;}
    break;

  case 439:

/* Line 1455 of yacc.c  */
#line 2596 "./parse.y"
    {
		      yyerrok;
		      (yyval.node) = 0;
		    ;}
    break;

  case 440:

/* Line 1455 of yacc.c  */
#line 2603 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(2) - (3)].node);
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    ;}
    break;

  case 441:

/* Line 1455 of yacc.c  */
#line 2609 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 442:

/* Line 1455 of yacc.c  */
#line 2615 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 443:

/* Line 1455 of yacc.c  */
#line 2619 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (8)].node), (yyvsp[(3) - (8)].node), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].node), (yyvsp[(8) - (8)].id));
		    ;}
    break;

  case 444:

/* Line 1455 of yacc.c  */
#line 2623 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node), 0, 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 445:

/* Line 1455 of yacc.c  */
#line 2627 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].node), 0, (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 446:

/* Line 1455 of yacc.c  */
#line 2631 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (4)].node), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 447:

/* Line 1455 of yacc.c  */
#line 2635 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (6)].node), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 448:

/* Line 1455 of yacc.c  */
#line 2639 "./parse.y"
    {
		      (yyval.node) = new_args(p, (yyvsp[(1) - (2)].node), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 449:

/* Line 1455 of yacc.c  */
#line 2643 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 450:

/* Line 1455 of yacc.c  */
#line 2647 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (6)].node), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].id));
		    ;}
    break;

  case 451:

/* Line 1455 of yacc.c  */
#line 2651 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (2)].node), 0, 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 452:

/* Line 1455 of yacc.c  */
#line 2655 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, (yyvsp[(1) - (4)].node), 0, (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 453:

/* Line 1455 of yacc.c  */
#line 2659 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 454:

/* Line 1455 of yacc.c  */
#line 2663 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].node), (yyvsp[(4) - (4)].id));
		    ;}
    break;

  case 455:

/* Line 1455 of yacc.c  */
#line 2667 "./parse.y"
    {
		      (yyval.node) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 456:

/* Line 1455 of yacc.c  */
#line 2671 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.node) = new_args(p, 0, 0, 0, 0, 0);
		    ;}
    break;

  case 457:

/* Line 1455 of yacc.c  */
#line 2678 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a constant");
		      (yyval.node) = 0;
		    ;}
    break;

  case 458:

/* Line 1455 of yacc.c  */
#line 2683 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be an instance variable");
		      (yyval.node) = 0;
		    ;}
    break;

  case 459:

/* Line 1455 of yacc.c  */
#line 2688 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a global variable");
		      (yyval.node) = 0;
		    ;}
    break;

  case 460:

/* Line 1455 of yacc.c  */
#line 2693 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a class variable");
		      (yyval.node) = 0;
		    ;}
    break;

  case 461:

/* Line 1455 of yacc.c  */
#line 2700 "./parse.y"
    {
		      (yyval.id) = 0;
		    ;}
    break;

  case 462:

/* Line 1455 of yacc.c  */
#line 2704 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 463:

/* Line 1455 of yacc.c  */
#line 2711 "./parse.y"
    {
		      (yyval.node) = new_arg(p, (yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 464:

/* Line 1455 of yacc.c  */
#line 2715 "./parse.y"
    {
		      (yyval.node) = new_masgn(p, (yyvsp[(2) - (3)].node), 0);
		    ;}
    break;

  case 465:

/* Line 1455 of yacc.c  */
#line 2721 "./parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 466:

/* Line 1455 of yacc.c  */
#line 2725 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 467:

/* Line 1455 of yacc.c  */
#line 2731 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.node) = cons((node*)(yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 468:

/* Line 1455 of yacc.c  */
#line 2738 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.node) = cons((node*)(yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 469:

/* Line 1455 of yacc.c  */
#line 2745 "./parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 470:

/* Line 1455 of yacc.c  */
#line 2749 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 471:

/* Line 1455 of yacc.c  */
#line 2755 "./parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 472:

/* Line 1455 of yacc.c  */
#line 2759 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 475:

/* Line 1455 of yacc.c  */
#line 2769 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 476:

/* Line 1455 of yacc.c  */
#line 2774 "./parse.y"
    {
		      (yyval.id) = 0;
		    ;}
    break;

  case 479:

/* Line 1455 of yacc.c  */
#line 2784 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 480:

/* Line 1455 of yacc.c  */
#line 2791 "./parse.y"
    {
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 481:

/* Line 1455 of yacc.c  */
#line 2795 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.id) = 0;
		    ;}
    break;

  case 482:

/* Line 1455 of yacc.c  */
#line 2802 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (1)].node);
		      if (!(yyval.node)) (yyval.node) = new_nil(p);
		    ;}
    break;

  case 483:

/* Line 1455 of yacc.c  */
#line 2806 "./parse.y"
    {p->lstate = EXPR_BEG;;}
    break;

  case 484:

/* Line 1455 of yacc.c  */
#line 2807 "./parse.y"
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

  case 486:

/* Line 1455 of yacc.c  */
#line 2830 "./parse.y"
    {
		      (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 487:

/* Line 1455 of yacc.c  */
#line 2836 "./parse.y"
    {
		      (yyval.node) = list1((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 488:

/* Line 1455 of yacc.c  */
#line 2840 "./parse.y"
    {
		      (yyval.node) = push((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 489:

/* Line 1455 of yacc.c  */
#line 2846 "./parse.y"
    {
		      (yyval.node) = cons((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 490:

/* Line 1455 of yacc.c  */
#line 2850 "./parse.y"
    {
		      (yyval.node) = cons(new_sym(p, (yyvsp[(1) - (2)].id)), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 512:

/* Line 1455 of yacc.c  */
#line 2894 "./parse.y"
    {yyerrok;;}
    break;

  case 515:

/* Line 1455 of yacc.c  */
#line 2899 "./parse.y"
    {yyerrok;;}
    break;

  case 516:

/* Line 1455 of yacc.c  */
#line 2903 "./parse.y"
    {
		      (yyval.node) = 0;
		    ;}
    break;



/* Line 1455 of yacc.c  */
#line 8566 "./y.tab.c"
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
#line 2907 "./parse.y"

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

  p->capture_errors = 0;

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
mrb_parse_nstring(mrb_state *mrb, const char *s, size_t len)
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
mrb_parse_nstring_ext(mrb_state *mrb, const char *s, size_t len)
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
mrb_parse_string(mrb_state *mrb, const char *s)
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

