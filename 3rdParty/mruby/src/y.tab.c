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
#include "mruby/proc.h"
#include "node.h"

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

#define BITSTACK_PUSH(stack, n) ((stack) = ((stack)<<1)|((n)&1))
#define BITSTACK_POP(stack)     ((stack) = (stack) >> 1)
#define BITSTACK_LEXPOP(stack)  ((stack) = ((stack) >> 1) | ((stack) & 1))
#define BITSTACK_SET_P(stack)   ((stack)&1)

#define COND_PUSH(n)    BITSTACK_PUSH(p->cond_stack, (n))
#define COND_POP()      BITSTACK_POP(p->cond_stack)
#define COND_LEXPOP()   BITSTACK_LEXPOP(p->cond_stack)
#define COND_P()        BITSTACK_SET_P(p->cond_stack)

#define CMDARG_PUSH(n)  BITSTACK_PUSH(p->cmdarg_stack, (n))
#define CMDARG_POP()    BITSTACK_POP(p->cmdarg_stack)
#define CMDARG_LEXPOP() BITSTACK_LEXPOP(p->cmdarg_stack)
#define CMDARG_P()      BITSTACK_SET_P(p->cmdarg_stack)

#define sym(x) ((mrb_sym)(intptr_t)(x))
#define nsym(x) ((node*)(intptr_t)(x))

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
    c = (node *)parser_palloc(p, sizeof(mrb_ast_node));
  }

  c->car = car;
  c->cdr = cdr;
  c->lineno = p->lineno;
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
  char *b = (char *)parser_palloc(p, len+1);

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
      if (sym(n->car) == sym) return 1;
      n = n->cdr;
    }
    l = l->cdr;
  }
  return 0;
}

static void
local_add_f(parser_state *p, mrb_sym sym)
{
  p->locals->car = push(p->locals->car, nsym(sym));
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
  return cons((node*)NODE_ALIAS, cons(nsym(a), nsym(b)));
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
  return list4((node*)NODE_CALL, a, nsym(b), c);
}

// (:fcall self mid args)
static node*
new_fcall(parser_state *p, mrb_sym b, node *c)
{
  return list4((node*)NODE_FCALL, new_self(p), nsym(b), c);
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
  return cons((node*)NODE_COLON2, cons(b, nsym(c)));
}

// (:colon3 . c)
static node*
new_colon3(parser_state *p, mrb_sym c)
{
  return cons((node*)NODE_COLON3, nsym(c));
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
  return cons((node*)NODE_SYM, nsym(sym));
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
  return cons((node*)NODE_LVAR, nsym(sym));
}

// (:gvar . a)
static node*
new_gvar(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_GVAR, nsym(sym));
}

// (:ivar . a)
static node*
new_ivar(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_IVAR, nsym(sym));
}

// (:cvar . a)
static node*
new_cvar(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_CVAR, nsym(sym));
}

// (:const . a)
static node*
new_const(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_CONST, nsym(sym));
}

// (:undef a...)
static node*
new_undef(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_UNDEF, nsym(sym));
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
  return list5((node*)NODE_DEF, nsym(m), p->locals->car, a, b);
}

// (:sdef obj m lv (arg . body))
static node*
new_sdef(parser_state *p, node *o, mrb_sym m, node *a, node *b)
{
  return list6((node*)NODE_SDEF, o, nsym(m), p->locals->car, a, b);
}

// (:arg . sym)
static node*
new_arg(parser_state *p, mrb_sym sym)
{
  return cons((node*)NODE_ARG, nsym(sym));
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

  n = cons(m2, nsym(blk));
  n = cons(nsym(rest), n);
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
  return list4((node*)NODE_OP_ASGN, a, nsym(op), b);
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
new_str(parser_state *p, const char *s, int len)
{
  return cons((node*)NODE_STR, cons((node*)strndup(s, len), (node*)(intptr_t)len));
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
  if ((int)(intptr_t)lhs->car == NODE_LVAR) {
    local_add(p, sym(lhs->cdr));
  }
}

static node*
var_reference(parser_state *p, node *lhs)
{
  node *n;

  if ((int)(intptr_t)lhs->car == NODE_LVAR) {
    if (!local_var_p(p, sym(lhs->cdr))) {
      n = new_fcall(p, sym(lhs->cdr), 0);
      cons_free(lhs);
      return n;
    }
  }

  return lhs;
}

// xxx -----------------------------



/* Line 268 of yacc.c  */
#line 902 "./y.tab.c"

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

/* Line 293 of yacc.c  */
#line 841 "./parse.y"

    node *nd;
    mrb_sym id;
    int num;
    unsigned int stack;
    const struct vtable *vars;



/* Line 293 of yacc.c  */
#line 1066 "./y.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 1078 "./y.tab.c"

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
#define YYLAST   10263

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  144
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  147
/* YYNRULES -- Number of rules.  */
#define YYNRULES  523
/* YYNRULES -- Number of states.  */
#define YYNSTATES  927

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
    1641,  1643,  1645,  1648
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     145,     0,    -1,    -1,   146,   147,    -1,   148,   282,    -1,
     290,    -1,   149,    -1,   148,   289,   149,    -1,     1,   149,
      -1,   154,    -1,    -1,    46,   150,   134,   147,   135,    -1,
     152,   238,   216,   241,    -1,   153,   282,    -1,   290,    -1,
     154,    -1,   153,   289,   154,    -1,     1,   154,    -1,    -1,
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
     181,    -1,   290,    -1,   192,   286,    -1,   192,   138,   276,
     286,    -1,   276,   286,    -1,   139,   186,   284,    -1,   290,
      -1,   184,    -1,   290,    -1,   187,    -1,   192,   138,    -1,
     192,   138,   276,   138,    -1,   276,   138,    -1,   163,    -1,
     192,   191,    -1,   276,   191,    -1,   192,   138,   276,   191,
      -1,   190,    -1,    -1,   189,   187,    -1,    97,   182,    -1,
     138,   190,    -1,   290,    -1,   182,    -1,    96,   182,    -1,
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
      -1,    14,   158,   213,   152,   215,    -1,   290,    -1,    15,
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
     271,    -1,   290,    -1,   223,    -1,   112,   224,   112,    -1,
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
     238,    -1,   290,    -1,   182,    -1,   193,    -1,   290,    -1,
      89,   171,    -1,   290,    -1,     9,   152,    -1,   290,    -1,
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
      97,    -1,   270,    51,    -1,   138,   271,    -1,   290,    -1,
     254,    -1,    -1,   139,   274,   157,   284,    -1,   290,    -1,
     276,   286,    -1,   277,    -1,   276,   138,   277,    -1,   182,
      89,   182,    -1,    57,   182,    -1,    51,    -1,    55,    -1,
      52,    -1,    51,    -1,    55,    -1,    52,    -1,   179,    -1,
      51,    -1,    52,    -1,   179,    -1,   137,    -1,    86,    -1,
      -1,   289,    -1,    -1,   288,    -1,   283,   140,    -1,   283,
     141,    -1,    -1,   288,    -1,   138,    -1,   142,    -1,   288,
      -1,   143,    -1,   287,    -1,   289,   142,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   996,   996,   996,  1006,  1012,  1016,  1020,  1024,  1030,
    1032,  1031,  1043,  1069,  1075,  1079,  1083,  1087,  1093,  1093,
    1097,  1101,  1105,  1109,  1113,  1117,  1121,  1126,  1127,  1131,
    1135,  1139,  1143,  1147,  1152,  1156,  1161,  1165,  1169,  1173,
    1176,  1180,  1187,  1188,  1192,  1196,  1200,  1204,  1207,  1214,
    1215,  1218,  1219,  1223,  1222,  1235,  1239,  1244,  1248,  1253,
    1257,  1262,  1266,  1270,  1274,  1278,  1284,  1288,  1294,  1295,
    1301,  1305,  1309,  1313,  1317,  1321,  1325,  1329,  1333,  1337,
    1343,  1344,  1350,  1354,  1360,  1364,  1370,  1374,  1378,  1382,
    1386,  1390,  1396,  1402,  1409,  1413,  1417,  1421,  1425,  1429,
    1435,  1441,  1448,  1452,  1455,  1459,  1463,  1469,  1470,  1471,
    1472,  1477,  1484,  1485,  1488,  1494,  1498,  1498,  1504,  1505,
    1506,  1507,  1508,  1509,  1510,  1511,  1512,  1513,  1514,  1515,
    1516,  1517,  1518,  1519,  1520,  1521,  1522,  1523,  1524,  1525,
    1526,  1527,  1528,  1529,  1530,  1531,  1534,  1534,  1534,  1535,
    1535,  1536,  1536,  1536,  1537,  1537,  1537,  1537,  1538,  1538,
    1538,  1539,  1539,  1539,  1540,  1540,  1540,  1540,  1541,  1541,
    1541,  1541,  1542,  1542,  1542,  1542,  1543,  1543,  1543,  1543,
    1544,  1544,  1544,  1544,  1545,  1545,  1548,  1552,  1556,  1560,
    1564,  1568,  1572,  1576,  1580,  1585,  1590,  1595,  1599,  1603,
    1607,  1611,  1615,  1619,  1623,  1627,  1631,  1635,  1639,  1643,
    1647,  1651,  1655,  1659,  1663,  1667,  1671,  1675,  1679,  1683,
    1687,  1696,  1700,  1704,  1708,  1712,  1716,  1720,  1724,  1728,
    1734,  1741,  1742,  1746,  1750,  1756,  1762,  1763,  1766,  1767,
    1768,  1772,  1776,  1782,  1786,  1790,  1794,  1798,  1804,  1804,
    1815,  1821,  1825,  1831,  1835,  1839,  1843,  1849,  1853,  1857,
    1863,  1864,  1865,  1866,  1867,  1868,  1873,  1872,  1883,  1883,
    1887,  1887,  1891,  1895,  1899,  1903,  1907,  1911,  1915,  1919,
    1923,  1927,  1931,  1935,  1939,  1940,  1946,  1945,  1958,  1965,
    1972,  1972,  1972,  1978,  1978,  1978,  1984,  1990,  1995,  1997,
    1994,  2004,  2003,  2016,  2021,  2015,  2034,  2033,  2046,  2045,
    2058,  2059,  2058,  2072,  2076,  2080,  2084,  2090,  2097,  2098,
    2099,  2102,  2103,  2106,  2107,  2115,  2116,  2122,  2126,  2129,
    2133,  2139,  2143,  2149,  2153,  2157,  2161,  2165,  2169,  2173,
    2177,  2181,  2187,  2191,  2195,  2199,  2203,  2207,  2211,  2215,
    2219,  2223,  2227,  2231,  2235,  2239,  2243,  2249,  2250,  2257,
    2262,  2267,  2274,  2278,  2284,  2285,  2288,  2293,  2296,  2300,
    2306,  2310,  2317,  2316,  2329,  2339,  2343,  2348,  2355,  2359,
    2363,  2367,  2371,  2375,  2379,  2383,  2387,  2394,  2393,  2404,
    2403,  2415,  2423,  2432,  2435,  2442,  2445,  2449,  2450,  2453,
    2457,  2460,  2464,  2467,  2468,  2471,  2472,  2473,  2477,  2484,
    2483,  2496,  2494,  2508,  2511,  2515,  2522,  2529,  2530,  2531,
    2532,  2533,  2537,  2543,  2544,  2545,  2549,  2555,  2559,  2563,
    2567,  2571,  2577,  2583,  2587,  2591,  2595,  2599,  2603,  2610,
    2619,  2620,  2623,  2628,  2627,  2636,  2643,  2649,  2655,  2659,
    2663,  2667,  2671,  2675,  2679,  2683,  2687,  2691,  2695,  2699,
    2703,  2707,  2712,  2718,  2723,  2728,  2733,  2740,  2744,  2751,
    2755,  2761,  2765,  2771,  2778,  2785,  2789,  2795,  2799,  2805,
    2806,  2809,  2814,  2821,  2822,  2825,  2832,  2836,  2843,  2848,
    2848,  2870,  2871,  2877,  2881,  2887,  2891,  2897,  2898,  2899,
    2902,  2903,  2904,  2905,  2908,  2909,  2910,  2913,  2914,  2917,
    2918,  2921,  2922,  2925,  2928,  2931,  2932,  2933,  2936,  2937,
    2940,  2946,  2947,  2951
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
  "opt_nl", "rparen", "rbracket", "trailer", "term", "nl", "terms", "none", 0
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
     288,   289,   289,   290
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
       1,     1,     2,     0
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,     0,     0,     0,     0,     0,   266,
       0,     0,   509,   290,   293,     0,   313,   314,   315,   316,
     277,   280,   385,   435,   434,   436,   437,   511,     0,    10,
       0,   439,   438,   427,   265,   429,   428,   431,   430,   423,
     424,   405,   413,   406,   440,   441,     0,     0,     0,     0,
     270,   523,   523,    78,   286,     0,     0,     0,     0,     0,
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
       0,   264,     0,   518,   520,   509,     0,   521,   519,   510,
       0,     0,     0,   328,   327,     0,     0,   433,   264,     0,
       0,     0,     0,   243,   230,   253,    64,   247,   523,   523,
     493,    65,    63,   511,    62,     0,   523,   384,    61,   511,
       0,   512,    18,     0,     0,   207,     0,   208,   274,     0,
       0,     0,   509,    15,   511,    68,    14,   268,   511,     0,
     515,   515,   231,     0,     0,   515,   491,     0,     0,    76,
       0,    86,    93,   462,   419,   418,   420,   421,     0,   417,
     416,   407,   409,     0,   425,   426,    46,   222,   223,     4,
     510,     0,     0,     0,     0,     0,     0,     0,   372,   374,
       0,    82,     0,    74,    71,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   523,     0,   508,   507,     0,   389,   387,
     285,     0,     0,   378,    55,   283,   303,   102,   103,   104,
     425,   426,     0,   443,   301,   442,     0,   523,     0,     0,
       0,   462,   310,     0,   116,     0,   523,   274,   319,     0,
     318,     0,     0,   523,     0,     0,     0,     0,     0,     0,
       0,   522,     0,     0,   274,     0,   523,     0,   298,   496,
     254,   250,     0,     0,   244,   252,     0,   245,   511,     0,
     279,   249,   511,   239,   523,   523,   238,   511,   282,    45,
       0,     0,     0,     0,     0,     0,    17,   511,   272,    13,
     510,    67,   511,   271,   275,   517,   232,   516,   517,   234,
     276,   492,    92,    84,     0,    79,     0,     0,   523,     0,
     468,   465,   464,   463,   466,     0,   480,   484,   483,   479,
     462,     0,   369,   467,   469,   471,   523,   477,   523,   482,
     523,     0,   461,   422,     0,     0,   408,   411,     0,     0,
       7,    21,    22,    23,    24,    25,    43,    44,   523,     0,
      28,    37,     0,    38,   511,     0,    72,    83,    41,    40,
       0,   186,   253,    36,   204,   212,   217,   218,   219,   214,
     216,   226,   227,   220,   221,   197,   198,   224,   225,   511,
     213,   215,   209,   210,   211,   199,   200,   201,   202,   203,
     500,   505,   501,   506,   383,   248,   381,   511,   500,   502,
     501,   503,   382,   523,   500,   501,   248,   523,   523,    29,
     188,    35,   196,    53,    56,     0,   445,     0,     0,   102,
     103,   106,     0,   511,   523,     0,   511,   462,     0,     0,
       0,     0,   267,   523,   523,   395,   523,   320,   186,   504,
     273,   511,   500,   501,   523,     0,     0,   297,   322,   291,
     321,   294,   504,   273,   511,   500,   501,     0,   495,     0,
     255,   251,   523,   494,   278,   513,   235,   240,   242,   281,
      19,     0,    26,   195,    69,    16,   269,   515,    85,    77,
      89,    91,   511,   500,   501,     0,   468,     0,   340,   331,
     333,   511,   329,   511,     0,     0,   287,     0,   454,   487,
       0,   457,   481,     0,   459,   485,   415,     0,     0,   205,
     206,   360,   511,     0,   358,   357,   259,     0,    81,    75,
       0,     0,     0,     0,     0,     0,   380,    59,     0,   386,
       0,     0,   237,   379,    57,   236,   375,    52,     0,     0,
       0,   523,   304,     0,     0,   386,   307,   490,   511,     0,
     447,   311,   114,   117,   396,   397,   523,   398,     0,   523,
     325,     0,     0,   323,     0,     0,   386,     0,     0,     0,
     296,     0,     0,     0,     0,   386,     0,   256,   246,   523,
      11,   233,    87,   473,   511,     0,   338,     0,   470,     0,
     362,     0,     0,   472,   523,   523,   486,   523,   478,   523,
     523,   410,     0,   468,   511,     0,   523,   475,   523,   523,
     356,     0,     0,   257,    73,   187,     0,    34,   193,    33,
     194,    60,   514,     0,    31,   191,    32,   192,    58,   376,
     377,     0,     0,   189,     0,     0,   444,   302,   446,   309,
     462,     0,     0,   400,   326,     0,    12,   402,     0,   288,
       0,   289,   255,   523,     0,     0,   299,   241,   330,   341,
       0,   336,   332,   368,     0,   371,   370,     0,   450,     0,
     452,     0,   458,     0,   455,   460,   412,     0,     0,   359,
     347,   349,     0,   352,     0,   354,   373,   258,   228,    30,
     190,   390,   388,     0,     0,     0,     0,   399,     0,    94,
     101,     0,   401,     0,   392,   393,   391,   292,   295,     0,
       0,   339,     0,   334,   366,   511,   364,   367,   523,   523,
     523,   523,     0,   474,   361,   523,   523,   523,   476,   523,
     523,    54,   305,     0,   100,     0,   523,     0,   523,   523,
       0,   337,     0,     0,   363,   451,     0,   448,   453,   456,
     274,     0,     0,   344,     0,   346,   353,     0,   350,   355,
     312,   504,    99,   511,   500,   501,   394,   324,   300,   335,
     365,   523,   504,   273,   523,   523,   523,   523,   386,   449,
     345,     0,   342,   348,   351,   523,   343
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    60,    61,    62,   253,   375,   376,   262,
     263,   420,    64,    65,   206,    66,    67,   554,   681,    68,
      69,   264,    70,    71,    72,   445,    73,   207,   105,   106,
     199,   200,   693,   201,   571,   186,   187,    75,   235,   269,
     534,   673,   412,   413,   244,   245,   237,   404,   414,   493,
      76,   203,   432,   268,   283,   220,   713,   221,   714,   597,
     849,   558,   555,   775,   369,   371,   570,   780,   256,   379,
     589,   702,   703,   226,   629,   630,   631,   744,   653,   654,
     729,   855,   856,   461,   636,   309,   488,    78,    79,   355,
     548,   547,   390,   846,   574,   696,   782,   786,    80,    81,
     293,   475,   648,    82,    83,    84,   290,    85,   209,   210,
      88,   211,   364,   557,   568,   569,   463,   464,   465,   466,
     467,   747,   748,   468,   469,   470,   471,   736,   638,   189,
     370,   274,   415,   240,    90,   562,   536,   347,   216,   409,
     410,   669,   436,   380,   251,   219,   266
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -728
static const yytype_int16 yypact[] =
{
    -728,   129,  2484,  -728,  6830,  8542,  8851,  4938,  6590,  -728,
    8221,  8221,  4419,  -728,  -728,  8645,  7044,  7044,  -728,  -728,
    7044,  5656,  5768,  -728,  -728,  -728,  -728,   220,  6590,  -728,
      23,  -728,  -728,  5060,  5184,  -728,  -728,  5308,  -728,  -728,
    -728,  -728,  -728,  -728,  -728,  -728,  8328,  8328,   205,  3736,
    8221,  7258,  7579,  6104,  -728,  6350,    48,   291,  8435,  8328,
    -728,   294,  -728,   862,  -728,   323,  -728,  -728,   163,    26,
    -728,   139,  8748,  -728,   160,  6211,   173,   200,    22,    91,
    -728,  -728,  -728,  -728,  -728,  -728,    30,   202,  -728,    32,
      93,  -728,  -728,  -728,  -728,  -728,   174,   183,   188,   497,
     529,  8221,   333,  3879,   386,  -728,    39,  -728,   218,  -728,
    -728,    93,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,
    -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,
    -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,
      57,   128,   182,   193,  -728,  -728,  -728,  -728,  -728,  -728,
     229,   239,  -728,   265,  -728,   270,  -728,  -728,  -728,  -728,
    -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,
    -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,
    -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,    22,
    -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  6470,  -728,
    -728,   257,  -728,  2945,   343,   323,    53,   334,   253,   196,
     361,   219,    53,  -728,  -728,   294,   434,  -728,  -728,   319,
    8221,  8221,   432,  -728,  -728,   339,   476,    97,   105,  8328,
    8328,  8328,  8328,  -728,  6211,   409,  -728,  -728,   365,   380,
    -728,  -728,  -728,  4293,  -728,  7044,  7044,  -728,  -728,  4547,
    8221,  -728,  -728,   381,  4022,  -728,   354,   442,    37,  6937,
    3736,   408,   294,   862,   400,   446,  -728,   323,   400,   417,
      10,   140,  -728,   409,   424,   140,  -728,   505,  8954,   427,
     356,   368,   373,   700,  -728,  -728,  -728,  -728,   418,  -728,
    -728,  -728,  -728,   422,   498,   499,  -728,  -728,  -728,  -728,
    4673,  8221,  8221,  8221,  8221,  6937,  8221,  8221,  -728,  -728,
    7686,  -728,  3736,  6214,   440,  7686,  8328,  8328,  8328,  8328,
    8328,  8328,  8328,  8328,  8328,  8328,  8328,  8328,  8328,  8328,
    8328,  8328,  8328,  8328,  8328,  8328,  8328,  8328,  8328,  8328,
    8328,  8328,  2347,  7044,  9214,  -728,  -728, 10090,  -728,  -728,
    -728,  8435,  8435,  -728,   475,  -728,   323,  -728,   377,  -728,
    -728,  -728,   294,  -728,  -728,  -728,  9287,  7044,  9360,  2945,
    8221,   918,  -728,   522,  -728,   575,   584,   308,  -728,  3079,
     583,  8328,  9433,  7044,  9506,  8328,  8328,  3337,   434,  7793,
     589,  -728,    43,    43,   133,  9579,  7044,  9652,  -728,  -728,
    -728,  -728,  8328,  7151,  -728,  -728,  7365,  -728,   400,   462,
    -728,  -728,   400,  -728,   465,   467,  -728,   142,  -728,  -728,
    6590,  3465,   471,  9433,  9506,  8328,   862,   400,  -728,  -728,
    4798,   469,   400,  -728,  -728,  7472,  -728,  -728,  7579,  -728,
    -728,  -728,   377,   139,  8954,  -728,  8954,  9725,  7044,  9798,
     507,  -728,  -728,  -728,  -728,  1138,  -728,  -728,  -728,  -728,
     728,    92,  -728,  -728,  -728,  -728,   478,  -728,   479,   570,
     485,   581,  -728,  -728,   444,  4022,  -728,  -728,  8328,  8328,
    -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,    64,  8328,
    -728,   488,   501,  -728,   400,  8954,   502,  -728,  -728,  -728,
     530,  1069,  -728,  -728,   442,  1131,  1131,  1131,  1131,   959,
     959,  9233,  2083,  1131,  1131, 10144, 10144,   732,   732,  2781,
     959,   959,  1042,  1042,  1048,    47,    47,   442,   442,   442,
    2661,  5880,  2802,  5992,  -728,   183,  -728,   400,   412,  -728,
     428,  -728,  -728,  5768,  -728,  -728,  1547,    64,    64,  -728,
    2029,  -728,  6211,  -728,  -728,   294,  -728,  8221,  2945,   538,
      -1,  -728,   183,   400,   183,   631,   142,   728,  2945,   294,
    6710,  6590,  -728,  7900,   628,  -728,   518,  -728,  2177,  5432,
    5544,   400,   340,   345,   628,   637,    50,  -728,  -728,  -728,
    -728,  -728,    72,    74,   400,   135,   136,  8221,  -728,  8328,
     409,  -728,   380,  -728,  -728,  -728,  -728,  7151,  7365,  -728,
    -728,   513,  -728,  6211,    67,   862,  -728,   140,   440,  -728,
     538,    -1,   400,   315,   322,  8328,  -728,  1138,   416,  -728,
     511,   400,  -728,   400,  4165,  4022,  -728,   728,  -728,  -728,
     728,  -728,  -728,   718,  -728,  -728,  -728,   519,  4022,   442,
     442,  -728,   903,  4165,  -728,  -728,   515,  8007,  -728,  -728,
    8954,  8435,  8328,   549,  8435,  8435,  -728,   475,   526,   473,
    8435,  8435,  -728,  -728,   475,  -728,    91,   163,  4165,  4022,
    8328,    64,  -728,   294,   649,  -728,  -728,  -728,   400,   660,
    -728,  -728,  -728,  -728,   488,  -728,   582,  -728,  3608,   664,
    -728,  8221,   668,  -728,  8328,  8328,   389,  8328,  8328,   671,
    -728,  8114,  3208,  4165,  4165,   137,    43,  -728,  -728,   545,
    -728,  -728,   388,  -728,   400,   873,   546,  1200,  -728,   548,
     543,   682,   560,  -728,   561,   564,  -728,   565,  -728,   572,
     565,  -728,   579,   608,   400,   604,   587,  -728,   588,   590,
    -728,   726,  8328,   601,  -728,  6211,  8328,  -728,  6211,  -728,
    6211,  -728,  -728,  8435,  -728,  6211,  -728,  6211,  -728,  -728,
    -728,   730,   610,  6211,  4022,  2945,  -728,  -728,  -728,  -728,
     918,  9057,    53,  -728,  -728,  4165,  -728,  -728,    53,  -728,
    8328,  -728,  -728,   230,   738,   739,  -728,  7365,  -728,   614,
     873,   667,  -728,  -728,   653,  -728,  -728,   728,  -728,   718,
    -728,   718,  -728,   718,  -728,  -728,  -728,  9160,   646,  -728,
     934,  -728,   934,  -728,   718,  -728,  -728,   621,  6211,  -728,
    6211,  -728,  -728,   626,   752,  2945,   708,  -728,   393,   368,
     373,  2945,  -728,  3079,  -728,  -728,  -728,  -728,  -728,  4165,
     873,   614,   873,   629,  -728,   149,  -728,  -728,   565,   630,
     565,   565,   715,   403,  -728,   638,   639,   565,  -728,   642,
     565,  -728,  -728,   765,   377,  9871,  7044,  9944,   584,   518,
     768,   614,   873,   653,  -728,  -728,   718,  -728,  -728,  -728,
    -728, 10017,   934,  -728,   718,  -728,  -728,   718,  -728,  -728,
    -728,   114,    -1,   400,    61,    77,  -728,  -728,  -728,   614,
    -728,   565,   648,   650,   565,   654,   565,   565,    79,  -728,
    -728,   718,  -728,  -728,  -728,   565,  -728
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -728,  -728,  -728,   367,  -728,    28,  -728,  -352,   282,  -728,
      87,  -728,  -289,   869,    41,   -52,  -728,  -484,  -728,    -5,
     778,  -167,   -10,   -69,  -277,  -391,   -18,    52,   -68,   792,
       9,   -14,  -728,  -728,  -728,  -195,  -728,  1049,   187,  -728,
     -15,   258,  -325,    78,    -9,  -728,  -359,  -231,    89,  -279,
       4,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,
    -728,  -728,  -728,  -728,  -728,  -728,  -728,  -728,   640,  -207,
    -368,   -80,  -496,  -728,  -624,  -674,   180,  -728,  -464,  -728,
    -572,  -728,   -74,  -728,  -728,   134,  -728,  -728,  -728,   -75,
    -728,  -728,  -366,  -728,   -65,  -728,  -728,  -728,  -728,  -728,
     528,  -728,  -728,  -728,  -728,    15,  -728,  -728,  1217,  1437,
     803,  1529,  -728,  -728,    40,  -262,  -727,     2,  -569,  -489,
    -592,  -715,     3,   185,  -728,  -506,  -728,  -259,  1024,  -728,
    -728,  -728,    21,  -377,  2003,  -282,  -728,   645,   -16,   -25,
    -136,  -535,  -244,     8,    18,   -28,    -2
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -524
static const yytype_int16 yytable[] =
{
      91,   444,   250,   314,   350,   387,   296,   247,   407,   107,
     107,   233,   233,   248,   252,   233,   185,   565,   537,   107,
     217,   462,   585,   202,   472,   591,   498,   439,   685,   603,
     218,   441,    92,   300,   359,   279,   503,   239,   239,   265,
     362,   239,   563,   202,   601,   299,   706,   601,   738,   272,
     276,   799,   212,   215,    74,   619,    74,   107,   581,   715,
     535,   603,   543,   378,   289,   546,   378,   224,   733,   217,
     588,   594,   271,   275,   -96,   353,   107,   857,   699,   218,
     745,   354,   247,   678,   679,  -273,   564,   722,   709,    63,
     -98,    63,   -95,   427,   236,   241,   353,   -97,   242,   -99,
     535,    74,   543,   802,   659,   238,   238,   868,   345,   238,
     291,   292,   472,   418,   365,   564,   316,   348,  -432,   348,
     352,   634,   -94,   622,   218,   425,   851,   -97,   431,     3,
    -101,   735,   433,   310,   739,  -273,  -273,   -94,  -501,  -101,
     270,   651,   564,  -435,  -100,   494,   749,   533,   435,   541,
     363,   737,   541,   214,   740,    74,   857,   254,  -100,   346,
     -96,   -98,   -95,   746,   339,   340,   341,   564,   -86,   444,
     -93,   533,   818,   541,   -67,   -92,   652,   868,   881,   306,
     307,   213,   214,   761,   407,   213,   214,   533,   711,   541,
     768,   308,   213,   214,  -435,   213,   214,   635,   633,   388,
     533,   472,   541,   -96,   -96,   -81,   684,   289,   909,   443,
     -89,  -500,   -91,  -501,  -434,   738,   689,   774,   444,   -98,
     -98,   -95,   -95,   217,   218,   349,   802,   349,   533,   541,
     218,   603,   246,   218,   430,   -86,   405,   405,   233,   273,
     233,   233,   733,   -93,   416,   698,   429,   389,   601,   601,
     265,   733,   533,  -500,   541,    74,   -97,   -97,   490,  -317,
     258,   392,   393,   499,   239,  -434,   239,   315,  -436,   754,
     217,   -92,   604,   -88,   -90,   -87,   606,   311,   438,  -437,
     218,   609,   107,   214,  -432,   214,   342,   883,   437,   437,
     351,   614,   214,   437,   695,   496,   616,   844,   561,   549,
     551,   859,   265,   -94,   366,   688,    74,   386,   472,  -317,
    -317,    74,    74,   243,   866,  -439,   869,   107,   858,  -436,
     860,   408,   246,   411,   861,  -438,  -101,   249,   480,   542,
    -437,   261,   238,   867,   238,   870,   343,   344,   233,   382,
     -88,   416,   481,   482,   483,   484,   426,   -90,   796,   294,
     295,  -427,    74,   542,   367,   368,  -431,    74,   658,   249,
     306,   307,   233,   214,    74,   416,  -439,   500,   918,   542,
     556,   718,   498,   721,   575,   618,  -438,   443,   233,   712,
     218,   416,   542,   444,   357,   261,   915,    63,   358,   383,
     384,   233,   485,   750,   416,   374,   425,   911,   377,   492,
     590,   590,  -427,   914,   492,   916,   610,  -431,   917,   542,
     218,   218,   405,   405,  -386,  -100,   399,   400,   401,    91,
     603,    74,   -88,   834,   602,   395,   443,   845,   707,   -90,
     687,    74,   925,   708,   542,   202,   213,   214,   601,    74,
     423,   381,   447,   233,   360,   361,   416,   -96,   107,   385,
     107,   389,   -98,   -88,  -433,   -88,   617,   632,   -88,  -264,
     -90,   391,   -90,  -274,   639,   -90,   639,   626,   639,   451,
     452,   453,   454,    74,  -386,   396,   397,   790,   586,   875,
     473,   292,    74,   873,   476,   477,   655,   394,   718,   891,
     383,   424,   448,   449,   663,   728,   -95,   491,   402,   107,
     670,   398,   502,   403,  -433,  -433,   646,   477,    63,  -264,
    -264,   316,   668,  -274,  -274,   421,   671,   615,   406,   -96,
     666,   472,  -386,  -497,  -386,  -386,   667,    74,   672,   876,
     877,   672,   701,   698,   674,   -98,   422,   677,   668,   367,
     368,   675,   261,   214,   675,   655,   655,   666,   428,   672,
     -88,   903,   778,   -66,   725,  -498,   668,   692,   434,   440,
     442,   763,   675,   682,  -504,   446,   -90,   478,   479,   668,
     553,   697,   700,   218,   700,   841,   502,   690,   497,   691,
     -95,   843,   700,  -427,   473,   572,   202,   218,   798,   598,
     600,   443,   573,   273,   261,   564,   577,   668,   683,   587,
     405,   769,   605,   607,   218,   608,   612,   -81,   730,   499,
      74,   -87,   757,   759,   625,  -431,   637,   640,   764,   766,
      74,   642,   600,   643,  -504,   273,  -253,   730,   719,   632,
     726,  -497,   645,  -427,  -427,   437,  -497,   661,   716,   657,
     660,   686,    77,   698,    77,   108,   108,   710,   720,   727,
     208,   208,   208,  -254,   741,   225,   208,   208,   756,   777,
     208,   576,   492,  -498,   107,  -431,  -431,   762,  -498,   584,
     779,   781,  -504,   785,  -504,  -504,   656,  -500,   789,   655,
     533,   791,   541,   797,   800,   804,    74,    74,   803,    77,
     208,   776,   805,   280,   783,   806,   533,   787,   208,   807,
      74,   218,   809,   811,   854,    74,   451,   452,   453,   454,
     813,   829,   280,   500,   816,   817,   819,   405,   626,   730,
     451,   452,   453,   454,   590,   820,   822,   632,   824,   632,
      74,    74,   639,   639,   218,   639,   826,   639,   639,  -255,
     831,   208,   788,    77,   639,   832,   639,   639,   847,   848,
      74,   450,   850,   451,   452,   453,   454,   647,   864,  -256,
     694,   871,   872,   874,    74,    74,    74,   882,   886,   626,
     890,   451,   452,   453,   454,   900,   892,   894,   908,   450,
     897,   451,   452,   453,   454,   107,   717,  -500,   611,  -501,
     455,   700,   921,   223,   600,   273,   456,   457,   112,   907,
     218,   316,   632,   853,   676,   852,   218,   724,   455,   910,
     188,   770,   723,   906,   458,   457,   474,   459,   455,     0,
     835,   107,   734,   865,   456,   457,    74,    74,     0,     0,
     884,     0,   458,   837,   372,     0,     0,    74,     0,   460,
       0,     0,   458,    77,   753,   459,     0,   337,   338,   339,
     340,   341,   632,     0,   632,     0,   639,   639,   639,   639,
     208,   208,   542,   639,   639,   639,     0,   639,   639,     0,
       0,   233,     0,     0,   416,     0,   575,   700,   668,   205,
     205,   205,     0,   208,   632,   208,   208,    74,     0,   208,
     208,     0,     0,    74,    77,    74,     0,     0,   792,    77,
      77,    74,   301,   302,   303,   304,   305,     0,     0,   639,
       0,     0,   639,   639,   639,   639,   731,   732,   280,   267,
       0,     0,     0,   639,   626,     0,   451,   452,   453,   454,
     742,     0,     0,     0,     0,   751,     0,     0,     0,   827,
      77,   208,   208,   208,   208,    77,   208,   208,     0,     0,
     208,     0,    77,   280,   743,   208,   451,   452,   453,   454,
     771,   772,     0,   627,     0,     0,     0,     0,     0,   450,
     356,   451,   452,   453,   454,     0,     0,     0,     0,     0,
     784,     0,     0,   208,   273,   743,     0,   451,   452,   453,
     454,   208,   208,   455,   793,   794,   795,     0,     0,   456,
     457,     0,     0,     0,     0,     0,     0,   208,   455,    77,
     208,     0,     0,     0,   456,   457,     0,   458,     0,    77,
     459,     0,     0,   208,   455,     0,     0,    77,   316,     0,
     456,   457,   458,     0,     0,   459,   208,     0,     0,     0,
       0,     0,     0,   329,   330,     0,   214,     0,   458,     0,
       0,   459,     0,     0,     0,     0,   833,   567,     0,     0,
       0,    77,     0,     0,     0,   234,   234,   842,     0,   234,
      77,   334,   335,   336,   337,   338,   339,   340,   341,     0,
       0,     0,     0,     0,   280,     0,   280,     0,   208,   205,
     205,     0,     0,     0,     0,   255,   257,     0,     0,     0,
     234,   234,     0,     0,     0,     0,     0,   297,   298,     0,
       0,   316,     0,   662,     0,    77,     0,   316,   417,   419,
       0,     0,     0,   878,     0,   879,   329,   330,     0,     0,
       0,   880,   329,   330,     0,   280,     0,     0,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,     0,     0,   329,   330,     0,   336,   337,   338,   339,
     340,   341,     0,   337,   338,   339,   340,   341,     0,     0,
     205,   205,   205,   205,     0,   486,   487,   331,     0,   332,
     333,   334,   335,   336,   337,   338,   339,   340,   341,   626,
       0,   451,   452,   453,   454,     0,     0,   208,    77,     0,
     316,  -524,  -524,  -524,  -524,   321,   322,  -230,    77,  -524,
    -524,     0,     0,     0,     0,   329,   330,     0,     0,    86,
       0,    86,   109,   109,   109,     0,     0,     0,   627,     0,
       0,     0,   227,     0,   628,     0,     0,   208,     0,   566,
       0,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,   626,     0,   451,   452,   453,   454,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    86,     0,     0,     0,
     281,     0,     0,     0,    77,    77,     0,     0,   234,   234,
     234,   297,     0,     0,     0,     0,     0,     0,    77,   281,
     627,     0,   234,    77,   234,   234,   801,     0,     0,     0,
     280,   208,     0,     0,   208,   208,     0,     0,     0,     0,
     208,   208,     0,     0,     0,     0,     0,     0,    77,    77,
      86,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    77,     0,
       0,   208,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    77,    77,    77,     0,     0,     0,     0,   234,
       0,     0,     0,     0,   501,   504,   505,   506,   507,   508,
     509,   510,   511,   512,   513,   514,   515,   516,   517,   518,
     519,   520,   521,   522,   523,   524,   525,   526,   527,   528,
     529,     0,   234,     0,     0,     0,     0,     0,     0,     0,
     550,   552,     0,   208,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    77,    77,   234,     0,     0,     0,
      86,   838,     0,     0,     0,    77,   205,     0,     0,     0,
     578,     0,   234,     0,   550,   552,     0,     0,   234,    87,
       0,    87,     0,     0,     0,   234,     0,     0,     0,     0,
       0,   234,   234,     0,     0,   234,     0,   863,     0,     0,
       0,     0,     0,     0,     0,     0,   205,     0,     0,     0,
       0,    86,     0,     0,   613,    77,    86,    86,     0,     0,
       0,    77,     0,    77,   234,     0,    87,   234,     0,    77,
       0,     0,   641,     0,   644,   281,     0,   234,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   208,    86,     0,     0,
       0,     0,    86,     0,     0,     0,     0,   649,   650,    86,
     281,    89,     0,    89,   110,   110,     0,     0,   234,     0,
      87,     0,     0,     0,   228,     0,     0,  -523,     0,     0,
       0,     0,     0,     0,     0,  -523,  -523,  -523,     0,     0,
    -523,  -523,  -523,     0,  -523,     0,     0,     0,     0,     0,
     205,     0,     0,  -523,  -523,     0,     0,     0,    89,     0,
       0,     0,   282,     0,  -523,  -523,    86,  -523,  -523,  -523,
    -523,  -523,     0,     0,     0,     0,    86,     0,     0,     0,
       0,   282,     0,     0,    86,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   234,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    89,  -523,     0,     0,     0,     0,    86,     0,
      87,     0,     0,     0,     0,     0,     0,    86,   234,     0,
       0,     0,     0,     0,     0,     0,   234,   234,     0,     0,
       0,   281,     0,   281,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   234,     0,     0,     0,     0,     0,
       0,  -523,  -523,     0,  -523,     0,   246,  -523,     0,  -523,
    -523,    87,    86,     0,     0,     0,    87,    87,     0,     0,
       0,     0,     0,     0,     0,     0,   234,     0,     0,     0,
     578,   755,   281,   758,   760,     0,     0,     0,     0,   765,
     767,     0,     0,     0,     0,     0,     0,     0,     0,   773,
       0,     0,    89,     0,     0,     0,     0,    87,     0,     0,
       0,     0,    87,     0,     0,     0,     0,     0,     0,    87,
       0,     0,     0,   758,   760,     0,   765,   767,   808,   810,
     234,   812,     0,   814,   815,     0,     0,     0,     0,     0,
     821,     0,   823,   825,     0,    86,     0,     0,     0,     0,
       0,     0,     0,    89,     0,    86,     0,     0,    89,    89,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   234,     0,     0,     0,   828,    87,   282,     0,     0,
       0,     0,   830,     0,     0,     0,    87,     0,     0,     0,
       0,     0,     0,     0,    87,     0,     0,     0,     0,    89,
       0,     0,     0,     0,    89,     0,     0,     0,     0,   830,
       0,    89,   282,     0,     0,     0,   234,     0,     0,     0,
       0,    86,    86,     0,     0,     0,     0,     0,    87,     0,
       0,     0,     0,     0,     0,    86,     0,    87,     0,     0,
      86,     0,     0,     0,     0,     0,     0,   281,     0,     0,
       0,     0,   885,   887,   888,   889,     0,     0,     0,   893,
     895,   896,     0,   898,   899,    86,    86,     0,    89,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    89,     0,
       0,     0,    87,     0,     0,    86,    89,     0,     0,     0,
       0,     0,     0,     0,     0,   234,     0,     0,     0,    86,
      86,    86,     0,     0,     0,   919,     0,     0,   920,   922,
     923,   924,     0,     0,     0,     0,     0,     0,     0,   926,
      89,     0,     0,     0,     0,     0,     0,     0,     0,    89,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   282,     0,   282,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    86,    86,     0,     0,    87,     0,     0,   839,     0,
       0,     0,    86,     0,    89,    87,     0,     0,   111,   111,
       0,     0,     0,     0,     0,     0,     0,     0,   111,     0,
       0,     0,     0,     0,   282,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   109,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   111,
     111,     0,    86,     0,   111,   111,   111,     0,    86,     0,
      86,     0,   111,     0,     0,     0,    86,     0,     0,     0,
       0,    87,    87,   680,     0,   111,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    87,     0,    89,     0,     0,
      87,     0,     0,     0,     0,     0,     0,    89,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,     0,     0,   329,   330,    87,    87,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    87,     0,   331,     0,   332,
     333,   334,   335,   336,   337,   338,   339,   340,   341,    87,
      87,    87,   316,   317,   318,   319,   320,   321,   322,   323,
       0,   325,   326,    89,    89,     0,     0,   329,   330,     0,
       0,     0,     0,     0,     0,     0,     0,    89,     0,     0,
       0,     0,    89,     0,     0,     0,     0,     0,     0,   282,
       0,     0,     0,   332,   333,   334,   335,   336,   337,   338,
     339,   340,   341,     0,     0,     0,     0,    89,    89,     0,
       0,    87,    87,     0,     0,     0,     0,     0,     0,     0,
       0,   662,    87,     0,     0,     0,     0,    89,     0,     0,
       0,     0,   111,   111,   111,   111,     0,     0,     0,     0,
       0,    89,    89,    89,     0,     0,   316,   317,   318,   319,
     320,   321,   322,   323,   324,   325,   326,   327,   328,     0,
       0,   329,   330,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    87,     0,     0,     0,     0,     0,    87,     0,
      87,   111,     0,     0,     0,   331,    87,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,     0,     0,     0,
       0,     0,     0,    89,    89,     0,     0,     0,     0,     0,
     840,     0,     0,     0,    89,     0,   111,     0,     0,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,     0,   110,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    89,     0,     0,     0,     0,     0,
      89,     0,    89,     0,     0,     0,     0,     0,    89,     0,
       0,     0,     0,     0,   111,     0,     0,     0,   111,   111,
       0,     0,   111,     0,     0,     0,     0,     0,   530,   531,
       0,     0,   532,     0,     0,   111,   111,     0,     0,   111,
       0,     0,     0,     0,   156,   157,   158,   159,   160,   161,
     162,   163,   164,     0,     0,   165,   166,     0,   111,   167,
     168,   169,   170,     0,     0,     0,     0,     0,   111,     0,
       0,   111,     0,   171,     0,     0,     0,   111,     0,   111,
       0,     0,     0,     0,     0,     0,     0,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,     0,   182,   183,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   111,   111,     0,  -523,     4,   246,     5,     6,     7,
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
       0,     0,     0,     0,     0,     0,  -523,  -523,   111,     0,
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
    -504,  -504,  -504,     0,     0,  -504,  -504,  -504,     0,   664,
       0,     0,     0,     0,     0,   111,     0,     0,     0,   111,
       0,     0,     0,     0,     0,     0,     0,     0,   -97,  -504,
       0,  -504,  -504,  -504,  -504,  -504,  -504,  -504,  -504,  -504,
    -504,     0,     0,     0,   111,     0,     0,     0,     0,     0,
       0,     0,     0,   111,     0,  -504,  -504,  -504,  -504,   -89,
     111,  -504,  -273,  -504,  -504,     0,     0,     0,     0,     0,
    -273,  -273,  -273,     0,     0,     0,  -273,  -273,     0,  -273,
     111,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -273,
    -273,     0,  -273,  -273,  -273,  -273,  -273,     0,     0,     0,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,     0,     0,   329,   330,     0,     0,     0,
       0,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,     0,     0,  -273,  -273,  -273,   331,
     665,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,     0,     0,     0,     0,     0,     0,     0,     0,   -99,
    -273,     0,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,     0,     0,   214,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -273,  -273,  -273,
     -91,     0,  -273,     0,  -273,  -273,   259,     0,     5,     6,
       7,     8,     9,  -523,  -523,  -523,    10,    11,     0,     0,
    -523,    12,     0,    13,    14,    15,    16,    17,    18,    19,
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
     259,     0,     5,     6,     7,     8,     9,  -523,  -523,  -523,
      10,    11,     0,  -523,  -523,    12,     0,    13,    14,    15,
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
      58,    59,     0,     0,     0,     0,     0,     0,     0,   259,
       0,     5,     6,     7,     8,     9,     0,     0,  -523,    10,
      11,  -523,  -523,  -523,    12,  -523,    13,    14,    15,    16,
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
      59,     0,     0,     0,     0,     0,     0,     0,   259,     0,
       5,     6,     7,     8,     9,     0,     0,  -523,    10,    11,
    -523,  -523,  -523,    12,     0,    13,    14,    15,    16,    17,
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
       7,     8,     9,     0,     0,     0,    10,    11,     0,  -523,
    -523,    12,     0,    13,    14,    15,    16,    17,    18,    19,
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
    -523,     0,     0,     0,     0,     0,     0,  -523,  -523,   259,
       0,     5,     6,     7,     8,     9,     0,  -523,  -523,    10,
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
      59,     0,     0,     0,     0,     0,     0,   259,     0,     5,
       6,     7,     8,     9,     0,     0,     0,    10,    11,     0,
    -523,  -523,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,     0,    30,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    48,     0,     0,   260,    50,     0,    51,
      52,     0,    53,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    57,    58,    59,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -523,     0,  -523,  -523,
     259,     0,     5,     6,     7,     8,     9,     0,     0,     0,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -523,
       0,  -523,  -523,   259,     0,     5,     6,     7,     8,     9,
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
       0,     0,     0,     0,     0,     0,     0,  -523,     0,     0,
       0,     0,     0,     0,  -523,  -523,   259,     0,     5,     6,
       7,     8,     9,     0,     0,  -523,    10,    11,     0,     0,
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
       9,     0,     0,     0,    10,    11,     0,  -523,  -523,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    98,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
     229,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     204,     0,     0,   103,    50,     0,    51,    52,     0,   230,
     231,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    57,   232,    59,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,   214,    13,    14,    15,
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
     214,    13,    14,    15,    16,    17,    18,    19,     0,     0,
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
      11,     0,     0,     0,    12,   391,    13,    14,    15,    16,
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
     391,   113,   114,   115,   116,   117,   118,   119,   120,   121,
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
     704,     0,  -500,  -500,     0,  -500,  -500,     0,  -500,  -500,
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
       0,  -501,   705,     0,  -501,  -501,     0,  -501,  -501,     0,
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
    -248,  -248,  -248,     0,     0,   243,     0,     0,  -248,  -248,
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
    -502,  -502,  -502,  -502,  -502,     0,     0,   246,     0,     0,
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
       0,   277,     0,     0,   103,    50,     0,    51,    52,     0,
       0,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,   104,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,    93,    94,    18,    19,     0,
       0,     0,   278,     0,    95,    96,    97,    23,    24,    25,
      26,     0,     0,    98,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,     0,     0,   329,   330,     0,     0,     0,
       0,   277,     0,     0,   103,    50,     0,    51,    52,     0,
       0,     0,    54,    55,     0,     0,     0,    56,     0,   331,
       0,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,     0,     0,     0,   104,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   495,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,     0,     0,     0,
     137,   138,   139,   190,   191,   192,   193,   144,   145,   146,
       0,     0,     0,     0,     0,   147,   148,   149,   194,   195,
     152,   196,   154,   284,   285,   197,   286,     0,     0,     0,
       0,     0,   287,     0,     0,     0,     0,   156,   157,   158,
     159,   160,   161,   162,   163,   164,     0,     0,   165,   166,
       0,     0,   167,   168,   169,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,     0,     0,     0,
       0,     0,     0,   288,     0,     0,     0,     0,     0,     0,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
       0,   182,   183,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,     0,     0,     0,
     137,   138,   139,   190,   191,   192,   193,   144,   145,   146,
       0,     0,     0,     0,     0,   147,   148,   149,   194,   195,
     152,   196,   154,   284,   285,   197,   286,     0,     0,     0,
       0,     0,   287,     0,     0,     0,     0,   156,   157,   158,
     159,   160,   161,   162,   163,   164,     0,     0,   165,   166,
       0,     0,   167,   168,   169,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,     0,     0,     0,
       0,     0,     0,   373,     0,     0,     0,     0,     0,     0,
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
      38,   229,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   204,     0,     0,   103,    50,     0,    51,    52,     0,
     230,   231,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    57,   232,    59,    12,     0,    13,
      14,    15,    93,    94,    18,    19,     0,     0,     0,     0,
       0,    95,    96,    97,    23,    24,    25,    26,     0,     0,
      98,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      32,     0,    33,    34,    35,    36,    37,    38,   229,    39,
      40,    41,    42,    43,     0,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   204,     0,
       0,   103,    50,     0,    51,    52,     0,   599,   231,    54,
      55,     0,     0,     0,    56,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    57,   232,    59,    12,     0,    13,    14,    15,    93,
      94,    18,    19,     0,     0,     0,     0,     0,    95,    96,
      97,    23,    24,    25,    26,     0,     0,    98,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,   229,    39,    40,    41,    42,
      43,     0,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   204,     0,     0,   103,    50,
       0,    51,    52,     0,   230,     0,    54,    55,     0,     0,
       0,    56,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    57,   232,
      59,    12,     0,    13,    14,    15,    93,    94,    18,    19,
       0,     0,     0,     0,     0,    95,    96,    97,    23,    24,
      25,    26,     0,     0,    98,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,     0,    33,    34,    35,    36,
      37,    38,   229,    39,    40,    41,    42,    43,     0,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   204,     0,     0,   103,    50,     0,    51,    52,
       0,     0,   231,    54,    55,     0,     0,     0,    56,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    57,   232,    59,    12,     0,
      13,    14,    15,    93,    94,    18,    19,     0,     0,     0,
       0,     0,    95,    96,    97,    23,    24,    25,    26,     0,
       0,    98,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,     0,    33,    34,    35,    36,    37,    38,   229,
      39,    40,    41,    42,    43,     0,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   204,
       0,     0,   103,    50,     0,    51,    52,     0,   599,     0,
      54,    55,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    57,   232,    59,    12,     0,    13,    14,    15,
      93,    94,    18,    19,     0,     0,     0,     0,     0,    95,
      96,    97,    23,    24,    25,    26,     0,     0,    98,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,     0,
      33,    34,    35,    36,    37,    38,   229,    39,    40,    41,
      42,    43,     0,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   204,     0,     0,   103,
      50,     0,    51,    52,     0,     0,     0,    54,    55,     0,
       0,     0,    56,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    57,
     232,    59,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    98,     0,     0,     0,     0,
       0,     0,     0,     0,    31,    32,     0,    33,    34,    35,
      36,    37,    38,     0,    39,    40,    41,    42,    43,     0,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   204,     0,     0,   103,    50,     0,    51,
      52,     0,   489,     0,    54,    55,     0,     0,     0,    56,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    57,   232,    59,    12,
       0,    13,    14,    15,    93,    94,    18,    19,     0,     0,
       0,     0,     0,    95,    96,    97,    23,    24,    25,    26,
       0,     0,    98,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    33,    34,    35,    36,    37,    38,
       0,    39,    40,    41,    42,    43,     0,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     204,     0,     0,   103,    50,     0,    51,    52,     0,   230,
       0,    54,    55,     0,     0,     0,    56,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    57,   232,    59,    12,     0,    13,    14,
      15,    93,    94,    18,    19,     0,     0,     0,     0,     0,
      95,    96,    97,    23,    24,    25,    26,     0,     0,    98,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   204,     0,     0,
     103,    50,     0,    51,    52,     0,   489,     0,    54,    55,
       0,     0,     0,    56,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      57,   232,    59,    12,     0,    13,    14,    15,    93,    94,
      18,    19,     0,     0,     0,     0,     0,    95,    96,    97,
      23,    24,    25,    26,     0,     0,    98,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   204,     0,     0,   103,    50,     0,
      51,    52,     0,   752,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    57,   232,    59,
      12,     0,    13,    14,    15,    93,    94,    18,    19,     0,
       0,     0,     0,     0,    95,    96,    97,    23,    24,    25,
      26,     0,     0,    98,     0,     0,     0,     0,     0,     0,
       0,     0,    31,    32,     0,    33,    34,    35,    36,    37,
      38,     0,    39,    40,    41,    42,    43,     0,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   204,     0,     0,   103,    50,     0,    51,    52,     0,
     599,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    57,   232,    59,    12,     0,    13,
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
       7,     0,     9,     0,     0,     0,    10,    11,    57,   232,
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
       0,     0,     0,    10,    11,    57,   232,    59,    12,     0,
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
       0,     0,   222,     0,     0,    49,    50,     0,    51,    52,
       0,    53,     0,    54,    55,     0,     0,     0,    56,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,   104,    13,    14,    15,    93,
      94,    18,    19,     0,     0,     0,     0,     0,    95,    96,
      97,    23,    24,    25,    26,     0,     0,    98,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    32,     0,    33,
      34,    35,    36,    37,    38,     0,    39,    40,    41,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   277,     0,     0,   312,    50,
       0,    51,    52,     0,   313,     0,    54,    55,     0,     0,
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
       0,   277,     0,     0,   312,    50,     0,    51,    52,     0,
       0,     0,    54,    55,     0,     0,     0,    56,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,   104,    13,    14,    15,    93,    94,
      18,    19,     0,     0,     0,     0,     0,    95,    96,    97,
      23,    24,    25,    26,     0,     0,    98,     0,     0,     0,
       0,     0,     0,     0,     0,    31,    32,     0,    33,    34,
      35,    36,    37,    38,     0,    39,    40,    41,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   836,     0,     0,   103,    50,     0,
      51,    52,     0,     0,     0,    54,    55,     0,     0,     0,
      56,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,   104,    13,    14,
      15,    93,    94,    18,    19,     0,     0,     0,     0,     0,
      95,    96,    97,    23,    24,    25,    26,     0,     0,    98,
       0,     0,     0,     0,     0,     0,     0,     0,    31,    32,
       0,    33,    34,    35,    36,    37,    38,     0,    39,    40,
      41,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   862,     0,     0,
     103,    50,     0,    51,    52,     0,     0,     0,    54,    55,
       0,     0,     0,    56,     0,   538,   539,     0,     0,   540,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     104,   156,   157,   158,   159,   160,   161,   162,   163,   164,
       0,     0,   165,   166,     0,     0,   167,   168,   169,   170,
       0,     0,   316,   317,   318,   319,   320,   321,   322,     0,
     171,   325,   326,     0,     0,     0,     0,   329,   330,     0,
       0,     0,     0,     0,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,     0,   182,   183,     0,   559,   531,
       0,     0,   560,   332,   333,   334,   335,   336,   337,   338,
     339,   340,   341,   246,   156,   157,   158,   159,   160,   161,
     162,   163,   164,     0,     0,   165,   166,     0,     0,   167,
     168,   169,   170,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   171,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,     0,   182,   183,
       0,   544,   539,     0,     0,   545,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   246,   156,   157,   158,
     159,   160,   161,   162,   163,   164,     0,     0,   165,   166,
       0,     0,   167,   168,   169,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
       0,   182,   183,     0,   579,   531,     0,     0,   580,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   246,
     156,   157,   158,   159,   160,   161,   162,   163,   164,     0,
       0,   165,   166,     0,     0,   167,   168,   169,   170,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   171,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   172,   173,   174,   175,   176,   177,   178,
     179,   180,   181,     0,   182,   183,     0,   582,   539,     0,
       0,   583,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   246,   156,   157,   158,   159,   160,   161,   162,
     163,   164,     0,     0,   165,   166,     0,     0,   167,   168,
     169,   170,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   171,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,     0,   182,   183,     0,
     592,   531,     0,     0,   593,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   246,   156,   157,   158,   159,
     160,   161,   162,   163,   164,     0,     0,   165,   166,     0,
       0,   167,   168,   169,   170,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   171,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   172,
     173,   174,   175,   176,   177,   178,   179,   180,   181,     0,
     182,   183,     0,   595,   539,     0,     0,   596,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   246,   156,
     157,   158,   159,   160,   161,   162,   163,   164,     0,     0,
     165,   166,     0,     0,   167,   168,   169,   170,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   171,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,     0,   182,   183,     0,   620,   531,     0,     0,
     621,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   246,   156,   157,   158,   159,   160,   161,   162,   163,
     164,     0,     0,   165,   166,     0,     0,   167,   168,   169,
     170,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   171,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   181,     0,   182,   183,     0,   623,
     539,     0,     0,   624,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   246,   156,   157,   158,   159,   160,
     161,   162,   163,   164,     0,     0,   165,   166,     0,     0,
     167,   168,   169,   170,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   171,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,     0,   182,
     183,     0,   901,   531,     0,     0,   902,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   246,   156,   157,
     158,   159,   160,   161,   162,   163,   164,     0,     0,   165,
     166,     0,     0,   167,   168,   169,   170,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   171,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   172,   173,   174,   175,   176,   177,   178,   179,   180,
     181,     0,   182,   183,     0,   904,   539,     0,     0,   905,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     246,   156,   157,   158,   159,   160,   161,   162,   163,   164,
       0,     0,   165,   166,     0,     0,   167,   168,   169,   170,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     171,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,     0,   182,   183,     0,   912,   531,
       0,     0,   913,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   246,   156,   157,   158,   159,   160,   161,
     162,   163,   164,     0,     0,   165,   166,     0,     0,   167,
     168,   169,   170,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   171,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,     0,   182,   183,
       0,   544,   539,     0,     0,   545,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   246,   156,   157,   158,
     159,   160,   161,   162,   163,   164,     0,     0,   165,   166,
       0,     0,   167,   168,   169,   170,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   171,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
       0,   182,   183,   316,   317,   318,   319,   320,   321,   322,
     323,   324,   325,   326,  -524,  -524,     0,     0,   329,   330,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   332,   333,   334,   335,   336,   337,
     338,   339,   340,   341
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-728))

#define yytable_value_is_error(yytable_value) \
  ((yytable_value) == (-524))

static const yytype_int16 yycheck[] =
{
       2,   278,    27,    72,    79,   212,    58,    22,   239,     5,
       6,    16,    17,    22,    28,    20,     7,   369,   343,    15,
      12,   283,   388,     8,   283,   393,   315,   271,   563,   406,
      12,   275,     4,    61,   102,    53,   315,    16,    17,    49,
       1,    20,   367,    28,   403,    61,   581,   406,   640,    51,
      52,   725,    11,    12,     2,   446,     4,    53,   383,   594,
     342,   438,   344,    13,    55,   347,    13,    15,   637,    61,
      27,   396,    51,    52,    13,    90,    72,   804,   574,    61,
     652,    90,    97,   547,   548,    86,   368,   622,   584,     2,
      13,     4,    13,   260,    16,    17,   111,    25,    20,    25,
     382,    49,   384,   727,   495,    16,    17,   822,    86,    20,
      62,    63,   371,   249,   106,   397,    69,    26,    88,    26,
      88,    29,    25,   448,   106,    88,   800,    13,   264,     0,
      25,   637,   268,   107,   640,   136,   137,   107,   139,   107,
      51,    77,   424,    86,   107,   312,   652,   342,   138,   344,
     111,   640,   347,   143,   643,   103,   883,   134,    25,   137,
      25,    25,    25,   652,   117,   118,   119,   449,   138,   446,
     138,   366,   744,   368,   107,   138,   112,   892,   852,    37,
      38,   142,   143,   667,   415,   142,   143,   382,   138,   384,
     674,    28,   142,   143,   137,   142,   143,   105,   460,   215,
     395,   460,   397,   142,   143,   138,   558,   198,   882,   278,
     138,   139,   138,   139,    86,   807,   568,   681,   495,   142,
     143,   142,   143,   215,   206,   134,   850,   134,   423,   424,
     212,   608,   139,   215,   262,   138,   238,   239,   243,    52,
     245,   246,   811,   138,   246,    15,   262,    17,   607,   608,
     260,   820,   447,   139,   449,   203,   142,   143,   310,    86,
      55,   220,   221,   315,   243,   137,   245,   107,    86,   660,
     262,   138,   408,   138,   138,   138,   412,   138,   138,    86,
     262,   417,   278,   143,    88,   143,    86,   138,   270,   271,
      88,   427,   143,   275,   573,   313,   432,   793,   366,   351,
     352,   807,   312,   107,    86,   567,   254,    88,   567,   136,
     137,   259,   260,   139,   820,    86,   822,   313,   807,   137,
     809,   243,   139,   245,   813,    86,   107,   139,   300,   344,
     137,    49,   243,   822,   245,   824,   136,   137,   343,    86,
      25,   343,   301,   302,   303,   304,   259,    25,   716,    58,
      59,    86,   300,   368,   136,   137,    86,   305,   494,   139,
      37,    38,   367,   143,   312,   367,   137,   315,   903,   384,
     362,   602,   661,   617,   376,   444,   137,   446,   383,   586,
     362,   383,   397,   660,    51,   103,   892,   300,    55,   136,
     137,   396,   305,   652,   396,   138,    88,   886,    55,   310,
     392,   393,   137,   892,   315,   894,   420,   137,   897,   424,
     392,   393,   414,   415,    26,   107,   229,   230,   231,   421,
     797,   369,   107,   775,   403,    86,   495,   793,    88,   107,
     566,   379,   921,    88,   449,   420,   142,   143,   797,   387,
      86,   107,    86,   448,    58,    59,   448,   107,   444,    88,
     446,    17,   107,   138,    86,   140,   435,   455,   143,    86,
     138,   142,   140,    86,   466,   143,   468,    51,   470,    53,
      54,    55,    56,   421,    86,   136,   137,    88,   389,    86,
      62,    63,   430,   835,    62,    63,   488,    55,   719,    86,
     136,   137,   136,   137,   519,   631,   107,   310,    89,   495,
      88,    25,   315,   138,   136,   137,    62,    63,   421,   136,
     137,    69,   537,   136,   137,   134,    88,   430,   138,   107,
     535,   780,   134,    26,   136,   137,   535,   475,   543,   136,
     137,   546,    14,    15,   543,   107,   254,   546,   563,   136,
     137,   543,   260,   143,   546,   547,   548,   562,   140,   564,
     138,   876,   688,   107,   138,    26,   581,   571,   141,   135,
      55,    88,   564,   555,    26,   138,   138,    69,    69,   594,
      95,   573,   574,   555,   576,   782,   389,   569,   138,   570,
     107,   788,   584,    86,    62,    10,   571,   569,   724,   402,
     403,   660,     8,   406,   312,   877,    13,   622,   557,    10,
     602,   676,   140,   138,   586,   138,   135,   138,   633,   661,
     558,   138,   664,   665,   107,    86,   138,   138,   670,   671,
     568,    51,   435,   138,    86,   438,   138,   652,   607,   627,
     628,   134,    51,   136,   137,   617,   139,   107,   597,   138,
     138,    10,     2,    15,     4,     5,     6,    10,   135,   138,
      10,    11,    12,   138,   135,    15,    16,    17,   109,    10,
      20,   379,   573,   134,   660,   136,   137,   141,   139,   387,
      10,    89,   134,     9,   136,   137,   489,   139,    10,   681,
     875,    10,   877,   138,   138,   142,   634,   635,   140,    49,
      50,   683,    10,    53,   696,   135,   891,   699,    58,   138,
     648,   683,   138,   138,    51,   653,    53,    54,    55,    56,
     138,   763,    72,   661,   135,   107,   112,   719,    51,   744,
      53,    54,    55,    56,   716,   138,   138,   725,   138,   727,
     678,   679,   734,   735,   716,   737,    10,   739,   740,   138,
      10,   101,   701,   103,   746,   135,   748,   749,    10,    10,
     698,    51,   138,    53,    54,    55,    56,   475,   112,   138,
     573,   135,    10,    55,   712,   713,   714,   138,   138,    51,
      55,    53,    54,    55,    56,    10,   138,   138,    10,    51,
     138,    53,    54,    55,    56,   781,   599,   139,   421,   139,
      90,   793,   138,    15,   607,   608,    96,    97,     6,   879,
     782,    69,   800,   801,   546,   138,   788,   627,    90,   883,
       7,   677,   625,   878,   114,    97,   288,   117,    90,    -1,
     780,   817,   637,   820,    96,    97,   774,   775,    -1,    -1,
     855,    -1,   114,   781,   189,    -1,    -1,   785,    -1,   139,
      -1,    -1,   114,   203,   657,   117,    -1,   115,   116,   117,
     118,   119,   850,    -1,   852,    -1,   858,   859,   860,   861,
     220,   221,   877,   865,   866,   867,    -1,   869,   870,    -1,
      -1,   876,    -1,    -1,   876,    -1,   878,   879,   903,    10,
      11,    12,    -1,   243,   882,   245,   246,   835,    -1,   249,
     250,    -1,    -1,   841,   254,   843,    -1,    -1,   711,   259,
     260,   849,    40,    41,    42,    43,    44,    -1,    -1,   911,
      -1,    -1,   914,   915,   916,   917,   634,   635,   278,    50,
      -1,    -1,    -1,   925,    51,    -1,    53,    54,    55,    56,
     648,    -1,    -1,    -1,    -1,   653,    -1,    -1,    -1,   752,
     300,   301,   302,   303,   304,   305,   306,   307,    -1,    -1,
     310,    -1,   312,   313,    51,   315,    53,    54,    55,    56,
     678,   679,    -1,    90,    -1,    -1,    -1,    -1,    -1,    51,
     101,    53,    54,    55,    56,    -1,    -1,    -1,    -1,    -1,
     698,    -1,    -1,   343,   797,    51,    -1,    53,    54,    55,
      56,   351,   352,    90,   712,   713,   714,    -1,    -1,    96,
      97,    -1,    -1,    -1,    -1,    -1,    -1,   367,    90,   369,
     370,    -1,    -1,    -1,    96,    97,    -1,   114,    -1,   379,
     117,    -1,    -1,   383,    90,    -1,    -1,   387,    69,    -1,
      96,    97,   114,    -1,    -1,   117,   396,    -1,    -1,    -1,
      -1,    -1,    -1,    84,    85,    -1,   143,    -1,   114,    -1,
      -1,   117,    -1,    -1,    -1,    -1,   774,   139,    -1,    -1,
      -1,   421,    -1,    -1,    -1,    16,    17,   785,    -1,    20,
     430,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
      -1,    -1,    -1,    -1,   444,    -1,   446,    -1,   448,   220,
     221,    -1,    -1,    -1,    -1,    46,    47,    -1,    -1,    -1,
      51,    52,    -1,    -1,    -1,    -1,    -1,    58,    59,    -1,
      -1,    69,    -1,    44,    -1,   475,    -1,    69,   249,   250,
      -1,    -1,    -1,   841,    -1,   843,    84,    85,    -1,    -1,
      -1,   849,    84,    85,    -1,   495,    -1,    -1,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    -1,    -1,    84,    85,    -1,   114,   115,   116,   117,
     118,   119,    -1,   115,   116,   117,   118,   119,    -1,    -1,
     301,   302,   303,   304,    -1,   306,   307,   108,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    51,
      -1,    53,    54,    55,    56,    -1,    -1,   557,   558,    -1,
      69,    70,    71,    72,    73,    74,    75,   138,   568,    78,
      79,    -1,    -1,    -1,    -1,    84,    85,    -1,    -1,     2,
      -1,     4,     5,     6,     7,    -1,    -1,    -1,    90,    -1,
      -1,    -1,    15,    -1,    96,    -1,    -1,   597,    -1,   370,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    51,    -1,    53,    54,    55,    56,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    49,    -1,    -1,    -1,
      53,    -1,    -1,    -1,   634,   635,    -1,    -1,   229,   230,
     231,   232,    -1,    -1,    -1,    -1,    -1,    -1,   648,    72,
      90,    -1,   243,   653,   245,   246,    96,    -1,    -1,    -1,
     660,   661,    -1,    -1,   664,   665,    -1,    -1,    -1,    -1,
     670,   671,    -1,    -1,    -1,    -1,    -1,    -1,   678,   679,
     103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   698,    -1,
      -1,   701,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   712,   713,   714,    -1,    -1,    -1,    -1,   310,
      -1,    -1,    -1,    -1,   315,   316,   317,   318,   319,   320,
     321,   322,   323,   324,   325,   326,   327,   328,   329,   330,
     331,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,    -1,   343,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     351,   352,    -1,   763,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   774,   775,   367,    -1,    -1,    -1,
     203,   781,    -1,    -1,    -1,   785,   557,    -1,    -1,    -1,
     381,    -1,   383,    -1,   385,   386,    -1,    -1,   389,     2,
      -1,     4,    -1,    -1,    -1,   396,    -1,    -1,    -1,    -1,
      -1,   402,   403,    -1,    -1,   406,    -1,   817,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   597,    -1,    -1,    -1,
      -1,   254,    -1,    -1,   425,   835,   259,   260,    -1,    -1,
      -1,   841,    -1,   843,   435,    -1,    49,   438,    -1,   849,
      -1,    -1,   468,    -1,   470,   278,    -1,   448,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   876,   300,    -1,    -1,
      -1,    -1,   305,    -1,    -1,    -1,    -1,   478,   479,   312,
     313,     2,    -1,     4,     5,     6,    -1,    -1,   489,    -1,
     103,    -1,    -1,    -1,    15,    -1,    -1,     0,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     8,     9,    10,    -1,    -1,
      13,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,    -1,
     701,    -1,    -1,    26,    27,    -1,    -1,    -1,    49,    -1,
      -1,    -1,    53,    -1,    37,    38,   369,    40,    41,    42,
      43,    44,    -1,    -1,    -1,    -1,   379,    -1,    -1,    -1,
      -1,    72,    -1,    -1,   387,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   573,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   103,    86,    -1,    -1,    -1,    -1,   421,    -1,
     203,    -1,    -1,    -1,    -1,    -1,    -1,   430,   599,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   607,   608,    -1,    -1,
      -1,   444,    -1,   446,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   625,    -1,    -1,    -1,    -1,    -1,
      -1,   134,   135,    -1,   137,    -1,   139,   140,    -1,   142,
     143,   254,   475,    -1,    -1,    -1,   259,   260,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   657,    -1,    -1,    -1,
     661,   662,   495,   664,   665,    -1,    -1,    -1,    -1,   670,
     671,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   680,
      -1,    -1,   203,    -1,    -1,    -1,    -1,   300,    -1,    -1,
      -1,    -1,   305,    -1,    -1,    -1,    -1,    -1,    -1,   312,
      -1,    -1,    -1,   704,   705,    -1,   707,   708,   734,   735,
     711,   737,    -1,   739,   740,    -1,    -1,    -1,    -1,    -1,
     746,    -1,   748,   749,    -1,   558,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   254,    -1,   568,    -1,    -1,   259,   260,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   752,    -1,    -1,    -1,   756,   369,   278,    -1,    -1,
      -1,    -1,   763,    -1,    -1,    -1,   379,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   387,    -1,    -1,    -1,    -1,   300,
      -1,    -1,    -1,    -1,   305,    -1,    -1,    -1,    -1,   790,
      -1,   312,   313,    -1,    -1,    -1,   797,    -1,    -1,    -1,
      -1,   634,   635,    -1,    -1,    -1,    -1,    -1,   421,    -1,
      -1,    -1,    -1,    -1,    -1,   648,    -1,   430,    -1,    -1,
     653,    -1,    -1,    -1,    -1,    -1,    -1,   660,    -1,    -1,
      -1,    -1,   858,   859,   860,   861,    -1,    -1,    -1,   865,
     866,   867,    -1,   869,   870,   678,   679,    -1,   369,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   379,    -1,
      -1,    -1,   475,    -1,    -1,   698,   387,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   876,    -1,    -1,    -1,   712,
     713,   714,    -1,    -1,    -1,   911,    -1,    -1,   914,   915,
     916,   917,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   925,
     421,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   430,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   444,    -1,   446,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   774,   775,    -1,    -1,   558,    -1,    -1,   781,    -1,
      -1,    -1,   785,    -1,   475,   568,    -1,    -1,     5,     6,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    15,    -1,
      -1,    -1,    -1,    -1,   495,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   817,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      47,    -1,   835,    -1,    51,    52,    53,    -1,   841,    -1,
     843,    -1,    59,    -1,    -1,    -1,   849,    -1,    -1,    -1,
      -1,   634,   635,    44,    -1,    72,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   648,    -1,   558,    -1,    -1,
     653,    -1,    -1,    -1,    -1,    -1,    -1,   568,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    -1,    -1,    84,    85,   678,   679,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   698,    -1,   108,    -1,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   712,
     713,   714,    69,    70,    71,    72,    73,    74,    75,    76,
      -1,    78,    79,   634,   635,    -1,    -1,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   648,    -1,    -1,
      -1,    -1,   653,    -1,    -1,    -1,    -1,    -1,    -1,   660,
      -1,    -1,    -1,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,    -1,    -1,   678,   679,    -1,
      -1,   774,   775,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    44,   785,    -1,    -1,    -1,    -1,   698,    -1,    -1,
      -1,    -1,   229,   230,   231,   232,    -1,    -1,    -1,    -1,
      -1,   712,   713,   714,    -1,    -1,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    -1,
      -1,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   835,    -1,    -1,    -1,    -1,    -1,   841,    -1,
     843,   278,    -1,    -1,    -1,   108,   849,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,    -1,    -1,
      -1,    -1,    -1,   774,   775,    -1,    -1,    -1,    -1,    -1,
     781,    -1,    -1,    -1,   785,    -1,   313,    -1,    -1,   316,
     317,   318,   319,   320,   321,   322,   323,   324,   325,   326,
     327,   328,   329,   330,   331,   332,   333,   334,   335,   336,
     337,   338,   339,   340,   341,    -1,   817,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   835,    -1,    -1,    -1,    -1,    -1,
     841,    -1,   843,    -1,    -1,    -1,    -1,    -1,   849,    -1,
      -1,    -1,    -1,    -1,   381,    -1,    -1,    -1,   385,   386,
      -1,    -1,   389,    -1,    -1,    -1,    -1,    -1,    51,    52,
      -1,    -1,    55,    -1,    -1,   402,   403,    -1,    -1,   406,
      -1,    -1,    -1,    -1,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    -1,    -1,    78,    79,    -1,   425,    82,
      83,    84,    85,    -1,    -1,    -1,    -1,    -1,   435,    -1,
      -1,   438,    -1,    96,    -1,    -1,    -1,   444,    -1,   446,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,   121,   122,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   478,   479,    -1,     0,     1,   139,     3,     4,     5,
       6,     7,   489,    -1,    -1,    11,    12,    -1,   495,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    -1,    51,    52,    53,    54,    55,
      56,    -1,    58,    59,    60,    61,    62,    -1,    64,    65,
      -1,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,   573,    93,    94,    -1,
      96,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   599,    -1,   120,   121,   122,    -1,    -1,    -1,
     607,   608,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   142,   143,   625,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     657,     0,    -1,   660,    -1,   662,    -1,    -1,    -1,     8,
       9,    10,    -1,    -1,    -1,    14,    15,    -1,    17,    -1,
      -1,    -1,    -1,   680,    -1,    -1,    -1,    26,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,
      -1,    40,    41,    42,    43,    44,    -1,   704,   705,    -1,
     707,   708,    -1,    -1,   711,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    -1,    -1,    84,    85,    86,    -1,    88,
      -1,    -1,    -1,    -1,    -1,   752,    -1,    -1,    -1,   756,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   107,   108,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,   781,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   790,    -1,   134,   135,   136,   137,   138,
     797,   140,     0,   142,   143,    -1,    -1,    -1,    -1,    -1,
       8,     9,    10,    -1,    -1,    -1,    14,    15,    -1,    17,
     817,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    -1,    40,    41,    42,    43,    44,    -1,    -1,    -1,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    -1,    -1,    84,    85,    -1,    -1,    -1,
      -1,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    -1,    -1,    84,    85,    86,   108,
      88,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   107,
     108,    -1,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,   143,    -1,    -1,    -1,    -1,    -1,
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
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    -1,    -1,    84,    85,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    -1,    93,    94,    -1,
      -1,    -1,    98,    99,    -1,    -1,    -1,   103,    -1,   108,
      -1,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,   120,    -1,    -1,    -1,    -1,    -1,
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
      -1,    -1,    69,    70,    71,    72,    73,    74,    75,    -1,
      96,    78,    79,    -1,    -1,    -1,    -1,    84,    85,    -1,
      -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,   121,   122,    -1,    51,    52,
      -1,    -1,    55,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   139,    67,    68,    69,    70,    71,    72,
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
      -1,    51,    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   139,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    -1,    -1,    78,    79,
      -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,   121,   122,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    -1,    -1,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119
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
     278,   290,   149,    21,    22,    30,    31,    32,    39,    51,
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
     253,   255,   158,   142,   143,   158,   282,   287,   288,   289,
     199,   201,    87,   164,   171,   212,   217,   252,   255,    57,
      96,    97,   121,   163,   181,   182,   187,   190,   192,   276,
     277,   187,   187,   139,   188,   189,   139,   184,   188,   139,
     283,   288,   175,   150,   134,   181,   212,   181,    55,     1,
      90,   152,   153,   154,   165,   166,   290,   157,   197,   183,
     192,   276,   290,   182,   275,   276,   290,    87,   138,   170,
     212,   252,   255,   198,    53,    54,    56,    62,   103,   174,
     250,    62,    63,   244,    58,    59,   159,   181,   181,   282,
     289,    40,    41,    42,    43,    44,    37,    38,    28,   229,
     107,   138,    90,    96,   167,   107,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    84,
      85,   108,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    86,   136,   137,    86,   137,   281,    26,   134,
     233,    88,    88,   184,   188,   233,   157,    51,    55,   172,
      58,    59,     1,   111,   256,   287,    86,   136,   137,   208,
     274,   209,   281,   103,   138,   151,   152,    55,    13,   213,
     287,   107,    86,   136,   137,    88,    88,   213,   282,    17,
     236,   142,   158,   158,    55,    86,   136,   137,    25,   182,
     182,   182,    89,   138,   191,   290,   138,   191,   187,   283,
     284,   187,   186,   187,   192,   276,   290,   157,   284,   157,
     155,   134,   152,    86,   137,    88,   154,   165,   140,   282,
     289,   284,   196,   284,   141,   138,   286,   288,   138,   286,
     135,   286,    55,   167,   168,   169,   138,    86,   136,   137,
      51,    53,    54,    55,    56,    90,    96,    97,   114,   117,
     139,   227,   259,   260,   261,   262,   263,   264,   267,   268,
     269,   270,   271,    62,   244,   245,    62,    63,    69,    69,
     149,   158,   158,   158,   158,   154,   157,   157,   230,    96,
     159,   182,   192,   193,   165,   138,   170,   138,   156,   159,
     171,   181,   182,   193,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
      51,    52,    55,   179,   184,   279,   280,   186,    51,    52,
      55,   179,   184,   279,    51,    55,   279,   235,   234,   159,
     181,   159,   181,    95,   161,   206,   287,   257,   205,    51,
      55,   172,   279,   186,   279,   151,   157,   139,   258,   259,
     210,   178,    10,     8,   238,   290,   152,    13,   181,    51,
      55,   186,    51,    55,   152,   236,   192,    10,    27,   214,
     287,   214,    51,    55,   186,    51,    55,   203,   182,    96,
     182,   190,   276,   277,   284,   140,   284,   138,   138,   284,
     175,   147,   135,   181,   284,   154,   284,   276,   167,   169,
      51,    55,   186,    51,    55,   107,    51,    90,    96,   218,
     219,   220,   261,   259,    29,   105,   228,   138,   272,   290,
     138,   272,    51,   138,   272,    51,    62,   152,   246,   181,
     181,    77,   112,   222,   223,   290,   182,   138,   284,   169,
     138,   107,    44,   283,    88,    88,   184,   188,   283,   285,
      88,    88,   184,   185,   188,   290,   185,   188,   222,   222,
      44,   162,   287,   158,   151,   285,    10,   284,   259,   151,
     287,   174,   175,   176,   182,   193,   239,   290,    15,   216,
     290,    14,   215,   216,    88,    88,   285,    88,    88,   216,
      10,   138,   213,   200,   202,   285,   158,   182,   191,   276,
     135,   286,   285,   182,   220,   138,   261,   138,   284,   224,
     283,   152,   152,   262,   267,   269,   271,   263,   264,   269,
     263,   135,   152,    51,   221,   224,   263,   265,   266,   269,
     271,   152,    96,   182,   169,   181,   109,   159,   181,   159,
     181,   161,   141,    88,   159,   181,   159,   181,   161,   233,
     229,   152,   152,   181,   222,   207,   287,    10,   284,    10,
     211,    89,   240,   290,   152,     9,   241,   290,   158,    10,
      88,    10,   182,   152,   152,   152,   214,   138,   284,   219,
     138,    96,   218,   140,   142,    10,   135,   138,   272,   138,
     272,   138,   272,   138,   272,   272,   135,   107,   224,   112,
     138,   272,   138,   272,   138,   272,    10,   182,   181,   159,
     181,    10,   135,   152,   151,   258,    87,   171,   212,   252,
     255,   213,   152,   213,   216,   236,   237,    10,    10,   204,
     138,   219,   138,   261,    51,   225,   226,   260,   263,   269,
     263,   263,    87,   212,   112,   266,   269,   263,   265,   269,
     263,   135,    10,   151,    55,    86,   136,   137,   152,   152,
     152,   219,   138,   138,   283,   272,   138,   272,   272,   272,
      55,    86,   138,   272,   138,   272,   272,   138,   272,   272,
      10,    51,    55,   186,    51,    55,   238,   215,    10,   219,
     226,   263,    51,    55,   263,   269,   263,   263,   285,   272,
     272,   138,   272,   272,   272,   263,   272
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

/* Line 1806 of yacc.c  */
#line 996 "./parse.y"
    {
		     p->lstate = EXPR_BEG;
		     if (!p->locals) p->locals = cons(0,0);
		   }
    break;

  case 3:

/* Line 1806 of yacc.c  */
#line 1001 "./parse.y"
    {
		      p->tree = new_scope(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 4:

/* Line 1806 of yacc.c  */
#line 1007 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 5:

/* Line 1806 of yacc.c  */
#line 1013 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 6:

/* Line 1806 of yacc.c  */
#line 1017 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 1021 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), newline_node((yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 1025 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 10:

/* Line 1806 of yacc.c  */
#line 1032 "./parse.y"
    {
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 1036 "./parse.y"
    {
		      yyerror(p, "BEGIN not supported");
		      local_resume(p, (yyvsp[(2) - (5)].nd));
		      (yyval.nd) = 0;
		    }
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 1047 "./parse.y"
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

/* Line 1806 of yacc.c  */
#line 1070 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 1076 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 1080 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 1084 "./parse.y"
    {
			(yyval.nd) = push((yyvsp[(1) - (3)].nd), newline_node((yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 17:

/* Line 1806 of yacc.c  */
#line 1088 "./parse.y"
    {
		      (yyval.nd) = new_begin(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 1093 "./parse.y"
    {p->lstate = EXPR_FNAME;}
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 1094 "./parse.y"
    {
		      (yyval.nd) = new_alias(p, (yyvsp[(2) - (4)].id), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 1098 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 1102 "./parse.y"
    {
			(yyval.nd) = new_if(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd), 0);
		    }
    break;

  case 22:

/* Line 1806 of yacc.c  */
#line 1106 "./parse.y"
    {
		      (yyval.nd) = new_unless(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd), 0);
		    }
    break;

  case 23:

/* Line 1806 of yacc.c  */
#line 1110 "./parse.y"
    {
		      (yyval.nd) = new_while(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd));
		    }
    break;

  case 24:

/* Line 1806 of yacc.c  */
#line 1114 "./parse.y"
    {
		      (yyval.nd) = new_until(p, cond((yyvsp[(3) - (3)].nd)), (yyvsp[(1) - (3)].nd));
		    }
    break;

  case 25:

/* Line 1806 of yacc.c  */
#line 1118 "./parse.y"
    {
		      (yyval.nd) = new_rescue(p, (yyvsp[(1) - (3)].nd), list1(list3(0, 0, (yyvsp[(3) - (3)].nd))), 0);
		    }
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 1122 "./parse.y"
    {
		      yyerror(p, "END not suported");
		      (yyval.nd) = new_postexe(p, (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 28:

/* Line 1806 of yacc.c  */
#line 1128 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(1) - (3)].nd), list1((yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 29:

/* Line 1806 of yacc.c  */
#line 1132 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 30:

/* Line 1806 of yacc.c  */
#line 1136 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].nd), intern("[]"), (yyvsp[(3) - (6)].nd)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 1140 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 1144 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 33:

/* Line 1806 of yacc.c  */
#line 1148 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.nd) = 0;
		    }
    break;

  case 34:

/* Line 1806 of yacc.c  */
#line 1153 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 35:

/* Line 1806 of yacc.c  */
#line 1157 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].nd));
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 36:

/* Line 1806 of yacc.c  */
#line 1162 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), new_array(p, (yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 37:

/* Line 1806 of yacc.c  */
#line 1166 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 38:

/* Line 1806 of yacc.c  */
#line 1170 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(1) - (3)].nd), new_array(p, (yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 40:

/* Line 1806 of yacc.c  */
#line 1177 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 1181 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 1189 "./parse.y"
    {
		      (yyval.nd) = new_and(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 44:

/* Line 1806 of yacc.c  */
#line 1193 "./parse.y"
    {
		      (yyval.nd) = new_or(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 45:

/* Line 1806 of yacc.c  */
#line 1197 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(3) - (3)].nd)), "!");
		    }
    break;

  case 46:

/* Line 1806 of yacc.c  */
#line 1201 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(2) - (2)].nd)), "!");
		    }
    break;

  case 48:

/* Line 1806 of yacc.c  */
#line 1208 "./parse.y"
    {
		      if (!(yyvsp[(1) - (1)].nd)) (yyval.nd) = new_nil(p);
		      else (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 53:

/* Line 1806 of yacc.c  */
#line 1223 "./parse.y"
    {
		      local_nest(p);
		    }
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 1229 "./parse.y"
    {
		      (yyval.nd) = new_block(p, (yyvsp[(3) - (5)].nd), (yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    }
    break;

  case 55:

/* Line 1806 of yacc.c  */
#line 1236 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 1240 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(2) - (3)].nd), (yyvsp[(3) - (3)].nd));
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (3)].id), (yyvsp[(2) - (3)].nd));
		    }
    break;

  case 57:

/* Line 1806 of yacc.c  */
#line 1245 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 58:

/* Line 1806 of yacc.c  */
#line 1249 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].nd), (yyvsp[(5) - (5)].nd));
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		   }
    break;

  case 59:

/* Line 1806 of yacc.c  */
#line 1254 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 60:

/* Line 1806 of yacc.c  */
#line 1258 "./parse.y"
    {
		      args_with_block(p, (yyvsp[(4) - (5)].nd), (yyvsp[(5) - (5)].nd));
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		    }
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 1263 "./parse.y"
    {
		      (yyval.nd) = new_super(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 62:

/* Line 1806 of yacc.c  */
#line 1267 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 63:

/* Line 1806 of yacc.c  */
#line 1271 "./parse.y"
    {
		      (yyval.nd) = new_return(p, ret_args(p, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 1275 "./parse.y"
    {
		      (yyval.nd) = new_break(p, ret_args(p, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 65:

/* Line 1806 of yacc.c  */
#line 1279 "./parse.y"
    {
		      (yyval.nd) = new_next(p, ret_args(p, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 66:

/* Line 1806 of yacc.c  */
#line 1285 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 1289 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 1296 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(2) - (3)].nd));
		    }
    break;

  case 70:

/* Line 1806 of yacc.c  */
#line 1302 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 71:

/* Line 1806 of yacc.c  */
#line 1306 "./parse.y"
    {
		      (yyval.nd) = list1(push((yyvsp[(1) - (2)].nd),(yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 72:

/* Line 1806 of yacc.c  */
#line 1310 "./parse.y"
    {
		      (yyval.nd) = list2((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 73:

/* Line 1806 of yacc.c  */
#line 1314 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].nd), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 74:

/* Line 1806 of yacc.c  */
#line 1318 "./parse.y"
    {
		      (yyval.nd) = list2((yyvsp[(1) - (2)].nd), new_nil(p));
		    }
    break;

  case 75:

/* Line 1806 of yacc.c  */
#line 1322 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (4)].nd), new_nil(p), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 76:

/* Line 1806 of yacc.c  */
#line 1326 "./parse.y"
    {
		      (yyval.nd) = list2(0, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 77:

/* Line 1806 of yacc.c  */
#line 1330 "./parse.y"
    {
		      (yyval.nd) = list3(0, (yyvsp[(2) - (4)].nd), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 78:

/* Line 1806 of yacc.c  */
#line 1334 "./parse.y"
    {
		      (yyval.nd) = list2(0, new_nil(p));
		    }
    break;

  case 79:

/* Line 1806 of yacc.c  */
#line 1338 "./parse.y"
    {
		      (yyval.nd) = list3(0, new_nil(p), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 81:

/* Line 1806 of yacc.c  */
#line 1345 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 82:

/* Line 1806 of yacc.c  */
#line 1351 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (2)].nd));
		    }
    break;

  case 83:

/* Line 1806 of yacc.c  */
#line 1355 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(2) - (3)].nd));
		    }
    break;

  case 84:

/* Line 1806 of yacc.c  */
#line 1361 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 85:

/* Line 1806 of yacc.c  */
#line 1365 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 86:

/* Line 1806 of yacc.c  */
#line 1371 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 87:

/* Line 1806 of yacc.c  */
#line 1375 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), intern("[]"), (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 88:

/* Line 1806 of yacc.c  */
#line 1379 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 89:

/* Line 1806 of yacc.c  */
#line 1383 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 90:

/* Line 1806 of yacc.c  */
#line 1387 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 1391 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 92:

/* Line 1806 of yacc.c  */
#line 1397 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 93:

/* Line 1806 of yacc.c  */
#line 1403 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].nd));
		      (yyval.nd) = 0;
		    }
    break;

  case 94:

/* Line 1806 of yacc.c  */
#line 1410 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 95:

/* Line 1806 of yacc.c  */
#line 1414 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), intern("[]"), (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 96:

/* Line 1806 of yacc.c  */
#line 1418 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 97:

/* Line 1806 of yacc.c  */
#line 1422 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 98:

/* Line 1806 of yacc.c  */
#line 1426 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 99:

/* Line 1806 of yacc.c  */
#line 1430 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 100:

/* Line 1806 of yacc.c  */
#line 1436 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "dynamic constant assignment");
		      (yyval.nd) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 101:

/* Line 1806 of yacc.c  */
#line 1442 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (1)].nd));
		      (yyval.nd) = 0;
		    }
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 1449 "./parse.y"
    {
		      yyerror(p, "class/module name must be CONSTANT");
		    }
    break;

  case 104:

/* Line 1806 of yacc.c  */
#line 1456 "./parse.y"
    {
		      (yyval.nd) = cons((node*)1, nsym((yyvsp[(2) - (2)].id)));
		    }
    break;

  case 105:

/* Line 1806 of yacc.c  */
#line 1460 "./parse.y"
    {
		      (yyval.nd) = cons((node*)0, nsym((yyvsp[(1) - (1)].id)));
		    }
    break;

  case 106:

/* Line 1806 of yacc.c  */
#line 1464 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (3)].nd), nsym((yyvsp[(3) - (3)].id)));
		    }
    break;

  case 110:

/* Line 1806 of yacc.c  */
#line 1473 "./parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    }
    break;

  case 111:

/* Line 1806 of yacc.c  */
#line 1478 "./parse.y"
    {
		      p->lstate = EXPR_ENDFN;
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    }
    break;

  case 114:

/* Line 1806 of yacc.c  */
#line 1489 "./parse.y"
    {
		      (yyval.nd) = new_sym(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 115:

/* Line 1806 of yacc.c  */
#line 1495 "./parse.y"
    {
		      (yyval.nd) = new_undef(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 116:

/* Line 1806 of yacc.c  */
#line 1498 "./parse.y"
    {p->lstate = EXPR_FNAME;}
    break;

  case 117:

/* Line 1806 of yacc.c  */
#line 1499 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), (node*)(yyvsp[(4) - (4)].nd));
		    }
    break;

  case 118:

/* Line 1806 of yacc.c  */
#line 1504 "./parse.y"
    { (yyval.id) = intern("|"); }
    break;

  case 119:

/* Line 1806 of yacc.c  */
#line 1505 "./parse.y"
    { (yyval.id) = intern("^"); }
    break;

  case 120:

/* Line 1806 of yacc.c  */
#line 1506 "./parse.y"
    { (yyval.id) = intern("&"); }
    break;

  case 121:

/* Line 1806 of yacc.c  */
#line 1507 "./parse.y"
    { (yyval.id) = intern("<=>"); }
    break;

  case 122:

/* Line 1806 of yacc.c  */
#line 1508 "./parse.y"
    { (yyval.id) = intern("=="); }
    break;

  case 123:

/* Line 1806 of yacc.c  */
#line 1509 "./parse.y"
    { (yyval.id) = intern("==="); }
    break;

  case 124:

/* Line 1806 of yacc.c  */
#line 1510 "./parse.y"
    { (yyval.id) = intern("=~"); }
    break;

  case 125:

/* Line 1806 of yacc.c  */
#line 1511 "./parse.y"
    { (yyval.id) = intern("!~"); }
    break;

  case 126:

/* Line 1806 of yacc.c  */
#line 1512 "./parse.y"
    { (yyval.id) = intern(">"); }
    break;

  case 127:

/* Line 1806 of yacc.c  */
#line 1513 "./parse.y"
    { (yyval.id) = intern(">="); }
    break;

  case 128:

/* Line 1806 of yacc.c  */
#line 1514 "./parse.y"
    { (yyval.id) = intern("<"); }
    break;

  case 129:

/* Line 1806 of yacc.c  */
#line 1515 "./parse.y"
    { (yyval.id) = intern("<="); }
    break;

  case 130:

/* Line 1806 of yacc.c  */
#line 1516 "./parse.y"
    { (yyval.id) = intern("!="); }
    break;

  case 131:

/* Line 1806 of yacc.c  */
#line 1517 "./parse.y"
    { (yyval.id) = intern("<<"); }
    break;

  case 132:

/* Line 1806 of yacc.c  */
#line 1518 "./parse.y"
    { (yyval.id) = intern(">>"); }
    break;

  case 133:

/* Line 1806 of yacc.c  */
#line 1519 "./parse.y"
    { (yyval.id) = intern("+"); }
    break;

  case 134:

/* Line 1806 of yacc.c  */
#line 1520 "./parse.y"
    { (yyval.id) = intern("-"); }
    break;

  case 135:

/* Line 1806 of yacc.c  */
#line 1521 "./parse.y"
    { (yyval.id) = intern("*"); }
    break;

  case 136:

/* Line 1806 of yacc.c  */
#line 1522 "./parse.y"
    { (yyval.id) = intern("*"); }
    break;

  case 137:

/* Line 1806 of yacc.c  */
#line 1523 "./parse.y"
    { (yyval.id) = intern("/"); }
    break;

  case 138:

/* Line 1806 of yacc.c  */
#line 1524 "./parse.y"
    { (yyval.id) = intern("%"); }
    break;

  case 139:

/* Line 1806 of yacc.c  */
#line 1525 "./parse.y"
    { (yyval.id) = intern("**"); }
    break;

  case 140:

/* Line 1806 of yacc.c  */
#line 1526 "./parse.y"
    { (yyval.id) = intern("!"); }
    break;

  case 141:

/* Line 1806 of yacc.c  */
#line 1527 "./parse.y"
    { (yyval.id) = intern("~"); }
    break;

  case 142:

/* Line 1806 of yacc.c  */
#line 1528 "./parse.y"
    { (yyval.id) = intern("+@"); }
    break;

  case 143:

/* Line 1806 of yacc.c  */
#line 1529 "./parse.y"
    { (yyval.id) = intern("-@"); }
    break;

  case 144:

/* Line 1806 of yacc.c  */
#line 1530 "./parse.y"
    { (yyval.id) = intern("[]"); }
    break;

  case 145:

/* Line 1806 of yacc.c  */
#line 1531 "./parse.y"
    { (yyval.id) = intern("[]="); }
    break;

  case 186:

/* Line 1806 of yacc.c  */
#line 1549 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 187:

/* Line 1806 of yacc.c  */
#line 1553 "./parse.y"
    {
		      (yyval.nd) = new_asgn(p, (yyvsp[(1) - (5)].nd), new_rescue(p, (yyvsp[(3) - (5)].nd), list1(list3(0, 0, (yyvsp[(5) - (5)].nd))), 0));
		    }
    break;

  case 188:

/* Line 1806 of yacc.c  */
#line 1557 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, (yyvsp[(1) - (3)].nd), (yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 189:

/* Line 1806 of yacc.c  */
#line 1561 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, (yyvsp[(1) - (5)].nd), (yyvsp[(2) - (5)].id), new_rescue(p, (yyvsp[(3) - (5)].nd), list1(list3(0, 0, (yyvsp[(5) - (5)].nd))), 0));
		    }
    break;

  case 190:

/* Line 1806 of yacc.c  */
#line 1565 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (6)].nd), intern("[]"), (yyvsp[(3) - (6)].nd)), (yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 191:

/* Line 1806 of yacc.c  */
#line 1569 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 192:

/* Line 1806 of yacc.c  */
#line 1573 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 193:

/* Line 1806 of yacc.c  */
#line 1577 "./parse.y"
    {
		      (yyval.nd) = new_op_asgn(p, new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), 0), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 194:

/* Line 1806 of yacc.c  */
#line 1581 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 195:

/* Line 1806 of yacc.c  */
#line 1586 "./parse.y"
    {
		      yyerror(p, "constant re-assignment");
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 196:

/* Line 1806 of yacc.c  */
#line 1591 "./parse.y"
    {
		      backref_error(p, (yyvsp[(1) - (3)].nd));
		      (yyval.nd) = new_begin(p, 0);
		    }
    break;

  case 197:

/* Line 1806 of yacc.c  */
#line 1596 "./parse.y"
    {
		      (yyval.nd) = new_dot2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 198:

/* Line 1806 of yacc.c  */
#line 1600 "./parse.y"
    {
		      (yyval.nd) = new_dot3(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 199:

/* Line 1806 of yacc.c  */
#line 1604 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "+", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 200:

/* Line 1806 of yacc.c  */
#line 1608 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "-", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 201:

/* Line 1806 of yacc.c  */
#line 1612 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "*", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 202:

/* Line 1806 of yacc.c  */
#line 1616 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "/", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 203:

/* Line 1806 of yacc.c  */
#line 1620 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "%", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 204:

/* Line 1806 of yacc.c  */
#line 1624 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "**", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 205:

/* Line 1806 of yacc.c  */
#line 1628 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].nd), "**", (yyvsp[(4) - (4)].nd)), "-@");
		    }
    break;

  case 206:

/* Line 1806 of yacc.c  */
#line 1632 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, call_bin_op(p, (yyvsp[(2) - (4)].nd), "**", (yyvsp[(4) - (4)].nd)), "-@");
		    }
    break;

  case 207:

/* Line 1806 of yacc.c  */
#line 1636 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, (yyvsp[(2) - (2)].nd), "+@");
		    }
    break;

  case 208:

/* Line 1806 of yacc.c  */
#line 1640 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, (yyvsp[(2) - (2)].nd), "-@");
		    }
    break;

  case 209:

/* Line 1806 of yacc.c  */
#line 1644 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "|", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 210:

/* Line 1806 of yacc.c  */
#line 1648 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "^", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 211:

/* Line 1806 of yacc.c  */
#line 1652 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "&", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 212:

/* Line 1806 of yacc.c  */
#line 1656 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<=>", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 213:

/* Line 1806 of yacc.c  */
#line 1660 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), ">", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 214:

/* Line 1806 of yacc.c  */
#line 1664 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), ">=", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 215:

/* Line 1806 of yacc.c  */
#line 1668 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 216:

/* Line 1806 of yacc.c  */
#line 1672 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<=", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 217:

/* Line 1806 of yacc.c  */
#line 1676 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "==", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 218:

/* Line 1806 of yacc.c  */
#line 1680 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "===", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 219:

/* Line 1806 of yacc.c  */
#line 1684 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "!=", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 220:

/* Line 1806 of yacc.c  */
#line 1688 "./parse.y"
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

/* Line 1806 of yacc.c  */
#line 1697 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "!~", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 222:

/* Line 1806 of yacc.c  */
#line 1701 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(2) - (2)].nd)), "!");
		    }
    break;

  case 223:

/* Line 1806 of yacc.c  */
#line 1705 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(2) - (2)].nd)), "~");
		    }
    break;

  case 224:

/* Line 1806 of yacc.c  */
#line 1709 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), "<<", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 225:

/* Line 1806 of yacc.c  */
#line 1713 "./parse.y"
    {
		      (yyval.nd) = call_bin_op(p, (yyvsp[(1) - (3)].nd), ">>", (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 226:

/* Line 1806 of yacc.c  */
#line 1717 "./parse.y"
    {
		      (yyval.nd) = new_and(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 227:

/* Line 1806 of yacc.c  */
#line 1721 "./parse.y"
    {
		      (yyval.nd) = new_or(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 228:

/* Line 1806 of yacc.c  */
#line 1725 "./parse.y"
    {
		      (yyval.nd) = new_if(p, cond((yyvsp[(1) - (6)].nd)), (yyvsp[(3) - (6)].nd), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 229:

/* Line 1806 of yacc.c  */
#line 1729 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 230:

/* Line 1806 of yacc.c  */
#line 1735 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		      if (!(yyval.nd)) (yyval.nd) = new_nil(p);
		    }
    break;

  case 232:

/* Line 1806 of yacc.c  */
#line 1743 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 233:

/* Line 1806 of yacc.c  */
#line 1747 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), new_hash(p, (yyvsp[(3) - (4)].nd)));
		    }
    break;

  case 234:

/* Line 1806 of yacc.c  */
#line 1751 "./parse.y"
    {
		      (yyval.nd) = new_hash(p, (yyvsp[(1) - (2)].nd));
		    }
    break;

  case 235:

/* Line 1806 of yacc.c  */
#line 1757 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 240:

/* Line 1806 of yacc.c  */
#line 1769 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (2)].nd),0);
		    }
    break;

  case 241:

/* Line 1806 of yacc.c  */
#line 1773 "./parse.y"
    {
		      (yyval.nd) = cons(push((yyvsp[(1) - (4)].nd), new_hash(p, (yyvsp[(3) - (4)].nd))), 0);
		    }
    break;

  case 242:

/* Line 1806 of yacc.c  */
#line 1777 "./parse.y"
    {
		      (yyval.nd) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].nd))), 0);
		    }
    break;

  case 243:

/* Line 1806 of yacc.c  */
#line 1783 "./parse.y"
    {
		      (yyval.nd) = cons(list1((yyvsp[(1) - (1)].nd)), 0);
		    }
    break;

  case 244:

/* Line 1806 of yacc.c  */
#line 1787 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 245:

/* Line 1806 of yacc.c  */
#line 1791 "./parse.y"
    {
		      (yyval.nd) = cons(list1(new_hash(p, (yyvsp[(1) - (2)].nd))), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 246:

/* Line 1806 of yacc.c  */
#line 1795 "./parse.y"
    {
		      (yyval.nd) = cons(push((yyvsp[(1) - (4)].nd), new_hash(p, (yyvsp[(3) - (4)].nd))), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 247:

/* Line 1806 of yacc.c  */
#line 1799 "./parse.y"
    {
		      (yyval.nd) = cons(0, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 248:

/* Line 1806 of yacc.c  */
#line 1804 "./parse.y"
    {
		      (yyval.stack) = p->cmdarg_stack;
		      CMDARG_PUSH(1);
		    }
    break;

  case 249:

/* Line 1806 of yacc.c  */
#line 1809 "./parse.y"
    {
		      p->cmdarg_stack = (yyvsp[(1) - (2)].stack);
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 250:

/* Line 1806 of yacc.c  */
#line 1816 "./parse.y"
    {
		      (yyval.nd) = new_block_arg(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 251:

/* Line 1806 of yacc.c  */
#line 1822 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 252:

/* Line 1806 of yacc.c  */
#line 1826 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 253:

/* Line 1806 of yacc.c  */
#line 1832 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (1)].nd), 0);
		    }
    break;

  case 254:

/* Line 1806 of yacc.c  */
#line 1836 "./parse.y"
    {
		      (yyval.nd) = cons(new_splat(p, (yyvsp[(2) - (2)].nd)), 0);
		    }
    break;

  case 255:

/* Line 1806 of yacc.c  */
#line 1840 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 256:

/* Line 1806 of yacc.c  */
#line 1844 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), new_splat(p, (yyvsp[(4) - (4)].nd)));
		    }
    break;

  case 257:

/* Line 1806 of yacc.c  */
#line 1850 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 258:

/* Line 1806 of yacc.c  */
#line 1854 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (4)].nd), new_splat(p, (yyvsp[(4) - (4)].nd)));
		    }
    break;

  case 259:

/* Line 1806 of yacc.c  */
#line 1858 "./parse.y"
    {
		      (yyval.nd) = list1(new_splat(p, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 265:

/* Line 1806 of yacc.c  */
#line 1869 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (1)].id), 0);
		    }
    break;

  case 266:

/* Line 1806 of yacc.c  */
#line 1873 "./parse.y"
    {
		      (yyvsp[(1) - (1)].stack) = p->cmdarg_stack;
		      p->cmdarg_stack = 0;
		    }
    break;

  case 267:

/* Line 1806 of yacc.c  */
#line 1879 "./parse.y"
    {
		      p->cmdarg_stack = (yyvsp[(1) - (4)].stack);
		      (yyval.nd) = (yyvsp[(3) - (4)].nd);
		    }
    break;

  case 268:

/* Line 1806 of yacc.c  */
#line 1883 "./parse.y"
    {p->lstate = EXPR_ENDARG;}
    break;

  case 269:

/* Line 1806 of yacc.c  */
#line 1884 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (4)].nd);
		    }
    break;

  case 270:

/* Line 1806 of yacc.c  */
#line 1887 "./parse.y"
    {p->lstate = EXPR_ENDARG;}
    break;

  case 271:

/* Line 1806 of yacc.c  */
#line 1888 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 272:

/* Line 1806 of yacc.c  */
#line 1892 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 273:

/* Line 1806 of yacc.c  */
#line 1896 "./parse.y"
    {
		      (yyval.nd) = new_colon2(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 274:

/* Line 1806 of yacc.c  */
#line 1900 "./parse.y"
    {
		      (yyval.nd) = new_colon3(p, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 275:

/* Line 1806 of yacc.c  */
#line 1904 "./parse.y"
    {
		      (yyval.nd) = new_array(p, (yyvsp[(2) - (3)].nd));
		    }
    break;

  case 276:

/* Line 1806 of yacc.c  */
#line 1908 "./parse.y"
    {
		      (yyval.nd) = new_hash(p, (yyvsp[(2) - (3)].nd));
		    }
    break;

  case 277:

/* Line 1806 of yacc.c  */
#line 1912 "./parse.y"
    {
		      (yyval.nd) = new_return(p, 0);
		    }
    break;

  case 278:

/* Line 1806 of yacc.c  */
#line 1916 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 279:

/* Line 1806 of yacc.c  */
#line 1920 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, 0);
		    }
    break;

  case 280:

/* Line 1806 of yacc.c  */
#line 1924 "./parse.y"
    {
		      (yyval.nd) = new_yield(p, 0);
		    }
    break;

  case 281:

/* Line 1806 of yacc.c  */
#line 1928 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, cond((yyvsp[(3) - (4)].nd)), "!");
		    }
    break;

  case 282:

/* Line 1806 of yacc.c  */
#line 1932 "./parse.y"
    {
		      (yyval.nd) = call_uni_op(p, new_nil(p), "!");
		    }
    break;

  case 283:

/* Line 1806 of yacc.c  */
#line 1936 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (2)].id), cons(0, (yyvsp[(2) - (2)].nd)));
		    }
    break;

  case 285:

/* Line 1806 of yacc.c  */
#line 1941 "./parse.y"
    {
		      call_with_block(p, (yyvsp[(1) - (2)].nd), (yyvsp[(2) - (2)].nd));
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 286:

/* Line 1806 of yacc.c  */
#line 1946 "./parse.y"
    {
		      local_nest(p);
		      (yyval.num) = p->lpar_beg;
		      p->lpar_beg = ++p->paren_nest;
		    }
    break;

  case 287:

/* Line 1806 of yacc.c  */
#line 1953 "./parse.y"
    {
		      p->lpar_beg = (yyvsp[(2) - (4)].num);
		      (yyval.nd) = new_lambda(p, (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].nd));
		      local_unnest(p);
		    }
    break;

  case 288:

/* Line 1806 of yacc.c  */
#line 1962 "./parse.y"
    {
		      (yyval.nd) = new_if(p, cond((yyvsp[(2) - (6)].nd)), (yyvsp[(4) - (6)].nd), (yyvsp[(5) - (6)].nd));
		    }
    break;

  case 289:

/* Line 1806 of yacc.c  */
#line 1969 "./parse.y"
    {
		      (yyval.nd) = new_unless(p, cond((yyvsp[(2) - (6)].nd)), (yyvsp[(4) - (6)].nd), (yyvsp[(5) - (6)].nd));
		    }
    break;

  case 290:

/* Line 1806 of yacc.c  */
#line 1972 "./parse.y"
    {COND_PUSH(1);}
    break;

  case 291:

/* Line 1806 of yacc.c  */
#line 1972 "./parse.y"
    {COND_POP();}
    break;

  case 292:

/* Line 1806 of yacc.c  */
#line 1975 "./parse.y"
    {
		      (yyval.nd) = new_while(p, cond((yyvsp[(3) - (7)].nd)), (yyvsp[(6) - (7)].nd));
		    }
    break;

  case 293:

/* Line 1806 of yacc.c  */
#line 1978 "./parse.y"
    {COND_PUSH(1);}
    break;

  case 294:

/* Line 1806 of yacc.c  */
#line 1978 "./parse.y"
    {COND_POP();}
    break;

  case 295:

/* Line 1806 of yacc.c  */
#line 1981 "./parse.y"
    {
		      (yyval.nd) = new_until(p, cond((yyvsp[(3) - (7)].nd)), (yyvsp[(6) - (7)].nd));
		    }
    break;

  case 296:

/* Line 1806 of yacc.c  */
#line 1987 "./parse.y"
    {
		      (yyval.nd) = new_case(p, (yyvsp[(2) - (5)].nd), (yyvsp[(4) - (5)].nd));
		    }
    break;

  case 297:

/* Line 1806 of yacc.c  */
#line 1991 "./parse.y"
    {
		      (yyval.nd) = new_case(p, 0, (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 298:

/* Line 1806 of yacc.c  */
#line 1995 "./parse.y"
    {COND_PUSH(1);}
    break;

  case 299:

/* Line 1806 of yacc.c  */
#line 1997 "./parse.y"
    {COND_POP();}
    break;

  case 300:

/* Line 1806 of yacc.c  */
#line 2000 "./parse.y"
    {
		      (yyval.nd) = new_for(p, (yyvsp[(2) - (9)].nd), (yyvsp[(5) - (9)].nd), (yyvsp[(8) - (9)].nd));
		    }
    break;

  case 301:

/* Line 1806 of yacc.c  */
#line 2004 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "class definition in method body");
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 302:

/* Line 1806 of yacc.c  */
#line 2011 "./parse.y"
    {
		      (yyval.nd) = new_class(p, (yyvsp[(2) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].nd));
		      local_resume(p, (yyvsp[(4) - (6)].nd));
		    }
    break;

  case 303:

/* Line 1806 of yacc.c  */
#line 2016 "./parse.y"
    {
		      (yyval.num) = p->in_def;
		      p->in_def = 0;
		    }
    break;

  case 304:

/* Line 1806 of yacc.c  */
#line 2021 "./parse.y"
    {
		      (yyval.nd) = cons(local_switch(p), (node*)(intptr_t)p->in_single);
		      p->in_single = 0;
		    }
    break;

  case 305:

/* Line 1806 of yacc.c  */
#line 2027 "./parse.y"
    {
		      (yyval.nd) = new_sclass(p, (yyvsp[(3) - (8)].nd), (yyvsp[(7) - (8)].nd));
		      local_resume(p, (yyvsp[(6) - (8)].nd)->car);
		      p->in_def = (yyvsp[(4) - (8)].num);
		      p->in_single = (int)(intptr_t)(yyvsp[(6) - (8)].nd)->cdr;
		    }
    break;

  case 306:

/* Line 1806 of yacc.c  */
#line 2034 "./parse.y"
    {
		      if (p->in_def || p->in_single)
			yyerror(p, "module definition in method body");
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 307:

/* Line 1806 of yacc.c  */
#line 2041 "./parse.y"
    {
		      (yyval.nd) = new_module(p, (yyvsp[(2) - (5)].nd), (yyvsp[(4) - (5)].nd));
		      local_resume(p, (yyvsp[(3) - (5)].nd));
		    }
    break;

  case 308:

/* Line 1806 of yacc.c  */
#line 2046 "./parse.y"
    {
		      p->in_def++;
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 309:

/* Line 1806 of yacc.c  */
#line 2053 "./parse.y"
    {
		      (yyval.nd) = new_def(p, (yyvsp[(2) - (6)].id), (yyvsp[(4) - (6)].nd), (yyvsp[(5) - (6)].nd));
		      local_resume(p, (yyvsp[(3) - (6)].nd));
		      p->in_def--;
		    }
    break;

  case 310:

/* Line 1806 of yacc.c  */
#line 2058 "./parse.y"
    {p->lstate = EXPR_FNAME;}
    break;

  case 311:

/* Line 1806 of yacc.c  */
#line 2059 "./parse.y"
    {
		      p->in_single++;
		      p->lstate = EXPR_ENDFN; /* force for args */
		      (yyval.nd) = local_switch(p);
		    }
    break;

  case 312:

/* Line 1806 of yacc.c  */
#line 2067 "./parse.y"
    {
		      (yyval.nd) = new_sdef(p, (yyvsp[(2) - (9)].nd), (yyvsp[(5) - (9)].id), (yyvsp[(7) - (9)].nd), (yyvsp[(8) - (9)].nd));
		      local_resume(p, (yyvsp[(6) - (9)].nd));
		      p->in_single--;
		    }
    break;

  case 313:

/* Line 1806 of yacc.c  */
#line 2073 "./parse.y"
    {
		      (yyval.nd) = new_break(p, 0);
		    }
    break;

  case 314:

/* Line 1806 of yacc.c  */
#line 2077 "./parse.y"
    {
		      (yyval.nd) = new_next(p, 0);
		    }
    break;

  case 315:

/* Line 1806 of yacc.c  */
#line 2081 "./parse.y"
    {
		      (yyval.nd) = new_redo(p);
		    }
    break;

  case 316:

/* Line 1806 of yacc.c  */
#line 2085 "./parse.y"
    {
		      (yyval.nd) = new_retry(p);
		    }
    break;

  case 317:

/* Line 1806 of yacc.c  */
#line 2091 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		      if (!(yyval.nd)) (yyval.nd) = new_nil(p);
		    }
    break;

  case 324:

/* Line 1806 of yacc.c  */
#line 2110 "./parse.y"
    {
		      (yyval.nd) = new_if(p, cond((yyvsp[(2) - (5)].nd)), (yyvsp[(4) - (5)].nd), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 326:

/* Line 1806 of yacc.c  */
#line 2117 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 327:

/* Line 1806 of yacc.c  */
#line 2123 "./parse.y"
    {
		      (yyval.nd) = list1(list1((yyvsp[(1) - (1)].nd)));
		    }
    break;

  case 329:

/* Line 1806 of yacc.c  */
#line 2130 "./parse.y"
    {
		      (yyval.nd) = new_arg(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 330:

/* Line 1806 of yacc.c  */
#line 2134 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(2) - (3)].nd), 0);
		    }
    break;

  case 331:

/* Line 1806 of yacc.c  */
#line 2140 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 332:

/* Line 1806 of yacc.c  */
#line 2144 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 333:

/* Line 1806 of yacc.c  */
#line 2150 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (1)].nd),0,0);
		    }
    break;

  case 334:

/* Line 1806 of yacc.c  */
#line 2154 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (4)].nd), new_arg(p, (yyvsp[(4) - (4)].id)), 0);
		    }
    break;

  case 335:

/* Line 1806 of yacc.c  */
#line 2158 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (6)].nd), new_arg(p, (yyvsp[(4) - (6)].id)), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 336:

/* Line 1806 of yacc.c  */
#line 2162 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (3)].nd), (node*)-1, 0);
		    }
    break;

  case 337:

/* Line 1806 of yacc.c  */
#line 2166 "./parse.y"
    {
		      (yyval.nd) = list3((yyvsp[(1) - (5)].nd), (node*)-1, (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 338:

/* Line 1806 of yacc.c  */
#line 2170 "./parse.y"
    {
		      (yyval.nd) = list3(0, new_arg(p, (yyvsp[(2) - (2)].id)), 0);
		    }
    break;

  case 339:

/* Line 1806 of yacc.c  */
#line 2174 "./parse.y"
    {
		      (yyval.nd) = list3(0, new_arg(p, (yyvsp[(2) - (4)].id)), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 340:

/* Line 1806 of yacc.c  */
#line 2178 "./parse.y"
    {
		      (yyval.nd) = list3(0, (node*)-1, 0);
		    }
    break;

  case 341:

/* Line 1806 of yacc.c  */
#line 2182 "./parse.y"
    {
		      (yyval.nd) = list3(0, (node*)-1, (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 342:

/* Line 1806 of yacc.c  */
#line 2188 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    }
    break;

  case 343:

/* Line 1806 of yacc.c  */
#line 2192 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (8)].nd), (yyvsp[(3) - (8)].nd), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].nd), (yyvsp[(8) - (8)].id));
		    }
    break;

  case 344:

/* Line 1806 of yacc.c  */
#line 2196 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].nd), 0, 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 345:

/* Line 1806 of yacc.c  */
#line 2200 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), 0, (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 346:

/* Line 1806 of yacc.c  */
#line 2204 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 347:

/* Line 1806 of yacc.c  */
#line 2208 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (2)].nd), 0, 1, 0, 0);
		    }
    break;

  case 348:

/* Line 1806 of yacc.c  */
#line 2212 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 349:

/* Line 1806 of yacc.c  */
#line 2216 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (2)].nd), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 350:

/* Line 1806 of yacc.c  */
#line 2220 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 351:

/* Line 1806 of yacc.c  */
#line 2224 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 352:

/* Line 1806 of yacc.c  */
#line 2228 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (2)].nd), 0, 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 353:

/* Line 1806 of yacc.c  */
#line 2232 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 354:

/* Line 1806 of yacc.c  */
#line 2236 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 355:

/* Line 1806 of yacc.c  */
#line 2240 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 356:

/* Line 1806 of yacc.c  */
#line 2244 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 358:

/* Line 1806 of yacc.c  */
#line 2251 "./parse.y"
    {
		      p->cmd_start = TRUE;
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 359:

/* Line 1806 of yacc.c  */
#line 2258 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.nd) = 0;
		    }
    break;

  case 360:

/* Line 1806 of yacc.c  */
#line 2263 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.nd) = 0;
		    }
    break;

  case 361:

/* Line 1806 of yacc.c  */
#line 2268 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (4)].nd);
		    }
    break;

  case 362:

/* Line 1806 of yacc.c  */
#line 2275 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 363:

/* Line 1806 of yacc.c  */
#line 2279 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 366:

/* Line 1806 of yacc.c  */
#line 2289 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      new_bv(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 368:

/* Line 1806 of yacc.c  */
#line 2297 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (4)].nd);
		    }
    break;

  case 369:

/* Line 1806 of yacc.c  */
#line 2301 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		    }
    break;

  case 370:

/* Line 1806 of yacc.c  */
#line 2307 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 371:

/* Line 1806 of yacc.c  */
#line 2311 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		    }
    break;

  case 372:

/* Line 1806 of yacc.c  */
#line 2317 "./parse.y"
    {
		      local_nest(p);
		    }
    break;

  case 373:

/* Line 1806 of yacc.c  */
#line 2323 "./parse.y"
    {
		      (yyval.nd) = new_block(p,(yyvsp[(3) - (5)].nd),(yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    }
    break;

  case 374:

/* Line 1806 of yacc.c  */
#line 2330 "./parse.y"
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

/* Line 1806 of yacc.c  */
#line 2340 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 376:

/* Line 1806 of yacc.c  */
#line 2344 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		      call_with_block(p, (yyval.nd), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 377:

/* Line 1806 of yacc.c  */
#line 2349 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (5)].nd), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].nd));
		      call_with_block(p, (yyval.nd), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 378:

/* Line 1806 of yacc.c  */
#line 2356 "./parse.y"
    {
		      (yyval.nd) = new_fcall(p, (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 379:

/* Line 1806 of yacc.c  */
#line 2360 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 380:

/* Line 1806 of yacc.c  */
#line 2364 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].nd));
		    }
    break;

  case 381:

/* Line 1806 of yacc.c  */
#line 2368 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 382:

/* Line 1806 of yacc.c  */
#line 2372 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), intern("call"), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 383:

/* Line 1806 of yacc.c  */
#line 2376 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (3)].nd), intern("call"), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 384:

/* Line 1806 of yacc.c  */
#line 2380 "./parse.y"
    {
		      (yyval.nd) = new_super(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 385:

/* Line 1806 of yacc.c  */
#line 2384 "./parse.y"
    {
		      (yyval.nd) = new_zsuper(p);
		    }
    break;

  case 386:

/* Line 1806 of yacc.c  */
#line 2388 "./parse.y"
    {
		      (yyval.nd) = new_call(p, (yyvsp[(1) - (4)].nd), intern("[]"), (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 387:

/* Line 1806 of yacc.c  */
#line 2394 "./parse.y"
    {
		      local_nest(p);
		    }
    break;

  case 388:

/* Line 1806 of yacc.c  */
#line 2399 "./parse.y"
    {
		      (yyval.nd) = new_block(p,(yyvsp[(3) - (5)].nd),(yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    }
    break;

  case 389:

/* Line 1806 of yacc.c  */
#line 2404 "./parse.y"
    {
		      local_nest(p);
		    }
    break;

  case 390:

/* Line 1806 of yacc.c  */
#line 2409 "./parse.y"
    {
		      (yyval.nd) = new_block(p,(yyvsp[(3) - (5)].nd),(yyvsp[(4) - (5)].nd));
		      local_unnest(p);
		    }
    break;

  case 391:

/* Line 1806 of yacc.c  */
#line 2418 "./parse.y"
    {
		      (yyval.nd) = cons(cons((yyvsp[(2) - (5)].nd), (yyvsp[(4) - (5)].nd)), (yyvsp[(5) - (5)].nd));
		    }
    break;

  case 392:

/* Line 1806 of yacc.c  */
#line 2424 "./parse.y"
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

/* Line 1806 of yacc.c  */
#line 2438 "./parse.y"
    {
		      (yyval.nd) = list1(list3((yyvsp[(2) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].nd)));
		      if ((yyvsp[(6) - (6)].nd)) (yyval.nd) = append((yyval.nd), (yyvsp[(6) - (6)].nd));
		    }
    break;

  case 396:

/* Line 1806 of yacc.c  */
#line 2446 "./parse.y"
    {
			(yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 399:

/* Line 1806 of yacc.c  */
#line 2454 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 401:

/* Line 1806 of yacc.c  */
#line 2461 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 407:

/* Line 1806 of yacc.c  */
#line 2474 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (2)].nd);
		    }
    break;

  case 408:

/* Line 1806 of yacc.c  */
#line 2478 "./parse.y"
    {
		      (yyval.nd) = new_dstr(p, push((yyvsp[(2) - (3)].nd), (yyvsp[(3) - (3)].nd)));
		    }
    break;

  case 409:

/* Line 1806 of yacc.c  */
#line 2484 "./parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    }
    break;

  case 410:

/* Line 1806 of yacc.c  */
#line 2490 "./parse.y"
    {
		      p->sterm = (yyvsp[(2) - (4)].num);
		      (yyval.nd) = list2((yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].nd));
		    }
    break;

  case 411:

/* Line 1806 of yacc.c  */
#line 2496 "./parse.y"
    {
		      (yyval.num) = p->sterm;
		      p->sterm = 0;
		    }
    break;

  case 412:

/* Line 1806 of yacc.c  */
#line 2502 "./parse.y"
    {
		      p->sterm = (yyvsp[(3) - (5)].num);
		      (yyval.nd) = push(push((yyvsp[(1) - (5)].nd), (yyvsp[(2) - (5)].nd)), (yyvsp[(4) - (5)].nd));
		    }
    break;

  case 414:

/* Line 1806 of yacc.c  */
#line 2512 "./parse.y"
    {
		      (yyval.nd) = new_sym(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 415:

/* Line 1806 of yacc.c  */
#line 2516 "./parse.y"
    {
		      p->lstate = EXPR_END;
		      (yyval.nd) = new_dsym(p, push((yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].nd)));
		    }
    break;

  case 416:

/* Line 1806 of yacc.c  */
#line 2523 "./parse.y"
    {
		      p->lstate = EXPR_END;
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    }
    break;

  case 421:

/* Line 1806 of yacc.c  */
#line 2534 "./parse.y"
    {
		      (yyval.id) = new_strsym(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 422:

/* Line 1806 of yacc.c  */
#line 2538 "./parse.y"
    {
		      (yyval.id) = new_strsym(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 425:

/* Line 1806 of yacc.c  */
#line 2546 "./parse.y"
    {
		      (yyval.nd) = negate_lit(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 426:

/* Line 1806 of yacc.c  */
#line 2550 "./parse.y"
    {
		      (yyval.nd) = negate_lit(p, (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 427:

/* Line 1806 of yacc.c  */
#line 2556 "./parse.y"
    {
		      (yyval.nd) = new_lvar(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 428:

/* Line 1806 of yacc.c  */
#line 2560 "./parse.y"
    {
		      (yyval.nd) = new_ivar(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 429:

/* Line 1806 of yacc.c  */
#line 2564 "./parse.y"
    {
		      (yyval.nd) = new_gvar(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 430:

/* Line 1806 of yacc.c  */
#line 2568 "./parse.y"
    {
		      (yyval.nd) = new_cvar(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 431:

/* Line 1806 of yacc.c  */
#line 2572 "./parse.y"
    {
		      (yyval.nd) = new_const(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 432:

/* Line 1806 of yacc.c  */
#line 2578 "./parse.y"
    {
		      assignable(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 433:

/* Line 1806 of yacc.c  */
#line 2584 "./parse.y"
    {
		      (yyval.nd) = var_reference(p, (yyvsp[(1) - (1)].nd));
		    }
    break;

  case 434:

/* Line 1806 of yacc.c  */
#line 2588 "./parse.y"
    {
		      (yyval.nd) = new_nil(p);
		    }
    break;

  case 435:

/* Line 1806 of yacc.c  */
#line 2592 "./parse.y"
    {
		      (yyval.nd) = new_self(p);
   		    }
    break;

  case 436:

/* Line 1806 of yacc.c  */
#line 2596 "./parse.y"
    {
		      (yyval.nd) = new_true(p);
   		    }
    break;

  case 437:

/* Line 1806 of yacc.c  */
#line 2600 "./parse.y"
    {
		      (yyval.nd) = new_false(p);
   		    }
    break;

  case 438:

/* Line 1806 of yacc.c  */
#line 2604 "./parse.y"
    {
		      if (!p->filename) {
			p->filename = "(null)";
		      }
		      (yyval.nd) = new_str(p, p->filename, strlen(p->filename));
		    }
    break;

  case 439:

/* Line 1806 of yacc.c  */
#line 2611 "./parse.y"
    {
		      char buf[16];

		      snprintf(buf, sizeof(buf), "%d", p->lineno);
		      (yyval.nd) = new_int(p, buf, 10);
		    }
    break;

  case 442:

/* Line 1806 of yacc.c  */
#line 2624 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;

  case 443:

/* Line 1806 of yacc.c  */
#line 2628 "./parse.y"
    {
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    }
    break;

  case 444:

/* Line 1806 of yacc.c  */
#line 2633 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(3) - (4)].nd);
		    }
    break;

  case 445:

/* Line 1806 of yacc.c  */
#line 2637 "./parse.y"
    {
		      yyerrok;
		      (yyval.nd) = 0;
		    }
    break;

  case 446:

/* Line 1806 of yacc.c  */
#line 2644 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(2) - (3)].nd);
		      p->lstate = EXPR_BEG;
		      p->cmd_start = TRUE;
		    }
    break;

  case 447:

/* Line 1806 of yacc.c  */
#line 2650 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 448:

/* Line 1806 of yacc.c  */
#line 2656 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), (yyvsp[(5) - (6)].id), 0, (yyvsp[(6) - (6)].id));
		    }
    break;

  case 449:

/* Line 1806 of yacc.c  */
#line 2660 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (8)].nd), (yyvsp[(3) - (8)].nd), (yyvsp[(5) - (8)].id), (yyvsp[(7) - (8)].nd), (yyvsp[(8) - (8)].id));
		    }
    break;

  case 450:

/* Line 1806 of yacc.c  */
#line 2664 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].nd), 0, 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 451:

/* Line 1806 of yacc.c  */
#line 2668 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].nd), 0, (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 452:

/* Line 1806 of yacc.c  */
#line 2672 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 453:

/* Line 1806 of yacc.c  */
#line 2676 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (6)].nd), 0, (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 454:

/* Line 1806 of yacc.c  */
#line 2680 "./parse.y"
    {
		      (yyval.nd) = new_args(p, (yyvsp[(1) - (2)].nd), 0, 0, 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 455:

/* Line 1806 of yacc.c  */
#line 2684 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), (yyvsp[(3) - (4)].id), 0, (yyvsp[(4) - (4)].id));
		    }
    break;

  case 456:

/* Line 1806 of yacc.c  */
#line 2688 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (6)].nd), (yyvsp[(3) - (6)].id), (yyvsp[(5) - (6)].nd), (yyvsp[(6) - (6)].id));
		    }
    break;

  case 457:

/* Line 1806 of yacc.c  */
#line 2692 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (2)].nd), 0, 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 458:

/* Line 1806 of yacc.c  */
#line 2696 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, (yyvsp[(1) - (4)].nd), 0, (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 459:

/* Line 1806 of yacc.c  */
#line 2700 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (2)].id), 0, (yyvsp[(2) - (2)].id));
		    }
    break;

  case 460:

/* Line 1806 of yacc.c  */
#line 2704 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, (yyvsp[(1) - (4)].id), (yyvsp[(3) - (4)].nd), (yyvsp[(4) - (4)].id));
		    }
    break;

  case 461:

/* Line 1806 of yacc.c  */
#line 2708 "./parse.y"
    {
		      (yyval.nd) = new_args(p, 0, 0, 0, 0, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 462:

/* Line 1806 of yacc.c  */
#line 2712 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.nd) = new_args(p, 0, 0, 0, 0, 0);
		    }
    break;

  case 463:

/* Line 1806 of yacc.c  */
#line 2719 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a constant");
		      (yyval.nd) = 0;
		    }
    break;

  case 464:

/* Line 1806 of yacc.c  */
#line 2724 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be an instance variable");
		      (yyval.nd) = 0;
		    }
    break;

  case 465:

/* Line 1806 of yacc.c  */
#line 2729 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a global variable");
		      (yyval.nd) = 0;
		    }
    break;

  case 466:

/* Line 1806 of yacc.c  */
#line 2734 "./parse.y"
    {
		      yyerror(p, "formal argument cannot be a class variable");
		      (yyval.nd) = 0;
		    }
    break;

  case 467:

/* Line 1806 of yacc.c  */
#line 2741 "./parse.y"
    {
		      (yyval.id) = 0;
		    }
    break;

  case 468:

/* Line 1806 of yacc.c  */
#line 2745 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (1)].id));
		      (yyval.id) = (yyvsp[(1) - (1)].id);
		    }
    break;

  case 469:

/* Line 1806 of yacc.c  */
#line 2752 "./parse.y"
    {
		      (yyval.nd) = new_arg(p, (yyvsp[(1) - (1)].id));
		    }
    break;

  case 470:

/* Line 1806 of yacc.c  */
#line 2756 "./parse.y"
    {
		      (yyval.nd) = new_masgn(p, (yyvsp[(2) - (3)].nd), 0);
		    }
    break;

  case 471:

/* Line 1806 of yacc.c  */
#line 2762 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 472:

/* Line 1806 of yacc.c  */
#line 2766 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 473:

/* Line 1806 of yacc.c  */
#line 2772 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.nd) = cons(nsym((yyvsp[(1) - (3)].id)), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 474:

/* Line 1806 of yacc.c  */
#line 2779 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(1) - (3)].id));
		      (yyval.nd) = cons(nsym((yyvsp[(1) - (3)].id)), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 475:

/* Line 1806 of yacc.c  */
#line 2786 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 476:

/* Line 1806 of yacc.c  */
#line 2790 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 477:

/* Line 1806 of yacc.c  */
#line 2796 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 478:

/* Line 1806 of yacc.c  */
#line 2800 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 481:

/* Line 1806 of yacc.c  */
#line 2810 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    }
    break;

  case 482:

/* Line 1806 of yacc.c  */
#line 2815 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.id) = -1;
		    }
    break;

  case 485:

/* Line 1806 of yacc.c  */
#line 2826 "./parse.y"
    {
		      local_add_f(p, (yyvsp[(2) - (2)].id));
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    }
    break;

  case 486:

/* Line 1806 of yacc.c  */
#line 2833 "./parse.y"
    {
		      (yyval.id) = (yyvsp[(2) - (2)].id);
		    }
    break;

  case 487:

/* Line 1806 of yacc.c  */
#line 2837 "./parse.y"
    {
		      local_add_f(p, 0);
		      (yyval.id) = 0;
		    }
    break;

  case 488:

/* Line 1806 of yacc.c  */
#line 2844 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (1)].nd);
		      if (!(yyval.nd)) (yyval.nd) = new_nil(p);
		    }
    break;

  case 489:

/* Line 1806 of yacc.c  */
#line 2848 "./parse.y"
    {p->lstate = EXPR_BEG;}
    break;

  case 490:

/* Line 1806 of yacc.c  */
#line 2849 "./parse.y"
    {
		      if ((yyvsp[(3) - (4)].nd) == 0) {
			yyerror(p, "can't define singleton method for ().");
		      }
		      else {
			switch ((enum node_type)(int)(intptr_t)(yyvsp[(3) - (4)].nd)->car) {
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

  case 492:

/* Line 1806 of yacc.c  */
#line 2872 "./parse.y"
    {
		      (yyval.nd) = (yyvsp[(1) - (2)].nd);
		    }
    break;

  case 493:

/* Line 1806 of yacc.c  */
#line 2878 "./parse.y"
    {
		      (yyval.nd) = list1((yyvsp[(1) - (1)].nd));
		    }
    break;

  case 494:

/* Line 1806 of yacc.c  */
#line 2882 "./parse.y"
    {
		      (yyval.nd) = push((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 495:

/* Line 1806 of yacc.c  */
#line 2888 "./parse.y"
    {
		      (yyval.nd) = cons((yyvsp[(1) - (3)].nd), (yyvsp[(3) - (3)].nd));
		    }
    break;

  case 496:

/* Line 1806 of yacc.c  */
#line 2892 "./parse.y"
    {
		      (yyval.nd) = cons(new_sym(p, (yyvsp[(1) - (2)].id)), (yyvsp[(2) - (2)].nd));
		    }
    break;

  case 518:

/* Line 1806 of yacc.c  */
#line 2936 "./parse.y"
    {yyerrok;}
    break;

  case 520:

/* Line 1806 of yacc.c  */
#line 2941 "./parse.y"
    {
		      p->lineno++;
		      p->column = 0;
		    }
    break;

  case 522:

/* Line 1806 of yacc.c  */
#line 2947 "./parse.y"
    {yyerrok;}
    break;

  case 523:

/* Line 1806 of yacc.c  */
#line 2951 "./parse.y"
    {
		      (yyval.nd) = 0;
		    }
    break;



/* Line 1806 of yacc.c  */
#line 8711 "./y.tab.c"
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
#line 2955 "./parse.y"

#define yylval  (*((YYSTYPE*)(p->ylval)))

static void
yyerror(parser_state *p, const char *s)
{
  char* c;
  int n;

  if (! p->capture_errors) {
#ifdef ENABLE_STDIO
    if (p->filename) {
      fprintf(stderr, "%s:%d:%d: %s\n", p->filename, p->lineno, p->column, s);
    }
    else {
      fprintf(stderr, "line %d:%d: %s\n", p->lineno, p->column, s);
    }
#endif
  }
  else if (p->nerr < sizeof(p->error_buffer) / sizeof(p->error_buffer[0])) {
    n = strlen(s);
    c = (char *)parser_palloc(p, n + 1);
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
  int n;

  if (! p->capture_errors) {
#ifdef ENABLE_STDIO
    if (p->filename) {
      fprintf(stderr, "%s:%d:%d: %s\n", p->filename, p->lineno, p->column, s);
    }
    else {
      fprintf(stderr, "line %d:%d: %s\n", p->lineno, p->column, s);
    }
#endif
  }
  else if (p->nerr < sizeof(p->warn_buffer) / sizeof(p->warn_buffer[0])) {
    n = strlen(s);
    c = (char *)parser_palloc(p, n + 1);
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
  int c;

  c = (int)(intptr_t)n->car;

  if (c == NODE_NTH_REF) {
    yyerror_i(p, "can't set variable $%d", (int)(intptr_t)n->cdr);
  } else if (c == NODE_BACK_REF) {
    yyerror_i(p, "can't set variable $%c", (int)(intptr_t)n->cdr);
  } else {
    mrb_bug("Internal error in backref_error() : n=>car == %d", c);
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

  while (len-- && *s && (tmp = (char *)strchr(hexdigit, *s))) {
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
    switch (p->lstate) {
    case EXPR_BEG:
    case EXPR_FNAME:
    case EXPR_DOT:
    case EXPR_CLASS:
    case EXPR_VALUE:
      p->lineno++;
      p->column = 0;
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
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
    } else {
      p->lstate = EXPR_BEG;
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
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
    } else {
      p->lstate = EXPR_BEG;
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
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
    } else {
      p->lstate = EXPR_BEG;
      if (p->lstate == EXPR_CLASS) {
        p->cmd_start = TRUE;
      }
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
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
    } else {
      p->lstate = EXPR_BEG;
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
	int c2;
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
	default:
	  c2 = 0;
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
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
    } else {
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
	      tokadd(p, tolower(c));
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
	double d;
	char *endp;

	errno = 0;
	d = strtod(tok(p), &endp);
	if (d == 0 && endp == tok(p)) {
	  yywarning_s(p, "corrupted float value %s", tok(p));
	}
	else if (errno == ERANGE) {
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
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
    } else {
      p->lstate = EXPR_BEG;
    }
    return '/';

  case '^':
    if ((c = nextc(p)) == '=') {
      yylval.id = intern("^");
      p->lstate = EXPR_BEG;
      return tOP_ASGN;
    }
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
    } else {
      p->lstate = EXPR_BEG;
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
    if (p->lstate == EXPR_FNAME || p->lstate == EXPR_DOT) {
      p->lstate = EXPR_ARG;
    } else {
      p->lstate = EXPR_BEG;
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

static void
parser_init_cxt(parser_state *p, mrbc_context *cxt)
{
  if (!cxt) return;
  if (cxt->lineno) p->lineno = cxt->lineno;
  if (cxt->filename) p->filename = cxt->filename;
  if (cxt->syms) {
    int i;

    p->locals = cons(0,0);
    for (i=0; i<cxt->slen; i++) {
      local_add_f(p, cxt->syms[i]);
    }
  }
  p->capture_errors = cxt->capture_errors;
}

static void
parser_update_cxt(parser_state *p, mrbc_context *cxt)
{
  node *n, *n0;
  int i = 0;

  if (!cxt) return;
  if ((int)(intptr_t)p->tree->car != NODE_SCOPE) return;
  n0 = n = p->tree->cdr->car;
  while (n) {
    i++;
    n = n->cdr;
  }
  cxt->syms = (mrb_sym *)mrb_realloc(p->mrb, cxt->syms, i*sizeof(mrb_sym));
  cxt->slen = i;
  for (i=0, n=n0; n; i++,n=n->cdr) {
    cxt->syms[i] = sym(n->car);
  }
}

void codedump_all(mrb_state*, int);
void parser_dump(mrb_state *mrb, node *tree, int offset);

void
mrb_parser_parse(parser_state *p, mrbc_context *c)
{
  if (setjmp(p->jmp) != 0) {
    yyerror(p, "memory allocation error");
    p->nerr++;
    p->tree = 0;
    return;
  }

  p->cmd_start = TRUE;
  p->in_def = p->in_single = FALSE;
  p->nerr = p->nwarn = 0;
  p->sterm = 0;

  parser_init_cxt(p, c);
  yyparse(p);
  if (!p->tree) {
    p->tree = new_nil(p);
  }
  parser_update_cxt(p, c);
  if (c && c->dump_result) {
    parser_dump(p->mrb, p->tree, 0);
  }
}

parser_state*
mrb_parser_new(mrb_state *mrb)
{
  mrb_pool *pool;
  parser_state *p;

  pool = mrb_pool_open(mrb);
  if (!pool) return 0;
  p = (parser_state *)mrb_pool_alloc(pool, sizeof(parser_state));
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

void
mrb_parser_free(parser_state *p) {
  mrb_pool_close(p->pool);
}

mrbc_context*
mrbc_context_new(mrb_state *mrb)
{
  mrbc_context *c;

  c = (mrbc_context *)mrb_calloc(mrb, 1, sizeof(mrbc_context));
  return c;
}

void
mrbc_context_free(mrb_state *mrb, mrbc_context *cxt)
{
  mrb_free(mrb, cxt->syms);
  mrb_free(mrb, cxt);
}

const char*
mrbc_filename(mrb_state *mrb, mrbc_context *c, const char *s)
{
  if (s) {
    int len = strlen(s);
    char *p = (char *)mrb_alloca(mrb, len + 1);

    memcpy(p, s, len + 1);
    c->filename = p;
    c->lineno = 1;
  }
  return c->filename;
}

parser_state*
mrb_parse_file(mrb_state *mrb, FILE *f, mrbc_context *c)
{
  parser_state *p;
 
  p = mrb_parser_new(mrb);
  if (!p) return 0;
  p->s = p->send = NULL;
  p->f = f;

  mrb_parser_parse(p, c);
  return p;
}

parser_state*
mrb_parse_nstring(mrb_state *mrb, const char *s, int len, mrbc_context *c)
{
  parser_state *p;

  p = mrb_parser_new(mrb);
  if (!p) return 0;
  p->s = s;
  p->send = s + len;

  mrb_parser_parse(p, c);
  return p;
}

parser_state*
mrb_parse_string(mrb_state *mrb, const char *s, mrbc_context *c)
{
  return mrb_parse_nstring(mrb, s, strlen(s), c);
}

static mrb_value
load_exec(mrb_state *mrb, parser_state *p, mrbc_context *c)
{
  int n;
  mrb_value v;

  if (!p) {
    mrb_parser_free(p);
    return mrb_undef_value();
  }
  if (!p->tree || p->nerr) {
    if (p->capture_errors) {
      char buf[256];

      n = snprintf(buf, sizeof(buf), "line %d: %s\n",
		   p->error_buffer[0].lineno, p->error_buffer[0].message);
      mrb->exc = (struct RObject*)mrb_object(mrb_exc_new(mrb, E_SYNTAX_ERROR, buf, n));
      mrb_parser_free(p);
      return mrb_undef_value();
    }
    else {
      static const char msg[] = "syntax error";
      mrb->exc = (struct RObject*)mrb_object(mrb_exc_new(mrb, E_SYNTAX_ERROR, msg, sizeof(msg) - 1));
      mrb_parser_free(p);
      return mrb_nil_value();
    }
  }
  n = mrb_generate_code(mrb, p);
  mrb_parser_free(p);
  if (n < 0) {
    static const char msg[] = "codegen error";
    mrb->exc = (struct RObject*)mrb_object(mrb_exc_new(mrb, E_SCRIPT_ERROR, msg, sizeof(msg) - 1));
    return mrb_nil_value();
  }
  if (c) {
    if (c->dump_result) codedump_all(mrb, n);
    if (c->no_exec) return mrb_fixnum_value(n);
  }
  v = mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_top_self(mrb));
  if (mrb->exc) return mrb_nil_value();
  return v;
}

mrb_value
mrb_load_file_cxt(mrb_state *mrb, FILE *f, mrbc_context *c)
{
  return load_exec(mrb, mrb_parse_file(mrb, f, c), c);
}

mrb_value
mrb_load_file(mrb_state *mrb, FILE *f)
{
  return mrb_load_file_cxt(mrb, f, NULL);
}

mrb_value
mrb_load_nstring_cxt(mrb_state *mrb, const char *s, int len, mrbc_context *c)
{
  return load_exec(mrb, mrb_parse_nstring(mrb, s, len, c), c);
}

mrb_value
mrb_load_nstring(mrb_state *mrb, const char *s, int len)
{
  return mrb_load_nstring_cxt(mrb, s, len, NULL);
}

mrb_value
mrb_load_string_cxt(mrb_state *mrb, const char *s, mrbc_context *c)
{
  return mrb_load_nstring_cxt(mrb, s, strlen(s), c);
}

mrb_value
mrb_load_string(mrb_state *mrb, const char *s)
{
  return mrb_load_string_cxt(mrb, s, NULL);
}

#ifdef ENABLE_STDIO

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

#endif

void
parser_dump(mrb_state *mrb, node *tree, int offset)
{
#ifdef ENABLE_STDIO
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
	    printf("%s=", mrb_sym2name(mrb, sym(n2->car->car)));
	    parser_dump(mrb, n2->car->cdr, 0);
	    n2 = n2->cdr;
	  }
	}
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("rest=*%s\n", mrb_sym2name(mrb, sym(n->car)));
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
	printf("blk=&%s\n", mrb_sym2name(mrb, sym(n)));
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
	dump_prefix(offset+2);
	while (n2) {
	  if (n2->car) {
	    if (n2 != tree->car) printf(", ");
	    printf("%s", mrb_sym2name(mrb, sym(n2->car)));
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
	   mrb_sym2name(mrb, sym(tree->cdr->car)),
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
    printf("::%s\n", mrb_sym2name(mrb, sym(tree->cdr)));
    break;

  case NODE_COLON3:
    printf("NODE_COLON3:\n");
    dump_prefix(offset+1);
    printf("::%s\n", mrb_sym2name(mrb, sym(tree)));
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
    printf("op='%s' (%d)\n", mrb_sym2name(mrb, sym(tree->car)), (int)(intptr_t)tree->car);
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
    printf("NODE_LVAR %s\n", mrb_sym2name(mrb, sym(tree)));
    break;

  case NODE_GVAR:
    printf("NODE_GVAR %s\n", mrb_sym2name(mrb, sym(tree)));
    break;

  case NODE_IVAR:
    printf("NODE_IVAR %s\n", mrb_sym2name(mrb, sym(tree)));
    break;

  case NODE_CVAR:
    printf("NODE_CVAR %s\n", mrb_sym2name(mrb, sym(tree)));
    break;

  case NODE_CONST:
    printf("NODE_CONST %s\n", mrb_sym2name(mrb, sym(tree)));
    break;

  case NODE_BACK_REF:
    printf("NODE_BACK_REF: $%c\n", (int)(intptr_t)tree);
    break;

  case NODE_NTH_REF:
    printf("NODE_NTH_REF: $%d\n", (int)(intptr_t)tree);
    break;

  case NODE_ARG:
    printf("NODE_ARG %s\n", mrb_sym2name(mrb, sym(tree)));
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
    printf("NODE_SYM :%s\n", mrb_sym2name(mrb, sym(tree)));
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
	   mrb_sym2name(mrb, sym(tree->car)),
	   mrb_sym2name(mrb, sym(tree->cdr)));
    break;

  case NODE_UNDEF:
    printf("NODE_UNDEF %s:\n",
	   mrb_sym2name(mrb, sym(tree)));
    break;

  case NODE_CLASS:
    printf("NODE_CLASS:\n");
    if (tree->car->car == (node*)0) {
      dump_prefix(offset+1);
      printf(":%s\n", mrb_sym2name(mrb, sym(tree->car->cdr)));
    }
    else if (tree->car->car == (node*)1) {
      dump_prefix(offset+1);
      printf("::%s\n", mrb_sym2name(mrb, sym(tree->car->cdr)));
    }
    else {
      parser_dump(mrb, tree->car->car, offset+1);
      dump_prefix(offset+1);
      printf("::%s\n", mrb_sym2name(mrb, sym(tree->car->cdr)));
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
      printf(":%s\n", mrb_sym2name(mrb, sym(tree->car->cdr)));
    }
    else if (tree->car->car == (node*)1) {
      dump_prefix(offset+1);
      printf("::%s\n", mrb_sym2name(mrb, sym(tree->car->cdr)));
    }
    else {
      parser_dump(mrb, tree->car->car, offset+1);
      dump_prefix(offset+1);
      printf("::%s\n", mrb_sym2name(mrb, sym(tree->car->cdr)));
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
    printf("%s\n", mrb_sym2name(mrb, sym(tree->car)));
    tree = tree->cdr;
    {
      node *n2 = tree->car;

      if (n2 && (n2->car || n2->cdr)) {
	dump_prefix(offset+1);
	printf("local variables:\n");
	dump_prefix(offset+2);
	while (n2) {
	  if (n2->car) {
	    if (n2 != tree->car) printf(", ");
	    printf("%s", mrb_sym2name(mrb, sym(n2->car)));
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
	    printf("%s=", mrb_sym2name(mrb, sym(n2->car->car)));
	    parser_dump(mrb, n2->car->cdr, 0);
	    n2 = n2->cdr;
	  }
	}
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("rest=*%s\n", mrb_sym2name(mrb, sym(n->car)));
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
	printf("blk=&%s\n", mrb_sym2name(mrb, sym(n)));
      }
    }
    parser_dump(mrb, tree->cdr->car, offset+1);
    break;

  case NODE_SDEF:
    printf("NODE_SDEF:\n");
    parser_dump(mrb, tree->car, offset+1);
    tree = tree->cdr;
    dump_prefix(offset+1);
    printf(":%s\n", mrb_sym2name(mrb, sym(tree->car)));
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
	    printf("%s=", mrb_sym2name(mrb, sym(n2->car->car)));
	    parser_dump(mrb, n2->car->cdr, 0);
	    n2 = n2->cdr;
	  }
	}
      }
      n = n->cdr;
      if (n->car) {
	dump_prefix(offset+1);
	printf("rest=*%s\n", mrb_sym2name(mrb, sym(n->car)));
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
	printf("blk=&%s\n", mrb_sym2name(mrb, sym(n)));
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
#endif
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

