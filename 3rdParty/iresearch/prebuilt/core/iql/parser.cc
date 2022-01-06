// A Bison parser, made by GNU Bison 3.5.1.

// Skeleton implementation for Bison GLR parsers in C

// Copyright (C) 2002-2015, 2018-2020 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

/* C GLR parser skeleton written by Paul Hilfinger.  */

// Undocumented macros, especially those whose name start with YY_,
// are private implementation details.  Do not rely on them.

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.5.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "glr.cc"

/* Pure parsers.  */
#define YYPURE 1







# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.hh"

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Default (constant) value used for initialization for null
   right-hand sides.  Unlike the standard yacc.c template, here we set
   the default value of $$ to a zeroed-out value.  Since the default
   value is undefined, this behavior is technically correct.  */
static YYSTYPE yyval_default;
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;

// Second part of user prologue.
#line 100 "iql/parser.cc"
static void yyerror (const iresearch::iql::parser::location_type *yylocationp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx, const char* msg);
#line 102 "iql/parser.cc"

// Unqualified %code blocks.
#line 142 "/home/gnusi/git/iresearch/core/iql/parser.yy"

  #define yylex(pValue, pLocation) ctx.yylex(*pValue, *pLocation)

  namespace iresearch {
    namespace iql {
      void parser::error(parser::location_type const& location, std::string const& sError) {
          ctx.yyerror(location, sError);
      }
    }
  }

  // this is required only for %skeleton "glr.cc" since it lacks default init
  // but yy_symbol_print uses yylocationp->[begin|end].filename and segfaults
  // not needed for %skeleton "lalr1.cc" or without %skeleton
  #if YYDEBUG
    static void yy_symbol_print(FILE*, int, iresearch::iql::parser::semantic_type const*, iresearch::iql::parser::location_type const*, iresearch::iql::parser&, iresearch::iql::context&);
    static void yy_symbol_print(FILE* filep, int yytype, YYSTYPE const* yyvaluep, YYLTYPE const* yylocationp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx) {
      iresearch::iql::parser::location_type yylocation = *yylocationp;

      yylocation.begin.filename = nullptr;
      yylocation.end.filename = nullptr;
      yy_symbol_print(filep, yytype, yyvaluep, &yylocation, yyparser, ctx);
    }
  #endif

  // ALWAYS!!! define YYDEBUG 1 for the length of the header so as to avoid
  // alignment issues when linkning without YYDEBUG agains a library that was
  // built with YYDEBUG or vice versa
  // add method stub implementations here for debug methods declared in header
  // since they are masked out in Bison generated code by !YYDEBUG
  #if !YYDEBUG
    namespace iresearch {
      namespace iql {
        inline void parser::yy_symbol_value_print_(int /* yytype */, const semantic_type* /* yyvaluep */, const location_type* /* yylocationp */) {}
        inline void parser::yy_symbol_print_(int /* yytype */, const semantic_type* /* yyvaluep */, const location_type* /* yylocationp */) {}
      }
    }
  #endif

  // Suppress warnings due to Bison generated code (push section, pop at end of file)
  #if defined(_MSC_VER)
    #pragma warning(disable : 4100)
    #pragma warning(disable : 4127)
    #pragma warning(disable : 4244)
    #pragma warning(disable : 4505)
    #pragma warning(disable : 4611)
    #pragma warning(disable : 4706)
  #endif

#line 155 "iql/parser.cc"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YYFREE
# define YYFREE free
#endif
#ifndef YYMALLOC
# define YYMALLOC malloc
#endif
#ifndef YYREALLOC
# define YYREALLOC realloc
#endif

#define YYSIZEMAX \
  (PTRDIFF_MAX < SIZE_MAX ? PTRDIFF_MAX : YY_CAST (ptrdiff_t, SIZE_MAX))

#ifdef __cplusplus
  typedef bool yybool;
# define yytrue true
# define yyfalse false
#else
  /* When we move to stdbool, get rid of the various casts to yybool.  */
  typedef signed char yybool;
# define yytrue 1
# define yyfalse 0
#endif

#ifndef YYSETJMP
# include <setjmp.h>
# define YYJMP_BUF jmp_buf
# define YYSETJMP(Env) setjmp (Env)
/* Pacify Clang and ICC.  */
# define YYLONGJMP(Env, Val)                    \
 do {                                           \
   longjmp (Env, Val);                          \
   YY_ASSERT (0);                               \
 } while (yyfalse)
#endif

#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* The _Noreturn keyword of C11.  */
#ifndef _Noreturn
# if (defined __cplusplus \
      && ((201103 <= __cplusplus && !(__GNUC__ == 4 && __GNUC_MINOR__ == 7)) \
          || (defined _MSC_VER && 1900 <= _MSC_VER)))
#  define _Noreturn [[noreturn]]
# elif ((!defined __cplusplus || defined __clang__) \
        && (201112 <= (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0)  \
            || 4 < __GNUC__ + (7 <= __GNUC_MINOR__)))
   /* _Noreturn works as-is.  */
# elif 2 < __GNUC__ + (8 <= __GNUC_MINOR__) || 0x5110 <= __SUNPRO_C
#  define _Noreturn __attribute__ ((__noreturn__))
# elif 1200 <= (defined _MSC_VER ? _MSC_VER : 0)
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   215

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  31
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  24
/* YYNRULES -- Number of rules.  */
#define YYNRULES  60
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  143
/* YYMAXRHS -- Maximum number of symbols on right-hand side of rule.  */
#define YYMAXRHS 7
/* YYMAXLEFT -- Maximum number of symbols to the left of a handle
   accessed by $0, $-1, etc., in any rule.  */
#define YYMAXLEFT 0

/* YYMAXUTOK -- Last valid token number (for yychar).  */
#define YYMAXUTOK   285
/* YYFAULTYTOK -- Token number (for yychar) that denotes a
   syntax_error thrown from the scanner.  */
#define YYFAULTYTOK (YYMAXUTOK + 1)
/* YYUNDEFTOK -- Symbol number (for yytoken) that denotes an unknown
   token.  */
#define YYUNDEFTOK  2

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                         \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
      25,    26,    27,    28,    29,    30
};

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   245,   245,   248,   250,   254,   255,   256,   260,   261,
     262,   266,   267,   268,   269,   273,   274,   278,   279,   283,
     284,   288,   289,   293,   294,   298,   299,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   314,   315,   316,   317,
     321,   322,   326,   330,   331,   332,   336,   337,   341,   342,
     346,   347,   350,   352,   355,   357,   361,   362,   366,   367,
     368
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "IQL_EOF", "error", "$undefined", "\"unknown token marker\"",
  "\"token separator\"", "\"data sub-sequence\"", "\"negation operator\"",
  "\"exclamation mark\"", "\"logical AND operator\"",
  "\"double ampersand\"", "\"logical OR operator\"", "\"double pipe\"",
  "\"not equal operator\"", "\"less than or equal operator\"",
  "\"equal operator\"", "\"greater than or equal operator\"",
  "\"similar operator\"", "\"asterisk operator\"",
  "\"ascending sort order\"", "\"descending sort order\"", "\"comma\"",
  "\"single quote\"", "\"double quote\"", "\"left chevron '<' delimiter\"",
  "\"right chevron '>' delimiter\"", "\"left parentheses '(' delimiter\"",
  "\"right parentheses ')' delimiter\"",
  "\"left square bracket '[' delimiter\"",
  "\"right square bracket ']' delimiter\"",
  "\"result limit section header\"", "\"sort order section header\"",
  "$accept", "input", "optional_sep", "union", "intersection",
  "expression", "boost", "negation", "subexpression", "function",
  "function_arg", "function_arg_list", "compare", "range", "term",
  "list_sep", "sequence", "plain_literal", "dquoted_literal",
  "squoted_literal", "limit", "order", "order_list", "order_term", YY_NULLPTR
};
#endif

#define YYPACT_NINF (-93)
#define YYTABLE_NINF (-56)

  // YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
  // STATE-NUM.
static const yytype_int16 yypact[] =
{
     -93,    17,   159,   -93,   -93,   -93,   -93,   -93,    24,    27,
     -93,   145,    -1,   -93,   -93,   -93,    77,   163,   -93,   -93,
      -5,    66,    13,    22,   148,   148,    25,    62,   159,    15,
      69,    83,   138,    40,    78,   178,   -93,   -93,   115,   118,
     124,   -93,   -93,    -5,   152,   -93,   -93,   -93,   164,   167,
     179,   -93,   160,   -93,   181,   -93,   -93,   -93,   -93,   -93,
     -93,   -93,   -93,   -93,   133,   -93,   166,   177,   189,    11,
     190,   112,   159,   196,   199,   190,   159,    71,    64,   122,
      64,   122,   122,   122,   122,   -93,   -93,   -93,   -93,    96,
     148,   -93,   -93,   -93,    43,   -93,   200,   156,   -93,   111,
     112,   -93,   -93,   152,   -93,   -93,   -93,   -93,   -93,   -93,
     -93,   -93,   -93,   -93,   -93,   121,   190,   -93,    95,    30,
     112,   -93,   122,   122,   -93,   -93,   -93,   -93,   -93,   -93,
     -93,   -93,   199,   112,   112,   -93,   -93,     5,    14,   -93,
     -93,   -93,   -93
};

  // YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
  // Performed when YYTABLE does not specify something else to do.  Zero
  // means the default is an error.
static const yytype_int8 yydefact[] =
{
       3,     0,     0,     1,     4,    46,     3,     3,     0,     0,
       3,     3,     5,     8,    11,    13,    14,    20,    12,     3,
      40,    43,    44,    45,     0,     0,     0,     0,     0,     0,
       0,    52,     0,     0,     0,     0,     3,    47,     0,     0,
       0,    17,    20,     0,    43,    18,    50,    48,     3,     0,
       0,     3,     0,     3,     0,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     0,     3,     0,     0,     0,     0,
       0,     0,     0,     0,     2,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    21,    24,    25,     3,    40,
       0,    49,    51,    19,     6,    41,    58,     3,    56,     7,
       0,     9,    10,    16,     3,     3,    34,    28,    30,    35,
      31,    32,    27,    29,    33,     0,     0,    15,     0,     0,
       0,    53,     0,     0,     3,    22,    26,    59,    60,    57,
       3,     3,    42,     0,     0,     3,     3,     0,     0,    38,
      39,    37,    36
};

  // YYPGOTO[NTERM-NUM].
static const yytype_int16 yypgoto[] =
{
     -93,   -93,     0,   180,   102,   -52,   -93,   -93,   -11,     2,
      89,   -93,   -93,   126,    28,   -92,   -23,    26,   -93,   -93,
     -93,   -93,   -93,    87
};

  // YYDEFGOTO[NTERM-NUM].
static const yytype_int8 yydefgoto[] =
{
      -1,     1,    33,    11,    12,    13,    14,    15,    16,    95,
      87,    88,    18,   106,    19,   116,    20,    44,    22,    23,
      53,    31,    97,    98
};

  // YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
  // positive, shift that token.  If negative, reduce the rule whose
  // number is the opposite.  If YYTABLE_NINF, syntax error.
static const yytype_int16 yytable[] =
{
       2,    43,    43,    32,    17,   120,    24,    25,    -3,     4,
      28,    30,    86,    41,    45,     4,    34,     3,     4,    35,
      36,    38,    51,   101,   102,    49,    42,    42,    21,    26,
      17,   139,    27,   140,     4,    39,    64,    93,   133,   134,
     141,    89,   142,    40,     4,    50,    46,    32,    69,    55,
     124,    72,    -3,    74,    21,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    86,    90,    17,    43,     4,     5,
      -3,    37,    17,     4,    17,     4,     5,    17,    17,   117,
      51,    -3,     4,    -3,    47,     8,     9,    52,   115,   104,
      21,   105,    42,    89,    -3,    56,    21,   119,    21,    96,
     -23,    21,    21,   103,   122,   123,   107,   108,   110,   111,
     112,   113,   114,   127,   128,    32,   -23,     5,    17,     4,
      -3,    36,   -23,    66,   132,     4,     4,     5,   121,    67,
     119,   119,    65,     8,     9,   137,   138,     4,     5,     6,
       7,   124,    21,     8,     9,   -54,    54,   125,    96,    29,
     130,   131,     4,     5,     8,     9,   -55,    37,    10,    85,
      -3,   135,   136,     4,     5,     6,     7,   -20,    68,     8,
       9,    70,    94,    10,    99,   -41,   -41,   -41,   -41,   -41,
       8,     9,     4,    71,    10,    75,   -41,   -41,    91,    73,
      57,    58,    59,    60,    61,     5,     6,     7,    92,    49,
     100,    62,    63,     4,   118,   126,   109,   129,    48,     0,
       0,     8,     9,     0,     0,    10
};

static const yytype_int16 yycheck[] =
{
       0,    24,    25,     4,     2,    97,     6,     7,     9,     4,
      10,    11,    64,    24,    25,     4,    16,     0,     4,    19,
      25,    21,    11,    75,    76,    10,    24,    25,     2,     5,
      28,    26,     5,    28,     4,    22,    36,    26,   130,   131,
      26,    64,    28,    21,     4,    30,    21,     4,    48,     9,
      20,    51,     9,    53,    28,    55,    56,    57,    58,    59,
      60,    61,    62,    63,   116,    65,    64,    90,     4,     5,
       4,     5,    70,     4,    72,     4,     5,    75,    76,    90,
      11,     4,     4,    17,    22,    21,    22,     4,    88,    25,
      64,    27,    90,   116,    17,    17,    70,    97,    72,    71,
       4,    75,    76,    77,   104,   105,    78,    79,    80,    81,
      82,    83,    84,    18,    19,     4,    20,     5,   116,     4,
       9,    25,    26,     5,   124,     4,     4,     5,   100,     5,
     130,   131,    17,    21,    22,   135,   136,     4,     5,     6,
       7,    20,   116,    21,    22,     0,     8,    26,   120,     4,
     122,   123,     4,     5,    21,    22,     0,     5,    25,    26,
       4,   133,   134,     4,     5,     6,     7,     4,     4,    21,
      22,     4,    70,    25,    72,    12,    13,    14,    15,    16,
      21,    22,     4,     4,    25,     4,    23,    24,    22,    29,
      12,    13,    14,    15,    16,     5,     6,     7,    21,    10,
       4,    23,    24,     4,     4,   116,    80,   120,    28,    -1,
      -1,    21,    22,    -1,    -1,    25
};

  // YYSTOS[STATE-NUM] -- The (internal number of the) accessing
  // symbol of state STATE-NUM.
static const yytype_int8 yystos[] =
{
       0,    32,    33,     0,     4,     5,     6,     7,    21,    22,
      25,    34,    35,    36,    37,    38,    39,    40,    43,    45,
      47,    48,    49,    50,    33,    33,     5,     5,    33,     4,
      33,    52,     4,    33,    33,    33,    25,     5,    33,    22,
      21,    39,    40,    47,    48,    39,    21,    22,    34,    10,
      30,    11,     4,    51,     8,     9,    17,    12,    13,    14,
      15,    16,    23,    24,    33,    17,     5,     5,     4,    33,
       4,     4,    33,    29,    33,     4,    33,    33,    33,    33,
      33,    33,    33,    33,    33,    26,    36,    41,    42,    47,
      33,    22,    21,    26,    35,    40,    45,    53,    54,    35,
       4,    36,    36,    48,    25,    27,    44,    45,    45,    44,
      45,    45,    45,    45,    45,    33,    46,    39,     4,    33,
      46,    45,    33,    33,    20,    26,    41,    18,    19,    54,
      45,    45,    33,    46,    46,    45,    45,    33,    33,    26,
      28,    26,    28
};

  // YYR1[YYN] -- Symbol number of symbol that rule YYN derives.
static const yytype_int8 yyr1[] =
{
       0,    31,    32,    33,    33,    34,    34,    34,    35,    35,
      35,    36,    36,    36,    36,    37,    37,    38,    38,    39,
      39,    40,    40,    41,    41,    42,    42,    43,    43,    43,
      43,    43,    43,    43,    43,    43,    44,    44,    44,    44,
      45,    45,    46,    47,    47,    47,    48,    48,    49,    49,
      50,    50,    51,    51,    52,    52,    53,    53,    54,    54,
      54
};

  // YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.
static const yytype_int8 yyr2[] =
{
       0,     2,     5,     0,     2,     1,     5,     5,     1,     5,
       5,     1,     1,     1,     1,     5,     5,     3,     3,     5,
       1,     4,     6,     1,     1,     1,     3,     5,     5,     5,
       5,     5,     5,     5,     5,     5,     7,     7,     7,     7,
       1,     1,     3,     1,     1,     1,     1,     2,     3,     4,
       3,     4,     0,     4,     0,     4,     1,     3,     1,     3,
       3
};


/* YYDPREC[RULE-NUM] -- Dynamic precedence of rule #RULE-NUM (0 if none).  */
static const yytype_int8 yydprec[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0
};

/* YYMERGER[RULE-NUM] -- Index of merging function for rule #RULE-NUM.  */
static const yytype_int8 yymerger[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0
};

/* YYIMMEDIATE[RULE-NUM] -- True iff rule #RULE-NUM is not to be deferred, as
   in the case of predicates.  */
static const yybool yyimmediate[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0
};

/* YYCONFLP[YYPACT[STATE-NUM]] -- Pointer into YYCONFL of start of
   list of conflicting reductions corresponding to action entry for
   state STATE-NUM in yytable.  0 means no conflicts.  The list in
   yyconfl is terminated by a rule number of 0.  */
static const yytype_int8 yyconflp[] =
{
       0,     0,     0,     4,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    19,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      11,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     7,     0,     0,     0,     0,     0,    13,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      17,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     1,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      24,     0,     0,     0,     0,     0,     0,     9,    15,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    22,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0
};

/* YYCONFL[I] -- lists of conflicting rule numbers, each terminated by
   0, pointed into by YYCONFLP.  */
static const short yyconfl[] =
{
       0,     3,    54,     0,     3,     5,     0,    14,     0,    41,
       0,    43,     0,    52,     0,     3,     0,    40,     0,     3,
       6,     0,    58,     0,    55,     0,     3,     7,     0
};

/* Error token number */
#define YYTERROR 1


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

# ifndef YYLLOC_DEFAULT
#  define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
        }                                                               \
    while (false)
# endif

# define YYRHSLOC(Rhs, K) ((Rhs)[K].yystate.yyloc)



#undef yynerrs
#define yynerrs (yystackp->yyerrcnt)
#undef yychar
#define yychar (yystackp->yyrawchar)
#undef yylval
#define yylval (yystackp->yyval)
#undef yylloc
#define yylloc (yystackp->yyloc)


static const int YYEOF = 0;
static const int YYEMPTY = -2;

typedef enum { yyok, yyaccept, yyabort, yyerr } YYRESULTTAG;

#define YYCHK(YYE)                              \
  do {                                          \
    YYRESULTTAG yychk_flag = YYE;               \
    if (yychk_flag != yyok)                     \
      return yychk_flag;                        \
  } while (0)

#if YYDEBUG

# ifndef YYFPRINTF
#  define YYFPRINTF fprintf
# endif

# define YY_FPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_FPRINTF_

# define YY_FPRINTF_(Args)                      \
  do {                                          \
    YYFPRINTF Args;                             \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)

# define YY_DPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_DPRINTF_

# define YY_DPRINTF_(Args)                      \
  do {                                          \
    if (yydebug)                                \
      YYFPRINTF Args;                           \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
 }

#  define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif



/*--------------------.
| Print this symbol.  |
`--------------------*/

static void
yy_symbol_print (FILE *, int yytype, const iresearch::iql::parser::semantic_type *yyvaluep, const iresearch::iql::parser::location_type *yylocationp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  YYUSE (yyparser);
  YYUSE (ctx);
  yyparser.yy_symbol_print_ (yytype, yyvaluep, yylocationp);
}


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                  \
  do {                                                                  \
    if (yydebug)                                                        \
      {                                                                 \
        YY_FPRINTF ((stderr, "%s ", Title));                            \
        yy_symbol_print (stderr, Type, Value, Location, yyparser, ctx);        \
        YY_FPRINTF ((stderr, "\n"));                                    \
      }                                                                 \
  } while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;

struct yyGLRStack;
static void yypstack (struct yyGLRStack* yystackp, ptrdiff_t yyk)
  YY_ATTRIBUTE_UNUSED;
static void yypdumpstack (struct yyGLRStack* yystackp)
  YY_ATTRIBUTE_UNUSED;

#else /* !YYDEBUG */

# define YY_DPRINTF(Args) do {} while (yyfalse)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)

#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYMAXDEPTH * sizeof (GLRStackItem)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

/* Minimum number of free items on the stack allowed after an
   allocation.  This is to allow allocation and initialization
   to be completed by functions that call yyexpandGLRStack before the
   stack is expanded, thus insuring that all necessary pointers get
   properly redirected to new data.  */
#define YYHEADROOM 2

#ifndef YYSTACKEXPANDABLE
#  define YYSTACKEXPANDABLE 1
#endif

#if YYSTACKEXPANDABLE
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyexpandGLRStack (Yystack);                       \
  } while (0)
#else
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyMemoryExhausted (Yystack);                      \
  } while (0)
#endif


#if YYERROR_VERBOSE

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
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
static ptrdiff_t
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      ptrdiff_t yyn = 0;
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
            else
              goto append;

          append:
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

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return YY_CAST (ptrdiff_t, strlen (yystr));
}
# endif

#endif /* !YYERROR_VERBOSE */

/** State numbers. */
typedef int yyStateNum;

/** Rule numbers. */
typedef int yyRuleNum;

/** Grammar symbol. */
typedef int yySymbol;

/** Item references. */
typedef short yyItemNum;

typedef struct yyGLRState yyGLRState;
typedef struct yyGLRStateSet yyGLRStateSet;
typedef struct yySemanticOption yySemanticOption;
typedef union yyGLRStackItem yyGLRStackItem;
typedef struct yyGLRStack yyGLRStack;

struct yyGLRState {
  /** Type tag: always true.  */
  yybool yyisState;
  /** Type tag for yysemantics.  If true, yysval applies, otherwise
   *  yyfirstVal applies.  */
  yybool yyresolved;
  /** Number of corresponding LALR(1) machine state.  */
  yyStateNum yylrState;
  /** Preceding state in this stack */
  yyGLRState* yypred;
  /** Source position of the last token produced by my symbol */
  ptrdiff_t yyposn;
  union {
    /** First in a chain of alternative reductions producing the
     *  nonterminal corresponding to this state, threaded through
     *  yynext.  */
    yySemanticOption* yyfirstVal;
    /** Semantic value for this state.  */
    YYSTYPE yysval;
  } yysemantics;
  /** Source location for this state.  */
  YYLTYPE yyloc;
};

struct yyGLRStateSet {
  yyGLRState** yystates;
  /** During nondeterministic operation, yylookaheadNeeds tracks which
   *  stacks have actually needed the current lookahead.  During deterministic
   *  operation, yylookaheadNeeds[0] is not maintained since it would merely
   *  duplicate yychar != YYEMPTY.  */
  yybool* yylookaheadNeeds;
  ptrdiff_t yysize;
  ptrdiff_t yycapacity;
};

struct yySemanticOption {
  /** Type tag: always false.  */
  yybool yyisState;
  /** Rule number for this reduction */
  yyRuleNum yyrule;
  /** The last RHS state in the list of states to be reduced.  */
  yyGLRState* yystate;
  /** The lookahead for this reduction.  */
  int yyrawchar;
  YYSTYPE yyval;
  YYLTYPE yyloc;
  /** Next sibling in chain of options.  To facilitate merging,
   *  options are chained in decreasing order by address.  */
  yySemanticOption* yynext;
};

/** Type of the items in the GLR stack.  The yyisState field
 *  indicates which item of the union is valid.  */
union yyGLRStackItem {
  yyGLRState yystate;
  yySemanticOption yyoption;
};

struct yyGLRStack {
  int yyerrState;
  /* To compute the location of the error token.  */
  yyGLRStackItem yyerror_range[3];

  int yyerrcnt;
  int yyrawchar;
  YYSTYPE yyval;
  YYLTYPE yyloc;

  YYJMP_BUF yyexception_buffer;
  yyGLRStackItem* yyitems;
  yyGLRStackItem* yynextFree;
  ptrdiff_t yyspaceLeft;
  yyGLRState* yysplitPoint;
  yyGLRState* yylastDeleted;
  yyGLRStateSet yytops;
};

#if YYSTACKEXPANDABLE
static void yyexpandGLRStack (yyGLRStack* yystackp);
#endif

_Noreturn static void
yyFail (yyGLRStack* yystackp, YYLTYPE *yylocp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx, const char* yymsg)
{
  if (yymsg != YY_NULLPTR)
    yyerror (yylocp, yyparser, ctx, yymsg);
  YYLONGJMP (yystackp->yyexception_buffer, 1);
}

_Noreturn static void
yyMemoryExhausted (yyGLRStack* yystackp)
{
  YYLONGJMP (yystackp->yyexception_buffer, 2);
}

#if YYDEBUG || YYERROR_VERBOSE
/** A printable representation of TOKEN.  */
static inline const char*
yytokenName (yySymbol yytoken)
{
  return yytoken == YYEMPTY ? "" : yytname[yytoken];
}
#endif

/** Fill in YYVSP[YYLOW1 .. YYLOW0-1] from the chain of states starting
 *  at YYVSP[YYLOW0].yystate.yypred.  Leaves YYVSP[YYLOW1].yystate.yypred
 *  containing the pointer to the next state in the chain.  */
static void yyfillin (yyGLRStackItem *, int, int) YY_ATTRIBUTE_UNUSED;
static void
yyfillin (yyGLRStackItem *yyvsp, int yylow0, int yylow1)
{
  int i;
  yyGLRState *s = yyvsp[yylow0].yystate.yypred;
  for (i = yylow0-1; i >= yylow1; i -= 1)
    {
#if YYDEBUG
      yyvsp[i].yystate.yylrState = s->yylrState;
#endif
      yyvsp[i].yystate.yyresolved = s->yyresolved;
      if (s->yyresolved)
        yyvsp[i].yystate.yysemantics.yysval = s->yysemantics.yysval;
      else
        /* The effect of using yysval or yyloc (in an immediate rule) is
         * undefined.  */
        yyvsp[i].yystate.yysemantics.yyfirstVal = YY_NULLPTR;
      yyvsp[i].yystate.yyloc = s->yyloc;
      s = yyvsp[i].yystate.yypred = s->yypred;
    }
}


/** If yychar is empty, fetch the next token.  */
static inline yySymbol
yygetToken (int *yycharp, yyGLRStack* yystackp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  yySymbol yytoken;
  YYUSE (yyparser);
  YYUSE (ctx);
  if (*yycharp == YYEMPTY)
    {
      YY_DPRINTF ((stderr, "Reading a token: "));
#if YY_EXCEPTIONS
      try
        {
#endif // YY_EXCEPTIONS
          *yycharp = yylex (&yylval, &yylloc);
#if YY_EXCEPTIONS
        }
      catch (const iresearch::iql::parser::syntax_error& yyexc)
        {
          YY_DPRINTF ((stderr, "Caught exception: %s\n", yyexc.what()));
          yylloc = yyexc.location;
          yyerror (&yylloc, yyparser, ctx, yyexc.what ());
          // Map errors caught in the scanner to the undefined token
          // (YYUNDEFTOK), so that error handling is started.
          // However, record this with this special value of yychar.
          *yycharp = YYFAULTYTOK;
        }
#endif // YY_EXCEPTIONS
    }
  if (*yycharp <= YYEOF)
    {
      *yycharp = yytoken = YYEOF;
      YY_DPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (*yycharp);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }
  return yytoken;
}

/* Do nothing if YYNORMAL or if *YYLOW <= YYLOW1.  Otherwise, fill in
 * YYVSP[YYLOW1 .. *YYLOW-1] as in yyfillin and set *YYLOW = YYLOW1.
 * For convenience, always return YYLOW1.  */
static inline int yyfill (yyGLRStackItem *, int *, int, yybool)
     YY_ATTRIBUTE_UNUSED;
static inline int
yyfill (yyGLRStackItem *yyvsp, int *yylow, int yylow1, yybool yynormal)
{
  if (!yynormal && yylow1 < *yylow)
    {
      yyfillin (yyvsp, *yylow, yylow1);
      *yylow = yylow1;
    }
  return yylow1;
}

/** Perform user action for rule number YYN, with RHS length YYRHSLEN,
 *  and top stack item YYVSP.  YYLVALP points to place to put semantic
 *  value ($$), and yylocp points to place for location information
 *  (@$).  Returns yyok for normal return, yyaccept for YYACCEPT,
 *  yyerr for YYERROR, yyabort for YYABORT.  */
static YYRESULTTAG
yyuserAction (yyRuleNum yyn, int yyrhslen, yyGLRStackItem* yyvsp,
              yyGLRStack* yystackp,
              YYSTYPE* yyvalp, YYLTYPE *yylocp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  yybool yynormal YY_ATTRIBUTE_UNUSED = yystackp->yysplitPoint == YY_NULLPTR;
  int yylow;
  YYUSE (yyvalp);
  YYUSE (yylocp);
  YYUSE (yyparser);
  YYUSE (ctx);
  YYUSE (yyrhslen);
# undef yyerrok
# define yyerrok (yystackp->yyerrState = 0)
# undef YYACCEPT
# define YYACCEPT return yyaccept
# undef YYABORT
# define YYABORT return yyabort
# undef YYERROR
# define YYERROR return yyerrok, yyerr
# undef YYRECOVERING
# define YYRECOVERING() (yystackp->yyerrState != 0)
# undef yyclearin
# define yyclearin (yychar = YYEMPTY)
# undef YYFILL
# define YYFILL(N) yyfill (yyvsp, &yylow, (N), yynormal)
# undef YYBACKUP
# define YYBACKUP(Token, Value)                                              \
  return yyerror (yylocp, yyparser, ctx, YY_("syntax error: cannot back up")),     \
         yyerrok, yyerr

  yylow = 1;
  if (yyrhslen == 0)
    *yyvalp = yyval_default;
  else
    *yyvalp = yyvsp[YYFILL (1-yyrhslen)].yystate.yysemantics.yysval;
  /* Default location. */
  YYLLOC_DEFAULT ((*yylocp), (yyvsp - yyrhslen), yyrhslen);
  yystackp->yyerror_range[1].yystate.yyloc = *yylocp;

#if YY_EXCEPTIONS
  typedef iresearch::iql::parser::syntax_error syntax_error;
  try
  {
#endif // YY_EXCEPTIONS
  switch (yyn)
    {
  case 2:
#line 245 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                              { if (!ctx.setQuery(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yysval)) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1229 "iql/parser.cc"
    break;

  case 6:
#line 255 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                            { if (!((*yyvalp) = ctx.op_or(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1235 "iql/parser.cc"
    break;

  case 7:
#line 256 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                            { if (!((*yyvalp) = ctx.op_or(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1241 "iql/parser.cc"
    break;

  case 9:
#line 261 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                               { if (!((*yyvalp) = ctx.op_and(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1247 "iql/parser.cc"
    break;

  case 10:
#line 262 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                               { if (!((*yyvalp) = ctx.op_and(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1253 "iql/parser.cc"
    break;

  case 15:
#line 273 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                                     { if (!((*yyvalp) = ctx.boost(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1259 "iql/parser.cc"
    break;

  case 16:
#line 274 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                                     { if (!((*yyvalp) = ctx.boost(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1265 "iql/parser.cc"
    break;

  case 17:
#line 278 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                         { if (!((*yyvalp) = ctx.negation(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1271 "iql/parser.cc"
    break;

  case 18:
#line 279 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                         { if (!((*yyvalp) = ctx.negation(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1277 "iql/parser.cc"
    break;

  case 19:
#line 283 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                        { (*yyvalp) = YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1283 "iql/parser.cc"
    break;

  case 21:
#line 288 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                                             { if (!((*yyvalp) = ctx.function(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1289 "iql/parser.cc"
    break;

  case 22:
#line 289 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                                             { if (!((*yyvalp) = ctx.function(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1295 "iql/parser.cc"
    break;

  case 26:
#line 299 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                          { if (!((*yyvalp) = ctx.list(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1301 "iql/parser.cc"
    break;

  case 27:
#line 302 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                    { if (!((*yyvalp) = ctx.op_like(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1307 "iql/parser.cc"
    break;

  case 28:
#line 303 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                    { if (!((*yyvalp) = ctx.negation((*yyvalp) = ctx.op_eq(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, (*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval, true, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval, true))))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1313 "iql/parser.cc"
    break;

  case 29:
#line 304 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                    { if (!((*yyvalp) = ctx.op_eq(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, (*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, true, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval, false)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1319 "iql/parser.cc"
    break;

  case 30:
#line 305 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                    { if (!((*yyvalp) = ctx.op_eq(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, (*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, true, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval, true)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1325 "iql/parser.cc"
    break;

  case 31:
#line 306 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                    { if (!((*yyvalp) = ctx.op_eq(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, (*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval, true, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval, true)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1331 "iql/parser.cc"
    break;

  case 32:
#line 307 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                    { if (!((*yyvalp) = ctx.op_eq(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, (*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval, true, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, true)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1337 "iql/parser.cc"
    break;

  case 33:
#line 308 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                    { if (!((*yyvalp) = ctx.op_eq(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, (*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval, false, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, true)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1343 "iql/parser.cc"
    break;

  case 34:
#line 309 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                    { if (!((*yyvalp) = ctx.negation((*yyvalp) = ctx.op_eq(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1349 "iql/parser.cc"
    break;

  case 35:
#line 310 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                    { if (!((*yyvalp) = ctx.op_eq(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1355 "iql/parser.cc"
    break;

  case 36:
#line 314 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                                           { if (!((*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, true, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, true))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1361 "iql/parser.cc"
    break;

  case 37:
#line 315 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                                           { if (!((*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, true, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, false))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1367 "iql/parser.cc"
    break;

  case 38:
#line 316 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                                           { if (!((*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, false, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, false))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1373 "iql/parser.cc"
    break;

  case 39:
#line 317 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                                           { if (!((*yyvalp) = ctx.range(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yysval, false, YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, true))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1379 "iql/parser.cc"
    break;

  case 46:
#line 336 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                             { if (!((*yyvalp) = ctx.sequence((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1385 "iql/parser.cc"
    break;

  case 47:
#line 337 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                             { if (!((*yyvalp) = ctx.append(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yysval, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1391 "iql/parser.cc"
    break;

  case 48:
#line 341 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                     { if (!((*yyvalp) = ctx.sequence((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1397 "iql/parser.cc"
    break;

  case 49:
#line 342 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                     { if (!((*yyvalp) = ctx.append((*yyvalp) = ctx.append(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yysval, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc)), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1403 "iql/parser.cc"
    break;

  case 50:
#line 346 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                     { if (!((*yyvalp) = ctx.sequence((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1409 "iql/parser.cc"
    break;

  case 51:
#line 347 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                                     { if (!((*yyvalp) = ctx.append((*yyvalp) = ctx.append(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yysval, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc)), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc)))) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1415 "iql/parser.cc"
    break;

  case 53:
#line 352 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                                 { if (!ctx.setLimit(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval)) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1421 "iql/parser.cc"
    break;

  case 58:
#line 366 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                        { if (!ctx.addOrder(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval)) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1427 "iql/parser.cc"
    break;

  case 59:
#line 367 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                        { if (!ctx.addOrder(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, true)) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1433 "iql/parser.cc"
    break;

  case 60:
#line 368 "/home/gnusi/git/iresearch/core/iql/parser.yy"
                        { if (!ctx.addOrder(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval, false)) YYERROR; (*yylocp).begin = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).begin; (*yylocp).end = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).end; }
#line 1439 "iql/parser.cc"
    break;


#line 1443 "iql/parser.cc"

      default: break;
    }
#if YY_EXCEPTIONS
  }
  catch (const syntax_error& yyexc)
    {
      YY_DPRINTF ((stderr, "Caught exception: %s\n", yyexc.what()));
      *yylocp = yyexc.location;
      yyerror (yylocp, yyparser, ctx, yyexc.what ());
      YYERROR;
    }
#endif // YY_EXCEPTIONS

  return yyok;
# undef yyerrok
# undef YYABORT
# undef YYACCEPT
# undef YYERROR
# undef YYBACKUP
# undef yyclearin
# undef YYRECOVERING
}


static void
yyuserMerge (int yyn, YYSTYPE* yy0, YYSTYPE* yy1)
{
  YYUSE (yy0);
  YYUSE (yy1);

  switch (yyn)
    {

      default: break;
    }
}

                              /* Bison grammar-table manipulation.  */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (yyparser);
  YYUSE (ctx);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/** Number of symbols composing the right hand side of rule #RULE.  */
static inline int
yyrhsLength (yyRuleNum yyrule)
{
  return yyr2[yyrule];
}

static void
yydestroyGLRState (char const *yymsg, yyGLRState *yys, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  if (yys->yyresolved)
    yydestruct (yymsg, yystos[yys->yylrState],
                &yys->yysemantics.yysval, &yys->yyloc, yyparser, ctx);
  else
    {
#if YYDEBUG
      if (yydebug)
        {
          if (yys->yysemantics.yyfirstVal)
            YY_FPRINTF ((stderr, "%s unresolved", yymsg));
          else
            YY_FPRINTF ((stderr, "%s incomplete", yymsg));
          YY_SYMBOL_PRINT ("", yystos[yys->yylrState], YY_NULLPTR, &yys->yyloc);
        }
#endif

      if (yys->yysemantics.yyfirstVal)
        {
          yySemanticOption *yyoption = yys->yysemantics.yyfirstVal;
          yyGLRState *yyrh;
          int yyn;
          for (yyrh = yyoption->yystate, yyn = yyrhsLength (yyoption->yyrule);
               yyn > 0;
               yyrh = yyrh->yypred, yyn -= 1)
            yydestroyGLRState (yymsg, yyrh, yyparser, ctx);
        }
    }
}

/** Left-hand-side symbol for rule #YYRULE.  */
static inline yySymbol
yylhsNonterm (yyRuleNum yyrule)
{
  return yyr1[yyrule];
}

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

/** True iff LR state YYSTATE has only a default reduction (regardless
 *  of token).  */
static inline yybool
yyisDefaultedState (yyStateNum yystate)
{
  return yypact_value_is_default (yypact[yystate]);
}

/** The default reduction for YYSTATE, assuming it has one.  */
static inline yyRuleNum
yydefaultAction (yyStateNum yystate)
{
  return yydefact[yystate];
}

#define yytable_value_is_error(Yyn) \
  0

/** The action to take in YYSTATE on seeing YYTOKEN.
 *  Result R means
 *    R < 0:  Reduce on rule -R.
 *    R = 0:  Error.
 *    R > 0:  Shift to state R.
 *  Set *YYCONFLICTS to a pointer into yyconfl to a 0-terminated list
 *  of conflicting reductions.
 */
static inline int
yygetLRActions (yyStateNum yystate, yySymbol yytoken, const short** yyconflicts)
{
  int yyindex = yypact[yystate] + yytoken;
  if (yyisDefaultedState (yystate)
      || yyindex < 0 || YYLAST < yyindex || yycheck[yyindex] != yytoken)
    {
      *yyconflicts = yyconfl;
      return -yydefact[yystate];
    }
  else if (! yytable_value_is_error (yytable[yyindex]))
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return yytable[yyindex];
    }
  else
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return 0;
    }
}

/** Compute post-reduction state.
 * \param yystate   the current state
 * \param yysym     the nonterminal to push on the stack
 */
static inline yyStateNum
yyLRgotoState (yyStateNum yystate, yySymbol yysym)
{
  int yyr = yypgoto[yysym - YYNTOKENS] + yystate;
  if (0 <= yyr && yyr <= YYLAST && yycheck[yyr] == yystate)
    return yytable[yyr];
  else
    return yydefgoto[yysym - YYNTOKENS];
}

static inline yybool
yyisShiftAction (int yyaction)
{
  return 0 < yyaction;
}

static inline yybool
yyisErrorAction (int yyaction)
{
  return yyaction == 0;
}

                                /* GLRStates */

/** Return a fresh GLRStackItem in YYSTACKP.  The item is an LR state
 *  if YYISSTATE, and otherwise a semantic option.  Callers should call
 *  YY_RESERVE_GLRSTACK afterwards to make sure there is sufficient
 *  headroom.  */

static inline yyGLRStackItem*
yynewGLRStackItem (yyGLRStack* yystackp, yybool yyisState)
{
  yyGLRStackItem* yynewItem = yystackp->yynextFree;
  yystackp->yyspaceLeft -= 1;
  yystackp->yynextFree += 1;
  yynewItem->yystate.yyisState = yyisState;
  return yynewItem;
}

/** Add a new semantic action that will execute the action for rule
 *  YYRULE on the semantic values in YYRHS to the list of
 *  alternative actions for YYSTATE.  Assumes that YYRHS comes from
 *  stack #YYK of *YYSTACKP. */
static void
yyaddDeferredAction (yyGLRStack* yystackp, ptrdiff_t yyk, yyGLRState* yystate,
                     yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yySemanticOption* yynewOption =
    &yynewGLRStackItem (yystackp, yyfalse)->yyoption;
  YY_ASSERT (!yynewOption->yyisState);
  yynewOption->yystate = yyrhs;
  yynewOption->yyrule = yyrule;
  if (yystackp->yytops.yylookaheadNeeds[yyk])
    {
      yynewOption->yyrawchar = yychar;
      yynewOption->yyval = yylval;
      yynewOption->yyloc = yylloc;
    }
  else
    yynewOption->yyrawchar = YYEMPTY;
  yynewOption->yynext = yystate->yysemantics.yyfirstVal;
  yystate->yysemantics.yyfirstVal = yynewOption;

  YY_RESERVE_GLRSTACK (yystackp);
}

                                /* GLRStacks */

/** Initialize YYSET to a singleton set containing an empty stack.  */
static yybool
yyinitStateSet (yyGLRStateSet* yyset)
{
  yyset->yysize = 1;
  yyset->yycapacity = 16;
  yyset->yystates
    = YY_CAST (yyGLRState**,
               YYMALLOC (YY_CAST (size_t, yyset->yycapacity)
                         * sizeof yyset->yystates[0]));
  if (! yyset->yystates)
    return yyfalse;
  yyset->yystates[0] = YY_NULLPTR;
  yyset->yylookaheadNeeds
    = YY_CAST (yybool*,
               YYMALLOC (YY_CAST (size_t, yyset->yycapacity)
                         * sizeof yyset->yylookaheadNeeds[0]));
  if (! yyset->yylookaheadNeeds)
    {
      YYFREE (yyset->yystates);
      return yyfalse;
    }
  memset (yyset->yylookaheadNeeds,
          0,
          YY_CAST (size_t, yyset->yycapacity) * sizeof yyset->yylookaheadNeeds[0]);
  return yytrue;
}

static void yyfreeStateSet (yyGLRStateSet* yyset)
{
  YYFREE (yyset->yystates);
  YYFREE (yyset->yylookaheadNeeds);
}

/** Initialize *YYSTACKP to a single empty stack, with total maximum
 *  capacity for all stacks of YYSIZE.  */
static yybool
yyinitGLRStack (yyGLRStack* yystackp, ptrdiff_t yysize)
{
  yystackp->yyerrState = 0;
  yynerrs = 0;
  yystackp->yyspaceLeft = yysize;
  yystackp->yyitems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (size_t, yysize)
                         * sizeof yystackp->yynextFree[0]));
  if (!yystackp->yyitems)
    return yyfalse;
  yystackp->yynextFree = yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;
  return yyinitStateSet (&yystackp->yytops);
}


#if YYSTACKEXPANDABLE
# define YYRELOC(YYFROMITEMS, YYTOITEMS, YYX, YYTYPE)                   \
  &((YYTOITEMS)                                                         \
    - ((YYFROMITEMS) - YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX))))->YYTYPE

/** If *YYSTACKP is expandable, extend it.  WARNING: Pointers into the
    stack from outside should be considered invalid after this call.
    We always expand when there are 1 or fewer items left AFTER an
    allocation, so that we can avoid having external pointers exist
    across an allocation.  */
static void
yyexpandGLRStack (yyGLRStack* yystackp)
{
  yyGLRStackItem* yynewItems;
  yyGLRStackItem* yyp0, *yyp1;
  ptrdiff_t yynewSize;
  ptrdiff_t yyn;
  ptrdiff_t yysize = yystackp->yynextFree - yystackp->yyitems;
  if (YYMAXDEPTH - YYHEADROOM < yysize)
    yyMemoryExhausted (yystackp);
  yynewSize = 2*yysize;
  if (YYMAXDEPTH < yynewSize)
    yynewSize = YYMAXDEPTH;
  yynewItems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (size_t, yynewSize)
                         * sizeof yynewItems[0]));
  if (! yynewItems)
    yyMemoryExhausted (yystackp);
  for (yyp0 = yystackp->yyitems, yyp1 = yynewItems, yyn = yysize;
       0 < yyn;
       yyn -= 1, yyp0 += 1, yyp1 += 1)
    {
      *yyp1 = *yyp0;
      if (*YY_REINTERPRET_CAST (yybool *, yyp0))
        {
          yyGLRState* yys0 = &yyp0->yystate;
          yyGLRState* yys1 = &yyp1->yystate;
          if (yys0->yypred != YY_NULLPTR)
            yys1->yypred =
              YYRELOC (yyp0, yyp1, yys0->yypred, yystate);
          if (! yys0->yyresolved && yys0->yysemantics.yyfirstVal != YY_NULLPTR)
            yys1->yysemantics.yyfirstVal =
              YYRELOC (yyp0, yyp1, yys0->yysemantics.yyfirstVal, yyoption);
        }
      else
        {
          yySemanticOption* yyv0 = &yyp0->yyoption;
          yySemanticOption* yyv1 = &yyp1->yyoption;
          if (yyv0->yystate != YY_NULLPTR)
            yyv1->yystate = YYRELOC (yyp0, yyp1, yyv0->yystate, yystate);
          if (yyv0->yynext != YY_NULLPTR)
            yyv1->yynext = YYRELOC (yyp0, yyp1, yyv0->yynext, yyoption);
        }
    }
  if (yystackp->yysplitPoint != YY_NULLPTR)
    yystackp->yysplitPoint = YYRELOC (yystackp->yyitems, yynewItems,
                                      yystackp->yysplitPoint, yystate);

  for (yyn = 0; yyn < yystackp->yytops.yysize; yyn += 1)
    if (yystackp->yytops.yystates[yyn] != YY_NULLPTR)
      yystackp->yytops.yystates[yyn] =
        YYRELOC (yystackp->yyitems, yynewItems,
                 yystackp->yytops.yystates[yyn], yystate);
  YYFREE (yystackp->yyitems);
  yystackp->yyitems = yynewItems;
  yystackp->yynextFree = yynewItems + yysize;
  yystackp->yyspaceLeft = yynewSize - yysize;
}
#endif

static void
yyfreeGLRStack (yyGLRStack* yystackp)
{
  YYFREE (yystackp->yyitems);
  yyfreeStateSet (&yystackp->yytops);
}

/** Assuming that YYS is a GLRState somewhere on *YYSTACKP, update the
 *  splitpoint of *YYSTACKP, if needed, so that it is at least as deep as
 *  YYS.  */
static inline void
yyupdateSplit (yyGLRStack* yystackp, yyGLRState* yys)
{
  if (yystackp->yysplitPoint != YY_NULLPTR && yystackp->yysplitPoint > yys)
    yystackp->yysplitPoint = yys;
}

/** Invalidate stack #YYK in *YYSTACKP.  */
static inline void
yymarkStackDeleted (yyGLRStack* yystackp, ptrdiff_t yyk)
{
  if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    yystackp->yylastDeleted = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yystates[yyk] = YY_NULLPTR;
}

/** Undelete the last stack in *YYSTACKP that was marked as deleted.  Can
    only be done once after a deletion, and only when all other stacks have
    been deleted.  */
static void
yyundeleteLastStack (yyGLRStack* yystackp)
{
  if (yystackp->yylastDeleted == YY_NULLPTR || yystackp->yytops.yysize != 0)
    return;
  yystackp->yytops.yystates[0] = yystackp->yylastDeleted;
  yystackp->yytops.yysize = 1;
  YY_DPRINTF ((stderr, "Restoring last deleted stack as stack #0.\n"));
  yystackp->yylastDeleted = YY_NULLPTR;
}

static inline void
yyremoveDeletes (yyGLRStack* yystackp)
{
  ptrdiff_t yyi, yyj;
  yyi = yyj = 0;
  while (yyj < yystackp->yytops.yysize)
    {
      if (yystackp->yytops.yystates[yyi] == YY_NULLPTR)
        {
          if (yyi == yyj)
            YY_DPRINTF ((stderr, "Removing dead stacks.\n"));
          yystackp->yytops.yysize -= 1;
        }
      else
        {
          yystackp->yytops.yystates[yyj] = yystackp->yytops.yystates[yyi];
          /* In the current implementation, it's unnecessary to copy
             yystackp->yytops.yylookaheadNeeds[yyi] since, after
             yyremoveDeletes returns, the parser immediately either enters
             deterministic operation or shifts a token.  However, it doesn't
             hurt, and the code might evolve to need it.  */
          yystackp->yytops.yylookaheadNeeds[yyj] =
            yystackp->yytops.yylookaheadNeeds[yyi];
          if (yyj != yyi)
            YY_DPRINTF ((stderr, "Rename stack %ld -> %ld.\n",
                        YY_CAST (long, yyi), YY_CAST (long, yyj)));
          yyj += 1;
        }
      yyi += 1;
    }
}

/** Shift to a new state on stack #YYK of *YYSTACKP, corresponding to LR
 * state YYLRSTATE, at input position YYPOSN, with (resolved) semantic
 * value *YYVALP and source location *YYLOCP.  */
static inline void
yyglrShift (yyGLRStack* yystackp, ptrdiff_t yyk, yyStateNum yylrState,
            ptrdiff_t yyposn,
            YYSTYPE* yyvalp, YYLTYPE* yylocp)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yytrue;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yysval = *yyvalp;
  yynewState->yyloc = *yylocp;
  yystackp->yytops.yystates[yyk] = yynewState;

  YY_RESERVE_GLRSTACK (yystackp);
}

/** Shift stack #YYK of *YYSTACKP, to a new state corresponding to LR
 *  state YYLRSTATE, at input position YYPOSN, with the (unresolved)
 *  semantic value of YYRHS under the action for YYRULE.  */
static inline void
yyglrShiftDefer (yyGLRStack* yystackp, ptrdiff_t yyk, yyStateNum yylrState,
                 ptrdiff_t yyposn, yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;
  YY_ASSERT (yynewState->yyisState);

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yyfalse;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yyfirstVal = YY_NULLPTR;
  yystackp->yytops.yystates[yyk] = yynewState;

  /* Invokes YY_RESERVE_GLRSTACK.  */
  yyaddDeferredAction (yystackp, yyk, yynewState, yyrhs, yyrule);
}

#if !YYDEBUG
# define YY_REDUCE_PRINT(Args)
#else
# define YY_REDUCE_PRINT(Args)          \
  do {                                  \
    if (yydebug)                        \
      yy_reduce_print Args;             \
  } while (0)

/*----------------------------------------------------------------------.
| Report that stack #YYK of *YYSTACKP is going to be reduced by YYRULE. |
`----------------------------------------------------------------------*/

static inline void
yy_reduce_print (yybool yynormal, yyGLRStackItem* yyvsp, ptrdiff_t yyk,
                 yyRuleNum yyrule, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  int yynrhs = yyrhsLength (yyrule);
  int yylow = 1;
  int yyi;
  YY_FPRINTF ((stderr, "Reducing stack %ld by rule %d (line %d):\n",
               YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule]));
  if (! yynormal)
    yyfillin (yyvsp, 1, -yynrhs);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YY_FPRINTF ((stderr, "   $%d = ", yyi + 1));
      yy_symbol_print (stderr,
                       yystos[yyvsp[yyi - yynrhs + 1].yystate.yylrState],
                       &yyvsp[yyi - yynrhs + 1].yystate.yysemantics.yysval,
                       &(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL ((yyi + 1) - (yynrhs))].yystate.yyloc)                       , yyparser, ctx);
      if (!yyvsp[yyi - yynrhs + 1].yystate.yyresolved)
        YY_FPRINTF ((stderr, " (unresolved)"));
      YY_FPRINTF ((stderr, "\n"));
    }
}
#endif

/** Pop the symbols consumed by reduction #YYRULE from the top of stack
 *  #YYK of *YYSTACKP, and perform the appropriate semantic action on their
 *  semantic values.  Assumes that all ambiguities in semantic values
 *  have been previously resolved.  Set *YYVALP to the resulting value,
 *  and *YYLOCP to the computed location (if any).  Return value is as
 *  for userAction.  */
static inline YYRESULTTAG
yydoAction (yyGLRStack* yystackp, ptrdiff_t yyk, yyRuleNum yyrule,
            YYSTYPE* yyvalp, YYLTYPE *yylocp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  int yynrhs = yyrhsLength (yyrule);

  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      /* Standard special case: single stack.  */
      yyGLRStackItem* yyrhs
        = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yytops.yystates[yyk]);
      YY_ASSERT (yyk == 0);
      yystackp->yynextFree -= yynrhs;
      yystackp->yyspaceLeft += yynrhs;
      yystackp->yytops.yystates[0] = & yystackp->yynextFree[-1].yystate;
      YY_REDUCE_PRINT ((yytrue, yyrhs, yyk, yyrule, yyparser, ctx));
      return yyuserAction (yyrule, yynrhs, yyrhs, yystackp,
                           yyvalp, yylocp, yyparser, ctx);
    }
  else
    {
      yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
      yyGLRState* yys = yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred
        = yystackp->yytops.yystates[yyk];
      int yyi;
      if (yynrhs == 0)
        /* Set default location.  */
        yyrhsVals[YYMAXRHS + YYMAXLEFT - 1].yystate.yyloc = yys->yyloc;
      for (yyi = 0; yyi < yynrhs; yyi += 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yystackp->yytops.yystates[yyk] = yys;
      YY_REDUCE_PRINT ((yyfalse, yyrhsVals + YYMAXRHS + YYMAXLEFT - 1, yyk, yyrule, yyparser, ctx));
      return yyuserAction (yyrule, yynrhs, yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, yyvalp, yylocp, yyparser, ctx);
    }
}

/** Pop items off stack #YYK of *YYSTACKP according to grammar rule YYRULE,
 *  and push back on the resulting nonterminal symbol.  Perform the
 *  semantic action associated with YYRULE and store its value with the
 *  newly pushed state, if YYFORCEEVAL or if *YYSTACKP is currently
 *  unambiguous.  Otherwise, store the deferred semantic action with
 *  the new state.  If the new state would have an identical input
 *  position, LR state, and predecessor to an existing state on the stack,
 *  it is identified with that existing state, eliminating stack #YYK from
 *  *YYSTACKP.  In this case, the semantic value is
 *  added to the options for the existing state's semantic value.
 */
static inline YYRESULTTAG
yyglrReduce (yyGLRStack* yystackp, ptrdiff_t yyk, yyRuleNum yyrule,
             yybool yyforceEval, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  ptrdiff_t yyposn = yystackp->yytops.yystates[yyk]->yyposn;

  if (yyforceEval || yystackp->yysplitPoint == YY_NULLPTR)
    {
      YYSTYPE yysval;
      YYLTYPE yyloc;

      YYRESULTTAG yyflag = yydoAction (yystackp, yyk, yyrule, &yysval, &yyloc, yyparser, ctx);
      if (yyflag == yyerr && yystackp->yysplitPoint != YY_NULLPTR)
        YY_DPRINTF ((stderr,
                     "Parse on stack %ld rejected by rule %d (line %d).\n",
                     YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule - 1]));
      if (yyflag != yyok)
        return yyflag;
      YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyrule], &yysval, &yyloc);
      yyglrShift (yystackp, yyk,
                  yyLRgotoState (yystackp->yytops.yystates[yyk]->yylrState,
                                 yylhsNonterm (yyrule)),
                  yyposn, &yysval, &yyloc);
    }
  else
    {
      ptrdiff_t yyi;
      int yyn;
      yyGLRState* yys, *yys0 = yystackp->yytops.yystates[yyk];
      yyStateNum yynewLRState;

      for (yys = yystackp->yytops.yystates[yyk], yyn = yyrhsLength (yyrule);
           0 < yyn; yyn -= 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yynewLRState = yyLRgotoState (yys->yylrState, yylhsNonterm (yyrule));
      YY_DPRINTF ((stderr,
                   "Reduced stack %ld by rule %d (line %d); action deferred.  "
                   "Now in state %d.\n",
                   YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule - 1],
                   yynewLRState));
      for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
        if (yyi != yyk && yystackp->yytops.yystates[yyi] != YY_NULLPTR)
          {
            yyGLRState *yysplit = yystackp->yysplitPoint;
            yyGLRState *yyp = yystackp->yytops.yystates[yyi];
            while (yyp != yys && yyp != yysplit && yyp->yyposn >= yyposn)
              {
                if (yyp->yylrState == yynewLRState && yyp->yypred == yys)
                  {
                    yyaddDeferredAction (yystackp, yyk, yyp, yys0, yyrule);
                    yymarkStackDeleted (yystackp, yyk);
                    YY_DPRINTF ((stderr, "Merging stack %ld into stack %ld.\n",
                                 YY_CAST (long, yyk), YY_CAST (long, yyi)));
                    return yyok;
                  }
                yyp = yyp->yypred;
              }
          }
      yystackp->yytops.yystates[yyk] = yys;
      yyglrShiftDefer (yystackp, yyk, yynewLRState, yyposn, yys0, yyrule);
    }
  return yyok;
}

static ptrdiff_t
yysplitStack (yyGLRStack* yystackp, ptrdiff_t yyk)
{
  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      YY_ASSERT (yyk == 0);
      yystackp->yysplitPoint = yystackp->yytops.yystates[yyk];
    }
  if (yystackp->yytops.yycapacity <= yystackp->yytops.yysize)
    {
      ptrdiff_t state_size = sizeof yystackp->yytops.yystates[0];
      ptrdiff_t half_max_capacity = YYSIZEMAX / 2 / state_size;
      if (half_max_capacity < yystackp->yytops.yycapacity)
        yyMemoryExhausted (yystackp);
      yystackp->yytops.yycapacity *= 2;

      {
        yyGLRState** yynewStates
          = YY_CAST (yyGLRState**,
                     YYREALLOC (yystackp->yytops.yystates,
                                (YY_CAST (size_t, yystackp->yytops.yycapacity)
                                 * sizeof yynewStates[0])));
        if (yynewStates == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yystates = yynewStates;
      }

      {
        yybool* yynewLookaheadNeeds
          = YY_CAST (yybool*,
                     YYREALLOC (yystackp->yytops.yylookaheadNeeds,
                                (YY_CAST (size_t, yystackp->yytops.yycapacity)
                                 * sizeof yynewLookaheadNeeds[0])));
        if (yynewLookaheadNeeds == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yylookaheadNeeds = yynewLookaheadNeeds;
      }
    }
  yystackp->yytops.yystates[yystackp->yytops.yysize]
    = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yylookaheadNeeds[yystackp->yytops.yysize]
    = yystackp->yytops.yylookaheadNeeds[yyk];
  yystackp->yytops.yysize += 1;
  return yystackp->yytops.yysize - 1;
}

/** True iff YYY0 and YYY1 represent identical options at the top level.
 *  That is, they represent the same rule applied to RHS symbols
 *  that produce the same terminal symbols.  */
static yybool
yyidenticalOptions (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  if (yyy0->yyrule == yyy1->yyrule)
    {
      yyGLRState *yys0, *yys1;
      int yyn;
      for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
           yyn = yyrhsLength (yyy0->yyrule);
           yyn > 0;
           yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
        if (yys0->yyposn != yys1->yyposn)
          return yyfalse;
      return yytrue;
    }
  else
    return yyfalse;
}

/** Assuming identicalOptions (YYY0,YYY1), destructively merge the
 *  alternative semantic values for the RHS-symbols of YYY1 and YYY0.  */
static void
yymergeOptionSets (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  yyGLRState *yys0, *yys1;
  int yyn;
  for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
       yyn = yyrhsLength (yyy0->yyrule);
       0 < yyn;
       yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
    {
      if (yys0 == yys1)
        break;
      else if (yys0->yyresolved)
        {
          yys1->yyresolved = yytrue;
          yys1->yysemantics.yysval = yys0->yysemantics.yysval;
        }
      else if (yys1->yyresolved)
        {
          yys0->yyresolved = yytrue;
          yys0->yysemantics.yysval = yys1->yysemantics.yysval;
        }
      else
        {
          yySemanticOption** yyz0p = &yys0->yysemantics.yyfirstVal;
          yySemanticOption* yyz1 = yys1->yysemantics.yyfirstVal;
          while (yytrue)
            {
              if (yyz1 == *yyz0p || yyz1 == YY_NULLPTR)
                break;
              else if (*yyz0p == YY_NULLPTR)
                {
                  *yyz0p = yyz1;
                  break;
                }
              else if (*yyz0p < yyz1)
                {
                  yySemanticOption* yyz = *yyz0p;
                  *yyz0p = yyz1;
                  yyz1 = yyz1->yynext;
                  (*yyz0p)->yynext = yyz;
                }
              yyz0p = &(*yyz0p)->yynext;
            }
          yys1->yysemantics.yyfirstVal = yys0->yysemantics.yyfirstVal;
        }
    }
}

/** Y0 and Y1 represent two possible actions to take in a given
 *  parsing state; return 0 if no combination is possible,
 *  1 if user-mergeable, 2 if Y0 is preferred, 3 if Y1 is preferred.  */
static int
yypreference (yySemanticOption* y0, yySemanticOption* y1)
{
  yyRuleNum r0 = y0->yyrule, r1 = y1->yyrule;
  int p0 = yydprec[r0], p1 = yydprec[r1];

  if (p0 == p1)
    {
      if (yymerger[r0] == 0 || yymerger[r0] != yymerger[r1])
        return 0;
      else
        return 1;
    }
  if (p0 == 0 || p1 == 0)
    return 0;
  if (p0 < p1)
    return 3;
  if (p1 < p0)
    return 2;
  return 0;
}

static YYRESULTTAG yyresolveValue (yyGLRState* yys,
                                   yyGLRStack* yystackp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx);


/** Resolve the previous YYN states starting at and including state YYS
 *  on *YYSTACKP. If result != yyok, some states may have been left
 *  unresolved possibly with empty semantic option chains.  Regardless
 *  of whether result = yyok, each state has been left with consistent
 *  data so that yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveStates (yyGLRState* yys, int yyn,
                 yyGLRStack* yystackp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  if (0 < yyn)
    {
      YY_ASSERT (yys->yypred);
      YYCHK (yyresolveStates (yys->yypred, yyn-1, yystackp, yyparser, ctx));
      if (! yys->yyresolved)
        YYCHK (yyresolveValue (yys, yystackp, yyparser, ctx));
    }
  return yyok;
}

/** Resolve the states for the RHS of YYOPT on *YYSTACKP, perform its
 *  user action, and return the semantic value and location in *YYVALP
 *  and *YYLOCP.  Regardless of whether result = yyok, all RHS states
 *  have been destroyed (assuming the user action destroys all RHS
 *  semantic values if invoked).  */
static YYRESULTTAG
yyresolveAction (yySemanticOption* yyopt, yyGLRStack* yystackp,
                 YYSTYPE* yyvalp, YYLTYPE *yylocp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
  int yynrhs = yyrhsLength (yyopt->yyrule);
  YYRESULTTAG yyflag =
    yyresolveStates (yyopt->yystate, yynrhs, yystackp, yyparser, ctx);
  if (yyflag != yyok)
    {
      yyGLRState *yys;
      for (yys = yyopt->yystate; yynrhs > 0; yys = yys->yypred, yynrhs -= 1)
        yydestroyGLRState ("Cleanup: popping", yys, yyparser, ctx);
      return yyflag;
    }

  yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred = yyopt->yystate;
  if (yynrhs == 0)
    /* Set default location.  */
    yyrhsVals[YYMAXRHS + YYMAXLEFT - 1].yystate.yyloc = yyopt->yystate->yyloc;
  {
    int yychar_current = yychar;
    YYSTYPE yylval_current = yylval;
    YYLTYPE yylloc_current = yylloc;
    yychar = yyopt->yyrawchar;
    yylval = yyopt->yyval;
    yylloc = yyopt->yyloc;
    yyflag = yyuserAction (yyopt->yyrule, yynrhs,
                           yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, yyvalp, yylocp, yyparser, ctx);
    yychar = yychar_current;
    yylval = yylval_current;
    yylloc = yylloc_current;
  }
  return yyflag;
}

#if YYDEBUG
static void
yyreportTree (yySemanticOption* yyx, int yyindent)
{
  int yynrhs = yyrhsLength (yyx->yyrule);
  int yyi;
  yyGLRState* yys;
  yyGLRState* yystates[1 + YYMAXRHS];
  yyGLRState yyleftmost_state;

  for (yyi = yynrhs, yys = yyx->yystate; 0 < yyi; yyi -= 1, yys = yys->yypred)
    yystates[yyi] = yys;
  if (yys == YY_NULLPTR)
    {
      yyleftmost_state.yyposn = 0;
      yystates[0] = &yyleftmost_state;
    }
  else
    yystates[0] = yys;

  if (yyx->yystate->yyposn < yys->yyposn + 1)
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, empty>\n",
                 yyindent, "", yytokenName (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1));
  else
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, tokens %ld .. %ld>\n",
                 yyindent, "", yytokenName (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1, YY_CAST (long, yys->yyposn + 1),
                 YY_CAST (long, yyx->yystate->yyposn)));
  for (yyi = 1; yyi <= yynrhs; yyi += 1)
    {
      if (yystates[yyi]->yyresolved)
        {
          if (yystates[yyi-1]->yyposn+1 > yystates[yyi]->yyposn)
            YY_FPRINTF ((stderr, "%*s%s <empty>\n", yyindent+2, "",
                         yytokenName (yystos[yystates[yyi]->yylrState])));
          else
            YY_FPRINTF ((stderr, "%*s%s <tokens %ld .. %ld>\n", yyindent+2, "",
                         yytokenName (yystos[yystates[yyi]->yylrState]),
                         YY_CAST (long, yystates[yyi-1]->yyposn + 1),
                         YY_CAST (long, yystates[yyi]->yyposn)));
        }
      else
        yyreportTree (yystates[yyi]->yysemantics.yyfirstVal, yyindent+2);
    }
}
#endif

static YYRESULTTAG
yyreportAmbiguity (yySemanticOption* yyx0,
                   yySemanticOption* yyx1, YYLTYPE *yylocp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  YYUSE (yyx0);
  YYUSE (yyx1);

#if YYDEBUG
  YY_FPRINTF ((stderr, "Ambiguity detected.\n"));
  YY_FPRINTF ((stderr, "Option 1,\n"));
  yyreportTree (yyx0, 2);
  YY_FPRINTF ((stderr, "\nOption 2,\n"));
  yyreportTree (yyx1, 2);
  YY_FPRINTF ((stderr, "\n"));
#endif

  yyerror (yylocp, yyparser, ctx, YY_("syntax is ambiguous"));
  return yyabort;
}

/** Resolve the locations for each of the YYN1 states in *YYSTACKP,
 *  ending at YYS1.  Has no effect on previously resolved states.
 *  The first semantic option of a state is always chosen.  */
static void
yyresolveLocations (yyGLRState *yys1, int yyn1,
                    yyGLRStack *yystackp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  if (0 < yyn1)
    {
      yyresolveLocations (yys1->yypred, yyn1 - 1, yystackp, yyparser, ctx);
      if (!yys1->yyresolved)
        {
          yyGLRStackItem yyrhsloc[1 + YYMAXRHS];
          int yynrhs;
          yySemanticOption *yyoption = yys1->yysemantics.yyfirstVal;
          YY_ASSERT (yyoption);
          yynrhs = yyrhsLength (yyoption->yyrule);
          if (0 < yynrhs)
            {
              yyGLRState *yys;
              int yyn;
              yyresolveLocations (yyoption->yystate, yynrhs,
                                  yystackp, yyparser, ctx);
              for (yys = yyoption->yystate, yyn = yynrhs;
                   yyn > 0;
                   yys = yys->yypred, yyn -= 1)
                yyrhsloc[yyn].yystate.yyloc = yys->yyloc;
            }
          else
            {
              /* Both yyresolveAction and yyresolveLocations traverse the GSS
                 in reverse rightmost order.  It is only necessary to invoke
                 yyresolveLocations on a subforest for which yyresolveAction
                 would have been invoked next had an ambiguity not been
                 detected.  Thus the location of the previous state (but not
                 necessarily the previous state itself) is guaranteed to be
                 resolved already.  */
              yyGLRState *yyprevious = yyoption->yystate;
              yyrhsloc[0].yystate.yyloc = yyprevious->yyloc;
            }
          YYLLOC_DEFAULT ((yys1->yyloc), yyrhsloc, yynrhs);
        }
    }
}

/** Resolve the ambiguity represented in state YYS in *YYSTACKP,
 *  perform the indicated actions, and set the semantic value of YYS.
 *  If result != yyok, the chain of semantic options in YYS has been
 *  cleared instead or it has been left unmodified except that
 *  redundant options may have been removed.  Regardless of whether
 *  result = yyok, YYS has been left with consistent data so that
 *  yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveValue (yyGLRState* yys, yyGLRStack* yystackp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  yySemanticOption* yyoptionList = yys->yysemantics.yyfirstVal;
  yySemanticOption* yybest = yyoptionList;
  yySemanticOption** yypp;
  yybool yymerge = yyfalse;
  YYSTYPE yysval;
  YYRESULTTAG yyflag;
  YYLTYPE *yylocp = &yys->yyloc;

  for (yypp = &yyoptionList->yynext; *yypp != YY_NULLPTR; )
    {
      yySemanticOption* yyp = *yypp;

      if (yyidenticalOptions (yybest, yyp))
        {
          yymergeOptionSets (yybest, yyp);
          *yypp = yyp->yynext;
        }
      else
        {
          switch (yypreference (yybest, yyp))
            {
            case 0:
              yyresolveLocations (yys, 1, yystackp, yyparser, ctx);
              return yyreportAmbiguity (yybest, yyp, yylocp, yyparser, ctx);
              break;
            case 1:
              yymerge = yytrue;
              break;
            case 2:
              break;
            case 3:
              yybest = yyp;
              yymerge = yyfalse;
              break;
            default:
              /* This cannot happen so it is not worth a YY_ASSERT (yyfalse),
                 but some compilers complain if the default case is
                 omitted.  */
              break;
            }
          yypp = &yyp->yynext;
        }
    }

  if (yymerge)
    {
      yySemanticOption* yyp;
      int yyprec = yydprec[yybest->yyrule];
      yyflag = yyresolveAction (yybest, yystackp, &yysval, yylocp, yyparser, ctx);
      if (yyflag == yyok)
        for (yyp = yybest->yynext; yyp != YY_NULLPTR; yyp = yyp->yynext)
          {
            if (yyprec == yydprec[yyp->yyrule])
              {
                YYSTYPE yysval_other;
                YYLTYPE yydummy;
                yyflag = yyresolveAction (yyp, yystackp, &yysval_other, &yydummy, yyparser, ctx);
                if (yyflag != yyok)
                  {
                    yydestruct ("Cleanup: discarding incompletely merged value for",
                                yystos[yys->yylrState],
                                &yysval, yylocp, yyparser, ctx);
                    break;
                  }
                yyuserMerge (yymerger[yyp->yyrule], &yysval, &yysval_other);
              }
          }
    }
  else
    yyflag = yyresolveAction (yybest, yystackp, &yysval, yylocp, yyparser, ctx);

  if (yyflag == yyok)
    {
      yys->yyresolved = yytrue;
      yys->yysemantics.yysval = yysval;
    }
  else
    yys->yysemantics.yyfirstVal = YY_NULLPTR;
  return yyflag;
}

static YYRESULTTAG
yyresolveStack (yyGLRStack* yystackp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  if (yystackp->yysplitPoint != YY_NULLPTR)
    {
      yyGLRState* yys;
      int yyn;

      for (yyn = 0, yys = yystackp->yytops.yystates[0];
           yys != yystackp->yysplitPoint;
           yys = yys->yypred, yyn += 1)
        continue;
      YYCHK (yyresolveStates (yystackp->yytops.yystates[0], yyn, yystackp
                             , yyparser, ctx));
    }
  return yyok;
}

static void
yycompressStack (yyGLRStack* yystackp)
{
  yyGLRState* yyp, *yyq, *yyr;

  if (yystackp->yytops.yysize != 1 || yystackp->yysplitPoint == YY_NULLPTR)
    return;

  for (yyp = yystackp->yytops.yystates[0], yyq = yyp->yypred, yyr = YY_NULLPTR;
       yyp != yystackp->yysplitPoint;
       yyr = yyp, yyp = yyq, yyq = yyp->yypred)
    yyp->yypred = yyr;

  yystackp->yyspaceLeft += yystackp->yynextFree - yystackp->yyitems;
  yystackp->yynextFree = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yysplitPoint) + 1;
  yystackp->yyspaceLeft -= yystackp->yynextFree - yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;

  while (yyr != YY_NULLPTR)
    {
      yystackp->yynextFree->yystate = *yyr;
      yyr = yyr->yypred;
      yystackp->yynextFree->yystate.yypred = &yystackp->yynextFree[-1].yystate;
      yystackp->yytops.yystates[0] = &yystackp->yynextFree->yystate;
      yystackp->yynextFree += 1;
      yystackp->yyspaceLeft -= 1;
    }
}

static YYRESULTTAG
yyprocessOneStack (yyGLRStack* yystackp, ptrdiff_t yyk,
                   ptrdiff_t yyposn, YYLTYPE *yylocp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  while (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    {
      yyStateNum yystate = yystackp->yytops.yystates[yyk]->yylrState;
      YY_DPRINTF ((stderr, "Stack %ld Entering state %d\n",
                   YY_CAST (long, yyk), yystate));

      YY_ASSERT (yystate != YYFINAL);

      if (yyisDefaultedState (yystate))
        {
          YYRESULTTAG yyflag;
          yyRuleNum yyrule = yydefaultAction (yystate);
          if (yyrule == 0)
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          yyflag = yyglrReduce (yystackp, yyk, yyrule, yyimmediate[yyrule], yyparser, ctx);
          if (yyflag == yyerr)
            {
              YY_DPRINTF ((stderr,
                           "Stack %ld dies "
                           "(predicate failure or explicit user error).\n",
                           YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          if (yyflag != yyok)
            return yyflag;
        }
      else
        {
          yySymbol yytoken = yygetToken (&yychar, yystackp, yyparser, ctx);
          const short* yyconflicts;
          const int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
          yystackp->yytops.yylookaheadNeeds[yyk] = yytrue;

          while (*yyconflicts != 0)
            {
              YYRESULTTAG yyflag;
              ptrdiff_t yynewStack = yysplitStack (yystackp, yyk);
              YY_DPRINTF ((stderr, "Splitting off stack %ld from %ld.\n",
                           YY_CAST (long, yynewStack), YY_CAST (long, yyk)));
              yyflag = yyglrReduce (yystackp, yynewStack,
                                    *yyconflicts,
                                    yyimmediate[*yyconflicts], yyparser, ctx);
              if (yyflag == yyok)
                YYCHK (yyprocessOneStack (yystackp, yynewStack,
                                          yyposn, yylocp, yyparser, ctx));
              else if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yynewStack)));
                  yymarkStackDeleted (yystackp, yynewStack);
                }
              else
                return yyflag;
              yyconflicts += 1;
            }

          if (yyisShiftAction (yyaction))
            break;
          else if (yyisErrorAction (yyaction))
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              break;
            }
          else
            {
              YYRESULTTAG yyflag = yyglrReduce (yystackp, yyk, -yyaction,
                                                yyimmediate[-yyaction], yyparser, ctx);
              if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr,
                               "Stack %ld dies "
                               "(predicate failure or explicit user error).\n",
                               YY_CAST (long, yyk)));
                  yymarkStackDeleted (yystackp, yyk);
                  break;
                }
              else if (yyflag != yyok)
                return yyflag;
            }
        }
    }
  return yyok;
}

static void
yyreportSyntaxError (yyGLRStack* yystackp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  if (yystackp->yyerrState != 0)
    return;
#if ! YYERROR_VERBOSE
  yyerror (&yylloc, yyparser, ctx, YY_("syntax error"));
#else
  {
  yySymbol yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);
  yybool yysize_overflow = yyfalse;
  char* yymsg = YY_NULLPTR;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Actual size of YYARG. */
  int yycount = 0;
  /* Cumulated lengths of YYARG.  */
  ptrdiff_t yysize = 0;

  /* There are many possibilities here to consider:
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
      int yyn = yypact[yystackp->yytops.yystates[0]->yylrState];
      ptrdiff_t yysize0 = yytnamerr (YY_NULLPTR, yytokenName (yytoken));
      yysize = yysize0;
      yyarg[yycount++] = yytokenName (yytoken);
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for this
             state because they are default actions.  */
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
                yyarg[yycount++] = yytokenName (yyx);
                {
                  ptrdiff_t yysz = yytnamerr (YY_NULLPTR, yytokenName (yyx));
                  if (YYSIZEMAX - yysize < yysz)
                    yysize_overflow = yytrue;
                  else
                    yysize += yysz;
                }
              }
        }
    }

  switch (yycount)
    {
#define YYCASE_(N, S)                   \
      case N:                           \
        yyformat = S;                   \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  {
    /* Don't count the "%s"s in the final size, but reserve room for
       the terminator.  */
    ptrdiff_t yysz = YY_CAST (ptrdiff_t, strlen (yyformat)) - 2 * yycount + 1;
    if (YYSIZEMAX - yysize < yysz)
      yysize_overflow = yytrue;
    else
      yysize += yysz;
  }

  if (!yysize_overflow)
    yymsg = YY_CAST (char *, YYMALLOC (YY_CAST (size_t, yysize)));

  if (yymsg)
    {
      char *yyp = yymsg;
      int yyi = 0;
      while ((*yyp = *yyformat))
        {
          if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
            {
              yyp += yytnamerr (yyp, yyarg[yyi++]);
              yyformat += 2;
            }
          else
            {
              ++yyp;
              ++yyformat;
            }
        }
      yyerror (&yylloc, yyparser, ctx, yymsg);
      YYFREE (yymsg);
    }
  else
    {
      yyerror (&yylloc, yyparser, ctx, YY_("syntax error"));
      yyMemoryExhausted (yystackp);
    }
  }
#endif /* YYERROR_VERBOSE */
  yynerrs += 1;
}

/* Recover from a syntax error on *YYSTACKP, assuming that *YYSTACKP->YYTOKENP,
   yylval, and yylloc are the syntactic category, semantic value, and location
   of the lookahead.  */
static void
yyrecoverSyntaxError (yyGLRStack* yystackp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  if (yystackp->yyerrState == 3)
    /* We just shifted the error token and (perhaps) took some
       reductions.  Skip tokens until we can proceed.  */
    while (yytrue)
      {
        yySymbol yytoken;
        int yyj;
        if (yychar == YYEOF)
          yyFail (yystackp, &yylloc, yyparser, ctx, YY_NULLPTR);
        if (yychar != YYEMPTY)
          {
            /* We throw away the lookahead, but the error range
               of the shifted error token must take it into account.  */
            yyGLRState *yys = yystackp->yytops.yystates[0];
            yyGLRStackItem yyerror_range[3];
            yyerror_range[1].yystate.yyloc = yys->yyloc;
            yyerror_range[2].yystate.yyloc = yylloc;
            YYLLOC_DEFAULT ((yys->yyloc), yyerror_range, 2);
            yytoken = YYTRANSLATE (yychar);
            yydestruct ("Error: discarding",
                        yytoken, &yylval, &yylloc, yyparser, ctx);
            yychar = YYEMPTY;
          }
        yytoken = yygetToken (&yychar, yystackp, yyparser, ctx);
        yyj = yypact[yystackp->yytops.yystates[0]->yylrState];
        if (yypact_value_is_default (yyj))
          return;
        yyj += yytoken;
        if (yyj < 0 || YYLAST < yyj || yycheck[yyj] != yytoken)
          {
            if (yydefact[yystackp->yytops.yystates[0]->yylrState] != 0)
              return;
          }
        else if (! yytable_value_is_error (yytable[yyj]))
          return;
      }

  /* Reduce to one stack.  */
  {
    ptrdiff_t yyk;
    for (yyk = 0; yyk < yystackp->yytops.yysize; yyk += 1)
      if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
        break;
    if (yyk >= yystackp->yytops.yysize)
      yyFail (yystackp, &yylloc, yyparser, ctx, YY_NULLPTR);
    for (yyk += 1; yyk < yystackp->yytops.yysize; yyk += 1)
      yymarkStackDeleted (yystackp, yyk);
    yyremoveDeletes (yystackp);
    yycompressStack (yystackp);
  }

  /* Now pop stack until we find a state that shifts the error token.  */
  yystackp->yyerrState = 3;
  while (yystackp->yytops.yystates[0] != YY_NULLPTR)
    {
      yyGLRState *yys = yystackp->yytops.yystates[0];
      int yyj = yypact[yys->yylrState];
      if (! yypact_value_is_default (yyj))
        {
          yyj += YYTERROR;
          if (0 <= yyj && yyj <= YYLAST && yycheck[yyj] == YYTERROR
              && yyisShiftAction (yytable[yyj]))
            {
              /* Shift the error token.  */
              int yyaction = yytable[yyj];
              /* First adjust its location.*/
              YYLTYPE yyerrloc;
              yystackp->yyerror_range[2].yystate.yyloc = yylloc;
              YYLLOC_DEFAULT (yyerrloc, (yystackp->yyerror_range), 2);
              YY_SYMBOL_PRINT ("Shifting", yystos[yyaction],
                               &yylval, &yyerrloc);
              yyglrShift (yystackp, 0, yyaction,
                          yys->yyposn, &yylval, &yyerrloc);
              yys = yystackp->yytops.yystates[0];
              break;
            }
        }
      yystackp->yyerror_range[1].yystate.yyloc = yys->yyloc;
      if (yys->yypred != YY_NULLPTR)
        yydestroyGLRState ("Error: popping", yys, yyparser, ctx);
      yystackp->yytops.yystates[0] = yys->yypred;
      yystackp->yynextFree -= 1;
      yystackp->yyspaceLeft += 1;
    }
  if (yystackp->yytops.yystates[0] == YY_NULLPTR)
    yyFail (yystackp, &yylloc, yyparser, ctx, YY_NULLPTR);
}

#define YYCHK1(YYE)                                                          \
  do {                                                                       \
    switch (YYE) {                                                           \
    case yyok:                                                               \
      break;                                                                 \
    case yyabort:                                                            \
      goto yyabortlab;                                                       \
    case yyaccept:                                                           \
      goto yyacceptlab;                                                      \
    case yyerr:                                                              \
      goto yyuser_error;                                                     \
    default:                                                                 \
      goto yybuglab;                                                         \
    }                                                                        \
  } while (0)

/*----------.
| yyparse.  |
`----------*/

int
yyparse (iresearch::iql::parser& yyparser, iresearch::iql::context& ctx)
{
  int yyresult;
  yyGLRStack yystack;
  yyGLRStack* const yystackp = &yystack;
  ptrdiff_t yyposn;

  YY_DPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY;
  yylval = yyval_default;
  yylloc = yyloc_default;

  // User initialization code.
yylloc.initialize ();
#line 2905 "iql/parser.cc"


  if (! yyinitGLRStack (yystackp, YYINITDEPTH))
    goto yyexhaustedlab;
  switch (YYSETJMP (yystack.yyexception_buffer))
    {
    case 0: break;
    case 1: goto yyabortlab;
    case 2: goto yyexhaustedlab;
    default: goto yybuglab;
    }
  yyglrShift (&yystack, 0, 0, 0, &yylval, &yylloc);
  yyposn = 0;

  while (yytrue)
    {
      /* For efficiency, we have two loops, the first of which is
         specialized to deterministic operation (single stack, no
         potential ambiguity).  */
      /* Standard mode */
      while (yytrue)
        {
          yyStateNum yystate = yystack.yytops.yystates[0]->yylrState;
          YY_DPRINTF ((stderr, "Entering state %d\n", yystate));
          if (yystate == YYFINAL)
            goto yyacceptlab;
          if (yyisDefaultedState (yystate))
            {
              yyRuleNum yyrule = yydefaultAction (yystate);
              if (yyrule == 0)
                {
                  yystack.yyerror_range[1].yystate.yyloc = yylloc;
                  yyreportSyntaxError (&yystack, yyparser, ctx);
                  goto yyuser_error;
                }
              YYCHK1 (yyglrReduce (&yystack, 0, yyrule, yytrue, yyparser, ctx));
            }
          else
            {
              yySymbol yytoken = yygetToken (&yychar, yystackp, yyparser, ctx);
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
              if (*yyconflicts != 0)
                break;
              if (yyisShiftAction (yyaction))
                {
                  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
                  yychar = YYEMPTY;
                  yyposn += 1;
                  yyglrShift (&yystack, 0, yyaction, yyposn, &yylval, &yylloc);
                  if (0 < yystack.yyerrState)
                    yystack.yyerrState -= 1;
                }
              else if (yyisErrorAction (yyaction))
                {
                  yystack.yyerror_range[1].yystate.yyloc = yylloc;
                  /* Don't issue an error message again for exceptions
                     thrown from the scanner.  */
                  if (yychar != YYFAULTYTOK)
                    yyreportSyntaxError (&yystack, yyparser, ctx);
                  goto yyuser_error;
                }
              else
                YYCHK1 (yyglrReduce (&yystack, 0, -yyaction, yytrue, yyparser, ctx));
            }
        }

      while (yytrue)
        {
          yySymbol yytoken_to_shift;
          ptrdiff_t yys;

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            yystackp->yytops.yylookaheadNeeds[yys] = yychar != YYEMPTY;

          /* yyprocessOneStack returns one of three things:

              - An error flag.  If the caller is yyprocessOneStack, it
                immediately returns as well.  When the caller is finally
                yyparse, it jumps to an error label via YYCHK1.

              - yyok, but yyprocessOneStack has invoked yymarkStackDeleted
                (&yystack, yys), which sets the top state of yys to NULL.  Thus,
                yyparse's following invocation of yyremoveDeletes will remove
                the stack.

              - yyok, when ready to shift a token.

             Except in the first case, yyparse will invoke yyremoveDeletes and
             then shift the next token onto all remaining stacks.  This
             synchronization of the shift (that is, after all preceding
             reductions on all stacks) helps prevent double destructor calls
             on yylval in the event of memory exhaustion.  */

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            YYCHK1 (yyprocessOneStack (&yystack, yys, yyposn, &yylloc, yyparser, ctx));
          yyremoveDeletes (&yystack);
          if (yystack.yytops.yysize == 0)
            {
              yyundeleteLastStack (&yystack);
              if (yystack.yytops.yysize == 0)
                yyFail (&yystack, &yylloc, yyparser, ctx, YY_("syntax error"));
              YYCHK1 (yyresolveStack (&yystack, yyparser, ctx));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yystack.yyerror_range[1].yystate.yyloc = yylloc;
              yyreportSyntaxError (&yystack, yyparser, ctx);
              goto yyuser_error;
            }

          /* If any yyglrShift call fails, it will fail after shifting.  Thus,
             a copy of yylval will already be on stack 0 in the event of a
             failure in the following loop.  Thus, yychar is set to YYEMPTY
             before the loop to make sure the user destructor for yylval isn't
             called twice.  */
          yytoken_to_shift = YYTRANSLATE (yychar);
          yychar = YYEMPTY;
          yyposn += 1;
          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            {
              yyStateNum yystate = yystack.yytops.yystates[yys]->yylrState;
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken_to_shift,
                              &yyconflicts);
              /* Note that yyconflicts were handled by yyprocessOneStack.  */
              YY_DPRINTF ((stderr, "On stack %ld, ", YY_CAST (long, yys)));
              YY_SYMBOL_PRINT ("shifting", yytoken_to_shift, &yylval, &yylloc);
              yyglrShift (&yystack, yys, yyaction, yyposn,
                          &yylval, &yylloc);
              YY_DPRINTF ((stderr, "Stack %ld now in state #%d\n",
                           YY_CAST (long, yys),
                           yystack.yytops.yystates[yys]->yylrState));
            }

          if (yystack.yytops.yysize == 1)
            {
              YYCHK1 (yyresolveStack (&yystack, yyparser, ctx));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yycompressStack (&yystack);
              break;
            }
        }
      continue;
    yyuser_error:
      yyrecoverSyntaxError (&yystack, yyparser, ctx);
      yyposn = yystack.yytops.yystates[0]->yyposn;
    }

 yyacceptlab:
  yyresult = 0;
  goto yyreturn;

 yybuglab:
  YY_ASSERT (yyfalse);
  goto yyabortlab;

 yyabortlab:
  yyresult = 1;
  goto yyreturn;

 yyexhaustedlab:
  yyerror (&yylloc, yyparser, ctx, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;

 yyreturn:
  if (yychar != YYEMPTY)
    yydestruct ("Cleanup: discarding lookahead",
                YYTRANSLATE (yychar), &yylval, &yylloc, yyparser, ctx);

  /* If the stack is well-formed, pop the stack until it is empty,
     destroying its entries as we go.  But free the stack regardless
     of whether it is well-formed.  */
  if (yystack.yyitems)
    {
      yyGLRState** yystates = yystack.yytops.yystates;
      if (yystates)
        {
          ptrdiff_t yysize = yystack.yytops.yysize;
          ptrdiff_t yyk;
          for (yyk = 0; yyk < yysize; yyk += 1)
            if (yystates[yyk])
              {
                while (yystates[yyk])
                  {
                    yyGLRState *yys = yystates[yyk];
                    yystack.yyerror_range[1].yystate.yyloc = yys->yyloc;
                    if (yys->yypred != YY_NULLPTR)
                      yydestroyGLRState ("Cleanup: popping", yys, yyparser, ctx);
                    yystates[yyk] = yys->yypred;
                    yystack.yynextFree -= 1;
                    yystack.yyspaceLeft += 1;
                  }
                break;
              }
        }
      yyfreeGLRStack (&yystack);
    }

  return yyresult;
}

/* DEBUGGING ONLY */
#if YYDEBUG
static void
yy_yypstack (yyGLRState* yys)
{
  if (yys->yypred)
    {
      yy_yypstack (yys->yypred);
      YY_FPRINTF ((stderr, " -> "));
    }
  YY_FPRINTF ((stderr, "%d@%ld", yys->yylrState, YY_CAST (long, yys->yyposn)));
}

static void
yypstates (yyGLRState* yyst)
{
  if (yyst == YY_NULLPTR)
    YY_FPRINTF ((stderr, "<null>"));
  else
    yy_yypstack (yyst);
  YY_FPRINTF ((stderr, "\n"));
}

static void
yypstack (yyGLRStack* yystackp, ptrdiff_t yyk)
{
  yypstates (yystackp->yytops.yystates[yyk]);
}

static void
yypdumpstack (yyGLRStack* yystackp)
{
#define YYINDEX(YYX)                                                    \
  YY_CAST (long,                                                        \
           ((YYX)                                                       \
            ? YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX)) - yystackp->yyitems \
            : -1))

  yyGLRStackItem* yyp;
  for (yyp = yystackp->yyitems; yyp < yystackp->yynextFree; yyp += 1)
    {
      YY_FPRINTF ((stderr, "%3ld. ",
                   YY_CAST (long, yyp - yystackp->yyitems)));
      if (*YY_REINTERPRET_CAST (yybool *, yyp))
        {
          YY_ASSERT (yyp->yystate.yyisState);
          YY_ASSERT (yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Res: %d, LR State: %d, posn: %ld, pred: %ld",
                       yyp->yystate.yyresolved, yyp->yystate.yylrState,
                       YY_CAST (long, yyp->yystate.yyposn),
                       YYINDEX (yyp->yystate.yypred)));
          if (! yyp->yystate.yyresolved)
            YY_FPRINTF ((stderr, ", firstVal: %ld",
                         YYINDEX (yyp->yystate.yysemantics.yyfirstVal)));
        }
      else
        {
          YY_ASSERT (!yyp->yystate.yyisState);
          YY_ASSERT (!yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Option. rule: %d, state: %ld, next: %ld",
                       yyp->yyoption.yyrule - 1,
                       YYINDEX (yyp->yyoption.yystate),
                       YYINDEX (yyp->yyoption.yynext)));
        }
      YY_FPRINTF ((stderr, "\n"));
    }

  YY_FPRINTF ((stderr, "Tops:"));
  {
    ptrdiff_t yyi;
    for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
      YY_FPRINTF ((stderr, "%ld: %ld; ", YY_CAST (long, yyi),
                   YYINDEX (yystackp->yytops.yystates[yyi])));
    YY_FPRINTF ((stderr, "\n"));
  }
#undef YYINDEX
}
#endif

#undef yylval
#undef yychar
#undef yynerrs
#undef yylloc



#line 371 "/home/gnusi/git/iresearch/core/iql/parser.yy"


// -----------------------------------------------------------------------------
// --SECTION--                                                          Epilogue
// -----------------------------------------------------------------------------

// define after 'yydebug' is declared by bison
namespace iresearch {
  namespace iql {
    void debug(parser& parser, bool bEnable) {
      #if YYDEBUG
        // suppress MSVC C4505 warnings ('#pragma warning(disable:4505)' is ignored by MSVS)
        // defined here only for the sake of being inside some function
        #if defined(_MSC_VER)
          if (false) yypstack(nullptr, 0);
          if (false) yypdumpstack(nullptr);
        #endif

        yydebug = bEnable ? 1 : 0;
        parser.set_debug_level(bEnable ? 1 : 0);
      #endif
    }
  }
}

// end of suppress of warnings due to Bison generated code (pop section, push at start of file)
#if defined(_MSC_VER)
  #pragma warning(default : 4706)
  #pragma warning(default : 4611)
  #pragma warning(default : 4505)
  #pragma warning(default : 4244)
  #pragma warning(default : 4127)
  #pragma warning(default : 4100)
#endif
#line 3228 "iql/parser.cc"

/*------------------.
| Report an error.  |
`------------------*/

static void
yyerror (const iresearch::iql::parser::location_type *yylocationp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx, const char* msg)
{
  YYUSE (yyparser);
  YYUSE (ctx);
  yyparser.error (*yylocationp, msg);
}


#line 31 "/home/gnusi/git/iresearch/core/iql/parser.yy"
namespace iresearch { namespace iql {
#line 3245 "iql/parser.cc"

  /// Build a parser object.
  parser::parser (iresearch::iql::context& ctx_yyarg)
    :
#if YYDEBUG
      yycdebug_ (&std::cerr),
#endif
      ctx (ctx_yyarg)
  {}

  parser::~parser ()
  {}

  parser::syntax_error::~syntax_error () YY_NOEXCEPT YY_NOTHROW
  {}

  int
  parser::operator() ()
  {
    return parse ();
  }

  int
  parser::parse ()
  {
    return ::yyparse (*this, ctx);
  }

#if YYDEBUG
  /*--------------------.
  | Print this symbol.  |
  `--------------------*/

  void
  parser::yy_symbol_value_print_ (int yytype,
                           const semantic_type* yyvaluep,
                           const location_type* yylocationp)
  {
    YYUSE (yylocationp);
    YYUSE (yyvaluep);
    std::ostream& yyo = debug_stream ();
    std::ostream& yyoutput = yyo;
    YYUSE (yyoutput);
    YYUSE (yytype);
  }


  void
  parser::yy_symbol_print_ (int yytype,
                           const semantic_type* yyvaluep,
                           const location_type* yylocationp)
  {
    *yycdebug_ << (yytype < YYNTOKENS ? "token" : "nterm")
               << ' ' << yytname[yytype] << " ("
               << *yylocationp << ": ";
    yy_symbol_value_print_ (yytype, yyvaluep, yylocationp);
    *yycdebug_ << ')';
  }

  std::ostream&
  parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  parser::debug_level_type
  parser::debug_level () const
  {
    return yydebug;
  }

  void
  parser::set_debug_level (debug_level_type l)
  {
    // Actually, it is yydebug which is really used.
    yydebug = l;
  }

#endif
#line 31 "/home/gnusi/git/iresearch/core/iql/parser.yy"
} } // iresearch::iql
#line 3334 "iql/parser.cc"
