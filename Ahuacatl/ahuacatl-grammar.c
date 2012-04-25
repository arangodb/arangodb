
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
#define yyparse         Ahuacatlparse
#define yylex           Ahuacatllex
#define yyerror         Ahuacatlerror
#define yylval          Ahuacatllval
#define yychar          Ahuacatlchar
#define yydebug         Ahuacatldebug
#define yynerrs         Ahuacatlnerrs
#define yylloc          Ahuacatllloc

/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 10 "Ahuacatl/ahuacatl-grammar.y"


#include <stdio.h>
#include <stdlib.h>

#include <BasicsC/common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/strings.h>

#include "Ahuacatl/ast-node.h"
#include "Ahuacatl/ahuacatl-parser.h"
#include "Ahuacatl/ahuacatl-error.h"



/* Line 189 of yacc.c  */
#line 97 "Ahuacatl/ahuacatl-grammar.c"

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
     T_END = 0,
     T_FOR = 258,
     T_LET = 259,
     T_FILTER = 260,
     T_RETURN = 261,
     T_COLLECT = 262,
     T_SORT = 263,
     T_LIMIT = 264,
     T_ASC = 265,
     T_DESC = 266,
     T_IN = 267,
     T_INTO = 268,
     T_NULL = 269,
     T_TRUE = 270,
     T_FALSE = 271,
     T_STRING = 272,
     T_QUOTED_STRING = 273,
     T_NUMBER = 274,
     T_PARAMETER = 275,
     T_ASSIGN = 276,
     T_NOT = 277,
     T_AND = 278,
     T_OR = 279,
     T_EQ = 280,
     T_NE = 281,
     T_LT = 282,
     T_GT = 283,
     T_LE = 284,
     T_GE = 285,
     T_PLUS = 286,
     T_MINUS = 287,
     T_TIMES = 288,
     T_DIV = 289,
     T_MOD = 290,
     T_QUESTION = 291,
     T_COLON = 292,
     T_COMMA = 293,
     T_OPEN = 294,
     T_CLOSE = 295,
     T_DOC_OPEN = 296,
     T_DOC_CLOSE = 297,
     T_LIST_OPEN = 298,
     T_LIST_CLOSE = 299,
     UPLUS = 300,
     UMINUS = 301,
     FUNCCALL = 302,
     REFERENCE = 303,
     INDEXED = 304
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 26 "Ahuacatl/ahuacatl-grammar.y"

  TRI_aql_node_t* node;
  char* strval;
  bool boolval;
  int64_t intval;



/* Line 214 of yacc.c  */
#line 192 "Ahuacatl/ahuacatl-grammar.c"
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
#line 33 "Ahuacatl/ahuacatl-grammar.y"


////////////////////////////////////////////////////////////////////////////////
/// @brief forward for lexer function defined in ahuacatl-tokens.l
////////////////////////////////////////////////////////////////////////////////

int Ahuacatllex (YYSTYPE*, YYLTYPE*, void*);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief register parse error
////////////////////////////////////////////////////////////////////////////////

void Ahuacatlerror (YYLTYPE* locp, TRI_aql_parse_context_t* const context, const char* err) {
  TRI_SetParseErrorAql(context, err, locp->first_line, locp->first_column);
}

#define scanner context->_parser->_scanner


/* Line 264 of yacc.c  */
#line 237 "Ahuacatl/ahuacatl-grammar.c"

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
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   367

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  52
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  46
/* YYNRULES -- Number of rules.  */
#define YYNRULES  101
/* YYNRULES -- Number of states.  */
#define YYNSTATES  164

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   304

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    51,    50,     2,     2,     2,
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
      45,    46,    47,    48,    49
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     4,     8,     9,    12,    14,    16,    18,
      20,    22,    24,    29,    32,    33,    41,    42,    47,    49,
      53,    57,    59,    60,    63,    64,    68,    70,    74,    77,
      78,    80,    82,    85,    90,    93,    97,   101,   103,   105,
     107,   108,   114,   116,   118,   120,   123,   126,   129,   133,
     137,   141,   145,   149,   153,   157,   161,   165,   169,   173,
     177,   181,   185,   191,   192,   194,   196,   200,   202,   204,
     205,   210,   211,   213,   215,   219,   220,   225,   226,   228,
     230,   234,   238,   240,   245,   252,   256,   258,   263,   267,
     269,   271,   273,   275,   277,   279,   281,   283,   285,   287,
     289,   291
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      53,     0,    -1,    -1,    54,    55,    72,    -1,    -1,    55,
      56,    -1,    57,    -1,    59,    -1,    58,    -1,    61,    -1,
      66,    -1,    71,    -1,     3,    96,    12,    73,    -1,     5,
      73,    -1,    -1,     4,    96,    60,    21,    39,    73,    40,
      -1,    -1,     7,    62,    63,    65,    -1,    64,    -1,    63,
      38,    64,    -1,    96,    21,    73,    -1,    73,    -1,    -1,
      13,    96,    -1,    -1,     8,    67,    68,    -1,    69,    -1,
      68,    38,    69,    -1,    73,    70,    -1,    -1,    10,    -1,
      11,    -1,     9,    97,    -1,     9,    97,    38,    97,    -1,
       6,    73,    -1,    39,    73,    40,    -1,    39,    53,    40,
      -1,    75,    -1,    76,    -1,    77,    -1,    -1,    17,    74,
      39,    78,    40,    -1,    80,    -1,    92,    -1,    90,    -1,
      31,    73,    -1,    32,    73,    -1,    22,    73,    -1,    73,
      24,    73,    -1,    73,    23,    73,    -1,    73,    31,    73,
      -1,    73,    32,    73,    -1,    73,    33,    73,    -1,    73,
      34,    73,    -1,    73,    35,    73,    -1,    73,    25,    73,
      -1,    73,    26,    73,    -1,    73,    27,    73,    -1,    73,
      28,    73,    -1,    73,    29,    73,    -1,    73,    30,    73,
      -1,    73,    12,    73,    -1,    73,    36,    73,    37,    73,
      -1,    -1,    79,    -1,    73,    -1,    79,    38,    73,    -1,
      81,    -1,    85,    -1,    -1,    43,    82,    83,    44,    -1,
      -1,    84,    -1,    73,    -1,    84,    38,    73,    -1,    -1,
      41,    86,    87,    42,    -1,    -1,    88,    -1,    89,    -1,
      88,    38,    89,    -1,    95,    37,    73,    -1,    17,    -1,
      90,    43,    73,    44,    -1,    90,    43,    33,    44,    50,
      91,    -1,    90,    50,    17,    -1,    17,    -1,    91,    43,
      73,    44,    -1,    91,    50,    17,    -1,    93,    -1,    94,
      -1,    18,    -1,    19,    -1,    14,    -1,    15,    -1,    16,
      -1,    20,    -1,    17,    -1,    18,    -1,    17,    -1,    19,
      -1,    51,    19,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   173,   173,   173,   189,   191,   196,   199,   202,   205,
     208,   211,   216,   227,   238,   238,   257,   257,   276,   278,
     283,   293,   301,   304,   310,   310,   330,   335,   343,   354,
     357,   360,   366,   374,   385,   397,   400,   408,   411,   414,
     417,   417,   439,   442,   445,   451,   459,   467,   478,   486,
     494,   502,   510,   518,   526,   534,   542,   550,   558,   566,
     574,   582,   593,   604,   606,   611,   614,   620,   623,   629,
     629,   642,   644,   649,   654,   662,   662,   675,   677,   682,
     684,   689,   697,   706,   713,   720,   730,   737,   744,   754,
     757,   763,   771,   785,   793,   801,   812,   823,   830,   839,
     845,   852
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of query string\"", "error", "$undefined", "\"FOR declaration\"",
  "\"LET declaration\"", "\"FILTER declaration\"",
  "\"RETURN declaration\"", "\"COLLECT declaration\"",
  "\"SORT declaration\"", "\"LIMIT declaration\"", "\"ASC keyword\"",
  "\"DESC keyword\"", "\"IN keyword\"", "\"INTO keyword\"", "\"null\"",
  "\"true\"", "\"false\"", "\"identifier\"", "\"quoted string\"",
  "\"number\"", "\"bind parameter\"", "\"assignment\"", "\"not operator\"",
  "\"and operator\"", "\"or operator\"", "\"== operator\"",
  "\"!= operator\"", "\"< operator\"", "\"> operator\"", "\"<= operator\"",
  "\">= operator\"", "\"+ operator\"", "\"- operator\"", "\"* operator\"",
  "\"/ operator\"", "\"% operator\"", "\"?\"", "\":\"", "\",\"", "\"(\"",
  "\")\"", "\"{\"", "\"}\"", "\"[\"", "\"]\"", "UPLUS", "UMINUS",
  "FUNCCALL", "REFERENCE", "INDEXED", "'.'", "'-'", "$accept", "query",
  "$@1", "optional_statement_block_statements",
  "statement_block_statement", "for_statement", "filter_statement",
  "let_statement", "$@2", "collect_statement", "$@3", "collect_list",
  "collect_element", "optional_into", "sort_statement", "$@4", "sort_list",
  "sort_element", "sort_direction", "limit_statement", "return_statement",
  "expression", "$@5", "operator_unary", "operator_binary",
  "operator_ternary", "optional_function_call_arguments",
  "function_arguments_list", "compound_type", "list", "$@6",
  "optional_list_elements", "list_elements_list", "array", "$@7",
  "optional_array_elements", "array_elements_list", "array_element",
  "reference", "expansion", "atomic_value", "value_literal",
  "bind_parameter", "array_element_name", "variable_name", "signed_number", 0
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
      46,    45
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    52,    54,    53,    55,    55,    56,    56,    56,    56,
      56,    56,    57,    58,    60,    59,    62,    61,    63,    63,
      64,    64,    65,    65,    67,    66,    68,    68,    69,    70,
      70,    70,    71,    71,    72,    73,    73,    73,    73,    73,
      74,    73,    73,    73,    73,    75,    75,    75,    76,    76,
      76,    76,    76,    76,    76,    76,    76,    76,    76,    76,
      76,    76,    77,    78,    78,    79,    79,    80,    80,    82,
      81,    83,    83,    84,    84,    86,    85,    87,    87,    88,
      88,    89,    90,    90,    90,    90,    91,    91,    91,    92,
      92,    93,    93,    93,    93,    93,    94,    95,    95,    96,
      97,    97
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     3,     0,     2,     1,     1,     1,     1,
       1,     1,     4,     2,     0,     7,     0,     4,     1,     3,
       3,     1,     0,     2,     0,     3,     1,     3,     2,     0,
       1,     1,     2,     4,     2,     3,     3,     1,     1,     1,
       0,     5,     1,     1,     1,     2,     2,     2,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     5,     0,     1,     1,     3,     1,     1,     0,
       4,     0,     1,     1,     3,     0,     4,     0,     1,     1,
       3,     3,     1,     4,     6,     3,     1,     4,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,     0,     4,     1,     0,     0,     0,     0,     0,    16,
      24,     0,     5,     6,     8,     7,     9,    10,    11,     3,
      99,     0,    14,    93,    94,    95,    82,    91,    92,    96,
       0,     0,     0,     2,    75,    69,    13,    37,    38,    39,
      42,    67,    68,    44,    43,    89,    90,    34,     0,     0,
     100,     0,    32,     0,     0,     0,    47,    45,    46,     0,
       0,    77,    71,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      82,    22,    18,    21,     0,    25,    26,    29,   101,     0,
      12,     0,    63,    36,    35,    97,    98,     0,    78,    79,
       0,    73,     0,    72,    61,    49,    48,    55,    56,    57,
      58,    59,    60,    50,    51,    52,    53,    54,     0,     0,
       0,    85,     0,     0,    17,     0,     0,    30,    31,    28,
      33,     0,    65,     0,    64,    76,     0,     0,    70,     0,
       0,     0,    83,    23,    19,    20,    27,     0,    41,     0,
      80,    81,    74,    62,     0,    15,    66,    86,    84,     0,
       0,     0,    88,    87
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     4,    12,    13,    14,    15,    54,    16,
      48,    81,    82,   124,    17,    49,    85,    86,   129,    18,
      19,    83,    55,    37,    38,    39,   133,   134,    40,    41,
      62,   102,   103,    42,    61,    97,    98,    99,    43,   158,
      44,    45,    46,   100,    84,    52
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -39
static const yytype_int16 yypact[] =
{
     -39,     7,   -39,   -39,    97,    -9,    -9,   129,   129,   -39,
     -39,   -10,   -39,   -39,   -39,   -39,   -39,   -39,   -39,   -39,
     -39,    -1,   -39,   -39,   -39,   -39,   -29,   -39,   -39,   -39,
     129,   129,   129,   129,   -39,   -39,   283,   -39,   -39,   -39,
     -39,   -39,   -39,   -38,   -39,   -39,   -39,   283,   159,   129,
     -39,     0,   -21,   129,     1,   -18,   -39,   -39,   -39,    14,
     180,    -3,   129,   129,   129,   129,   129,   129,   129,   129,
     129,   129,   129,   129,   129,   129,   129,   129,    95,    27,
     -19,     5,   -39,   283,    24,     9,   -39,   232,   -39,   -10,
     283,    69,   129,   -39,   -39,   -39,   -39,    74,    83,   -39,
      85,   283,    79,    87,    49,   320,   308,   332,   332,    18,
      18,    18,    18,    39,    39,   -39,   -39,   -39,   257,    91,
       4,   -39,    -9,   159,   -39,   129,   129,   -39,   -39,   -39,
     -39,   129,   283,    89,    93,   -39,    -3,   129,   -39,   129,
     129,    90,   -39,   -39,   -39,   283,   -39,   205,   -39,   129,
     -39,   283,   283,   283,   120,   -39,   283,   -39,   -37,   129,
     122,    63,   -39,   -39
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -39,   108,   -39,   -39,   -39,   -39,   -39,   -39,   -39,   -39,
     -39,   -39,    30,   -39,   -39,   -39,   -39,    28,   -39,   -39,
     -39,    -7,   -39,   -39,   -39,   -39,   -39,   -39,   -39,   -39,
     -39,   -39,   -39,   -39,   -39,   -39,   -39,    19,   -39,   -39,
     -39,   -39,   -39,   -39,    -2,    61
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -100
static const yytype_int16 yytable[] =
{
      36,    47,   -99,    21,    22,    78,   159,     3,    20,    50,
     -40,    53,    79,   160,    95,    96,    63,    89,   122,    88,
     -40,    92,    91,    56,    57,    58,    60,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    51,    87,   123,   121,   125,    90,   126,   142,    72,
      73,    74,    75,    76,    93,   101,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   120,    74,    75,    76,    63,    68,    69,    70,    71,
      72,    73,    74,    75,    76,   132,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
       5,     6,     7,     8,     9,    10,    11,   163,   131,    23,
      24,    25,    26,    27,    28,    29,   135,    30,   145,    87,
     143,   136,   137,   138,   147,   139,    31,    32,   119,   148,
     151,   149,   152,   153,    33,   141,    34,   157,    35,   162,
     154,    59,   156,    23,    24,    25,    26,    27,    28,    29,
     130,    30,   161,   144,   146,   150,     0,     0,     0,     0,
      31,    32,     0,     0,     0,     0,     0,     0,    33,     0,
      34,     0,    35,    23,    24,    25,    80,    27,    28,    29,
       0,    30,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,    63,     0,     0,     0,     0,     0,    33,     0,
      34,     0,    35,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    63,     0,     0,
      94,     0,     0,     0,     0,     0,     0,     0,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,   127,   128,    63,   155,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    63,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,   140,    63,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      63,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    64,    63,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    63,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,     0,     0,     0,    68,
      69,    70,    71,    72,    73,    74,    75,    76
};

static const yytype_int16 yycheck[] =
{
       7,     8,    21,     5,     6,    43,    43,     0,    17,    19,
      39,    12,    50,    50,    17,    18,    12,    38,    13,    19,
      39,    39,    21,    30,    31,    32,    33,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    51,    49,    38,    17,    21,    53,    38,    44,    31,
      32,    33,    34,    35,    40,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    33,    34,    35,    12,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    92,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
       3,     4,     5,     6,     7,     8,     9,    44,    39,    14,
      15,    16,    17,    18,    19,    20,    42,    22,   125,   126,
     122,    38,    37,    44,   131,    38,    31,    32,    33,    40,
     137,    38,   139,   140,    39,    44,    41,    17,    43,    17,
      50,    33,   149,    14,    15,    16,    17,    18,    19,    20,
      89,    22,   159,   123,   126,   136,    -1,    -1,    -1,    -1,
      31,    32,    -1,    -1,    -1,    -1,    -1,    -1,    39,    -1,
      41,    -1,    43,    14,    15,    16,    17,    18,    19,    20,
      -1,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      31,    32,    12,    -1,    -1,    -1,    -1,    -1,    39,    -1,
      41,    -1,    43,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    12,    -1,    -1,
      40,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    10,    11,    12,    40,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    12,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    23,    12,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    12,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    -1,    27,
      28,    29,    30,    31,    32,    33,    34,    35
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    53,    54,     0,    55,     3,     4,     5,     6,     7,
       8,     9,    56,    57,    58,    59,    61,    66,    71,    72,
      17,    96,    96,    14,    15,    16,    17,    18,    19,    20,
      22,    31,    32,    39,    41,    43,    73,    75,    76,    77,
      80,    81,    85,    90,    92,    93,    94,    73,    62,    67,
      19,    51,    97,    12,    60,    74,    73,    73,    73,    53,
      73,    86,    82,    12,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    43,    50,
      17,    63,    64,    73,    96,    68,    69,    73,    19,    38,
      73,    21,    39,    40,    40,    17,    18,    87,    88,    89,
      95,    73,    83,    84,    73,    73,    73,    73,    73,    73,
      73,    73,    73,    73,    73,    73,    73,    73,    73,    33,
      73,    17,    13,    38,    65,    21,    38,    10,    11,    70,
      97,    39,    73,    78,    79,    42,    38,    37,    44,    38,
      37,    44,    44,    96,    64,    73,    69,    73,    40,    38,
      89,    73,    73,    73,    50,    40,    73,    17,    91,    43,
      50,    73,    17,    44
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
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, TRI_aql_parse_context_t* const context)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    TRI_aql_parse_context_t* const context;
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, TRI_aql_parse_context_t* const context)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    TRI_aql_parse_context_t* const context;
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
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, TRI_aql_parse_context_t* const context)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule, context)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
    TRI_aql_parse_context_t* const context;
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, TRI_aql_parse_context_t* const context)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp, context)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
    TRI_aql_parse_context_t* const context;
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
int yyparse (TRI_aql_parse_context_t* const context);
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
yyparse (TRI_aql_parse_context_t* const context)
#else
int
yyparse (context)
    TRI_aql_parse_context_t* const context;
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
#line 173 "Ahuacatl/ahuacatl-grammar.y"
    {
      // a query or a sub-query always starts a new scope
      if (!TRI_StartScopeParseContextAql(context)) {
        YYABORT;
      }
    ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 178 "Ahuacatl/ahuacatl-grammar.y"
    {
      // end the scope
      TRI_AddStatementAql(context, (yyvsp[(3) - (3)].node));
      
      (yyval.node) = (TRI_aql_node_t*) TRI_GetFirstStatementAql(context);

      TRI_EndScopeParseContextAql(context);
    ;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 189 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 191 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 196 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_AddStatementAql(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 199 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_AddStatementAql(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 202 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_AddStatementAql(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 205 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_AddStatementAql(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 208 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_AddStatementAql(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 211 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_AddStatementAql(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 216 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeForAql(context, (yyvsp[(2) - (4)].strval), (yyvsp[(4) - (4)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 227 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeFilterAql(context, (yyvsp[(2) - (2)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 238 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!TRI_PushStackAql(context, (yyvsp[(2) - (2)].strval)) || !TRI_StartScopeParseContextAql(context)) {
        YYABORT;
      }
    ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 242 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      TRI_EndScopeParseContextAql(context);

      node = TRI_CreateNodeAssignAql(context, TRI_PopStackAql(context), (yyvsp[(6) - (7)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 257 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 265 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeCollectAql(context, TRI_PopStackAql(context), (yyvsp[(4) - (4)].strval));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 276 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 278 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 283 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeAssignAql(context, (yyvsp[(1) - (3)].strval), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      if (!TRI_PushListAql(context, node)) {
        YYABORT;
      }
    ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 293 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!TRI_PushListAql(context, (yyvsp[(1) - (1)].node))) {
        YYABORT;
      }
    ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 301 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.strval) = NULL;
    ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 304 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.strval) = (yyvsp[(2) - (2)].strval);
    ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 310 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 318 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* list = TRI_PopStackAql(context);
      TRI_aql_node_t* node = TRI_CreateNodeSortAql(context, list);
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 330 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!TRI_PushListAql(context, (yyvsp[(1) - (1)].node))) {
        YYABORT;
      }
    ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 335 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!TRI_PushListAql(context, (yyvsp[(3) - (3)].node))) {
        YYABORT;
      }
    ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 343 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeSortElementAql(context, (yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].boolval));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 354 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.boolval) = true;
    ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 357 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.boolval) = true;
    ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 360 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.boolval) = false;
    ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 366 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, 0, (yyvsp[(2) - (2)].intval));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 374 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, (yyvsp[(2) - (4)].intval), (yyvsp[(4) - (4)].intval));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 385 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeReturnAql(context, (yyvsp[(2) - (2)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 397 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(2) - (3)].node);
    ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 400 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeSubqueryAql(context, (yyvsp[(2) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 408 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 411 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 414 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 417 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      if (!TRI_PushStackAql(context, (yyvsp[(1) - (1)].strval))) {
        YYABORT;
      }

      node = TRI_CreateNodeListAql(context);
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 430 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* list = TRI_PopStackAql(context);
      TRI_aql_node_t* node = TRI_CreateNodeFcallAql(context, TRI_PopStackAql(context), list);
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 439 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 442 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 445 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 451 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryPlusAql(context, (yyvsp[(2) - (2)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 459 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryMinusAql(context, (yyvsp[(2) - (2)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 467 "Ahuacatl/ahuacatl-grammar.y"
    { 
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryNotAql(context, (yyvsp[(2) - (2)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 478 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryOrAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 486 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryAndAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 494 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryPlusAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 502 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryMinusAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 510 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryTimesAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 518 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryDivAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 526 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryModAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 534 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryEqAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 542 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryNeAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 550 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLtAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 558 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGtAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 566 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLeAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 574 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGeAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 582 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryInAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 593 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorTernaryAql(context, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].node), (yyvsp[(5) - (5)].node));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 604 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 606 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 611 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_PushListAql(context, (yyvsp[(1) - (1)].node));
    ;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 614 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_PushListAql(context, (yyvsp[(3) - (3)].node));
    ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 620 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 623 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 629 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 636 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = TRI_PopStackAql(context);
    ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 642 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 644 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 649 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!TRI_PushListAql(context, (yyvsp[(1) - (1)].node))) {
        YYABORT;
      }
    ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 654 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!TRI_PushListAql(context, (yyvsp[(3) - (3)].node))) {
        YYABORT;
      }
    ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 662 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeArrayAql(context);
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 669 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = TRI_PopStackAql(context);
    ;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 675 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 677 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 682 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 684 "Ahuacatl/ahuacatl-grammar.y"
    {
    ;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 689 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!TRI_PushArrayAql(context, (yyvsp[(1) - (3)].strval), (yyvsp[(3) - (3)].node))) {
        YYABORT;
      }
    ;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 697 "Ahuacatl/ahuacatl-grammar.y"
    {
      // variable or collection
      TRI_aql_node_t* node = TRI_CreateNodeReferenceAql(context, (yyvsp[(1) - (1)].strval), !TRI_VariableExistsAql(context, (yyvsp[(1) - (1)].strval)));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 706 "Ahuacatl/ahuacatl-grammar.y"
    {
      // variable[]
      (yyval.node) = TRI_CreateNodeIndexedAql(context, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));
      if (!(yyval.node)) {
        YYABORT;
      }
    ;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 713 "Ahuacatl/ahuacatl-grammar.y"
    {
      // variable[*]
      (yyval.node) = TRI_CreateNodeExpandAql(context, (yyvsp[(1) - (6)].node), (yyvsp[(6) - (6)].node));
      if (!(yyval.node)) {
        YYABORT;
      }
    ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 720 "Ahuacatl/ahuacatl-grammar.y"
    {
      // variable.reference
      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].strval));
      if (!(yyval.node)) {
        YYABORT;
      }
    ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 730 "Ahuacatl/ahuacatl-grammar.y"
    {
      // reference
      (yyval.node) = TRI_CreateNodeAttributeAql(context, (yyvsp[(1) - (1)].strval));
      if (!(yyval.node)) {
        YYABORT;
      }
    ;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 737 "Ahuacatl/ahuacatl-grammar.y"
    {
      // variable[]
      (yyval.node) = TRI_CreateNodeIndexedAql(context, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));
      if (!(yyval.node)) {
        YYABORT;
      }
    ;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 744 "Ahuacatl/ahuacatl-grammar.y"
    {
      // variable.variable
      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].strval));
      if (!(yyval.node)) {
        YYABORT;
      }
    ;}
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 754 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 757 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 763 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueStringAql(context, (yyvsp[(1) - (1)].strval));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 771 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      if (!(yyvsp[(1) - (1)].strval)) {
        YYABORT;
      }
      
      node = TRI_CreateNodeValueDoubleAql(context, TRI_DoubleString((yyvsp[(1) - (1)].strval)));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 785 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueNullAql(context);
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 793 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, true);
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 801 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, false);
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 812 "Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeParameterAql(context, (yyvsp[(1) - (1)].strval));
      if (!node) {
        YYABORT;
      }

      (yyval.node) = node;
    ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 823 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!(yyvsp[(1) - (1)].strval)) {
        YYABORT;
      }

      (yyval.strval) = (yyvsp[(1) - (1)].strval);
    ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 830 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!(yyvsp[(1) - (1)].strval)) {
        YYABORT;
      }

      (yyval.strval) = (yyvsp[(1) - (1)].strval);
    ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 839 "Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.strval) = (yyvsp[(1) - (1)].strval);
    ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 845 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!(yyvsp[(1) - (1)].strval)) {
        YYABORT;
      }

      (yyval.intval) = TRI_Int64String((yyvsp[(1) - (1)].strval));
    ;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 852 "Ahuacatl/ahuacatl-grammar.y"
    {
      if (!(yyvsp[(2) - (2)].strval)) {
        YYABORT;
      }

      (yyval.intval) = - TRI_Int64String((yyvsp[(2) - (2)].strval));
    ;}
    break;



/* Line 1455 of yacc.c  */
#line 2869 "Ahuacatl/ahuacatl-grammar.c"
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



