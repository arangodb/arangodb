
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
     IDENTICAL = 286,
     UNIDENTICAL = 287,
     NULLX = 288,
     TRUE = 289,
     FALSE = 290,
     UNDEFINED = 291,
     IDENTIFIER = 292,
     QUOTED_IDENTIFIER = 293,
     PARAMETER = 294,
     PARAMETER_NAMED = 295,
     STRING = 296,
     REAL = 297,
     COLON = 298,
     TERNARY = 299,
     FCALL = 300,
     UPLUS = 301,
     UMINUS = 302,
     MEMBER = 303
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
#line 198 "QL/parser.c"
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
#line 237 "QL/parser.c"

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
#define YYLAST   295

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  63
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  44
/* YYNRULES -- Number of rules.  */
#define YYNRULES  112
/* YYNRULES -- Number of states.  */
#define YYNSTATES  201

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   303

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    49,     2,     2,
      55,    57,    47,    45,    56,    46,    60,    48,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    54,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    61,     2,    62,     2,     2,     2,     2,     2,     2,
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
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      50,    51,    52,    53
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     8,    10,    13,    14,    21,    23,
      24,    28,    31,    38,    39,    54,    69,    70,    73,    74,
      75,    80,    82,    86,    89,    90,    92,    94,    95,    98,
     102,   107,   113,   115,   118,   119,   124,   126,   130,   132,
     136,   140,   143,   145,   147,   149,   151,   153,   155,   157,
     160,   162,   165,   169,   172,   176,   179,   183,   185,   187,
     189,   191,   192,   196,   198,   199,   203,   205,   206,   210,
     212,   215,   218,   222,   226,   229,   232,   235,   239,   243,
     247,   251,   255,   259,   263,   267,   271,   275,   279,   283,
     287,   291,   295,   299,   305,   307,   311,   312,   318,   320,
     324,   327,   328,   333,   335,   339,   341,   343,   345,   347,
     349,   351,   353
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      64,     0,    -1,    66,    -1,    66,    54,    -1,    65,    -1,
      65,    54,    -1,    -1,     3,    67,    68,    72,    73,    78,
      -1,    79,    -1,    -1,     4,    69,    70,    -1,    84,    71,
      -1,    70,    87,    84,    71,    12,    91,    -1,    -1,    17,
      86,    24,    55,    95,    56,    95,    56,    42,    56,    42,
      56,    42,    57,    -1,    18,    86,    24,    55,    95,    56,
      95,    56,    42,    56,    42,    56,    42,    57,    -1,    -1,
       5,    91,    -1,    -1,    -1,    13,    14,    74,    75,    -1,
      76,    -1,    75,    56,    76,    -1,    91,    77,    -1,    -1,
      15,    -1,    16,    -1,    -1,    19,    42,    -1,    19,    46,
      42,    -1,    19,    42,    56,    42,    -1,    19,    42,    56,
      46,    42,    -1,    86,    -1,    58,    59,    -1,    -1,    58,
      80,    81,    59,    -1,    82,    -1,    81,    56,    82,    -1,
      83,    -1,    37,    43,    91,    -1,    41,    43,    91,    -1,
      85,    86,    -1,    37,    -1,    38,    -1,    37,    -1,    38,
      -1,    88,    -1,    89,    -1,    90,    -1,     7,     6,    -1,
       6,    -1,     8,     6,    -1,    10,     9,     6,    -1,    10,
       6,    -1,    11,     9,     6,    -1,    11,     6,    -1,    55,
      91,    57,    -1,    96,    -1,    97,    -1,    98,    -1,    99,
      -1,    -1,    79,    92,    95,    -1,    79,    -1,    -1,   103,
      93,    95,    -1,   103,    -1,    -1,   106,    94,    95,    -1,
     106,    -1,    60,    37,    -1,    60,    99,    -1,    95,    60,
      37,    -1,    95,    60,    99,    -1,    45,    91,    -1,    46,
      91,    -1,    22,    91,    -1,    91,    21,    91,    -1,    91,
      20,    91,    -1,    91,    45,    91,    -1,    91,    46,    91,
      -1,    91,    47,    91,    -1,    91,    48,    91,    -1,    91,
      49,    91,    -1,    91,    31,    91,    -1,    91,    32,    91,
      -1,    91,    29,    91,    -1,    91,    30,    91,    -1,    91,
      26,    91,    -1,    91,    25,    91,    -1,    91,    28,    91,
      -1,    91,    27,    91,    -1,    91,    23,    91,    -1,    91,
      44,    91,    43,    91,    -1,   100,    -1,    37,    55,    57,
      -1,    -1,    37,    55,   101,   102,    57,    -1,    91,    -1,
     102,    56,    91,    -1,    61,    62,    -1,    -1,    61,   104,
     105,    62,    -1,    91,    -1,   105,    56,    91,    -1,    41,
      -1,    42,    -1,    33,    -1,    36,    -1,    34,    -1,    35,
      -1,    39,    -1,    40,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   127,   127,   129,   131,   133,   138,   144,   157,   165,
     165,   177,   182,   196,   199,   201,   207,   210,   218,   221,
     221,   233,   237,   244,   255,   260,   265,   273,   275,   288,
     301,   321,   344,   349,   354,   354,   367,   371,   378,   385,
     398,   414,   446,   452,   462,   468,   478,   482,   486,   493,
     500,   504,   511,   515,   519,   523,   530,   534,   538,   542,
     546,   550,   550,   561,   565,   565,   576,   580,   580,   591,
     599,   606,   610,   617,   625,   631,   637,   646,   654,   662,
     670,   678,   686,   694,   702,   710,   718,   726,   734,   742,
     750,   758,   766,   777,   794,   801,   813,   813,   831,   834,
     841,   845,   845,   857,   860,   867,   874,   885,   889,   893,
     898,   903,   916
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
  "UNEQUAL", "IDENTICAL", "UNIDENTICAL", "NULLX", "TRUE", "FALSE",
  "UNDEFINED", "IDENTIFIER", "QUOTED_IDENTIFIER", "PARAMETER",
  "PARAMETER_NAMED", "STRING", "REAL", "COLON", "TERNARY", "'+'", "'-'",
  "'*'", "'/'", "'%'", "FCALL", "UPLUS", "UMINUS", "MEMBER", "';'", "'('",
  "','", "')'", "'{'", "'}'", "'.'", "'['", "']'", "$accept", "query",
  "empty_query", "select_query", "select_clause", "from_clause", "$@1",
  "from_list", "geo_expression", "where_clause", "order_clause", "$@2",
  "order_list", "order_element", "order_direction", "limit_clause",
  "document", "$@3", "attribute_list", "attribute", "named_attribute",
  "collection_reference", "collection_name", "collection_alias",
  "join_type", "list_join", "inner_join", "outer_join", "expression",
  "$@4", "$@5", "$@6", "object_access", "unary_operator",
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
     295,   296,   297,   298,   299,    43,    45,    42,    47,    37,
     300,   301,   302,   303,    59,    40,    44,    41,   123,   125,
      46,    91,    93
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    63,    64,    64,    64,    64,    65,    66,    67,    69,
      68,    70,    70,    71,    71,    71,    72,    72,    73,    74,
      73,    75,    75,    76,    77,    77,    77,    78,    78,    78,
      78,    78,    79,    79,    80,    79,    81,    81,    82,    83,
      83,    84,    85,    85,    86,    86,    87,    87,    87,    88,
      89,    89,    90,    90,    90,    90,    91,    91,    91,    91,
      91,    92,    91,    91,    93,    91,    91,    94,    91,    91,
      95,    95,    95,    95,    96,    96,    96,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    98,    99,   100,   101,   100,   102,   102,
     103,   104,   103,   105,   105,   106,   106,   106,   106,   106,
     106,   106,   106
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     1,     2,     0,     6,     1,     0,
       3,     2,     6,     0,    14,    14,     0,     2,     0,     0,
       4,     1,     3,     2,     0,     1,     1,     0,     2,     3,
       4,     5,     1,     2,     0,     4,     1,     3,     1,     3,
       3,     2,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     2,     3,     2,     3,     2,     3,     1,     1,     1,
       1,     0,     3,     1,     0,     3,     1,     0,     3,     1,
       2,     2,     3,     3,     2,     2,     2,     3,     3,     3,
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
       6,     0,     0,     4,     2,    44,    45,    34,     0,     8,
      32,     1,     5,     3,    33,     0,     9,    16,     0,     0,
       0,    36,    38,     0,     0,    18,     0,     0,     0,    35,
      42,    43,    10,    13,     0,     0,   107,   109,   110,   108,
      44,   111,   112,   105,   106,     0,     0,     0,   101,    63,
      17,    57,    58,    59,    60,    94,    66,    69,     0,    27,
      39,    40,    37,    50,     0,     0,     0,     0,     0,    46,
      47,    48,     0,     0,    11,    41,    76,    96,    74,    75,
       0,   100,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    19,     0,     7,    49,    51,    53,     0,
      55,     0,    13,     0,     0,    95,     0,    56,   103,     0,
       0,    62,    78,    77,    92,    89,    88,    91,    90,    86,
      87,    84,    85,     0,    79,    80,    81,    82,    83,    65,
      68,     0,    28,     0,    52,    54,     0,     0,     0,    98,
       0,     0,   102,    70,    71,     0,     0,    20,    21,    24,
       0,    29,     0,     0,     0,     0,    97,   104,    72,    73,
      93,     0,    25,    26,    23,    30,     0,    12,     0,     0,
      99,    22,    31,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    14,
      15
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,     4,     8,    17,    23,    32,    74,    25,
      59,   141,   157,   158,   174,   105,    49,    15,    20,    21,
      22,    33,    34,    10,    68,    69,    70,    71,   159,    83,
     101,   102,   121,    51,    52,    53,    54,    55,   116,   150,
      56,    82,   119,    57
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -115
static const yytype_int16 yypact[] =
{
       4,   -20,    14,   -12,     5,  -115,  -115,    22,    79,  -115,
    -115,  -115,  -115,  -115,  -115,   -32,  -115,   107,    75,    83,
      77,  -115,  -115,   -18,    -9,   104,    -9,    -9,   -32,  -115,
    -115,  -115,    37,    17,    13,    -9,  -115,  -115,  -115,  -115,
      76,  -115,  -115,  -115,  -115,    -9,    -9,    -9,    81,    70,
     179,  -115,  -115,  -115,  -115,  -115,    93,    94,   123,   140,
     179,   179,  -115,  -115,   154,   155,    87,   146,   -18,  -115,
    -115,  -115,    13,    13,  -115,  -115,  -115,   105,  -115,  -115,
      59,  -115,    -9,   111,    -9,    -9,    -9,    -9,    -9,    -9,
      -9,    -9,    -9,    -9,    -9,    -9,    -9,    -9,    -9,    -9,
      -9,   111,   111,  -115,   -34,  -115,  -115,  -115,  -115,   167,
    -115,   176,    17,   159,   160,  -115,    -9,  -115,   179,   -52,
     148,   126,   219,   209,    74,     8,     8,     8,     8,   246,
     246,   246,   246,   149,   109,   109,  -115,  -115,  -115,   126,
     126,    -9,   131,   147,  -115,  -115,   178,   133,   136,   179,
      38,    -9,  -115,    76,  -115,   164,    -9,   156,  -115,   119,
      67,  -115,    -9,   111,   111,    -9,  -115,   179,    76,  -115,
     179,    -9,  -115,  -115,  -115,  -115,   161,   179,    54,    55,
     179,  -115,  -115,   111,   111,    68,    69,   171,   172,   162,
     163,   173,   174,   165,   166,   175,   188,   186,   195,  -115,
    -115
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,   108,  -115,
    -115,  -115,  -115,    60,  -115,  -115,   232,  -115,  -115,   225,
    -115,   191,  -115,   -33,  -115,  -115,  -115,  -115,   -24,  -115,
    -115,  -115,   -86,  -115,  -115,  -115,  -114,  -115,  -115,  -115,
    -115,  -115,  -115,  -115
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -68
static const yytype_int16 yytable[] =
{
      50,    75,    60,    61,   151,    18,   154,     1,   142,    19,
     152,    76,   143,    35,    11,   139,   140,     5,     6,    30,
      31,    78,    79,    80,    36,    37,    38,    39,    40,     6,
      41,    42,    43,    44,    72,    73,    45,    46,     7,   113,
     114,   169,    12,    63,    64,    65,    47,    66,    67,     7,
       5,     6,    48,    96,    97,    98,    99,   100,   118,    13,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   178,   179,    84,
      85,    14,    86,    16,    87,    88,    89,    90,    91,    92,
      93,    94,   149,   108,   165,   166,   109,   185,   186,    87,
      88,    89,    90,    95,    96,    97,    98,    99,   100,   175,
     183,   184,    24,   176,   155,   155,   117,    58,    26,    96,
      97,    98,    99,   100,   187,   188,    27,   167,   155,   155,
     -61,    77,   170,    28,   172,   173,    29,   103,   177,    84,
      85,   180,    86,    81,    87,    88,    89,    90,    91,    92,
      93,    94,   110,   -64,   -67,   111,    98,    99,   100,   104,
     106,   107,   115,    95,    96,    97,    98,    99,   100,    84,
      85,   120,    86,   144,    87,    88,    89,    90,    91,    92,
      93,    94,   145,   147,   148,   153,   155,   160,   163,   161,
     162,   164,   156,    95,    96,    97,    98,    99,   100,    84,
      85,   168,    86,   182,    87,    88,    89,    90,    91,    92,
      93,    94,   171,   189,   190,   193,   194,   197,   191,   192,
     146,   195,   196,    95,    96,    97,    98,    99,   100,    84,
     198,   181,    86,     9,    87,    88,    89,    90,    91,    92,
      93,    94,    86,   199,    87,    88,    89,    90,    91,    92,
      93,    94,   200,    62,    96,    97,    98,    99,   100,   112,
       0,     0,     0,     0,    96,    97,    98,    99,   100,    86,
       0,    87,    88,    89,    90,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    96,    97,    98,    99,   100
};

static const yytype_int16 yycheck[] =
{
      24,    34,    26,    27,    56,    37,   120,     3,    42,    41,
      62,    35,    46,    22,     0,   101,   102,    37,    38,    37,
      38,    45,    46,    47,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    17,    18,    45,    46,    58,    72,
      73,   155,    54,     6,     7,     8,    55,    10,    11,    58,
      37,    38,    61,    45,    46,    47,    48,    49,    82,    54,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   163,   164,    20,
      21,    59,    23,     4,    25,    26,    27,    28,    29,    30,
      31,    32,   116,     6,    56,    57,     9,   183,   184,    25,
      26,    27,    28,    44,    45,    46,    47,    48,    49,    42,
      56,    56,     5,    46,    60,    60,    57,    13,    43,    45,
      46,    47,    48,    49,    56,    56,    43,   151,    60,    60,
      60,    55,   156,    56,    15,    16,    59,    14,   162,    20,
      21,   165,    23,    62,    25,    26,    27,    28,    29,    30,
      31,    32,     6,    60,    60,     9,    47,    48,    49,    19,
       6,     6,    57,    44,    45,    46,    47,    48,    49,    20,
      21,    60,    23,     6,    25,    26,    27,    28,    29,    30,
      31,    32,     6,    24,    24,    37,    60,    56,    55,    42,
      12,    55,    43,    44,    45,    46,    47,    48,    49,    20,
      21,    37,    23,    42,    25,    26,    27,    28,    29,    30,
      31,    32,    56,    42,    42,    42,    42,    42,    56,    56,
     112,    56,    56,    44,    45,    46,    47,    48,    49,    20,
      42,   171,    23,     1,    25,    26,    27,    28,    29,    30,
      31,    32,    23,    57,    25,    26,    27,    28,    29,    30,
      31,    32,    57,    28,    45,    46,    47,    48,    49,    68,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    23,
      -1,    25,    26,    27,    28,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    45,    46,    47,    48,    49
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,    64,    65,    66,    37,    38,    58,    67,    79,
      86,     0,    54,    54,    59,    80,     4,    68,    37,    41,
      81,    82,    83,    69,     5,    72,    43,    43,    56,    59,
      37,    38,    70,    84,    85,    22,    33,    34,    35,    36,
      37,    39,    40,    41,    42,    45,    46,    55,    61,    79,
      91,    96,    97,    98,    99,   100,   103,   106,    13,    73,
      91,    91,    82,     6,     7,     8,    10,    11,    87,    88,
      89,    90,    17,    18,    71,    86,    91,    55,    91,    91,
      91,    62,   104,    92,    20,    21,    23,    25,    26,    27,
      28,    29,    30,    31,    32,    44,    45,    46,    47,    48,
      49,    93,    94,    14,    19,    78,     6,     6,     6,     9,
       6,     9,    84,    86,    86,    57,   101,    57,    91,   105,
      60,    95,    91,    91,    91,    91,    91,    91,    91,    91,
      91,    91,    91,    91,    91,    91,    91,    91,    91,    95,
      95,    74,    42,    46,     6,     6,    71,    24,    24,    91,
     102,    56,    62,    37,    99,    60,    43,    75,    76,    91,
      56,    42,    12,    55,    55,    56,    57,    91,    37,    99,
      91,    56,    15,    16,    77,    42,    46,    91,    95,    95,
      91,    76,    42,    56,    56,    95,    95,    56,    56,    42,
      42,    56,    56,    42,    42,    56,    56,    42,    42,    57,
      57
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
#line 127 "QL/parser.y"
    {
    ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 129 "QL/parser.y"
    {
    ;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 131 "QL/parser.y"
    {
    ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 133 "QL/parser.y"
    {
    ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 138 "QL/parser.y"
    { 
      context->_query->_type        = QLQueryTypeEmpty;
    ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 144 "QL/parser.y"
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
#line 157 "QL/parser.y"
    {
      // select part of a SELECT
      (yyval.node) = (yyvsp[(1) - (1)].node);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 165 "QL/parser.y"
    {
      // from part of a SELECT
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 170 "QL/parser.y"
    {
      (yyval.node) = QLParseContextPop(context);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 177 "QL/parser.y"
    {
      // single table query
      ABORT_IF_OOM((yyvsp[(1) - (2)].node));
      QLParseContextAddElement(context, (yyvsp[(1) - (2)].node));
    ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 182 "QL/parser.y"
    {
      // multi-table query
      ABORT_IF_OOM((yyvsp[(2) - (6)].node));
      ABORT_IF_OOM((yyvsp[(3) - (6)].node));
      ABORT_IF_OOM((yyvsp[(6) - (6)].node));
      (yyval.node) = (yyvsp[(2) - (6)].node);
      (yyval.node)->_lhs  = (yyvsp[(3) - (6)].node);
      (yyval.node)->_rhs  = (yyvsp[(6) - (6)].node);
      
      QLParseContextAddElement(context, (yyvsp[(2) - (6)].node));
    ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 196 "QL/parser.y"
    {
      (yyval.node) = 0;
    ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 199 "QL/parser.y"
    {
    ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 201 "QL/parser.y"
    {
    ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 207 "QL/parser.y"
    {
      (yyval.node) = 0;
    ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 210 "QL/parser.y"
    {
      // where condition set
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node) = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 218 "QL/parser.y"
    {
      (yyval.node) = 0;
    ;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 221 "QL/parser.y"
    {
      // order by part of a query
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 226 "QL/parser.y"
    {
      (yyval.node) = QLParseContextPop(context);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 233 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      QLParseContextAddElement(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 237 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 244 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeContainerOrderElement);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (2)].node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (2)].node);
      (yyval.node)->_rhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 255 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueOrderDirection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = true;
    ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 260 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueOrderDirection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = true;
    ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 265 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueOrderDirection);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = false;
    ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 273 "QL/parser.y"
    {
    ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 275 "QL/parser.y"
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

  case 29:

/* Line 1455 of yacc.c  */
#line 288 "QL/parser.y"
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

  case 30:

/* Line 1455 of yacc.c  */
#line 301 "QL/parser.y"
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

  case 31:

/* Line 1455 of yacc.c  */
#line 321 "QL/parser.y"
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

  case 32:

/* Line 1455 of yacc.c  */
#line 344 "QL/parser.y"
    {
      // document is a reference to a collection (by using its alias)
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node); 
    ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 349 "QL/parser.y"
    {
      // empty document
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueDocument);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 354 "QL/parser.y"
    {
      // listing of document attributes
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 359 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueDocument);
      ABORT_IF_OOM((yyval.node));
      QLPopIntoRhs((yyval.node), context);
    ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 367 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      QLParseContextAddElement(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 371 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 378 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 385 "QL/parser.y"
    {
      size_t outLength;
      QL_ast_node_t* str = QLAstNodeCreate(context, QLNodeValueString);
      ABORT_IF_OOM(str);
      ABORT_IF_OOM((yyvsp[(1) - (3)].strval));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      str->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String((yyvsp[(1) - (3)].strval), strlen((yyvsp[(1) - (3)].strval)), &outLength)); 

      (yyval.node) = QLAstNodeCreate(context, QLNodeValueNamedValue);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_lhs = str;
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 398 "QL/parser.y"
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

  case 41:

/* Line 1455 of yacc.c  */
#line 414 "QL/parser.y"
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

  case 42:

/* Line 1455 of yacc.c  */
#line 446 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
    ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 452 "QL/parser.y"
    {
      size_t outLength;
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String((yyvsp[(1) - (1)].strval) + 1, strlen((yyvsp[(1) - (1)].strval)) - 2, &outLength)); 
    ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 462 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeReferenceCollectionAlias);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
    ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 468 "QL/parser.y"
    {
      size_t outLength;
      (yyval.node) = QLAstNodeCreate(context, QLNodeReferenceCollectionAlias);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String((yyvsp[(1) - (1)].strval) + 1, strlen((yyvsp[(1) - (1)].strval)) - 2, &outLength)); 
    ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 478 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 482 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 486 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 493 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinList);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 500 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinInner);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 504 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinInner);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 511 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinLeft);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 515 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinLeft);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 519 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinRight);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 523 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeJoinRight);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 530 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(2) - (3)].node));
      (yyval.node) = (yyvsp[(2) - (3)].node);
    ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 534 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 538 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 542 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 546 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 550 "QL/parser.y"
    { 
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 554 "QL/parser.y"
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
#line 561 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 565 "QL/parser.y"
    {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 569 "QL/parser.y"
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
#line 576 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);  
    ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 580 "QL/parser.y"
    {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 584 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      QLPopIntoRhs((yyval.node), context);
    ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 591 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 599 "QL/parser.y"
    {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(2) - (2)].strval));
      name->_value._stringValue = (yyvsp[(2) - (2)].strval);
      QLParseContextAddElement(context, name);
    ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 606 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      QLParseContextAddElement(context, (yyvsp[(2) - (2)].node));
    ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 610 "QL/parser.y"
    {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM((yyvsp[(3) - (3)].strval));
      name->_value._stringValue = (yyvsp[(3) - (3)].strval);
      QLParseContextAddElement(context, name);
    ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 617 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 625 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeUnaryOperatorPlus);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 631 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeUnaryOperatorMinus);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 637 "QL/parser.y"
    { 
      (yyval.node) = QLAstNodeCreate(context, QLNodeUnaryOperatorNot);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(2) - (2)].node));
      (yyval.node)->_lhs = (yyvsp[(2) - (2)].node);
    ;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 646 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorOr);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 654 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorAnd);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node); 
    ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 662 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorAdd);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 670 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorSubtract);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 678 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorMultiply);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 686 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorDivide);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 694 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorModulus);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 702 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorIdentical);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 710 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorUnidentical);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 718 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorEqual);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 726 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorUnequal);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 734 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorLess);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 742 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorGreater);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 750 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorLessEqual);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 758 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorGreaterEqual);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 766 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeBinaryOperatorIn);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (3)].node));
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      (yyval.node)->_lhs = (yyvsp[(1) - (3)].node);
      (yyval.node)->_rhs = (yyvsp[(3) - (3)].node);
    ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 777 "QL/parser.y"
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

  case 94:

/* Line 1455 of yacc.c  */
#line 794 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(1) - (1)].node));
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 801 "QL/parser.y"
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

  case 96:

/* Line 1455 of yacc.c  */
#line 813 "QL/parser.y"
    {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 817 "QL/parser.y"
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

  case 98:

/* Line 1455 of yacc.c  */
#line 831 "QL/parser.y"
    {
      QLParseContextAddElement(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 834 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 841 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueArray);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 845 "QL/parser.y"
    {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    ;}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 849 "QL/parser.y"
    { 
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueArray);
      ABORT_IF_OOM((yyval.node));
      QLPopIntoRhs((yyval.node), context);
    ;}
    break;

  case 103:

/* Line 1455 of yacc.c  */
#line 857 "QL/parser.y"
    {
      QLParseContextAddElement(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 860 "QL/parser.y"
    {
      ABORT_IF_OOM((yyvsp[(3) - (3)].node));
      QLParseContextAddElement(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 867 "QL/parser.y"
    {
      size_t outLength;
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueString);
      ABORT_IF_OOM((yyval.node));
      ABORT_IF_OOM((yyvsp[(1) - (1)].strval));
      (yyval.node)->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String((yyvsp[(1) - (1)].strval) + 1, strlen((yyvsp[(1) - (1)].strval)) - 2, &outLength)); 
    ;}
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 874 "QL/parser.y"
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

  case 107:

/* Line 1455 of yacc.c  */
#line 885 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueNull);
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 108:

/* Line 1455 of yacc.c  */
#line 889 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueUndefined); 
      ABORT_IF_OOM((yyval.node));
    ;}
    break;

  case 109:

/* Line 1455 of yacc.c  */
#line 893 "QL/parser.y"
    { 
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueBool);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = true;
    ;}
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 898 "QL/parser.y"
    {
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueBool);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._boolValue = false;
    ;}
    break;

  case 111:

/* Line 1455 of yacc.c  */
#line 903 "QL/parser.y"
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

  case 112:

/* Line 1455 of yacc.c  */
#line 916 "QL/parser.y"
    {
      // named parameter
      (yyval.node) = QLAstNodeCreate(context, QLNodeValueParameterNamed);
      ABORT_IF_OOM((yyval.node));
      (yyval.node)->_value._stringValue = (yyvsp[(1) - (1)].strval);
    ;}
    break;



/* Line 1455 of yacc.c  */
#line 3051 "QL/parser.c"
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
#line 924 "QL/parser.y"



