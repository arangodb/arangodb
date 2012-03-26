
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
#define YYLSP_NEEDED 1

/* Substitute the variable and function names.  */
#define yyparse         QLparse
#define yylex           QLlex
#define yyerror         QLerror
#define yylval          QLlval
#define yychar          QLchar
#define yydebug         QLdebug
#define yynerrs         QLnerrs
#define yylloc          QLlloc

/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 10 "QL/parser.y"


#include <stdio.h>
#include <stdlib.h>

#include <BasicsC/common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/strings.h>
#include <BasicsC/voc-errors.h>

#include "VocBase/query-node.h"
#include "VocBase/query-base.h"
#include "VocBase/query-parse.h"

#define ABORT_IF_OOM(ptr) \
  if (!ptr) { \
    TRI_SetQueryError(&template_->_error, TRI_ERROR_OUT_OF_MEMORY, NULL); \
    YYABORT; \
  }



/* Line 189 of yacc.c  */
#line 104 "QL/parser.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
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
     SELECT = 258,
     FROM = 259,
     WHERE = 260,
     JOIN = 261,
     LIST = 262,
     INNER = 263,
     OUTER = 264,
     LEFT = 265,
     RIGHT = 266,
     ON = 267,
     ORDER = 268,
     BY = 269,
     ASC = 270,
     DESC = 271,
     WITHIN = 272,
     NEAR = 273,
     LIMIT = 274,
     AND = 275,
     OR = 276,
     NOT = 277,
     IN = 278,
     ASSIGNMENT = 279,
     GREATER = 280,
     LESS = 281,
     GREATER_EQUAL = 282,
     LESS_EQUAL = 283,
     EQUAL = 284,
     UNEQUAL = 285,
     NULLX = 286,
     TRUE = 287,
     FALSE = 288,
     UNDEFINED = 289,
     IDENTIFIER = 290,
     QUOTED_IDENTIFIER = 291,
     PARAMETER = 292,
     PARAMETER_NAMED = 293,
     STRING = 294,
     REAL = 295,
     COLON = 296,
     TERNARY = 297,
     FCALL = 298,
     UPLUS = 299,
     UMINUS = 300,
     MEMBER = 301
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 32 "QL/parser.y"

  TRI_query_node_t* node;
  int intval;
  double floatval;
  char *strval;



/* Line 214 of yacc.c  */
#line 195 "QL/parser.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


/* Copy the second part of user declarations.  */

/* Line 264 of yacc.c  */
#line 40 "QL/parser.y"

int QLlex (YYSTYPE*, YYLTYPE*, void*);

void QLerror (YYLTYPE* locp, TRI_query_template_t* const template_, const char* err) {
  TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_PARSE, err); 
}

#define scanner template_->_parser->_scanner


/* Line 264 of yacc.c  */
#line 231 "QL/parser.c"

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
	 || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
	     && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

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
#define YYFINAL  11
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   283

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  61
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  52
/* YYNRULES -- Number of rules.  */
#define YYNRULES  122
/* YYNRULES -- Number of states.  */
#define YYNSTATES  216

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   301

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    47,     2,     2,
      56,    57,    45,    43,    54,    44,    60,    46,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    52,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    53,     2,    55,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    58,     2,    59,     2,     2,     2,     2,
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
      35,    36,    37,    38,    39,    40,    41,    42,    48,    49,
      50,    51
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     6,     9,    10,    12,    13,    20,    22,
      23,    27,    30,    37,    43,    45,    48,    50,    54,    60,
      61,    65,    67,    71,    72,    83,    94,    95,    98,    99,
     100,   105,   107,   111,   114,   115,   117,   119,   120,   123,
     127,   132,   138,   140,   143,   144,   149,   151,   155,   157,
     161,   165,   168,   170,   172,   174,   176,   178,   180,   182,
     185,   187,   190,   194,   197,   201,   204,   208,   210,   212,
     214,   216,   217,   221,   223,   224,   228,   230,   231,   235,
     237,   240,   243,   246,   250,   254,   258,   261,   264,   267,
     271,   275,   279,   283,   287,   291,   295,   299,   303,   307,
     311,   315,   319,   323,   329,   331,   335,   336,   342,   344,
     348,   351,   352,   357,   359,   363,   365,   367,   369,   371,
     373,   375,   377
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      62,     0,    -1,    65,    63,    -1,    64,    63,    -1,    -1,
      52,    -1,    -1,     3,    66,    67,    78,    79,    84,    -1,
      85,    -1,    -1,     4,    68,    69,    -1,    90,    77,    -1,
      69,    93,    90,    77,    12,    97,    -1,    53,    71,    54,
      71,    55,    -1,    40,    -1,    44,    40,    -1,    70,    -1,
      71,    54,    71,    -1,    53,    74,    54,    74,    55,    -1,
      -1,    92,    75,   101,    -1,    73,    -1,    74,    54,    74,
      -1,    -1,    17,    92,    24,    56,    76,    54,    72,    54,
      40,    57,    -1,    18,    92,    24,    56,    76,    54,    72,
      54,    40,    57,    -1,    -1,     5,    97,    -1,    -1,    -1,
      13,    14,    80,    81,    -1,    82,    -1,    81,    54,    82,
      -1,    97,    83,    -1,    -1,    15,    -1,    16,    -1,    -1,
      19,    40,    -1,    19,    44,    40,    -1,    19,    40,    54,
      40,    -1,    19,    40,    54,    44,    40,    -1,    92,    -1,
      58,    59,    -1,    -1,    58,    86,    87,    59,    -1,    88,
      -1,    87,    54,    88,    -1,    89,    -1,    35,    41,    97,
      -1,    39,    41,    97,    -1,    91,    92,    -1,    35,    -1,
      36,    -1,    35,    -1,    36,    -1,    94,    -1,    95,    -1,
      96,    -1,     7,     6,    -1,     6,    -1,     8,     6,    -1,
      10,     9,     6,    -1,    10,     6,    -1,    11,     9,     6,
      -1,    11,     6,    -1,    56,    97,    57,    -1,   102,    -1,
     103,    -1,   104,    -1,   105,    -1,    -1,    85,    98,   101,
      -1,    85,    -1,    -1,   109,    99,   101,    -1,   109,    -1,
      -1,   112,   100,   101,    -1,   112,    -1,    60,    35,    -1,
      60,    36,    -1,    60,   105,    -1,   101,    60,    35,    -1,
     101,    60,    36,    -1,   101,    60,   105,    -1,    43,    97,
      -1,    44,    97,    -1,    22,    97,    -1,    97,    21,    97,
      -1,    97,    20,    97,    -1,    97,    43,    97,    -1,    97,
      44,    97,    -1,    97,    45,    97,    -1,    97,    46,    97,
      -1,    97,    47,    97,    -1,    97,    29,    97,    -1,    97,
      30,    97,    -1,    97,    26,    97,    -1,    97,    25,    97,
      -1,    97,    28,    97,    -1,    97,    27,    97,    -1,    97,
      23,    97,    -1,    97,    42,    97,    41,    97,    -1,   106,
      -1,    35,    56,    57,    -1,    -1,    35,    56,   107,   108,
      57,    -1,    97,    -1,   108,    54,    97,    -1,    53,    55,
      -1,    -1,    53,   110,   111,    55,    -1,    97,    -1,   111,
      54,    97,    -1,    39,    -1,    40,    -1,    31,    -1,    34,
      -1,    32,    -1,    33,    -1,    37,    -1,    38,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   129,   129,   131,   135,   137,   142,   148,   161,   169,
     169,   181,   193,   214,   227,   241,   258,   262,   275,   287,
     287,   301,   305,   317,   320,   347,   381,   384,   392,   395,
     395,   407,   411,   418,   429,   434,   439,   447,   449,   462,
     475,   495,   518,   523,   528,   528,   541,   545,   552,   559,
     573,   590,   621,   627,   638,   644,   655,   659,   663,   670,
     677,   681,   688,   692,   696,   700,   707,   711,   715,   719,
     723,   727,   727,   738,   742,   742,   753,   757,   757,   768,
     776,   783,   792,   796,   803,   812,   820,   826,   832,   841,
     849,   857,   865,   873,   881,   889,   897,   905,   913,   921,
     929,   937,   945,   956,   973,   980,   992,   992,  1010,  1013,
    1020,  1024,  1024,  1036,  1039,  1046,  1054,  1065,  1069,  1073,
    1078,  1083,  1097
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "SELECT", "FROM", "WHERE", "JOIN",
  "LIST", "INNER", "OUTER", "LEFT", "RIGHT", "ON", "ORDER", "BY", "ASC",
  "DESC", "WITHIN", "NEAR", "LIMIT", "AND", "OR", "NOT", "IN",
  "ASSIGNMENT", "GREATER", "LESS", "GREATER_EQUAL", "LESS_EQUAL", "EQUAL",
  "UNEQUAL", "NULLX", "TRUE", "FALSE", "UNDEFINED", "IDENTIFIER",
  "QUOTED_IDENTIFIER", "PARAMETER", "PARAMETER_NAMED", "STRING", "REAL",
  "COLON", "TERNARY", "'+'", "'-'", "'*'", "'/'", "'%'", "FCALL", "UPLUS",
  "UMINUS", "MEMBER", "';'", "'['", "','", "']'", "'('", "')'", "'{'",
  "'}'", "'.'", "$accept", "query", "query_terminator", "empty_query",
  "select_query", "select_clause", "from_clause", "$@1", "from_list",
  "geo_2dvalue", "geo_1dvalue", "geo_value", "geo_2dreference",
  "geo_1dreference", "$@2", "geo_reference", "geo_restriction",
  "where_clause", "order_clause", "$@3", "order_list", "order_element",
  "order_direction", "limit_clause", "document", "$@4", "attribute_list",
  "attribute", "named_attribute", "collection_reference",
  "collection_name", "collection_alias", "join_type", "list_join",
  "inner_join", "outer_join", "expression", "$@5", "$@6", "$@7",
  "object_access", "unary_operator", "binary_operator",
  "conditional_operator", "function_call", "function_invocation", "$@8",
  "function_args_list", "array_declaration", "$@9", "array_list", "atom", 0
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
     295,   296,   297,    43,    45,    42,    47,    37,   298,   299,
     300,   301,    59,    91,    44,    93,    40,    41,   123,   125,
      46
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    61,    62,    62,    63,    63,    64,    65,    66,    68,
      67,    69,    69,    70,    71,    71,    72,    72,    73,    75,
      74,    76,    76,    77,    77,    77,    78,    78,    79,    80,
      79,    81,    81,    82,    83,    83,    83,    84,    84,    84,
      84,    84,    85,    85,    86,    85,    87,    87,    88,    89,
      89,    90,    91,    91,    92,    92,    93,    93,    93,    94,
      95,    95,    96,    96,    96,    96,    97,    97,    97,    97,
      97,    98,    97,    97,    99,    97,    97,   100,    97,    97,
     101,   101,   101,   101,   101,   101,   102,   102,   102,   103,
     103,   103,   103,   103,   103,   103,   103,   103,   103,   103,
     103,   103,   103,   104,   105,   106,   107,   106,   108,   108,
     109,   110,   109,   111,   111,   112,   112,   112,   112,   112,
     112,   112,   112
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     0,     1,     0,     6,     1,     0,
       3,     2,     6,     5,     1,     2,     1,     3,     5,     0,
       3,     1,     3,     0,    10,    10,     0,     2,     0,     0,
       4,     1,     3,     2,     0,     1,     1,     0,     2,     3,
       4,     5,     1,     2,     0,     4,     1,     3,     1,     3,
       3,     2,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     2,     3,     2,     3,     2,     3,     1,     1,     1,
       1,     0,     3,     1,     0,     3,     1,     0,     3,     1,
       2,     2,     2,     3,     3,     3,     2,     2,     2,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     5,     1,     3,     0,     5,     1,     3,
       2,     0,     4,     1,     3,     1,     1,     1,     1,     1,
       1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       6,     0,     0,     4,     4,    54,    55,    44,     0,     8,
      42,     1,     5,     3,     2,    43,     0,     9,    26,     0,
       0,     0,    46,    48,     0,     0,    28,     0,     0,     0,
      45,    52,    53,    10,    23,     0,     0,   117,   119,   120,
     118,    54,   121,   122,   115,   116,     0,     0,   111,     0,
      73,    27,    67,    68,    69,    70,   104,    76,    79,     0,
      37,    49,    50,    47,    60,     0,     0,     0,     0,     0,
      56,    57,    58,     0,     0,    11,    51,    88,   106,    86,
      87,   110,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    29,     0,     7,    59,    61,    63,     0,    65,
       0,    23,     0,     0,   105,     0,   113,     0,    66,     0,
      72,    90,    89,   102,    99,    98,   101,   100,    96,    97,
       0,    91,    92,    93,    94,    95,    75,    78,     0,    38,
       0,    62,    64,     0,     0,     0,   108,     0,     0,   112,
      80,    81,    82,     0,     0,    30,    31,    34,     0,    39,
       0,     0,     0,     0,   107,   114,    83,    84,    85,   103,
       0,    35,    36,    33,    40,     0,    12,     0,    21,     0,
       0,    19,     0,   109,    32,    41,     0,     0,     0,     0,
       0,     0,    22,    14,     0,     0,    16,     0,     0,    20,
       0,     0,    15,     0,     0,     0,     0,    18,     0,    17,
       0,     0,     0,    24,    25,    13
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,    13,     3,     4,     8,    18,    24,    33,   196,
     197,   198,   178,   179,   189,   180,    75,    26,    60,   138,
     155,   156,   173,   104,    50,    16,    21,    22,    23,    34,
      35,    10,    69,    70,    71,    72,   157,    84,   100,   101,
     120,    52,    53,    54,    55,    56,   115,   147,    57,    82,
     117,    58
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -156
static const yytype_int16 yypact[] =
{
      18,   -16,    41,     3,     3,  -156,  -156,   -31,   108,  -156,
    -156,  -156,  -156,  -156,  -156,  -156,    13,  -156,    38,    46,
      80,   -30,  -156,  -156,    54,    64,   110,    64,    64,    13,
    -156,  -156,  -156,    70,    75,    74,    64,  -156,  -156,  -156,
    -156,    69,  -156,  -156,  -156,  -156,    64,    64,    82,    64,
      78,   173,  -156,  -156,  -156,  -156,  -156,    81,    91,   126,
     129,   173,   173,  -156,  -156,   147,   152,     1,    48,    54,
    -156,  -156,  -156,    74,    74,  -156,  -156,  -156,   112,  -156,
    -156,  -156,    64,   -12,   107,    64,    64,    64,    64,    64,
      64,    64,    64,    64,    64,    64,    64,    64,    64,    64,
     107,   107,  -156,    16,  -156,  -156,  -156,  -156,   179,  -156,
     189,    75,   181,   182,  -156,    64,   173,    51,  -156,    83,
     148,   209,   184,   197,   100,   100,   100,   100,    88,    88,
     145,   137,   137,  -156,  -156,  -156,   148,   148,    64,   143,
     186,  -156,  -156,   221,   165,   190,   173,    28,    64,  -156,
      69,  -156,  -156,    93,    64,   191,  -156,   134,    39,  -156,
      64,    -9,    -9,    64,  -156,   173,    69,  -156,  -156,   173,
      64,  -156,  -156,  -156,  -156,   207,   173,    74,  -156,   194,
     195,  -156,   196,   173,  -156,  -156,   203,    74,     6,   107,
       6,    74,  -156,  -156,   211,    44,  -156,   204,   205,   148,
     206,   208,  -156,   210,    44,   222,   225,  -156,    44,  -156,
     212,   213,   216,  -156,  -156,  -156
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -156,  -156,   257,  -156,  -156,  -156,  -156,  -156,  -156,  -156,
    -155,    76,  -156,  -140,  -156,   105,   157,  -156,  -156,  -156,
    -156,   102,  -156,  -156,   272,  -156,  -156,   245,  -156,   214,
    -156,   -35,  -156,  -156,  -156,  -156,   -24,  -156,  -156,  -156,
     -95,  -156,  -156,  -156,  -117,  -156,  -156,  -156,  -156,  -156,
    -156,  -156
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -78
static const yytype_int16 yytable[] =
{
      76,    51,   152,    61,    62,   136,   137,   107,    85,    86,
     108,    87,    77,    88,    89,    90,    91,    92,    93,     5,
       6,     1,    79,    80,    29,    83,     5,     6,    15,    30,
      94,    95,    96,    97,    98,    99,   168,   186,   112,   113,
     203,    11,     7,    25,   177,   118,   193,   192,    19,   209,
     194,   201,    20,   212,   109,    12,   139,   110,   116,   195,
     140,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,    64,    65,    66,   174,
      67,    68,   163,   175,   193,   164,    36,    27,   194,    31,
      32,   146,    73,    74,   199,    37,    38,    39,    40,    41,
       6,    42,    43,    44,    45,   148,   149,    46,    47,     5,
       6,    87,    17,    88,    89,    90,    91,    48,   150,   151,
      49,    28,     7,    59,   165,    78,   181,   181,   166,   167,
     169,    95,    96,    97,    98,    99,   176,    81,   -71,   183,
     102,   -74,   181,    95,    96,    97,    98,    99,   103,   171,
     172,   -77,   181,   105,    85,    86,   181,    87,   106,    88,
      89,    90,    91,    92,    93,    85,    86,   119,    87,   114,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,    97,    98,    99,   141,   154,    94,    95,    96,
      97,    98,    99,    85,    86,   142,    87,   158,    88,    89,
      90,    91,    92,    93,    85,   144,   145,    87,   153,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   161,    88,    89,    90,    91,   159,    95,    96,    97,
      98,    99,    87,   160,    88,    89,    90,    91,    92,    93,
      95,    96,    97,    98,    99,   170,   162,   185,   187,   188,
     190,   202,    95,    96,    97,    98,    99,   191,   204,   205,
     206,    14,   210,   207,   208,   211,   200,   182,   143,   213,
     214,   215,   184,     9,    63,     0,     0,     0,     0,     0,
       0,     0,     0,   111
};

static const yytype_int16 yycheck[] =
{
      35,    25,   119,    27,    28,   100,   101,     6,    20,    21,
       9,    23,    36,    25,    26,    27,    28,    29,    30,    35,
      36,     3,    46,    47,    54,    49,    35,    36,    59,    59,
      42,    43,    44,    45,    46,    47,   153,   177,    73,    74,
     195,     0,    58,     5,    53,    57,    40,   187,    35,   204,
      44,   191,    39,   208,     6,    52,    40,     9,    82,    53,
      44,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,     6,     7,     8,    40,
      10,    11,    54,    44,    40,    57,    22,    41,    44,    35,
      36,   115,    17,    18,   189,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    54,    55,    43,    44,    35,
      36,    23,     4,    25,    26,    27,    28,    53,    35,    36,
      56,    41,    58,    13,   148,    56,   161,   162,    35,    36,
     154,    43,    44,    45,    46,    47,   160,    55,    60,   163,
      14,    60,   177,    43,    44,    45,    46,    47,    19,    15,
      16,    60,   187,     6,    20,    21,   191,    23,     6,    25,
      26,    27,    28,    29,    30,    20,    21,    60,    23,    57,
      25,    26,    27,    28,    29,    30,    42,    43,    44,    45,
      46,    47,    45,    46,    47,     6,    41,    42,    43,    44,
      45,    46,    47,    20,    21,     6,    23,    54,    25,    26,
      27,    28,    29,    30,    20,    24,    24,    23,    60,    25,
      26,    27,    28,    29,    30,    42,    43,    44,    45,    46,
      47,    56,    25,    26,    27,    28,    40,    43,    44,    45,
      46,    47,    23,    12,    25,    26,    27,    28,    29,    30,
      43,    44,    45,    46,    47,    54,    56,    40,    54,    54,
      54,    40,    43,    44,    45,    46,    47,    54,    54,    54,
      54,     4,    40,    55,    54,    40,   190,   162,   111,    57,
      57,    55,   170,     1,    29,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    69
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,    62,    64,    65,    35,    36,    58,    66,    85,
      92,     0,    52,    63,    63,    59,    86,     4,    67,    35,
      39,    87,    88,    89,    68,     5,    78,    41,    41,    54,
      59,    35,    36,    69,    90,    91,    22,    31,    32,    33,
      34,    35,    37,    38,    39,    40,    43,    44,    53,    56,
      85,    97,   102,   103,   104,   105,   106,   109,   112,    13,
      79,    97,    97,    88,     6,     7,     8,    10,    11,    93,
      94,    95,    96,    17,    18,    77,    92,    97,    56,    97,
      97,    55,   110,    97,    98,    20,    21,    23,    25,    26,
      27,    28,    29,    30,    42,    43,    44,    45,    46,    47,
      99,   100,    14,    19,    84,     6,     6,     6,     9,     6,
       9,    90,    92,    92,    57,   107,    97,   111,    57,    60,
     101,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,   101,   101,    80,    40,
      44,     6,     6,    77,    24,    24,    97,   108,    54,    55,
      35,    36,   105,    60,    41,    81,    82,    97,    54,    40,
      12,    56,    56,    54,    57,    97,    35,    36,   105,    97,
      54,    15,    16,    83,    40,    44,    97,    53,    73,    74,
      76,    92,    76,    97,    82,    40,    74,    54,    54,    75,
      54,    54,    74,    40,    44,    53,    70,    71,    72,   101,
      72,    74,    40,    71,    54,    54,    54,    55,    54,    71,
      40,    40,    71,    57,    57,    55
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
      yyerror (&yylloc, template_, YY_("syntax error: cannot back up")); \
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
# define YYLEX yylex (&yylval, &yylloc, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, &yylloc, scanner)
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
		  Type, Value, Location, template_); \
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
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, TRI_query_template_t* const template_)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, template_)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    TRI_query_template_t* const template_;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
  YYUSE (template_);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, TRI_query_template_t* const template_)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp, template_)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    TRI_query_template_t* const template_;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, template_);
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
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, TRI_query_template_t* const template_)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule, template_)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
    TRI_query_template_t* const template_;
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
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       , template_);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule, template_); \
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, TRI_query_template_t* const template_)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp, template_)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
    TRI_query_template_t* const template_;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (template_);

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
int yyparse (TRI_query_template_t* const template_);
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
yyparse (TRI_query_template_t* const template_)
#else
int
yyparse (template_)
    TRI_query_template_t* const template_;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Location data for the lookahead symbol.  */
YYLTYPE yylloc;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.
       `yyls': related to locations.

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

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[2];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yyls = yylsa;
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
  yylsp = yyls;

#if YYLTYPE_IS_TRIVIAL
  /* Initialize the default location before parsing starts.  */
  yylloc.first_line   = yylloc.last_line   = 1;
  yylloc.first_column = yylloc.last_column = 1;
#endif

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
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);

	yyls = yyls1;
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
	YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

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
  *++yylsp = yylloc;
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

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1455 of yacc.c  */
#line 129 "QL/parser.y"
    {
    ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 131 "QL/parser.y"
    {
    ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 137 "QL/parser.y"
    {
    ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 142 "QL/parser.y"
    { 
      template_->_query->_type = QUERY_TYPE_EMPTY;
    ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 148 "QL/parser.y"
    {
      // full blown SELECT query
      template_->_query->_type         = QUERY_TYPE_SELECT;
      template_->_query->_select._base = (yyvsp[(2) - (6)].node);
      template_->_query->_from._base   = (yyvsp[(3) - (6)].node);
      template_->_query->_where._base  = (yyvsp[(4) - (6)].node);
      template_->_query->_order._base  = (yyvsp[(5) - (6)].node);
    ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 161 "QL/parser.y"
    {
      // select part of a SELECT
      (yyval.node) = (yyvsp[(1) - (1)].node);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 169 "QL/parser.y"
    {
      // from part of a SELECT
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 174 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryContextPop(template_);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 181 "QL/parser.y"
    {
      // single table query
      ABORT_IF_OOM((yyvsp[(1) - (2)].node));
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(1) - (2)].node));

      if ((yyvsp[(2) - (2)].node)) {
        if (!QLAstQueryAddGeoRestriction(template_->_query, (yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node))) {
          TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID, ((TRI_query_node_t*) (yyvsp[(2) - (2)].node)->_lhs)->_value._stringValue);
          YYABORT;
        }
      }
    ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 193 "QL/parser.y"
    {
      // multi-table query
      ABORT_IF_OOM((yyvsp[(2) - (6)].node));
      ABORT_IF_OOM((yyvsp[(3) - (6)].node));
      ABORT_IF_OOM((yyvsp[(6) - (6)].node));
      (yyval.node) = (yyvsp[(2) - (6)].node);
      (yyval.node)->_lhs  = (yyvsp[(3) - (6)].node);
      (yyval.node)->_rhs  = (yyvsp[(6) - (6)].node);
      
      if ((yyvsp[(4) - (6)].node)) {
        if (!QLAstQueryAddGeoRestriction(template_->_query, (yyvsp[(3) - (6)].node), (yyvsp[(4) - (6)].node))) {
          TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID, ((TRI_query_node_t*) (yyvsp[(4) - (6)].node)->_lhs)->_value._stringValue);
          YYABORT;
        }
      }
      
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(2) - (6)].node));
    ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 214 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(2) - (5)].node));
      ABORT_IF_OOM((yyvsp[(4) - (5)].node));

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueCoordinate);
      ABORT_IF_OOM((yyval.node));

      (yyval.node)->_lhs = (yyvsp[(2) - (5)].node);
      (yyval.node)->_rhs = (yyvsp[(4) - (5)].node);
    ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 227 "QL/parser.y"
    {
      double d;

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNumberDouble);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      
      d = TRI_DoubleString((yyvsp[(1) - (1)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, (yyvsp[(1) - (1)].strval));
        YYABORT;
      }
      (yyval.node)->_value._doubleValue = d; 
    ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 241 "QL/parser.y"
    { 
      double d;

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNumberDouble);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].strval));
      
      d = TRI_DoubleString((yyvsp[(2) - (2)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, (yyvsp[(2) - (2)].strval));
        YYABORT;
      }
      (yyval.node)->_value._doubleValue = -1.0 * d; 
    ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 258 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 262 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueCoordinate);
      ABORT_IF_OOM((yyval.node));

      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 275 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(2) - (5)].node));
      ABORT_IF_OOM((yyvsp[(4) - (5)].node));

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueCoordinate);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = (yyvsp[(2) - (5)].node);
      (yyval.node)->_rhs = (yyvsp[(4) - (5)].node);
    ;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 287 "QL/parser.y"
    {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 291 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerMemberAccess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      TRI_ParseQueryPopIntoRhs((yyval.node), template_);
    ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 301 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 305 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueCoordinate);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 317 "QL/parser.y"
    {
      (yyval.node) = 0;
    ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 320 "QL/parser.y"
    {
      TRI_query_node_t* comp;
      double distance;
      
      ABORT_IF_OOM((yyvsp[(2) - (10)].node));
      ABORT_IF_OOM((yyvsp[(5) - (10)].node));
      ABORT_IF_OOM((yyvsp[(7) - (10)].node));
      ABORT_IF_OOM((yyvsp[(9) - (10)].strval));
      
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeRestrictWithin);
      ABORT_IF_OOM((yyval.node));

      distance = TRI_DoubleString((yyvsp[(9) - (10)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, (yyvsp[(9) - (10)].strval));
        YYABORT;
      }
      (yyval.node)->_value._doubleValue = distance;

      comp = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerCoordinatePair);
      ABORT_IF_OOM(comp);
      comp->_lhs = (yyvsp[(5) - (10)].node);
      comp->_rhs = (yyvsp[(7) - (10)].node);

      (yyval.node)->_lhs = (yyvsp[(2) - (10)].node);
      (yyval.node)->_rhs = comp;
    ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 347 "QL/parser.y"
    {
      TRI_query_node_t* comp;
      int64_t num;
      
      ABORT_IF_OOM((yyvsp[(2) - (10)].node));
      ABORT_IF_OOM((yyvsp[(5) - (10)].node));
      ABORT_IF_OOM((yyvsp[(7) - (10)].node));
      ABORT_IF_OOM((yyvsp[(9) - (10)].strval));
      
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeRestrictNear);
      ABORT_IF_OOM((yyval.node));

      num = TRI_Int64String((yyvsp[(9) - (10)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(9) - (10)].strval));
        YYABORT;
      }
      (yyval.node)->_value._intValue = num;

      comp = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerCoordinatePair);
      ABORT_IF_OOM(comp);
      comp->_lhs = (yyvsp[(5) - (10)].node);
      comp->_rhs = (yyvsp[(7) - (10)].node);

      (yyval.node)->_lhs = (yyvsp[(2) - (10)].node);
      (yyval.node)->_rhs = comp;
    ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 381 "QL/parser.y"
    {
      (yyval.node) = 0;
    ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 384 "QL/parser.y"
    {
      // where condition set
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node) = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 392 "QL/parser.y"
    {
      (yyval.node) = 0;
    ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 395 "QL/parser.y"
    {
      // order by part of a query
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 400 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryContextPop(template_);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 407 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 411 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 418 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerOrderElement);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (2)].node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (2)].node);
      (yyval.node)->_rhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 429 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueOrderDirection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = true;
    ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 434 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueOrderDirection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = true;
    ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 439 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueOrderDirection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = false;
    ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 447 "QL/parser.y"
    {
    ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 449 "QL/parser.y"
    {
      // limit value
      int64_t d = TRI_Int64String((yyvsp[(2) - (2)].strval));

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(2) - (2)].strval));
        YYABORT;
      }
      
      template_->_query->_limit._isUsed = true;
      template_->_query->_limit._offset = 0; 
      template_->_query->_limit._count  = d; 
    ;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 462 "QL/parser.y"
    {
      // limit - value
      int64_t d = TRI_Int64String((yyvsp[(3) - (3)].strval));

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(3) - (3)].strval));
        YYABORT;
      }
      
      template_->_query->_limit._isUsed = true;
      template_->_query->_limit._offset = 0; 
      template_->_query->_limit._count  = -d; 
    ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 475 "QL/parser.y"
    { 
      // limit value, value
      int64_t d1, d2;
      
      d1 = TRI_Int64String((yyvsp[(2) - (4)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(2) - (4)].strval));
        YYABORT;
      }

      d2 = TRI_Int64String((yyvsp[(4) - (4)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(4) - (4)].strval));
        YYABORT;
      }

      template_->_query->_limit._isUsed = true;
      template_->_query->_limit._offset = d1; 
      template_->_query->_limit._count  = d2;
    ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 495 "QL/parser.y"
    { 
      // limit value, -value
      int64_t d1, d2;
      
      d1 = TRI_Int64String((yyvsp[(2) - (5)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(2) - (5)].strval));
        YYABORT;
      }

      d2 = TRI_Int64String((yyvsp[(5) - (5)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(5) - (5)].strval));
        YYABORT;
      }

      template_->_query->_limit._isUsed = true;
      template_->_query->_limit._offset = d1; 
      template_->_query->_limit._count  = -d2;
    ;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 518 "QL/parser.y"
    {
      // document is a reference to a collection (by using its alias)
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node); 
    ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 523 "QL/parser.y"
    {
      // empty document
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueDocument);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 528 "QL/parser.y"
    {
      // listing of document attributes
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 533 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueDocument);
      ABORT_IF_OOM((yyval.node));
      TRI_ParseQueryPopIntoRhs((yyval.node), template_);
    ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 541 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 545 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 552 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 559 "QL/parser.y"
    {
      size_t outLength;
      TRI_query_node_t* str = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueString);
      ABORT_IF_OOM(str);
      ABORT_IF_OOM((yyvsp[(1) - (3)].strval));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      str->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String((yyvsp[(1) - (3)].strval), strlen((yyvsp[(1) - (3)].strval)), &outLength)); 
      ABORT_IF_OOM(str->_value._stringValue);

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNamedValue);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = str;
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 573 "QL/parser.y"
    {
      size_t outLength;
      TRI_query_node_t* str = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueString);
      ABORT_IF_OOM(str);
      ABORT_IF_OOM((yyvsp[(1) - (3)].strval));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      str->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String((yyvsp[(1) - (3)].strval) + 1, strlen((yyvsp[(1) - (3)].strval)) - 2, &outLength)); 
      ABORT_IF_OOM(str->_value._stringValue);

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNamedValue);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = str;
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 590 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (2)].node));
      ABORT_IF_OOM((yyvsp[(1) - (2)].node)->_value._stringValue);
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node)->_value._stringValue);

      if (!TRI_ParseQueryValidateCollectionName((yyvsp[(1) - (2)].node)->_value._stringValue)) {
        // validate collection name
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_COLLECTION_NAME_INVALID, (yyvsp[(1) - (2)].node)->_value._stringValue);
        YYABORT;
      }

      if (!TRI_ParseQueryValidateCollectionAlias((yyvsp[(2) - (2)].node)->_value._stringValue)) {
        // validate alias
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_COLLECTION_ALIAS_INVALID, (yyvsp[(2) - (2)].node)->_value._stringValue);
        YYABORT;
      }

      if (!QLAstQueryAddCollection(template_->_query, (yyvsp[(1) - (2)].node)->_value._stringValue, (yyvsp[(2) - (2)].node)->_value._stringValue)) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_COLLECTION_ALIAS_REDECLARED, (yyvsp[(2) - (2)].node)->_value._stringValue);
        YYABORT;
      }

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeReferenceCollection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = (yyvsp[(1) - (2)].node);
      (yyval.node)->_rhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 621 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
    ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 627 "QL/parser.y"
    {
      size_t outLength;
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String((yyvsp[(1) - (1)].strval) + 1, strlen((yyvsp[(1) - (1)].strval)) - 2, &outLength)); 
      ABORT_IF_OOM((yyval.node)->_value._stringValue);
    ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 638 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeReferenceCollectionAlias);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
    ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 644 "QL/parser.y"
    {
      size_t outLength;
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeReferenceCollectionAlias);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String((yyvsp[(1) - (1)].strval) + 1, strlen((yyvsp[(1) - (1)].strval)) - 2, &outLength)); 
      ABORT_IF_OOM((yyval.node)->_value._stringValue);
    ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 655 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 659 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 663 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 670 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinList);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 677 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinInner);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 681 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinInner);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 688 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinLeft);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 692 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinLeft);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 696 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinRight);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 700 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinRight);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 707 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(2) - (3)].node));
      (yyval.node) = (yyvsp[(2) - (3)].node);
    ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 711 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 715 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 719 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 723 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 727 "QL/parser.y"
    { 
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 731 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerMemberAccess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      TRI_ParseQueryPopIntoRhs((yyval.node), template_);
    ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 738 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 742 "QL/parser.y"
    {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 746 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerMemberAccess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      TRI_ParseQueryPopIntoRhs((yyval.node), template_);
    ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 753 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);  
    ;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 757 "QL/parser.y"
    {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    ;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 761 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerMemberAccess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      TRI_ParseQueryPopIntoRhs((yyval.node), template_);
    ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 768 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 776 "QL/parser.y"
    {
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(2) - (2)].strval));
      name->_value._stringValue = (yyvsp[(2) - (2)].strval);
      TRI_ParseQueryContextAddElement(template_, name);
    ;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 783 "QL/parser.y"
    {
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      size_t outLength;
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(2) - (2)].strval));
      name->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String((yyvsp[(2) - (2)].strval) + 1, strlen((yyvsp[(2) - (2)].strval)) - 2, &outLength));
      ABORT_IF_OOM(name->_value._stringValue);
      TRI_ParseQueryContextAddElement(template_, name);
    ;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 792 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(2) - (2)].node));
    ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 796 "QL/parser.y"
    {
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(3) - (3)].strval));
      name->_value._stringValue = (yyvsp[(3) - (3)].strval);
      TRI_ParseQueryContextAddElement(template_, name);
    ;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 803 "QL/parser.y"
    {
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      size_t outLength;
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(3) - (3)].strval));
      name->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String((yyvsp[(3) - (3)].strval) + 1, strlen((yyvsp[(3) - (3)].strval)) - 2, &outLength));
      ABORT_IF_OOM(name->_value._stringValue);
      TRI_ParseQueryContextAddElement(template_, name);
    ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 812 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 820 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeUnaryOperatorPlus);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 826 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeUnaryOperatorMinus);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 832 "QL/parser.y"
    { 
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeUnaryOperatorNot);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 841 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorOr);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 849 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorAnd);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node); 
    ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 857 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorAdd);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 865 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorSubtract);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 873 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorMultiply);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 881 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorDivide);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 889 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorModulus);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 897 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorEqual);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 905 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorUnequal);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 913 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorLess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 921 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorGreater);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 929 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorLessEqual);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 937 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorGreaterEqual);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 945 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorIn);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 103:

/* Line 1455 of yacc.c  */
#line 956 "QL/parser.y"
    {
      TRI_query_node_t* node = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerTernarySwitch);
      ABORT_IF_OOM(node);
      ABORT_IF_OOM((yyvsp[(1) - (5)].node));
      ABORT_IF_OOM((yyvsp[(3) - (5)].node));
      ABORT_IF_OOM((yyvsp[(5) - (5)].node));
      node->_lhs = (yyvsp[(3) - (5)].node);
      node->_rhs = (yyvsp[(5) - (5)].node);

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeControlTernary);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = (yyvsp[(1) - (5)].node);
      (yyval.node)->_rhs = node;
    ;}
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 973 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 980 "QL/parser.y"
    {
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(1) - (3)].strval));
      name->_value._stringValue = (yyvsp[(1) - (3)].strval);
      
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeControlFunctionCall);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = name;
      (yyval.node)->_rhs = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM((yyval.node)->_rhs);
    ;}
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 992 "QL/parser.y"
    {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    ;}
    break;

  case 107:

/* Line 1455 of yacc.c  */
#line 996 "QL/parser.y"
    {
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(1) - (5)].strval));
      name->_value._stringValue = (yyvsp[(1) - (5)].strval);

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeControlFunctionCall);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = name;
      TRI_ParseQueryPopIntoRhs((yyval.node), template_);
    ;}
    break;

  case 108:

/* Line 1455 of yacc.c  */
#line 1010 "QL/parser.y"
    {
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 109:

/* Line 1455 of yacc.c  */
#line 1013 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 1020 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueArray);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 111:

/* Line 1455 of yacc.c  */
#line 1024 "QL/parser.y"
    {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    ;}
    break;

  case 112:

/* Line 1455 of yacc.c  */
#line 1028 "QL/parser.y"
    { 
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueArray);
      ABORT_IF_OOM((yyval.node));
      TRI_ParseQueryPopIntoRhs((yyval.node), template_);
    ;}
    break;

  case 113:

/* Line 1455 of yacc.c  */
#line 1036 "QL/parser.y"
    {
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 1039 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      TRI_ParseQueryContextAddElement(template_, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 115:

/* Line 1455 of yacc.c  */
#line 1046 "QL/parser.y"
    {
      size_t outLength;
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueString);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String((yyvsp[(1) - (1)].strval) + 1, strlen((yyvsp[(1) - (1)].strval)) - 2, &outLength)); 
      ABORT_IF_OOM((yyval.node)->_value._stringValue);
    ;}
    break;

  case 116:

/* Line 1455 of yacc.c  */
#line 1054 "QL/parser.y"
    {
      double d = TRI_DoubleString((yyvsp[(1) - (1)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, (yyvsp[(1) - (1)].strval));
        YYABORT;
      }
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNumberDoubleString);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval); 
    ;}
    break;

  case 117:

/* Line 1455 of yacc.c  */
#line 1065 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNull);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 118:

/* Line 1455 of yacc.c  */
#line 1069 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueUndefined); 
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 1073 "QL/parser.y"
    { 
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueBool);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = true;
    ;}
    break;

  case 120:

/* Line 1455 of yacc.c  */
#line 1078 "QL/parser.y"
    {
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueBool);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = false;
    ;}
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 1083 "QL/parser.y"
    {
      // numbered parameter
      int64_t d = TRI_Int64String((yyvsp[(1) - (1)].strval));

      if (TRI_errno() != TRI_ERROR_NO_ERROR || d < 0 || d >= 256) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE, (yyvsp[(1) - (1)].strval));
        YYABORT;
      }

      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueParameterNamed);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
      TRI_AddBindParameterQueryTemplate(template_, TRI_CreateBindParameter((yyvsp[(1) - (1)].strval), NULL));   
    ;}
    break;

  case 122:

/* Line 1455 of yacc.c  */
#line 1097 "QL/parser.y"
    {
      // named parameter
      (yyval.node) = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueParameterNamed);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
      TRI_AddBindParameterQueryTemplate(template_, TRI_CreateBindParameter((yyvsp[(1) - (1)].strval), NULL));   
    ;}
    break;



/* Line 1455 of yacc.c  */
#line 3266 "QL/parser.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

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
      yyerror (&yylloc, template_, YY_("syntax error"));
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
	    yyerror (&yylloc, template_, yymsg);
	  }
	else
	  {
	    yyerror (&yylloc, template_, YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }

  yyerror_range[0] = yylloc;

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
		      yytoken, &yylval, &yylloc, template_);
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

  yyerror_range[0] = yylsp[1-yylen];
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

      yyerror_range[0] = *yylsp;
      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, yylsp, template_);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;

  yyerror_range[1] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, (yyerror_range - 1), 2);
  *++yylsp = yyloc;

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
  yyerror (&yylloc, template_, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, &yylloc, template_);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp, template_);
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
#line 1106 "QL/parser.y"



