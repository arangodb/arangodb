
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
#include <stdarg.h>

#include <BasicsC/common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/strings.h>

#include "QL/ast-node.h"
#include "QL/parser-context.h"
#include "QL/formatter.h"
#include "QL/error.h"

#define ABORT_IF_OOM(ptr) \
  if (!ptr) { \
    QLParseRegisterParseError(context, ERR_OOM); \
    YYABORT; \
  }



/* Line 189 of yacc.c  */
#line 105 "QL/parser.c"

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
     LIMIT = 272,
     AND = 273,
     OR = 274,
     NOT = 275,
     IN = 276,
     ASSIGNMENT = 277,
     GREATER = 278,
     LESS = 279,
     GREATER_EQUAL = 280,
     LESS_EQUAL = 281,
     EQUAL = 282,
     UNEQUAL = 283,
     IDENTICAL = 284,
     UNIDENTICAL = 285,
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
#line 33 "QL/parser.y"

  QL_ast_node_t *node;
  int intval;
  double floatval;
  char *strval;



/* Line 214 of yacc.c  */
#line 196 "QL/parser.c"
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
#line 41 "QL/parser.y"

int QLlex (YYSTYPE *,YYLTYPE *, void *);

void QLerror (YYLTYPE *locp,QL_parser_context_t *context, const char *err) {
  context->_lexState._errorState._code      = ERR_PARSE; 
  context->_lexState._errorState._message   = QLParseAllocString(context, (char *) err);
  context->_lexState._errorState._line      = locp->first_line;
  context->_lexState._errorState._column    = locp->first_column;
}

#define scanner context->_scanner


/* Line 264 of yacc.c  */
#line 235 "QL/parser.c"

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
#define YYLAST   259

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  61
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  43
/* YYNRULES -- Number of rules.  */
#define YYNRULES  109
/* YYNRULES -- Number of states.  */
#define YYNSTATES  171

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
      56,    57,    45,    43,    53,    44,    58,    46,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    52,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    59,     2,    60,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    54,     2,    55,     2,     2,     2,     2,
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
       0,     0,     3,     5,     8,    10,    13,    14,    21,    23,
      24,    28,    30,    36,    37,    40,    41,    42,    47,    49,
      53,    56,    57,    59,    61,    62,    65,    69,    74,    80,
      82,    85,    86,    91,    93,    97,    99,   103,   107,   110,
     112,   114,   116,   118,   120,   122,   124,   127,   129,   132,
     136,   139,   143,   146,   150,   152,   154,   156,   158,   159,
     163,   165,   166,   170,   172,   173,   177,   179,   182,   185,
     189,   193,   196,   199,   202,   206,   210,   214,   218,   222,
     226,   230,   234,   238,   242,   246,   250,   254,   258,   262,
     266,   272,   274,   278,   279,   285,   287,   291,   294,   295,
     300,   302,   306,   308,   310,   312,   314,   316,   318,   320
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      62,     0,    -1,    64,    -1,    64,    52,    -1,    63,    -1,
      63,    52,    -1,    -1,     3,    65,    66,    69,    70,    75,
      -1,    76,    -1,    -1,     4,    67,    68,    -1,    81,    -1,
      68,    84,    81,    12,    88,    -1,    -1,     5,    88,    -1,
      -1,    -1,    13,    14,    71,    72,    -1,    73,    -1,    72,
      53,    73,    -1,    88,    74,    -1,    -1,    15,    -1,    16,
      -1,    -1,    17,    40,    -1,    17,    44,    40,    -1,    17,
      40,    53,    40,    -1,    17,    40,    53,    44,    40,    -1,
      83,    -1,    54,    55,    -1,    -1,    54,    77,    78,    55,
      -1,    79,    -1,    78,    53,    79,    -1,    80,    -1,    35,
      41,    88,    -1,    39,    41,    88,    -1,    82,    83,    -1,
      35,    -1,    36,    -1,    35,    -1,    36,    -1,    85,    -1,
      86,    -1,    87,    -1,     7,     6,    -1,     6,    -1,     8,
       6,    -1,    10,     9,     6,    -1,    10,     6,    -1,    11,
       9,     6,    -1,    11,     6,    -1,    56,    88,    57,    -1,
      93,    -1,    94,    -1,    95,    -1,    96,    -1,    -1,    76,
      89,    92,    -1,    76,    -1,    -1,   100,    90,    92,    -1,
     100,    -1,    -1,   103,    91,    92,    -1,   103,    -1,    58,
      35,    -1,    58,    96,    -1,    92,    58,    35,    -1,    92,
      58,    96,    -1,    43,    88,    -1,    44,    88,    -1,    20,
      88,    -1,    88,    19,    88,    -1,    88,    18,    88,    -1,
      88,    43,    88,    -1,    88,    44,    88,    -1,    88,    45,
      88,    -1,    88,    46,    88,    -1,    88,    47,    88,    -1,
      88,    29,    88,    -1,    88,    30,    88,    -1,    88,    27,
      88,    -1,    88,    28,    88,    -1,    88,    24,    88,    -1,
      88,    23,    88,    -1,    88,    26,    88,    -1,    88,    25,
      88,    -1,    88,    21,    88,    -1,    88,    42,    88,    41,
      88,    -1,    97,    -1,    35,    56,    57,    -1,    -1,    35,
      56,    98,    99,    57,    -1,    88,    -1,    99,    53,    88,
      -1,    59,    60,    -1,    -1,    59,   101,   102,    60,    -1,
      88,    -1,   102,    53,    88,    -1,    39,    -1,    40,    -1,
      31,    -1,    34,    -1,    32,    -1,    33,    -1,    37,    -1,
      38,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   125,   125,   127,   129,   131,   136,   142,   155,   163,
     163,   175,   180,   194,   197,   205,   208,   208,   220,   224,
     231,   242,   247,   252,   260,   262,   275,   288,   308,   331,
     336,   341,   341,   354,   358,   365,   372,   384,   400,   432,
     438,   448,   454,   464,   468,   472,   479,   486,   490,   497,
     501,   505,   509,   516,   520,   524,   528,   532,   536,   536,
     547,   551,   551,   562,   566,   566,   577,   585,   592,   596,
     603,   611,   617,   623,   632,   640,   648,   656,   664,   672,
     680,   688,   696,   704,   712,   720,   728,   736,   744,   752,
     763,   780,   787,   799,   799,   817,   820,   827,   831,   831,
     843,   846,   853,   860,   871,   875,   879,   884,   889,   902
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "SELECT", "FROM", "WHERE", "JOIN",
  "LIST", "INNER", "OUTER", "LEFT", "RIGHT", "ON", "ORDER", "BY", "ASC",
  "DESC", "LIMIT", "AND", "OR", "NOT", "IN", "ASSIGNMENT", "GREATER",
  "LESS", "GREATER_EQUAL", "LESS_EQUAL", "EQUAL", "UNEQUAL", "IDENTICAL",
  "UNIDENTICAL", "NULLX", "TRUE", "FALSE", "UNDEFINED", "IDENTIFIER",
  "QUOTED_IDENTIFIER", "PARAMETER", "PARAMETER_NAMED", "STRING", "REAL",
  "COLON", "TERNARY", "'+'", "'-'", "'*'", "'/'", "'%'", "FCALL", "UPLUS",
  "UMINUS", "MEMBER", "';'", "','", "'{'", "'}'", "'('", "')'", "'.'",
  "'['", "']'", "$accept", "query", "empty_query", "select_query",
  "select_clause", "from_clause", "$@1", "from_list", "where_clause",
  "order_clause", "$@2", "order_list", "order_element", "order_direction",
  "limit_clause", "document", "$@3", "attribute_list", "attribute",
  "named_attribute", "collection_reference", "collection_name",
  "collection_alias", "join_type", "list_join", "inner_join", "outer_join",
  "expression", "$@4", "$@5", "$@6", "object_access", "unary_operator",
  "binary_operator", "conditional_operator", "function_call",
  "function_invocation", "$@7", "function_args_list", "array_declaration",
  "$@8", "array_list", "atom", 0
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
     300,   301,    59,    44,   123,   125,    40,    41,    46,    91,
      93
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    61,    62,    62,    62,    62,    63,    64,    65,    67,
      66,    68,    68,    69,    69,    70,    71,    70,    72,    72,
      73,    74,    74,    74,    75,    75,    75,    75,    75,    76,
      76,    77,    76,    78,    78,    79,    80,    80,    81,    82,
      82,    83,    83,    84,    84,    84,    85,    86,    86,    87,
      87,    87,    87,    88,    88,    88,    88,    88,    89,    88,
      88,    90,    88,    88,    91,    88,    88,    92,    92,    92,
      92,    93,    93,    93,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,    94,    94,    94,
      95,    96,    97,    98,    97,    99,    99,   100,   101,   100,
     102,   102,   103,   103,   103,   103,   103,   103,   103,   103
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     1,     2,     0,     6,     1,     0,
       3,     1,     5,     0,     2,     0,     0,     4,     1,     3,
       2,     0,     1,     1,     0,     2,     3,     4,     5,     1,
       2,     0,     4,     1,     3,     1,     3,     3,     2,     1,
       1,     1,     1,     1,     1,     1,     2,     1,     2,     3,
       2,     3,     2,     3,     1,     1,     1,     1,     0,     3,
       1,     0,     3,     1,     0,     3,     1,     2,     2,     3,
       3,     2,     2,     2,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       5,     1,     3,     0,     5,     1,     3,     2,     0,     4,
       1,     3,     1,     1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       6,     0,     0,     4,     2,    41,    42,    31,     0,     8,
      29,     1,     5,     3,    30,     0,     9,    13,     0,     0,
       0,    33,    35,     0,     0,    15,     0,     0,     0,    32,
      39,    40,    10,    11,     0,     0,   104,   106,   107,   105,
      41,   108,   109,   102,   103,     0,     0,     0,    98,    60,
      14,    54,    55,    56,    57,    91,    63,    66,     0,    24,
      36,    37,    34,    47,     0,     0,     0,     0,     0,    43,
      44,    45,    38,    73,    93,    71,    72,     0,    97,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      16,     0,     7,    46,    48,    50,     0,    52,     0,     0,
      92,     0,    53,   100,     0,     0,    59,    75,    74,    89,
      86,    85,    88,    87,    83,    84,    81,    82,     0,    76,
      77,    78,    79,    80,    62,    65,     0,    25,     0,    49,
      51,     0,    95,     0,     0,    99,    67,    68,     0,     0,
      17,    18,    21,     0,    26,    12,     0,    94,   101,    69,
      70,    90,     0,    22,    23,    20,    27,     0,    96,    19,
      28
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,     4,     8,    17,    23,    32,    25,    59,
     136,   150,   151,   165,   102,    49,    15,    20,    21,    22,
      33,    34,    10,    68,    69,    70,    71,   152,    80,    98,
      99,   116,    51,    52,    53,    54,    55,   111,   143,    56,
      79,   114,    57
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -112
static const yytype_int16 yypact[] =
{
      -2,   -30,     9,   -32,   -26,  -112,  -112,   -13,    77,  -112,
    -112,  -112,  -112,  -112,  -112,    17,  -112,    81,    67,    78,
      71,  -112,  -112,    18,    57,   105,    57,    57,    17,  -112,
    -112,  -112,    33,  -112,    63,    57,  -112,  -112,  -112,  -112,
      73,  -112,  -112,  -112,  -112,    57,    57,    57,    74,    90,
     172,  -112,  -112,  -112,  -112,  -112,    91,    92,   137,   135,
     172,   172,  -112,  -112,   147,   156,   101,   103,    18,  -112,
    -112,  -112,  -112,  -112,   107,  -112,  -112,   -11,  -112,    57,
     115,    57,    57,    57,    57,    57,    57,    57,    57,    57,
      57,    57,    57,    57,    57,    57,    57,    57,   115,   115,
    -112,    34,  -112,  -112,  -112,  -112,   168,  -112,   169,   164,
    -112,    57,  -112,   172,   -15,   143,   119,   212,   202,    59,
     100,   100,   100,   100,     4,     4,     4,     4,   142,    76,
      76,  -112,  -112,  -112,   119,   119,    57,   126,   140,  -112,
    -112,    57,   172,    22,    57,  -112,    73,  -112,   146,    57,
     129,  -112,   112,    36,  -112,   172,    57,  -112,   172,    73,
    -112,   172,    57,  -112,  -112,  -112,  -112,   152,   172,  -112,
    -112
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -112,  -112,  -112,  -112,  -112,  -112,  -112,  -112,  -112,  -112,
    -112,  -112,    32,  -112,  -112,   203,  -112,  -112,   175,  -112,
     138,  -112,   171,  -112,  -112,  -112,  -112,   -24,  -112,  -112,
    -112,    16,  -112,  -112,  -112,  -111,  -112,  -112,  -112,  -112,
    -112,  -112,  -112
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -65
static const yytype_int16 yytable[] =
{
      50,     1,    60,    61,   147,     5,     6,    81,    82,    11,
      83,    73,    84,    85,    86,    87,    88,    89,    90,    91,
      12,    75,    76,    77,     7,    83,    13,    84,    85,    86,
      87,    92,    93,    94,    95,    96,    97,   160,   144,    63,
      64,    65,    14,    66,    67,   145,   112,    93,    94,    95,
      96,    97,    18,    30,    31,   113,    19,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   137,   156,   166,    35,   138,   157,
     167,    16,    84,    85,    86,    87,    24,   142,    36,    37,
      38,    39,    40,     6,    41,    42,    43,    44,     5,     6,
      45,    46,    93,    94,    95,    96,    97,   105,    26,   107,
     106,     7,   108,    47,   134,   135,    48,   155,    58,    27,
     158,    95,    96,    97,    28,   161,    29,   163,   164,    74,
      81,    82,   168,    83,    78,    84,    85,    86,    87,    88,
      89,    90,    91,    93,    94,    95,    96,    97,   -58,   -61,
     -64,   100,   101,   103,    92,    93,    94,    95,    96,    97,
      81,    82,   104,    83,   110,    84,    85,    86,    87,    88,
      89,    90,    91,   115,   139,   140,   141,   148,   146,   153,
     154,   159,   162,   149,    92,    93,    94,    95,    96,    97,
      81,    82,   170,    83,   169,    84,    85,    86,    87,    88,
      89,    90,    91,    62,     9,    72,   109,     0,     0,     0,
       0,     0,     0,     0,    92,    93,    94,    95,    96,    97,
      81,     0,     0,    83,     0,    84,    85,    86,    87,    88,
      89,    90,    91,    83,     0,    84,    85,    86,    87,    88,
      89,    90,    91,     0,     0,    93,    94,    95,    96,    97,
       0,     0,     0,     0,     0,    93,    94,    95,    96,    97
};

static const yytype_int16 yycheck[] =
{
      24,     3,    26,    27,   115,    35,    36,    18,    19,     0,
      21,    35,    23,    24,    25,    26,    27,    28,    29,    30,
      52,    45,    46,    47,    54,    21,    52,    23,    24,    25,
      26,    42,    43,    44,    45,    46,    47,   148,    53,     6,
       7,     8,    55,    10,    11,    60,    57,    43,    44,    45,
      46,    47,    35,    35,    36,    79,    39,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    40,    53,    40,    20,    44,    57,
      44,     4,    23,    24,    25,    26,     5,   111,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    35,    36,
      43,    44,    43,    44,    45,    46,    47,     6,    41,     6,
       9,    54,     9,    56,    98,    99,    59,   141,    13,    41,
     144,    45,    46,    47,    53,   149,    55,    15,    16,    56,
      18,    19,   156,    21,    60,    23,    24,    25,    26,    27,
      28,    29,    30,    43,    44,    45,    46,    47,    58,    58,
      58,    14,    17,     6,    42,    43,    44,    45,    46,    47,
      18,    19,     6,    21,    57,    23,    24,    25,    26,    27,
      28,    29,    30,    58,     6,     6,    12,    58,    35,    53,
      40,    35,    53,    41,    42,    43,    44,    45,    46,    47,
      18,    19,    40,    21,   162,    23,    24,    25,    26,    27,
      28,    29,    30,    28,     1,    34,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    42,    43,    44,    45,    46,    47,
      18,    -1,    -1,    21,    -1,    23,    24,    25,    26,    27,
      28,    29,    30,    21,    -1,    23,    24,    25,    26,    27,
      28,    29,    30,    -1,    -1,    43,    44,    45,    46,    47,
      -1,    -1,    -1,    -1,    -1,    43,    44,    45,    46,    47
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,    62,    63,    64,    35,    36,    54,    65,    76,
      83,     0,    52,    52,    55,    77,     4,    66,    35,    39,
      78,    79,    80,    67,     5,    69,    41,    41,    53,    55,
      35,    36,    68,    81,    82,    20,    31,    32,    33,    34,
      35,    37,    38,    39,    40,    43,    44,    56,    59,    76,
      88,    93,    94,    95,    96,    97,   100,   103,    13,    70,
      88,    88,    79,     6,     7,     8,    10,    11,    84,    85,
      86,    87,    83,    88,    56,    88,    88,    88,    60,   101,
      89,    18,    19,    21,    23,    24,    25,    26,    27,    28,
      29,    30,    42,    43,    44,    45,    46,    47,    90,    91,
      14,    17,    75,     6,     6,     6,     9,     6,     9,    81,
      57,    98,    57,    88,   102,    58,    92,    88,    88,    88,
      88,    88,    88,    88,    88,    88,    88,    88,    88,    88,
      88,    88,    88,    88,    92,    92,    71,    40,    44,     6,
       6,    12,    88,    99,    53,    60,    35,    96,    58,    41,
      72,    73,    88,    53,    40,    88,    53,    57,    88,    35,
      96,    88,    53,    15,    16,    74,    40,    44,    88,    73,
      40
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
      yyerror (&yylloc, context, YY_("syntax error: cannot back up")); \
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
		  Type, Value, Location, context); \
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
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, QL_parser_context_t *context)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    QL_parser_context_t *context;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
  YYUSE (context);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, QL_parser_context_t *context)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    QL_parser_context_t *context;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, context);
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
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, QL_parser_context_t *context)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule, context)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
    QL_parser_context_t *context;
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
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       , context);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule, context); \
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, QL_parser_context_t *context)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp, context)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
    QL_parser_context_t *context;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (context);

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
int yyparse (QL_parser_context_t *context);
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
yyparse (QL_parser_context_t *context)
#else
int
yyparse (context)
    QL_parser_context_t *context;
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
#line 125 "QL/parser.y"
    {
    ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 127 "QL/parser.y"
    {
    ;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 129 "QL/parser.y"
    {
    ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 131 "QL/parser.y"
    {
    ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 136 "QL/parser.y"
    { 
      context->_query->_type        = QLQueryTypeEmpty;
    ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 142 "QL/parser.y"
    {
      // full blown SELECT query
      context->_query->_type         = QLQueryTypeSelect;
      context->_query->_select._base = (yyvsp[(2) - (6)].node);
      context->_query->_from._base   = (yyvsp[(3) - (6)].node);
      context->_query->_where._base  = (yyvsp[(4) - (6)].node);
      context->_query->_order._base  = (yyvsp[(5) - (6)].node);
    ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 155 "QL/parser.y"
    {
      // select part of a SELECT
      (yyval.node) = (yyvsp[(1) - (1)].node);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 163 "QL/parser.y"
    {
      // from part of a SELECT
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 168 "QL/parser.y"
    {
      (yyval.node) = QLParseContextPop(context);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 175 "QL/parser.y"
    {
      // single table query
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      QLParseContextAddElement(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 180 "QL/parser.y"
    {
      // multi-table query
      ABORT_IF_OOM((yyvsp[(2) - (5)].node));
      ABORT_IF_OOM((yyvsp[(3) - (5)].node));
      ABORT_IF_OOM((yyvsp[(5) - (5)].node));
      (yyval.node) = (yyvsp[(2) - (5)].node);
      (yyval.node)->_lhs  = (yyvsp[(3) - (5)].node);
      (yyval.node)->_rhs  = (yyvsp[(5) - (5)].node);
      
      QLParseContextAddElement(context, (yyvsp[(2) - (5)].node));
    ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 194 "QL/parser.y"
    {
      (yyval.node) = 0;
    ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 197 "QL/parser.y"
    {
      // where condition set
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node) = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 205 "QL/parser.y"
    {
      (yyval.node) = 0;
    ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 208 "QL/parser.y"
    {
      // order by part of a query
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 213 "QL/parser.y"
    {
      (yyval.node) = QLParseContextPop(context);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 220 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      QLParseContextAddElement(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 224 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 231 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeContainerOrderElement);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (2)].node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (2)].node);
      (yyval.node)->_rhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 242 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueOrderDirection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = true;
    ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 247 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueOrderDirection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = true;
    ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 252 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueOrderDirection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = false;
    ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 260 "QL/parser.y"
    {
    ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 262 "QL/parser.y"
    {
      // limit value
      int64_t d = TRI_Int64String((yyvsp[(2) - (2)].strval));

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(2) - (2)].strval));
        YYABORT;
      }
      
      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = 0; 
      context->_query->_limit._count  = d; 
    ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 275 "QL/parser.y"
    {
      // limit - value
      int64_t d = TRI_Int64String((yyvsp[(3) - (3)].strval));

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(3) - (3)].strval));
        YYABORT;
      }
      
      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = 0; 
      context->_query->_limit._count  = -d; 
    ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 288 "QL/parser.y"
    { 
      // limit value, value
      int64_t d1, d2;
      
      d1 = TRI_Int64String((yyvsp[(2) - (4)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(2) - (4)].strval));
        YYABORT;
      }

      d2 = TRI_Int64String((yyvsp[(4) - (4)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(4) - (4)].strval));
        YYABORT;
      }

      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = d1; 
      context->_query->_limit._count  = d2;
    ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 308 "QL/parser.y"
    { 
      // limit value, -value
      int64_t d1, d2;
      
      d1 = TRI_Int64String((yyvsp[(2) - (5)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(2) - (5)].strval));
        YYABORT;
      }

      d2 = TRI_Int64String((yyvsp[(5) - (5)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, (yyvsp[(5) - (5)].strval));
        YYABORT;
      }

      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = d1; 
      context->_query->_limit._count  = -d2;
    ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 331 "QL/parser.y"
    {
      // document is a reference to a collection (by using its alias)
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node); 
    ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 336 "QL/parser.y"
    {
      // empty document
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueDocument);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 341 "QL/parser.y"
    {
      // listing of document attributes
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 346 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueDocument);
      ABORT_IF_OOM((yyval.node));
      QLPopIntoRhs((yyval.node), context);
    ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 354 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      QLParseContextAddElement(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 358 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 365 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 372 "QL/parser.y"
    {
      QL_ast_node_t* identifier = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(identifier);
      ABORT_IF_OOM((yyvsp[(1) - (3)].strval));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      identifier->_value._stringValue = (yyvsp[(1) - (3)].strval);

      (yyval.node) = QLAstNodeCreate(context, QLNodeValueNamedValue);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = identifier;
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 384 "QL/parser.y"
    {
      size_t outLength;
      QL_ast_node_t* str = QLAstNodeCreate(context, QLNodeValueString);
      ABORT_IF_OOM(str);
      ABORT_IF_OOM((yyvsp[(1) - (3)].strval));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      str->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String((yyvsp[(1) - (3)].strval) + 1, strlen((yyvsp[(1) - (3)].strval)) - 2, &outLength)); 

      (yyval.node) = QLAstNodeCreate(context, QLNodeValueNamedValue);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = str;
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 400 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (2)].node));
      ABORT_IF_OOM((yyvsp[(1) - (2)].node)->_value._stringValue);
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node)->_value._stringValue);

      if (!QLParseValidateCollectionName((yyvsp[(1) - (2)].node)->_value._stringValue)) {
        // validate collection name
        QLParseRegisterParseError(context, ERR_COLLECTION_NAME_INVALID, (yyvsp[(1) - (2)].node)->_value._stringValue);
        YYABORT;
      }

      if (!QLParseValidateCollectionAlias((yyvsp[(2) - (2)].node)->_value._stringValue)) {
        // validate alias
        QLParseRegisterParseError(context, ERR_COLLECTION_ALIAS_INVALID, (yyvsp[(2) - (2)].node)->_value._stringValue);
        YYABORT;
      }

      if (!QLAstQueryAddCollection(context->_query, (yyvsp[(1) - (2)].node)->_value._stringValue, (yyvsp[(2) - (2)].node)->_value._stringValue)) {
        QLParseRegisterParseError(context, ERR_COLLECTION_ALIAS_REDECLARED, (yyvsp[(2) - (2)].node)->_value._stringValue);
        YYABORT;
      }

      (yyval.node) = QLAstNodeCreate(context, QLNodeReferenceCollection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = (yyvsp[(1) - (2)].node);
      (yyval.node)->_rhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 432 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
    ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 438 "QL/parser.y"
    {
      size_t outLength;
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String((yyvsp[(1) - (1)].strval) + 1, strlen((yyvsp[(1) - (1)].strval)) - 2, &outLength)); 
    ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 448 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeReferenceCollectionAlias);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
    ;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 454 "QL/parser.y"
    {
      size_t outLength;
      (yyval.node) = QLAstNodeCreate(context, QLNodeReferenceCollectionAlias);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String((yyvsp[(1) - (1)].strval) + 1, strlen((yyvsp[(1) - (1)].strval)) - 2, &outLength)); 
    ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 464 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 468 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 472 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 479 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinList);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 486 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinInner);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 490 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinInner);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 497 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinLeft);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 501 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinLeft);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 505 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinRight);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 509 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinRight);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 516 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(2) - (3)].node));
      (yyval.node) = (yyvsp[(2) - (3)].node);
    ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 520 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 524 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 528 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 532 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 536 "QL/parser.y"
    { 
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 540 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      QLPopIntoRhs((yyval.node), context);
    ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 547 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 551 "QL/parser.y"
    {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 555 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      QLPopIntoRhs((yyval.node), context);
    ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 562 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);  
    ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 566 "QL/parser.y"
    {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 570 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      QLPopIntoRhs((yyval.node), context);
    ;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 577 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 585 "QL/parser.y"
    {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(2) - (2)].strval));
      name->_value._stringValue = (yyvsp[(2) - (2)].strval);
      QLParseContextAddElement(context, name);
    ;}
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 592 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      QLParseContextAddElement(context, (yyvsp[(2) - (2)].node));
    ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 596 "QL/parser.y"
    {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(3) - (3)].strval));
      name->_value._stringValue = (yyvsp[(3) - (3)].strval);
      QLParseContextAddElement(context, name);
    ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 603 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 611 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeUnaryOperatorPlus);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 617 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeUnaryOperatorMinus);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 623 "QL/parser.y"
    { 
      (yyval.node) = QLAstNodeCreate(context, QLNodeUnaryOperatorNot);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 632 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorOr);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 640 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorAnd);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node); 
    ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 648 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorAdd);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 656 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorSubtract);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 664 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorMultiply);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 672 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorDivide);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 680 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorModulus);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 688 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorIdentical);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 696 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorUnidentical);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 704 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorEqual);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 712 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorUnequal);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 720 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorLess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 728 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorGreater);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 736 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorLessEqual);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 744 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorGreaterEqual);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 752 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorIn);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 763 "QL/parser.y"
    {
      QL_ast_node_t* node = QLAstNodeCreate(context, QLNodeContainerTernarySwitch);
      ABORT_IF_OOM(node);
      ABORT_IF_OOM((yyvsp[(1) - (5)].node));
      ABORT_IF_OOM((yyvsp[(3) - (5)].node));
      ABORT_IF_OOM((yyvsp[(5) - (5)].node));
      node->_lhs = (yyvsp[(3) - (5)].node);
      node->_rhs = (yyvsp[(5) - (5)].node);

      (yyval.node) = QLAstNodeCreate(context, QLNodeControlTernary);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = (yyvsp[(1) - (5)].node);
      (yyval.node)->_rhs = node;
    ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 780 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 787 "QL/parser.y"
    {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(1) - (3)].strval));
      name->_value._stringValue = (yyvsp[(1) - (3)].strval);
      
      (yyval.node) = QLAstNodeCreate(context, QLNodeControlFunctionCall);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = name;
      (yyval.node)->_rhs = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM((yyval.node)->_rhs);
    ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 799 "QL/parser.y"
    {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 803 "QL/parser.y"
    {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(1) - (5)].strval));
      name->_value._stringValue = (yyvsp[(1) - (5)].strval);

      (yyval.node) = QLAstNodeCreate(context, QLNodeControlFunctionCall);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = name;
      QLPopIntoRhs((yyval.node), context);
    ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 817 "QL/parser.y"
    {
      QLParseContextAddElement(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 820 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 827 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueArray);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 831 "QL/parser.y"
    {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 835 "QL/parser.y"
    { 
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueArray);
      ABORT_IF_OOM((yyval.node));
      QLPopIntoRhs((yyval.node), context);
    ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 843 "QL/parser.y"
    {
      QLParseContextAddElement(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 846 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 853 "QL/parser.y"
    {
      size_t outLength;
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueString);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String((yyvsp[(1) - (1)].strval) + 1, strlen((yyvsp[(1) - (1)].strval)) - 2, &outLength)); 
    ;}
    break;

  case 103:

/* Line 1455 of yacc.c  */
#line 860 "QL/parser.y"
    {
      double d = TRI_DoubleString((yyvsp[(1) - (1)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        QLParseRegisterParseError(context, ERR_NUMBER_OUT_OF_RANGE, (yyvsp[(1) - (1)].strval));
        YYABORT;
      }
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueNumberDoubleString);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval); 
    ;}
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 871 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueNull);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 875 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueUndefined); 
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 879 "QL/parser.y"
    { 
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueBool);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = true;
    ;}
    break;

  case 107:

/* Line 1455 of yacc.c  */
#line 884 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueBool);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = false;
    ;}
    break;

  case 108:

/* Line 1455 of yacc.c  */
#line 889 "QL/parser.y"
    {
      // numbered parameter
      int64_t d = TRI_Int64String((yyvsp[(1) - (1)].strval));

      if (TRI_errno() != TRI_ERROR_NO_ERROR || d < 0 || d >= 256) {
        QLParseRegisterParseError(context, ERR_PARAMETER_NUMBER_OUT_OF_RANGE, (yyvsp[(1) - (1)].strval));
        YYABORT;
      }

      (yyval.node) = QLAstNodeCreate(context, QLNodeValueParameterNumeric);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._intValue = d;
    ;}
    break;

  case 109:

/* Line 1455 of yacc.c  */
#line 902 "QL/parser.y"
    {
      // named parameter
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueParameterNamed);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
    ;}
    break;



/* Line 1455 of yacc.c  */
#line 2998 "QL/parser.c"
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
      yyerror (&yylloc, context, YY_("syntax error"));
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
	    yyerror (&yylloc, context, yymsg);
	  }
	else
	  {
	    yyerror (&yylloc, context, YY_("syntax error"));
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
		      yytoken, &yylval, &yylloc, context);
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
		  yystos[yystate], yyvsp, yylsp, context);
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
  yyerror (&yylloc, context, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, &yylloc, context);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp, context);
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
#line 910 "QL/parser.y"



