/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 98 "jamgram.y" /* yacc.c:339  */

#include "jam.h"

#include "lists.h"
#include "parse.h"
#include "scan.h"
#include "compile.h"
#include "object.h"
#include "rules.h"

# define YYMAXDEPTH 10000	/* for OSF and other less endowed yaccs */

# define F0 -1
# define P0 (PARSE *)0
# define S0 (OBJECT *)0

# define pappend( l,r )    	parse_make( PARSE_APPEND,l,r,P0,S0,S0,0 )
# define peval( c,l,r )	parse_make( PARSE_EVAL,l,r,P0,S0,S0,c )
# define pfor( s,l,r,x )    	parse_make( PARSE_FOREACH,l,r,P0,s,S0,x )
# define pif( l,r,t )	  	parse_make( PARSE_IF,l,r,t,S0,S0,0 )
# define pincl( l )       	parse_make( PARSE_INCLUDE,l,P0,P0,S0,S0,0 )
# define plist( s )	  	parse_make( PARSE_LIST,P0,P0,P0,s,S0,0 )
# define plocal( l,r,t )  	parse_make( PARSE_LOCAL,l,r,t,S0,S0,0 )
# define pmodule( l,r )	  	parse_make( PARSE_MODULE,l,r,P0,S0,S0,0 )
# define pclass( l,r )	  	parse_make( PARSE_CLASS,l,r,P0,S0,S0,0 )
# define pnull()	  	parse_make( PARSE_NULL,P0,P0,P0,S0,S0,0 )
# define pon( l,r )	  	parse_make( PARSE_ON,l,r,P0,S0,S0,0 )
# define prule( s,p )     	parse_make( PARSE_RULE,p,P0,P0,s,S0,0 )
# define prules( l,r )	  	parse_make( PARSE_RULES,l,r,P0,S0,S0,0 )
# define pset( l,r,a )          parse_make( PARSE_SET,l,r,P0,S0,S0,a )
# define pset1( l,r,t,a )	parse_make( PARSE_SETTINGS,l,r,t,S0,S0,a )
# define psetc( s,p,a,l )     	parse_make( PARSE_SETCOMP,p,a,P0,s,S0,l )
# define psete( s,l,s1,f ) 	parse_make( PARSE_SETEXEC,l,P0,P0,s,s1,f )
# define pswitch( l,r )   	parse_make( PARSE_SWITCH,l,r,P0,S0,S0,0 )
# define pwhile( l,r )   	parse_make( PARSE_WHILE,l,r,P0,S0,S0,0 )
# define preturn( l )       parse_make( PARSE_RETURN,l,P0,P0,S0,S0,0 )
# define pbreak()           parse_make( PARSE_BREAK,P0,P0,P0,S0,S0,0 )
# define pcontinue()        parse_make( PARSE_CONTINUE,P0,P0,P0,S0,S0,0 )

# define pnode( l,r )    	parse_make( F0,l,r,P0,S0,S0,0 )
# define psnode( s,l )     	parse_make( F0,l,P0,P0,s,S0,0 )


#line 110 "y.tab.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    _BANG_t = 258,
    _BANG_EQUALS_t = 259,
    _AMPER_t = 260,
    _AMPERAMPER_t = 261,
    _LPAREN_t = 262,
    _RPAREN_t = 263,
    _PLUS_EQUALS_t = 264,
    _COLON_t = 265,
    _SEMIC_t = 266,
    _LANGLE_t = 267,
    _LANGLE_EQUALS_t = 268,
    _EQUALS_t = 269,
    _RANGLE_t = 270,
    _RANGLE_EQUALS_t = 271,
    _QUESTION_EQUALS_t = 272,
    _LBRACKET_t = 273,
    _RBRACKET_t = 274,
    ACTIONS_t = 275,
    BIND_t = 276,
    BREAK_t = 277,
    CASE_t = 278,
    CLASS_t = 279,
    CONTINUE_t = 280,
    DEFAULT_t = 281,
    ELSE_t = 282,
    EXISTING_t = 283,
    FOR_t = 284,
    IF_t = 285,
    IGNORE_t = 286,
    IN_t = 287,
    INCLUDE_t = 288,
    LOCAL_t = 289,
    MODULE_t = 290,
    ON_t = 291,
    PIECEMEAL_t = 292,
    QUIETLY_t = 293,
    RETURN_t = 294,
    RULE_t = 295,
    SWITCH_t = 296,
    TOGETHER_t = 297,
    UPDATED_t = 298,
    WHILE_t = 299,
    _LBRACE_t = 300,
    _BAR_t = 301,
    _BARBAR_t = 302,
    _RBRACE_t = 303,
    ARG = 304,
    STRING = 305
  };
#endif
/* Tokens.  */
#define _BANG_t 258
#define _BANG_EQUALS_t 259
#define _AMPER_t 260
#define _AMPERAMPER_t 261
#define _LPAREN_t 262
#define _RPAREN_t 263
#define _PLUS_EQUALS_t 264
#define _COLON_t 265
#define _SEMIC_t 266
#define _LANGLE_t 267
#define _LANGLE_EQUALS_t 268
#define _EQUALS_t 269
#define _RANGLE_t 270
#define _RANGLE_EQUALS_t 271
#define _QUESTION_EQUALS_t 272
#define _LBRACKET_t 273
#define _RBRACKET_t 274
#define ACTIONS_t 275
#define BIND_t 276
#define BREAK_t 277
#define CASE_t 278
#define CLASS_t 279
#define CONTINUE_t 280
#define DEFAULT_t 281
#define ELSE_t 282
#define EXISTING_t 283
#define FOR_t 284
#define IF_t 285
#define IGNORE_t 286
#define IN_t 287
#define INCLUDE_t 288
#define LOCAL_t 289
#define MODULE_t 290
#define ON_t 291
#define PIECEMEAL_t 292
#define QUIETLY_t 293
#define RETURN_t 294
#define RULE_t 295
#define SWITCH_t 296
#define TOGETHER_t 297
#define UPDATED_t 298
#define WHILE_t 299
#define _LBRACE_t 300
#define _BAR_t 301
#define _BARBAR_t 302
#define _RBRACE_t 303
#define ARG 304
#define STRING 305

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 261 "y.tab.c" /* yacc.c:358  */

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
#else
typedef signed char yytype_int8;
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
# elif ! defined YYSIZE_T
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
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
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
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  42
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   242

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  51
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  68
/* YYNRULES -- Number of rules.  */
#define YYNRULES  121
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  207

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   305

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
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
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   144,   144,   146,   157,   159,   163,   165,   167,   167,
     167,   172,   175,   175,   177,   181,   184,   187,   190,   193,
     196,   198,   200,   200,   202,   202,   204,   204,   206,   206,
     206,   208,   208,   210,   212,   214,   214,   214,   216,   216,
     216,   218,   218,   218,   220,   220,   220,   222,   222,   222,
     224,   224,   224,   226,   226,   226,   226,   228,   231,   233,
     230,   242,   244,   246,   248,   255,   257,   257,   259,   259,
     261,   261,   263,   263,   265,   265,   267,   267,   269,   269,
     271,   271,   273,   273,   275,   275,   277,   277,   279,   279,
     281,   281,   293,   294,   298,   298,   298,   307,   309,   319,
     324,   325,   329,   331,   331,   340,   340,   342,   342,   344,
     344,   355,   356,   360,   362,   364,   366,   368,   370,   380,
     381,   381
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "_BANG_t", "_BANG_EQUALS_t", "_AMPER_t",
  "_AMPERAMPER_t", "_LPAREN_t", "_RPAREN_t", "_PLUS_EQUALS_t", "_COLON_t",
  "_SEMIC_t", "_LANGLE_t", "_LANGLE_EQUALS_t", "_EQUALS_t", "_RANGLE_t",
  "_RANGLE_EQUALS_t", "_QUESTION_EQUALS_t", "_LBRACKET_t", "_RBRACKET_t",
  "ACTIONS_t", "BIND_t", "BREAK_t", "CASE_t", "CLASS_t", "CONTINUE_t",
  "DEFAULT_t", "ELSE_t", "EXISTING_t", "FOR_t", "IF_t", "IGNORE_t", "IN_t",
  "INCLUDE_t", "LOCAL_t", "MODULE_t", "ON_t", "PIECEMEAL_t", "QUIETLY_t",
  "RETURN_t", "RULE_t", "SWITCH_t", "TOGETHER_t", "UPDATED_t", "WHILE_t",
  "_LBRACE_t", "_BAR_t", "_BARBAR_t", "_RBRACE_t", "ARG", "STRING",
  "$accept", "run", "block", "rules", "$@1", "$@2", "null",
  "assign_list_opt", "$@3", "arglist_opt", "local_opt", "else_opt", "rule",
  "$@4", "$@5", "$@6", "$@7", "$@8", "$@9", "$@10", "$@11", "$@12", "$@13",
  "$@14", "$@15", "$@16", "$@17", "$@18", "$@19", "$@20", "$@21", "$@22",
  "$@23", "$@24", "$@25", "$@26", "assign", "expr", "$@27", "$@28", "$@29",
  "$@30", "$@31", "$@32", "$@33", "$@34", "$@35", "$@36", "$@37", "$@38",
  "$@39", "cases", "case", "$@40", "$@41", "lol", "list", "listp", "arg",
  "@42", "func", "$@43", "$@44", "$@45", "eflags", "eflag", "bindlist",
  "$@46", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305
};
# endif

#define YYPACT_NINF -119

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-119)))

#define YYTABLE_NINF -25

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     140,  -119,  -119,     1,  -119,     2,   -18,  -119,  -119,   -23,
    -119,    -9,  -119,  -119,  -119,   140,    12,    31,  -119,     4,
     140,    77,   -17,   186,  -119,  -119,  -119,  -119,    -7,     3,
    -119,  -119,  -119,  -119,   177,  -119,  -119,     3,    -5,  -119,
    -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119,    33,  -119,
    -119,    -9,  -119,    29,  -119,  -119,  -119,  -119,  -119,  -119,
      35,  -119,    14,    50,    -9,    34,  -119,  -119,    23,    39,
      52,    53,    40,  -119,    66,    45,    94,  -119,    67,    30,
    -119,  -119,  -119,    16,  -119,  -119,  -119,    47,  -119,  -119,
    -119,  -119,     3,     3,  -119,  -119,  -119,  -119,  -119,  -119,
    -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119,    84,
    -119,  -119,  -119,    51,  -119,  -119,    32,   105,  -119,  -119,
    -119,  -119,  -119,   140,  -119,  -119,  -119,    68,     3,     3,
       3,     3,     3,     3,     3,     3,   140,     3,     3,  -119,
    -119,  -119,   140,    95,   140,   110,  -119,  -119,  -119,  -119,
    -119,    69,    73,    87,  -119,    89,   139,   139,  -119,  -119,
      89,  -119,  -119,    90,   226,   226,  -119,  -119,   140,    91,
    -119,    97,    95,    98,  -119,  -119,  -119,  -119,  -119,  -119,
    -119,  -119,   108,  -119,  -119,    88,  -119,  -119,  -119,   141,
     177,   145,   102,   140,   177,  -119,   149,  -119,  -119,  -119,
    -119,   115,  -119,  -119,  -119,   140,  -119
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,   103,   111,     0,    47,     0,    18,    41,    22,     8,
      44,     0,    31,    38,    50,    11,   102,     0,     3,     0,
       6,     0,     0,     0,    33,   100,    34,    17,     0,     0,
     100,   100,   100,   102,    18,   100,   100,     0,     0,     5,
       4,   100,     1,    53,     7,    62,    61,    63,     0,    28,
      26,     0,   105,     0,   118,   115,   117,   116,   114,   113,
     119,   112,     0,    97,    99,     0,    88,    90,     0,    65,
       0,    11,     0,    57,     0,     0,    51,    21,     0,     0,
      64,   100,   100,     0,   100,   104,   120,     0,    48,   100,
     101,    35,     0,     0,    68,    78,    80,    70,    72,    66,
      74,    76,    42,    82,    84,    86,    23,    12,    14,     0,
      45,    32,    39,     0,    25,    54,     0,     0,   109,   107,
     106,   100,    58,    11,    98,   100,    89,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    11,     0,     0,   100,
     100,     9,    11,    92,    11,    16,    29,    27,   100,   100,
     121,     0,     0,     0,    91,    69,    79,    81,    71,    73,
      67,    75,    77,     0,    83,    85,    87,    13,    11,     0,
      94,     0,    92,     0,   100,    55,   100,   110,   108,    59,
      49,    36,    20,    10,    46,     0,    40,    93,    52,     0,
      18,     0,     0,    11,    18,    43,     0,    15,    56,    30,
      60,     0,    19,    95,    37,    11,    96
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -119,  -119,  -118,    25,  -119,  -119,    96,  -119,  -119,  -119,
     160,  -119,   -33,  -119,  -119,  -119,  -119,  -119,  -119,  -119,
    -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119,
    -119,  -119,  -119,  -119,  -119,  -119,    55,    -4,  -119,  -119,
    -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119,
    -119,     5,  -119,  -119,  -119,   -27,   -28,  -119,     0,  -119,
    -119,  -119,  -119,  -119,  -119,  -119,  -119,  -119
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    17,    38,    39,    31,   168,    40,   109,   140,   175,
      19,   195,    20,    30,    41,    82,    81,   176,    35,   125,
     193,    36,   143,    29,   136,    32,   142,    25,   123,    37,
     113,    79,   145,   190,   151,   192,    50,    68,   133,   128,
     131,   132,   134,   135,   129,   130,   137,   138,   139,    92,
      93,   171,   172,   185,   205,    62,    63,    64,    69,    22,
      53,    84,   149,   148,    23,    61,    87,   121
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      21,    73,    70,    71,    72,   152,    66,    74,    75,     1,
      67,    34,    24,    26,    78,    21,    27,   -17,   163,    51,
      21,     1,   -24,   -24,   169,    18,   173,    94,    95,    96,
     -24,    42,    52,    76,    21,    97,    98,    99,   100,   101,
      33,    45,    65,    77,    43,    44,    46,    80,    85,    47,
     183,    83,    33,   116,   117,   118,    86,   120,    48,    88,
      89,   -24,   124,   106,    90,   119,    91,   107,   102,   103,
     104,   105,    94,    95,    96,   201,   154,   111,   114,   115,
      97,    98,    99,   100,   101,   110,    45,   206,   126,   127,
     112,    46,   122,   150,    47,   141,   144,   153,    94,    95,
      96,    97,    98,    48,   100,   101,    97,    98,    99,   100,
     101,   166,   167,    49,   103,   104,   147,   174,   170,   179,
     177,   180,   178,    21,   155,   156,   157,   158,   159,   160,
     161,   162,   181,   164,   165,   194,    21,   196,   182,   184,
     103,   104,    21,    94,    21,   186,   188,   189,   191,   197,
     200,    97,    98,    99,   100,   101,   199,   198,     1,   203,
       2,   202,     3,   204,     4,     5,    28,   108,    21,     6,
       7,   146,     0,     8,     9,    10,    11,   187,     0,    12,
     -18,    13,     0,     0,    14,    15,     0,     0,     0,    16,
      21,     0,     0,    21,    21,     1,     0,     2,     0,     3,
       0,     4,     5,     0,     0,    21,     6,     7,     0,     0,
       8,    27,    10,    11,    54,     0,    12,    55,    13,     0,
       0,    14,    15,    56,    57,     0,    16,     0,    58,    59,
      94,    95,    96,     0,     0,    60,     0,     0,    97,    98,
      99,   100,   101
};

static const yytype_int16 yycheck[] =
{
       0,    34,    30,    31,    32,   123,     3,    35,    36,    18,
       7,    11,    11,    11,    41,    15,    34,    40,   136,    36,
      20,    18,    10,    11,   142,     0,   144,     4,     5,     6,
      18,     0,    49,    37,    34,    12,    13,    14,    15,    16,
      49,     9,    49,    48,    40,    20,    14,    14,    19,    17,
     168,    51,    49,    81,    82,    39,    21,    84,    26,    45,
      10,    49,    89,    11,    64,    49,    32,    14,    45,    46,
      47,    32,     4,     5,     6,   193,     8,    11,    11,    49,
      12,    13,    14,    15,    16,    45,     9,   205,    92,    93,
      45,    14,    45,   121,    17,    11,    45,   125,     4,     5,
       6,    12,    13,    26,    15,    16,    12,    13,    14,    15,
      16,   139,   140,    36,    46,    47,    11,     7,    23,    50,
     148,    48,   149,   123,   128,   129,   130,   131,   132,   133,
     134,   135,    45,   137,   138,    27,   136,    49,    48,    48,
      46,    47,   142,     4,   144,    48,    48,   174,   176,     8,
      48,    12,    13,    14,    15,    16,    11,   190,    18,    10,
      20,   194,    22,    48,    24,    25,     6,    71,   168,    29,
      30,   116,    -1,    33,    34,    35,    36,   172,    -1,    39,
      40,    41,    -1,    -1,    44,    45,    -1,    -1,    -1,    49,
     190,    -1,    -1,   193,   194,    18,    -1,    20,    -1,    22,
      -1,    24,    25,    -1,    -1,   205,    29,    30,    -1,    -1,
      33,    34,    35,    36,    28,    -1,    39,    31,    41,    -1,
      -1,    44,    45,    37,    38,    -1,    49,    -1,    42,    43,
       4,     5,     6,    -1,    -1,    49,    -1,    -1,    12,    13,
      14,    15,    16
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    18,    20,    22,    24,    25,    29,    30,    33,    34,
      35,    36,    39,    41,    44,    45,    49,    52,    54,    61,
      63,   109,   110,   115,    11,    78,    11,    34,    61,    74,
      64,    55,    76,    49,   109,    69,    72,    80,    53,    54,
      57,    65,     0,    40,    54,     9,    14,    17,    26,    36,
      87,    36,    49,   111,    28,    31,    37,    38,    42,    43,
      49,   116,   106,   107,   108,    49,     3,     7,    88,   109,
     107,   107,   107,    63,   107,   107,    88,    48,   106,    82,
      14,    67,    66,   109,   112,    19,    21,   117,    45,    10,
     109,    32,   100,   101,     4,     5,     6,    12,    13,    14,
      15,    16,    45,    46,    47,    32,    11,    14,    57,    58,
      45,    11,    45,    81,    11,    49,   107,   107,    39,    49,
     106,   118,    45,    79,   106,    70,    88,    88,    90,    95,
      96,    91,    92,    89,    93,    94,    75,    97,    98,    99,
      59,    11,    77,    73,    45,    83,    87,    11,   114,   113,
     107,    85,    53,   107,     8,    88,    88,    88,    88,    88,
      88,    88,    88,    53,    88,    88,   107,   107,    56,    53,
      23,   102,   103,    53,     7,    60,    68,   107,   106,    50,
      48,    45,    48,    53,    48,   104,    48,   102,    48,   106,
      84,   107,    86,    71,    27,    62,    49,     8,    63,    11,
      48,    53,    63,    10,    48,   105,    53
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    51,    52,    52,    53,    53,    54,    54,    55,    56,
      54,    57,    59,    58,    58,    60,    60,    61,    61,    62,
      62,    63,    64,    63,    65,    63,    66,    63,    67,    68,
      63,    69,    63,    63,    63,    70,    71,    63,    72,    73,
      63,    74,    75,    63,    76,    77,    63,    78,    79,    63,
      80,    81,    63,    82,    83,    84,    63,    63,    85,    86,
      63,    87,    87,    87,    87,    88,    89,    88,    90,    88,
      91,    88,    92,    88,    93,    88,    94,    88,    95,    88,
      96,    88,    97,    88,    98,    88,    99,    88,   100,    88,
     101,    88,   102,   102,   104,   105,   103,   106,   106,   107,
     108,   108,   109,   110,   109,   112,   111,   113,   111,   114,
     111,   115,   115,   116,   116,   116,   116,   116,   116,   117,
     118,   117
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     1,     1,     1,     1,     2,     0,     0,
       7,     0,     0,     3,     1,     3,     0,     1,     0,     2,
       0,     3,     0,     4,     0,     4,     0,     5,     0,     0,
       8,     0,     4,     2,     2,     0,     0,    10,     0,     0,
       7,     0,     0,     8,     0,     0,     7,     0,     0,     7,
       0,     0,     7,     0,     0,     0,     8,     3,     0,     0,
       9,     1,     1,     1,     2,     1,     0,     4,     0,     4,
       0,     4,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     4,     0,     4,     0,     4,     0,     3,
       0,     4,     0,     2,     0,     0,     6,     1,     3,     1,
       0,     2,     1,     0,     4,     0,     3,     0,     5,     0,
       5,     0,     2,     1,     1,     1,     1,     1,     1,     0,
       0,     3
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

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
#ifndef YYINITDEPTH
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
static YYSIZE_T
yystrlen (const char *yystr)
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
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

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
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
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

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

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

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
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
  int yytoken = 0;
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

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
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
      yychar = yylex ();
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
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 3:
#line 147 "jamgram.y" /* yacc.c:1646  */
    { parse_save( (yyvsp[0]).parse ); }
#line 1508 "y.tab.c" /* yacc.c:1646  */
    break;

  case 4:
#line 158 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[0]).parse; }
#line 1514 "y.tab.c" /* yacc.c:1646  */
    break;

  case 5:
#line 160 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[0]).parse; }
#line 1520 "y.tab.c" /* yacc.c:1646  */
    break;

  case 6:
#line 164 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[0]).parse; }
#line 1526 "y.tab.c" /* yacc.c:1646  */
    break;

  case 7:
#line 166 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = prules( (yyvsp[-1]).parse, (yyvsp[0]).parse ); }
#line 1532 "y.tab.c" /* yacc.c:1646  */
    break;

  case 8:
#line 167 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_ASSIGN ); }
#line 1538 "y.tab.c" /* yacc.c:1646  */
    break;

  case 9:
#line 167 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 1544 "y.tab.c" /* yacc.c:1646  */
    break;

  case 10:
#line 168 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = plocal( (yyvsp[-4]).parse, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1550 "y.tab.c" /* yacc.c:1646  */
    break;

  case 11:
#line 172 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pnull(); }
#line 1556 "y.tab.c" /* yacc.c:1646  */
    break;

  case 12:
#line 175 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1562 "y.tab.c" /* yacc.c:1646  */
    break;

  case 13:
#line 176 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[0]).parse; (yyval).number = ASSIGN_SET; }
#line 1568 "y.tab.c" /* yacc.c:1646  */
    break;

  case 14:
#line 178 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[0]).parse; (yyval).number = ASSIGN_APPEND; }
#line 1574 "y.tab.c" /* yacc.c:1646  */
    break;

  case 15:
#line 182 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[-1]).parse; }
#line 1580 "y.tab.c" /* yacc.c:1646  */
    break;

  case 16:
#line 184 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = P0; }
#line 1586 "y.tab.c" /* yacc.c:1646  */
    break;

  case 17:
#line 188 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = 1; }
#line 1592 "y.tab.c" /* yacc.c:1646  */
    break;

  case 18:
#line 190 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = 0; }
#line 1598 "y.tab.c" /* yacc.c:1646  */
    break;

  case 19:
#line 194 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[0]).parse; }
#line 1604 "y.tab.c" /* yacc.c:1646  */
    break;

  case 20:
#line 196 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pnull(); }
#line 1610 "y.tab.c" /* yacc.c:1646  */
    break;

  case 21:
#line 199 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[-1]).parse; }
#line 1616 "y.tab.c" /* yacc.c:1646  */
    break;

  case 22:
#line 200 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1622 "y.tab.c" /* yacc.c:1646  */
    break;

  case 23:
#line 201 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pincl( (yyvsp[-1]).parse ); yymode( SCAN_NORMAL ); }
#line 1628 "y.tab.c" /* yacc.c:1646  */
    break;

  case 24:
#line 202 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1634 "y.tab.c" /* yacc.c:1646  */
    break;

  case 25:
#line 203 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = prule( (yyvsp[-3]).string, (yyvsp[-1]).parse ); yymode( SCAN_NORMAL ); }
#line 1640 "y.tab.c" /* yacc.c:1646  */
    break;

  case 26:
#line 204 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1646 "y.tab.c" /* yacc.c:1646  */
    break;

  case 27:
#line 205 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pset( (yyvsp[-4]).parse, (yyvsp[-1]).parse, (yyvsp[-3]).number ); yymode( SCAN_NORMAL ); }
#line 1652 "y.tab.c" /* yacc.c:1646  */
    break;

  case 28:
#line 206 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_ASSIGN ); }
#line 1658 "y.tab.c" /* yacc.c:1646  */
    break;

  case 29:
#line 206 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1664 "y.tab.c" /* yacc.c:1646  */
    break;

  case 30:
#line 207 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pset1( (yyvsp[-7]).parse, (yyvsp[-4]).parse, (yyvsp[-1]).parse, (yyvsp[-3]).number ); yymode( SCAN_NORMAL ); }
#line 1670 "y.tab.c" /* yacc.c:1646  */
    break;

  case 31:
#line 208 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1676 "y.tab.c" /* yacc.c:1646  */
    break;

  case 32:
#line 209 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = preturn( (yyvsp[-1]).parse ); yymode( SCAN_NORMAL ); }
#line 1682 "y.tab.c" /* yacc.c:1646  */
    break;

  case 33:
#line 211 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pbreak(); }
#line 1688 "y.tab.c" /* yacc.c:1646  */
    break;

  case 34:
#line 213 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pcontinue(); }
#line 1694 "y.tab.c" /* yacc.c:1646  */
    break;

  case 35:
#line 214 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1700 "y.tab.c" /* yacc.c:1646  */
    break;

  case 36:
#line 214 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 1706 "y.tab.c" /* yacc.c:1646  */
    break;

  case 37:
#line 215 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pfor( (yyvsp[-7]).string, (yyvsp[-4]).parse, (yyvsp[-1]).parse, (yyvsp[-8]).number ); }
#line 1712 "y.tab.c" /* yacc.c:1646  */
    break;

  case 38:
#line 216 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1718 "y.tab.c" /* yacc.c:1646  */
    break;

  case 39:
#line 216 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 1724 "y.tab.c" /* yacc.c:1646  */
    break;

  case 40:
#line 217 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pswitch( (yyvsp[-4]).parse, (yyvsp[-1]).parse ); }
#line 1730 "y.tab.c" /* yacc.c:1646  */
    break;

  case 41:
#line 218 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1736 "y.tab.c" /* yacc.c:1646  */
    break;

  case 42:
#line 218 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 1742 "y.tab.c" /* yacc.c:1646  */
    break;

  case 43:
#line 219 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pif( (yyvsp[-5]).parse, (yyvsp[-2]).parse, (yyvsp[0]).parse ); }
#line 1748 "y.tab.c" /* yacc.c:1646  */
    break;

  case 44:
#line 220 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1754 "y.tab.c" /* yacc.c:1646  */
    break;

  case 45:
#line 220 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 1760 "y.tab.c" /* yacc.c:1646  */
    break;

  case 46:
#line 221 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pmodule( (yyvsp[-4]).parse, (yyvsp[-1]).parse ); }
#line 1766 "y.tab.c" /* yacc.c:1646  */
    break;

  case 47:
#line 222 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1772 "y.tab.c" /* yacc.c:1646  */
    break;

  case 48:
#line 222 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 1778 "y.tab.c" /* yacc.c:1646  */
    break;

  case 49:
#line 223 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pclass( (yyvsp[-4]).parse, (yyvsp[-1]).parse ); }
#line 1784 "y.tab.c" /* yacc.c:1646  */
    break;

  case 50:
#line 224 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1790 "y.tab.c" /* yacc.c:1646  */
    break;

  case 51:
#line 224 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 1796 "y.tab.c" /* yacc.c:1646  */
    break;

  case 52:
#line 225 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pwhile( (yyvsp[-4]).parse, (yyvsp[-1]).parse ); }
#line 1802 "y.tab.c" /* yacc.c:1646  */
    break;

  case 53:
#line 226 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 1808 "y.tab.c" /* yacc.c:1646  */
    break;

  case 54:
#line 226 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PARAMS ); }
#line 1814 "y.tab.c" /* yacc.c:1646  */
    break;

  case 55:
#line 226 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 1820 "y.tab.c" /* yacc.c:1646  */
    break;

  case 56:
#line 227 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = psetc( (yyvsp[-4]).string, (yyvsp[0]).parse, (yyvsp[-2]).parse, (yyvsp[-7]).number ); }
#line 1826 "y.tab.c" /* yacc.c:1646  */
    break;

  case 57:
#line 229 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pon( (yyvsp[-1]).parse, (yyvsp[0]).parse ); }
#line 1832 "y.tab.c" /* yacc.c:1646  */
    break;

  case 58:
#line 231 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_STRING ); }
#line 1838 "y.tab.c" /* yacc.c:1646  */
    break;

  case 59:
#line 233 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 1844 "y.tab.c" /* yacc.c:1646  */
    break;

  case 60:
#line 235 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = psete( (yyvsp[-6]).string,(yyvsp[-5]).parse,(yyvsp[-2]).string,(yyvsp[-7]).number ); }
#line 1850 "y.tab.c" /* yacc.c:1646  */
    break;

  case 61:
#line 243 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = ASSIGN_SET; }
#line 1856 "y.tab.c" /* yacc.c:1646  */
    break;

  case 62:
#line 245 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = ASSIGN_APPEND; }
#line 1862 "y.tab.c" /* yacc.c:1646  */
    break;

  case 63:
#line 247 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = ASSIGN_DEFAULT; }
#line 1868 "y.tab.c" /* yacc.c:1646  */
    break;

  case 64:
#line 249 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = ASSIGN_DEFAULT; }
#line 1874 "y.tab.c" /* yacc.c:1646  */
    break;

  case 65:
#line 256 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_EXISTS, (yyvsp[0]).parse, pnull() ); yymode( SCAN_COND ); }
#line 1880 "y.tab.c" /* yacc.c:1646  */
    break;

  case 66:
#line 257 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1886 "y.tab.c" /* yacc.c:1646  */
    break;

  case 67:
#line 258 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_EQUALS, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1892 "y.tab.c" /* yacc.c:1646  */
    break;

  case 68:
#line 259 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1898 "y.tab.c" /* yacc.c:1646  */
    break;

  case 69:
#line 260 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_NOTEQ, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1904 "y.tab.c" /* yacc.c:1646  */
    break;

  case 70:
#line 261 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1910 "y.tab.c" /* yacc.c:1646  */
    break;

  case 71:
#line 262 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_LESS, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1916 "y.tab.c" /* yacc.c:1646  */
    break;

  case 72:
#line 263 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1922 "y.tab.c" /* yacc.c:1646  */
    break;

  case 73:
#line 264 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_LESSEQ, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1928 "y.tab.c" /* yacc.c:1646  */
    break;

  case 74:
#line 265 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1934 "y.tab.c" /* yacc.c:1646  */
    break;

  case 75:
#line 266 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_MORE, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1940 "y.tab.c" /* yacc.c:1646  */
    break;

  case 76:
#line 267 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1946 "y.tab.c" /* yacc.c:1646  */
    break;

  case 77:
#line 268 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_MOREEQ, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1952 "y.tab.c" /* yacc.c:1646  */
    break;

  case 78:
#line 269 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1958 "y.tab.c" /* yacc.c:1646  */
    break;

  case 79:
#line 270 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_AND, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1964 "y.tab.c" /* yacc.c:1646  */
    break;

  case 80:
#line 271 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1970 "y.tab.c" /* yacc.c:1646  */
    break;

  case 81:
#line 272 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_AND, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1976 "y.tab.c" /* yacc.c:1646  */
    break;

  case 82:
#line 273 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1982 "y.tab.c" /* yacc.c:1646  */
    break;

  case 83:
#line 274 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_OR, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 1988 "y.tab.c" /* yacc.c:1646  */
    break;

  case 84:
#line 275 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 1994 "y.tab.c" /* yacc.c:1646  */
    break;

  case 85:
#line 276 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_OR, (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 2000 "y.tab.c" /* yacc.c:1646  */
    break;

  case 86:
#line 277 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 2006 "y.tab.c" /* yacc.c:1646  */
    break;

  case 87:
#line 278 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_IN, (yyvsp[-3]).parse, (yyvsp[0]).parse ); yymode( SCAN_COND ); }
#line 2012 "y.tab.c" /* yacc.c:1646  */
    break;

  case 88:
#line 279 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 2018 "y.tab.c" /* yacc.c:1646  */
    break;

  case 89:
#line 280 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = peval( EXPR_NOT, (yyvsp[0]).parse, pnull() ); }
#line 2024 "y.tab.c" /* yacc.c:1646  */
    break;

  case 90:
#line 281 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CONDB ); }
#line 2030 "y.tab.c" /* yacc.c:1646  */
    break;

  case 91:
#line 282 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[-1]).parse; }
#line 2036 "y.tab.c" /* yacc.c:1646  */
    break;

  case 92:
#line 293 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = P0; }
#line 2042 "y.tab.c" /* yacc.c:1646  */
    break;

  case 93:
#line 295 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pnode( (yyvsp[-1]).parse, (yyvsp[0]).parse ); }
#line 2048 "y.tab.c" /* yacc.c:1646  */
    break;

  case 94:
#line 298 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_CASE ); }
#line 2054 "y.tab.c" /* yacc.c:1646  */
    break;

  case 95:
#line 298 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_NORMAL ); }
#line 2060 "y.tab.c" /* yacc.c:1646  */
    break;

  case 96:
#line 299 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = psnode( (yyvsp[-3]).string, (yyvsp[0]).parse ); }
#line 2066 "y.tab.c" /* yacc.c:1646  */
    break;

  case 97:
#line 308 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pnode( P0, (yyvsp[0]).parse ); }
#line 2072 "y.tab.c" /* yacc.c:1646  */
    break;

  case 98:
#line 310 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pnode( (yyvsp[0]).parse, (yyvsp[-2]).parse ); }
#line 2078 "y.tab.c" /* yacc.c:1646  */
    break;

  case 99:
#line 320 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[0]).parse; }
#line 2084 "y.tab.c" /* yacc.c:1646  */
    break;

  case 100:
#line 324 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pnull(); }
#line 2090 "y.tab.c" /* yacc.c:1646  */
    break;

  case 101:
#line 326 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pappend( (yyvsp[-1]).parse, (yyvsp[0]).parse ); }
#line 2096 "y.tab.c" /* yacc.c:1646  */
    break;

  case 102:
#line 330 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = plist( (yyvsp[0]).string ); }
#line 2102 "y.tab.c" /* yacc.c:1646  */
    break;

  case 103:
#line 331 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = yymode( SCAN_CALL ); }
#line 2108 "y.tab.c" /* yacc.c:1646  */
    break;

  case 104:
#line 332 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[-1]).parse; yymode( (yyvsp[-2]).number ); }
#line 2114 "y.tab.c" /* yacc.c:1646  */
    break;

  case 105:
#line 340 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 2120 "y.tab.c" /* yacc.c:1646  */
    break;

  case 106:
#line 341 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = prule( (yyvsp[-2]).string, (yyvsp[0]).parse ); }
#line 2126 "y.tab.c" /* yacc.c:1646  */
    break;

  case 107:
#line 342 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 2132 "y.tab.c" /* yacc.c:1646  */
    break;

  case 108:
#line 343 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pon( (yyvsp[-3]).parse, prule( (yyvsp[-2]).string, (yyvsp[0]).parse ) ); }
#line 2138 "y.tab.c" /* yacc.c:1646  */
    break;

  case 109:
#line 344 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 2144 "y.tab.c" /* yacc.c:1646  */
    break;

  case 110:
#line 345 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pon( (yyvsp[-3]).parse, (yyvsp[0]).parse ); }
#line 2150 "y.tab.c" /* yacc.c:1646  */
    break;

  case 111:
#line 355 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = 0; }
#line 2156 "y.tab.c" /* yacc.c:1646  */
    break;

  case 112:
#line 357 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = (yyvsp[-1]).number | (yyvsp[0]).number; }
#line 2162 "y.tab.c" /* yacc.c:1646  */
    break;

  case 113:
#line 361 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = EXEC_UPDATED; }
#line 2168 "y.tab.c" /* yacc.c:1646  */
    break;

  case 114:
#line 363 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = EXEC_TOGETHER; }
#line 2174 "y.tab.c" /* yacc.c:1646  */
    break;

  case 115:
#line 365 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = EXEC_IGNORE; }
#line 2180 "y.tab.c" /* yacc.c:1646  */
    break;

  case 116:
#line 367 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = EXEC_QUIETLY; }
#line 2186 "y.tab.c" /* yacc.c:1646  */
    break;

  case 117:
#line 369 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = EXEC_PIECEMEAL; }
#line 2192 "y.tab.c" /* yacc.c:1646  */
    break;

  case 118:
#line 371 "jamgram.y" /* yacc.c:1646  */
    { (yyval).number = EXEC_EXISTING; }
#line 2198 "y.tab.c" /* yacc.c:1646  */
    break;

  case 119:
#line 380 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = pnull(); }
#line 2204 "y.tab.c" /* yacc.c:1646  */
    break;

  case 120:
#line 381 "jamgram.y" /* yacc.c:1646  */
    { yymode( SCAN_PUNCT ); }
#line 2210 "y.tab.c" /* yacc.c:1646  */
    break;

  case 121:
#line 382 "jamgram.y" /* yacc.c:1646  */
    { (yyval).parse = (yyvsp[0]).parse; }
#line 2216 "y.tab.c" /* yacc.c:1646  */
    break;


#line 2220 "y.tab.c" /* yacc.c:1646  */
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

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
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
        yyerror (yymsgp);
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
                      yytoken, &yylval);
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

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

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
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


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

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
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
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp);
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
  return yyresult;
}
