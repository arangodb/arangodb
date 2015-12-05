/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         Aqlparse
#define yylex           Aqllex
#define yyerror         Aqlerror
#define yydebug         Aqldebug
#define yynerrs         Aqlnerrs


/* Copy the first part of user declarations.  */
#line 9 "arangod/Aql/grammar.y" /* yacc.c:339  */

#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/Parser.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"

#line 80 "arangod/Aql/grammar.cpp" /* yacc.c:339  */

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
# define YYERROR_VERBOSE 1
#endif

/* In a future release of Bison, this section will be replaced
   by #include "grammar.hpp".  */
#ifndef YY_AQL_ARANGOD_AQL_GRAMMAR_HPP_INCLUDED
# define YY_AQL_ARANGOD_AQL_GRAMMAR_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int Aqldebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
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
    T_WITH = 268,
    T_INTO = 269,
    T_GRAPH = 270,
    T_DISTINCT = 271,
    T_REMOVE = 272,
    T_INSERT = 273,
    T_UPDATE = 274,
    T_REPLACE = 275,
    T_UPSERT = 276,
    T_NULL = 277,
    T_TRUE = 278,
    T_FALSE = 279,
    T_STRING = 280,
    T_QUOTED_STRING = 281,
    T_INTEGER = 282,
    T_DOUBLE = 283,
    T_PARAMETER = 284,
    T_ASSIGN = 285,
    T_NOT = 286,
    T_AND = 287,
    T_OR = 288,
    T_EQ = 289,
    T_NE = 290,
    T_LT = 291,
    T_GT = 292,
    T_LE = 293,
    T_GE = 294,
    T_PLUS = 295,
    T_MINUS = 296,
    T_TIMES = 297,
    T_DIV = 298,
    T_MOD = 299,
    T_QUESTION = 300,
    T_COLON = 301,
    T_SCOPE = 302,
    T_RANGE = 303,
    T_COMMA = 304,
    T_OPEN = 305,
    T_CLOSE = 306,
    T_OBJECT_OPEN = 307,
    T_OBJECT_CLOSE = 308,
    T_ARRAY_OPEN = 309,
    T_ARRAY_CLOSE = 310,
    T_OUTBOUND = 311,
    T_INBOUND = 312,
    T_ANY = 313,
    T_ALL = 314,
    T_NIN = 315,
    UMINUS = 316,
    UPLUS = 317,
    FUNCCALL = 318,
    REFERENCE = 319,
    INDEXED = 320,
    EXPANSION = 321
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 17 "arangod/Aql/grammar.y" /* yacc.c:355  */

  triagens::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 198 "arangod/Aql/grammar.cpp" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif



int Aqlparse (triagens::aql::Parser* parser);

#endif /* !YY_AQL_ARANGOD_AQL_GRAMMAR_HPP_INCLUDED  */

/* Copy the second part of user declarations.  */
#line 27 "arangod/Aql/grammar.y" /* yacc.c:358  */


using namespace triagens::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief forward for lexer function defined in Aql/tokens.ll
////////////////////////////////////////////////////////////////////////////////

int Aqllex (YYSTYPE*, 
            YYLTYPE*, 
            void*);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief register parse error
////////////////////////////////////////////////////////////////////////////////

void Aqlerror (YYLTYPE* locp, 
               triagens::aql::Parser* parser,
               char const* message) {
  parser->registerParseError(TRI_ERROR_QUERY_PARSE, message, locp->first_line, locp->first_column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro for signaling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM                                   \
  parser->registerError(TRI_ERROR_OUT_OF_MEMORY);   \
  YYABORT;

#define scanner parser->scanner()


#line 259 "arangod/Aql/grammar.cpp" /* yacc.c:358  */

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
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   999

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  68
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  81
/* YYNRULES -- Number of rules.  */
#define YYNRULES  183
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  304

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   321

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
       2,     2,     2,     2,     2,     2,    67,     2,     2,     2,
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
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   216,   216,   221,   223,   226,   229,   232,   235,   241,
     243,   248,   250,   252,   254,   256,   258,   260,   262,   264,
     266,   268,   273,   279,   284,   289,   297,   305,   310,   312,
     317,   324,   334,   334,   348,   364,   391,   418,   450,   480,
     482,   487,   494,   498,   505,   519,   536,   536,   550,   550,
     561,   564,   570,   576,   579,   582,   585,   591,   596,   603,
     611,   614,   620,   630,   640,   648,   659,   664,   672,   683,
     688,   691,   697,   697,   748,   748,   758,   764,   767,   770,
     773,   776,   779,   785,   788,   804,   804,   816,   819,   822,
     828,   831,   834,   837,   840,   843,   846,   849,   852,   855,
     858,   861,   864,   867,   870,   876,   882,   884,   889,   892,
     892,   908,   911,   917,   920,   926,   926,   935,   937,   942,
     945,   951,   954,   968,   968,   977,   979,   984,   986,   991,
    1005,  1009,  1018,  1025,  1028,  1034,  1037,  1043,  1046,  1049,
    1055,  1058,  1064,  1067,  1074,  1078,  1085,  1091,  1090,  1099,
    1103,  1112,  1115,  1118,  1124,  1127,  1133,  1165,  1168,  1171,
    1178,  1188,  1188,  1201,  1216,  1230,  1244,  1244,  1287,  1290,
    1296,  1303,  1313,  1316,  1319,  1322,  1325,  1331,  1334,  1337,
    1347,  1353,  1356,  1361
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of query string\"", "error", "$undefined", "\"FOR declaration\"",
  "\"LET declaration\"", "\"FILTER declaration\"",
  "\"RETURN declaration\"", "\"COLLECT declaration\"",
  "\"SORT declaration\"", "\"LIMIT declaration\"", "\"ASC keyword\"",
  "\"DESC keyword\"", "\"IN keyword\"", "\"WITH keyword\"",
  "\"INTO keyword\"", "\"GRAPH keyword\"", "\"DISTINCT modifier\"",
  "\"REMOVE command\"", "\"INSERT command\"", "\"UPDATE command\"",
  "\"REPLACE command\"", "\"UPSERT command\"", "\"null\"", "\"true\"",
  "\"false\"", "\"identifier\"", "\"quoted string\"", "\"integer number\"",
  "\"number\"", "\"bind parameter\"", "\"assignment\"", "\"not operator\"",
  "\"and operator\"", "\"or operator\"", "\"== operator\"",
  "\"!= operator\"", "\"< operator\"", "\"> operator\"", "\"<= operator\"",
  "\">= operator\"", "\"+ operator\"", "\"- operator\"", "\"* operator\"",
  "\"/ operator\"", "\"% operator\"", "\"?\"", "\":\"", "\"::\"", "\"..\"",
  "\",\"", "\"(\"", "\")\"", "\"{\"", "\"}\"", "\"[\"", "\"]\"",
  "\"outbound direction\"", "\"inbound direction\"", "\"any direction\"",
  "\"all modifier\"", "T_NIN", "UMINUS", "UPLUS", "FUNCCALL", "REFERENCE",
  "INDEXED", "EXPANSION", "'.'", "$accept", "query", "final_statement",
  "optional_statement_block_statements", "statement_block_statement",
  "for_statement", "filter_statement", "let_statement", "let_list",
  "let_element", "count_into", "collect_variable_list", "$@1",
  "collect_statement", "collect_list", "collect_element", "optional_into",
  "variable_list", "keep", "$@2", "sort_statement", "$@3", "sort_list",
  "sort_element", "sort_direction", "limit_statement", "return_statement",
  "in_or_into_collection", "remove_statement", "insert_statement",
  "update_parameters", "update_statement", "replace_parameters",
  "replace_statement", "update_or_replace", "upsert_statement", "$@4",
  "distinct_expression", "$@5", "expression", "function_name",
  "function_call", "$@6", "operator_unary", "operator_binary",
  "operator_ternary", "optional_function_call_arguments",
  "expression_or_query", "$@7", "function_arguments_list",
  "compound_value", "array", "$@8", "optional_array_elements",
  "array_elements_list", "options", "object", "$@9",
  "optional_object_elements", "object_elements_list", "object_element",
  "array_filter_operator", "optional_array_filter", "optional_array_limit",
  "optional_array_return", "graph_collection", "graph_collection_list",
  "graph_subject", "$@10", "graph_direction", "graph_direction_steps",
  "reference", "$@11", "$@12", "simple_value", "numeric_value",
  "value_literal", "collection_name", "bind_parameter",
  "object_element_name", "variable_name", YY_NULLPTR
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
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,    46
};
# endif

#define YYPACT_NINF -264

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-264)))

#define YYTABLE_NINF -182

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -264,    83,   978,  -264,    -2,    -2,   919,   463,    52,  -264,
     214,   919,   919,   919,   919,  -264,  -264,  -264,  -264,  -264,
    -264,    40,  -264,  -264,  -264,  -264,    12,    17,    35,    59,
      70,  -264,     7,    69,  -264,    75,  -264,  -264,  -264,    65,
    -264,  -264,  -264,  -264,   919,   919,   919,   919,  -264,  -264,
     751,    42,  -264,  -264,  -264,  -264,  -264,  -264,  -264,   -41,
    -264,  -264,  -264,  -264,  -264,   751,    89,    97,    -2,   919,
      91,  -264,  -264,   665,   665,  -264,   559,  -264,   596,   919,
      -2,    97,   118,   778,    -2,    -2,   919,   114,   114,   114,
     391,  -264,    -5,   919,   919,   134,   919,   919,   919,   919,
     919,   919,   919,   919,   919,   919,   919,   919,   919,   919,
     919,   130,   106,   853,    23,   919,   145,   108,  -264,   123,
    -264,   155,   137,  -264,   433,   214,   939,    53,    97,    97,
     919,    97,   919,    97,   699,   173,  -264,   108,    97,  -264,
    -264,  -264,  -264,   289,  -264,   919,    15,  -264,   751,  -264,
     159,   166,  -264,   167,   919,   162,   168,  -264,   170,   751,
     164,   175,   410,   919,   826,   808,   502,   502,    94,    94,
      94,    94,   138,   138,   114,   114,   114,   156,   110,  -264,
     886,  -264,   323,   183,  -264,  -264,   751,    -2,  -264,    -2,
     919,   919,  -264,  -264,  -264,  -264,  -264,    25,   158,   291,
    -264,  -264,  -264,  -264,  -264,  -264,  -264,   665,  -264,   665,
    -264,   919,   919,    -2,  -264,  -264,   522,   778,    -2,  -264,
     919,   357,  -264,    -5,   919,  -264,   919,   410,   919,   751,
     176,  -264,  -264,   177,  -264,  -264,   223,  -264,  -264,   751,
    -264,    97,    97,   630,   733,   180,  -264,   102,  -264,   182,
    -264,  -264,   289,   919,   221,   751,   188,  -264,   751,   751,
     751,  -264,  -264,   919,   919,   226,  -264,  -264,  -264,  -264,
     919,  -264,    -2,  -264,  -264,  -264,   522,   778,   919,  -264,
     751,   919,   238,   665,  -264,    56,  -264,   919,   751,   487,
     919,   190,    97,  -264,   197,   522,   919,   751,  -264,  -264,
      56,  -264,   751,  -264
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       9,     0,     0,     1,     0,     0,     0,     0,    32,    48,
       0,     0,     0,     0,     0,    72,     2,    10,    11,    13,
      12,    42,    14,    15,    16,     3,    17,    18,    19,    20,
      21,   183,     0,    27,    28,     0,   174,   175,   176,   156,
     172,   170,   171,   180,     0,     0,     0,   161,   123,   115,
      26,    85,   159,    77,    78,    79,   157,   113,   114,    81,
     173,    80,   158,    74,    59,    76,     0,   121,     0,     0,
      57,   168,   169,     0,     0,    66,     0,    69,     0,     0,
       0,   121,   121,     0,     0,     0,     0,    89,    87,    88,
       0,     9,   125,   117,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    34,    33,
      39,     0,    49,    50,    53,     0,     0,     0,   121,   121,
       0,   121,     0,   121,     0,    43,    35,    46,   121,    36,
     151,   152,   153,    22,   154,     0,     0,    29,    30,   160,
       0,   129,   182,     0,     0,     0,   126,   127,     0,   119,
       0,   118,   103,     0,    91,    90,    97,    98,    99,   100,
     101,   102,    92,    93,    94,    95,    96,     0,    82,    84,
     109,   133,     0,   166,   163,   164,    75,     0,   122,     0,
       0,     0,    54,    55,    52,    56,    58,   156,   172,   180,
      60,   177,   178,   179,    61,    62,    63,     0,    64,     0,
      67,     0,     0,     0,    37,   155,     0,     0,     0,   162,
       0,     0,   124,     0,     0,   116,     0,   104,     0,   108,
       0,   111,     9,   107,   165,   134,   135,    31,    40,    41,
      51,   121,   121,     0,   121,    47,    44,     0,   142,   146,
      23,   143,     0,     0,     0,   131,     0,   128,   130,   120,
     105,    86,   110,   109,     0,   137,    65,    68,    70,    71,
       0,    38,     0,   150,   149,   147,     0,     0,     0,   112,
     136,     0,   140,     0,    45,     0,    24,     0,   132,   138,
       0,     0,   121,   144,   148,     0,     0,   141,   167,    73,
       0,    25,   139,   145
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -264,   -88,  -264,  -264,  -264,  -264,  -264,  -264,  -264,   163,
     228,  -264,  -264,  -264,  -264,    61,  -264,  -264,  -264,  -264,
    -264,  -264,  -264,    60,  -264,  -264,  -264,   -60,  -264,  -264,
    -264,  -264,  -264,  -264,  -264,  -264,  -264,  -264,  -264,    -6,
    -264,  -264,  -264,  -264,  -264,  -264,  -264,   -11,  -264,  -264,
    -264,  -264,  -264,  -264,  -264,   -71,   -66,  -264,  -264,  -264,
      30,  -264,  -264,  -264,  -264,  -263,  -264,  -240,  -264,  -139,
    -208,  -264,  -264,  -264,     8,  -264,     5,   127,    -8,  -264,
      43
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,    16,     2,    17,    18,    19,    20,    33,    34,
      67,    21,    68,    22,   119,   120,    82,   245,   138,   213,
      23,    69,   122,   123,   194,    24,    25,   128,    26,    27,
      75,    28,    77,    29,   270,    30,    79,    64,   115,   124,
      51,    52,   112,    53,    54,    55,   230,   231,   232,   233,
      56,    57,    93,   160,   161,   118,    58,    92,   155,   156,
     157,   183,   265,   282,   291,   249,   294,   250,   285,   144,
     145,    59,    91,   236,    70,    60,    61,   200,    62,   158,
      35
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      50,    65,    72,   150,   215,    73,    74,    76,    78,   253,
     136,   139,    -4,   113,   129,    71,   131,    -5,   133,    83,
     151,   152,   293,    31,   153,  -177,   114,   217,  -177,  -177,
    -177,  -177,  -177,  -177,  -177,    -6,   286,   303,    87,    88,
      89,    90,  -177,  -177,  -177,  -177,  -177,    32,   184,   154,
    -177,   188,    43,    66,    80,   301,    84,   205,   206,    -7,
     208,    -4,   210,    -4,   218,    66,    -5,   214,    -5,   287,
      -8,   188,   -83,   134,  -177,   -83,  -177,   143,   201,   202,
     148,   248,   203,     3,    -6,    43,    -6,   159,   162,   111,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,    86,   185,   182,    -7,   186,
      -7,   121,   -83,   215,   116,   -83,    72,    72,    85,    -8,
     162,    -8,   117,   135,   207,    95,   209,   146,   273,    71,
      71,    43,   195,   196,   104,   105,   106,   107,   108,   216,
     125,    95,   110,   137,   262,    95,   163,   241,   221,   242,
     104,   105,   106,   107,   108,   179,   180,   227,  -178,   187,
      48,  -178,  -178,  -178,  -178,  -178,  -178,  -178,    94,    95,
     266,   267,   189,   271,   229,  -178,  -178,  -178,  -178,  -178,
     106,   107,   108,  -178,   239,   190,   191,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   228,   212,   110,   243,   244,  -178,   251,  -178,
     219,   252,  -181,   220,   255,   222,   224,   223,   258,   225,
     259,   299,   260,   292,   226,   235,   263,   261,   264,   272,
     237,   275,   121,   277,   278,   281,    36,    37,    38,   274,
      40,    41,    42,    43,   290,   298,   300,   276,   147,    81,
     238,   240,   279,   257,   204,     0,   246,   229,   280,     0,
       0,   254,     0,     0,   283,     0,     0,     0,   251,     0,
       0,   252,   288,     0,     0,   289,     0,   251,     0,     0,
       0,   295,     0,     0,   297,     0,     0,   251,     0,     0,
     302,  -179,   251,     0,  -179,  -179,  -179,  -179,  -179,  -179,
    -179,    94,     0,     0,     0,     0,     0,     0,  -179,  -179,
    -179,  -179,  -179,     0,     0,   284,  -179,     0,     0,     0,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,    94,     0,   110,     0,     0,
    -179,     0,  -179,     0,     0,   140,   141,   142,     0,     0,
       0,     0,     0,     0,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,    94,
       0,   110,     0,     0,     0,     0,     0,     0,   234,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,    94,     0,   110,     0,     0,     0,     0,
       0,     0,   256,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,     0,     0,   110,
       0,    95,   149,   192,   193,    94,   100,   101,   102,   103,
     104,   105,   106,   107,   108,    36,    37,    38,   110,    40,
      41,    42,    43,     0,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,    63,
       0,   110,     0,     0,     0,    36,    37,    38,    39,    40,
      41,    42,    43,     0,    44,     0,     0,     0,     0,    94,
       0,     0,     0,    45,    46,     0,     0,     0,     0,     0,
       0,     0,     0,    47,    94,    48,     0,    49,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,    95,    94,   110,   296,   247,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   248,     0,     0,
     110,    43,     0,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,     0,     0,
     110,   126,   130,   127,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,     0,     0,   110,   126,   132,
     127,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,    94,     0,   110,     0,     0,     0,     0,   268,
     269,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,     0,   126,   110,   127,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,    94,     0,   110,     0,     0,     0,   211,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,    94,     0,   110,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   117,     0,
       0,     0,     0,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,     0,
       0,   110,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,     0,     0,   110,
      36,    37,    38,    39,    40,    41,    42,    43,     0,    44,
       0,     0,     0,     0,     0,     0,     0,     0,    45,    46,
      94,     0,     0,     0,     0,     0,     0,     0,    47,     0,
      48,     0,    49,     0,   140,   141,   142,     0,    94,    95,
      96,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,     0,     0,     0,   110,    95,     0,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,     0,     0,     0,   110,    36,    37,    38,    39,    40,
      41,    42,    43,     0,    44,     0,     0,     0,     0,     0,
       0,     0,     0,    45,    46,   181,     0,     0,     0,     0,
       0,     0,     0,    47,     0,    48,     0,    49,    36,    37,
      38,    39,    40,    41,    42,    43,     0,    44,     0,     0,
       0,     0,     0,     0,     0,     0,    45,    46,     0,     0,
       0,     0,     0,     0,     0,     0,    47,  -106,    48,     0,
      49,    36,    37,    38,    39,    40,    41,    42,    43,     0,
      44,     0,     0,     0,     0,     0,     0,     0,     0,    45,
      46,    36,    37,    38,   197,   198,    41,    42,   199,    47,
      44,    48,     0,    49,     0,     0,     0,     0,     0,    45,
      46,     4,     5,     6,     7,     8,     9,    10,     0,    47,
       0,    48,     0,    49,     0,    11,    12,    13,    14,    15
};

static const yytype_int16 yycheck[] =
{
       6,     7,    10,    91,   143,    11,    12,    13,    14,   217,
      81,    82,     0,    54,    74,    10,    76,     0,    78,    12,
      25,    26,   285,    25,    29,     0,    67,    12,     3,     4,
       5,     6,     7,     8,     9,     0,   276,   300,    44,    45,
      46,    47,    17,    18,    19,    20,    21,     4,    25,    54,
      25,   117,    29,    13,    14,   295,    49,   128,   129,     0,
     131,    49,   133,    51,    49,    13,    49,   138,    51,   277,
       0,   137,    47,    79,    49,    50,    51,    83,    25,    26,
      86,    25,    29,     0,    49,    29,    51,    93,    94,    47,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,    30,   114,   113,    49,   115,
      51,    68,    47,   252,    25,    50,   124,   125,    49,    49,
     126,    51,    25,    80,   130,    31,   132,    84,    26,   124,
     125,    29,   124,   125,    40,    41,    42,    43,    44,   145,
      49,    31,    48,    25,   232,    31,    12,   207,   154,   209,
      40,    41,    42,    43,    44,    25,    50,   163,     0,    14,
      52,     3,     4,     5,     6,     7,     8,     9,    12,    31,
     241,   242,    49,   244,   180,    17,    18,    19,    20,    21,
      42,    43,    44,    25,   190,    30,    49,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    30,    48,   211,   212,    49,   216,    51,
      51,   217,    46,    46,   220,    53,    46,    49,   224,    55,
     226,   292,   228,   283,    49,    42,    49,    51,     5,    49,
     187,    49,   189,    12,    46,     9,    22,    23,    24,   247,
      26,    27,    28,    29,     6,    55,    49,   253,    85,    21,
     189,   191,   263,   223,   127,    -1,   213,   263,   264,    -1,
      -1,   218,    -1,    -1,   270,    -1,    -1,    -1,   276,    -1,
      -1,   277,   278,    -1,    -1,   281,    -1,   285,    -1,    -1,
      -1,   287,    -1,    -1,   290,    -1,    -1,   295,    -1,    -1,
     296,     0,   300,    -1,     3,     4,     5,     6,     7,     8,
       9,    12,    -1,    -1,    -1,    -1,    -1,    -1,    17,    18,
      19,    20,    21,    -1,    -1,   272,    25,    -1,    -1,    -1,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    12,    -1,    48,    -1,    -1,
      49,    -1,    51,    -1,    -1,    56,    57,    58,    -1,    -1,
      -1,    -1,    -1,    -1,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    12,
      -1,    48,    -1,    -1,    -1,    -1,    -1,    -1,    55,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    12,    -1,    48,    -1,    -1,    -1,    -1,
      -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    -1,    -1,    48,
      -1,    31,    51,    10,    11,    12,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    22,    23,    24,    48,    26,
      27,    28,    29,    -1,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    16,
      -1,    48,    -1,    -1,    -1,    22,    23,    24,    25,    26,
      27,    28,    29,    -1,    31,    -1,    -1,    -1,    -1,    12,
      -1,    -1,    -1,    40,    41,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    50,    12,    52,    -1,    54,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    31,    12,    48,    49,    15,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    25,    -1,    -1,
      48,    29,    -1,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    -1,    -1,
      48,    12,    13,    14,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    -1,    -1,    48,    12,    13,
      14,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    12,    -1,    48,    -1,    -1,    -1,    -1,    19,
      20,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    -1,    12,    48,    14,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    12,    -1,    48,    -1,    -1,    -1,    18,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    12,    -1,    48,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,
      -1,    -1,    -1,    12,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    -1,
      -1,    48,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    -1,    -1,    48,
      22,    23,    24,    25,    26,    27,    28,    29,    -1,    31,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    40,    41,
      12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    50,    -1,
      52,    -1,    54,    -1,    56,    57,    58,    -1,    12,    31,
      32,    -1,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    -1,    -1,    -1,    48,    31,    -1,    -1,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    -1,    -1,    -1,    48,    22,    23,    24,    25,    26,
      27,    28,    29,    -1,    31,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    40,    41,    42,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    50,    -1,    52,    -1,    54,    22,    23,
      24,    25,    26,    27,    28,    29,    -1,    31,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    40,    41,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    50,    51,    52,    -1,
      54,    22,    23,    24,    25,    26,    27,    28,    29,    -1,
      31,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    40,
      41,    22,    23,    24,    25,    26,    27,    28,    29,    50,
      31,    52,    -1,    54,    -1,    -1,    -1,    -1,    -1,    40,
      41,     3,     4,     5,     6,     7,     8,     9,    -1,    50,
      -1,    52,    -1,    54,    -1,    17,    18,    19,    20,    21
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    69,    71,     0,     3,     4,     5,     6,     7,     8,
       9,    17,    18,    19,    20,    21,    70,    72,    73,    74,
      75,    79,    81,    88,    93,    94,    96,    97,    99,   101,
     103,    25,   148,    76,    77,   148,    22,    23,    24,    25,
      26,    27,    28,    29,    31,    40,    41,    50,    52,    54,
     107,   108,   109,   111,   112,   113,   118,   119,   124,   139,
     143,   144,   146,    16,   105,   107,    13,    78,    80,    89,
     142,   144,   146,   107,   107,    98,   107,   100,   107,   104,
      14,    78,    84,    12,    49,    49,    30,   107,   107,   107,
     107,   140,   125,   120,    12,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      48,    47,   110,    54,    67,   106,    25,    25,   123,    82,
      83,   148,    90,    91,   107,    49,    12,    14,    95,    95,
      13,    95,    13,    95,   107,   148,   123,    25,    86,   123,
      56,    57,    58,   107,   137,   138,   148,    77,   107,    51,
      69,    25,    26,    29,    54,   126,   127,   128,   147,   107,
     121,   122,   107,    12,   107,   107,   107,   107,   107,   107,
     107,   107,   107,   107,   107,   107,   107,   107,   107,    25,
      50,    42,   107,   129,    25,   146,   107,    14,   124,    49,
      30,    49,    10,    11,    92,   142,   142,    25,    26,    29,
     145,    25,    26,    29,   145,   123,   123,   107,   123,   107,
     123,    18,    30,    87,   123,   137,   107,    12,    49,    51,
      46,   107,    53,    49,    46,    55,    49,   107,    46,   107,
     114,   115,   116,   117,    55,    42,   141,   148,    83,   107,
      91,    95,    95,   107,   107,    85,   148,    15,    25,   133,
     135,   146,   107,   138,   148,   107,    55,   128,   107,   107,
     107,    51,    69,    49,     5,   130,   123,   123,    19,    20,
     102,   123,    49,    26,   146,    49,   107,    12,    46,   115,
     107,     9,   131,   107,   148,   136,   135,   138,   107,   107,
       6,   132,    95,   133,   134,   107,    49,   107,    55,   123,
      49,   135,   107,   133
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    68,    69,    70,    70,    70,    70,    70,    70,    71,
      71,    72,    72,    72,    72,    72,    72,    72,    72,    72,
      72,    72,    73,    73,    73,    73,    74,    75,    76,    76,
      77,    78,    80,    79,    81,    81,    81,    81,    81,    82,
      82,    83,    84,    84,    85,    85,    87,    86,    89,    88,
      90,    90,    91,    92,    92,    92,    92,    93,    93,    94,
      95,    95,    96,    97,    98,    98,    99,   100,   100,   101,
     102,   102,   104,   103,   106,   105,   105,   107,   107,   107,
     107,   107,   107,   108,   108,   110,   109,   111,   111,   111,
     112,   112,   112,   112,   112,   112,   112,   112,   112,   112,
     112,   112,   112,   112,   112,   113,   114,   114,   115,   116,
     115,   117,   117,   118,   118,   120,   119,   121,   121,   122,
     122,   123,   123,   125,   124,   126,   126,   127,   127,   128,
     128,   128,   128,   129,   129,   130,   130,   131,   131,   131,
     132,   132,   133,   133,   134,   134,   135,   136,   135,   135,
     135,   137,   137,   137,   138,   138,   139,   139,   139,   139,
     139,   140,   139,   139,   139,   139,   141,   139,   142,   142,
     143,   143,   144,   144,   144,   144,   144,   145,   145,   145,
     146,   147,   147,   148
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     1,     1,     1,     1,     1,     0,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     4,     6,     8,    10,     2,     2,     1,     3,
       3,     4,     0,     3,     3,     3,     3,     4,     6,     1,
       3,     3,     0,     2,     1,     3,     0,     3,     0,     3,
       1,     3,     2,     0,     1,     1,     1,     2,     4,     2,
       2,     2,     4,     4,     3,     5,     2,     3,     5,     2,
       1,     1,     0,     9,     0,     3,     1,     1,     1,     1,
       1,     1,     3,     1,     3,     0,     5,     2,     2,     2,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     4,     5,     0,     1,     1,     0,
       2,     1,     3,     1,     1,     0,     4,     0,     1,     1,
       3,     0,     2,     0,     4,     0,     1,     1,     3,     1,
       3,     3,     5,     1,     2,     0,     2,     0,     2,     4,
       0,     2,     1,     1,     1,     3,     1,     0,     4,     2,
       2,     1,     1,     1,     1,     2,     1,     1,     1,     1,
       3,     0,     4,     3,     3,     4,     0,     8,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1
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
      yyerror (&yylloc, parser, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


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


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static unsigned
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  unsigned res = 0;
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


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, Location, parser); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, triagens::aql::Parser* parser)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (yylocationp);
  YYUSE (parser);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, triagens::aql::Parser* parser)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, parser);
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, triagens::aql::Parser* parser)
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
                       , &(yylsp[(yyi + 1) - (yynrhs)])                       , parser);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, parser); \
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, triagens::aql::Parser* parser)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (parser);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (triagens::aql::Parser* parser)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.
       'yyls': related to locations.

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

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
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

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yylsp = yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  yylsp[0] = yylloc;
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
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, &yylloc, scanner);
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
     '$$ = $1'.

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
#line 216 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1837 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 3:
#line 221 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1844 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 4:
#line 223 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 1852 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 226 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 1860 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 229 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 1868 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 232 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 1876 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 8:
#line 235 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 1884 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 241 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1891 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 243 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1898 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 248 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1905 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 250 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1912 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 13:
#line 252 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1919 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 14:
#line 254 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1926 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 256 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1933 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 258 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1940 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 260 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1947 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 262 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1954 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 264 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1961 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 266 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1968 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 268 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1975 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 273 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
     
      auto node = parser->ast()->createNodeFor((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 1986 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 279 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 1996 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 284 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-6].strval).value, (yyvsp[-6].strval).length, (yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2006 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 289 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-8].strval).value, (yyvsp[-8].strval).length, (yyvsp[-6].strval).value, (yyvsp[-6].strval).length, (yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2016 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 297 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // operand is a reference. can use it directly
      auto node = parser->ast()->createNodeFilter((yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2026 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 305 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2033 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 310 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2040 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 312 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2047 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 317 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLet((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2056 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 324 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[-2].strval).value, "COUNT")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'COUNT'", (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2068 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 334 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2077 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 337 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    { 
      auto list = static_cast<AstNode*>(parser->popStack());

      if (list == nullptr) {
        ABORT_OOM
      }
      (yyval.node) = list;
    }
#line 2090 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 348 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);
      }

      auto node = parser->ast()->createNodeCollectCount(parser->ast()->createNodeArray(), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2111 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 364 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = (yyvsp[-2].node)->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = (yyvsp[-2].node)->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      auto node = parser->ast()->createNodeCollectCount((yyvsp[-2].node), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2143 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 391 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = (yyvsp[-2].node)->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = (yyvsp[-2].node)->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      auto node = parser->ast()->createNodeCollect((yyvsp[-2].node), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, nullptr, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2175 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 418 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = (yyvsp[-3].node)->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = (yyvsp[-3].node)->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      if ((yyvsp[-2].strval).value == nullptr && 
          (yyvsp[-1].node) != nullptr) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of 'KEEP' without 'INTO'", yylloc.first_line, yylloc.first_column);
      } 

      auto node = parser->ast()->createNodeCollect((yyvsp[-3].node), (yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2212 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 450 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = (yyvsp[-5].node)->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = (yyvsp[-5].node)->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      auto node = parser->ast()->createNodeCollectExpression((yyvsp[-5].node), (yyvsp[-3].strval).value, (yyvsp[-3].strval).length, (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2244 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 480 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2251 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 482 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2258 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 487 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeAssign((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
      parser->pushArrayElement(node);
    }
#line 2267 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 494 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval).value = nullptr;
      (yyval.strval).length = 0;
    }
#line 2276 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 498 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval).value = (yyvsp[0].strval).value;
      (yyval.strval).length = (yyvsp[0].strval).length;
    }
#line 2285 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 505 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->ast()->scopes()->existsVariable((yyvsp[0].strval).value, (yyvsp[0].strval).length)) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of unknown variable '%s' for KEEP", (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }
        
      auto node = parser->ast()->createNodeReference((yyvsp[0].strval).value, (yyvsp[0].strval).length);
      if (node == nullptr) {
        ABORT_OOM
      }

      // indicate the this node is a reference to the variable name, not the variable value
      node->setFlag(FLAG_KEEP_VARIABLENAME);
      parser->pushArrayElement(node);
    }
#line 2304 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 519 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->ast()->scopes()->existsVariable((yyvsp[0].strval).value, (yyvsp[0].strval).length)) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of unknown variable '%s' for KEEP", (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }
        
      auto node = parser->ast()->createNodeReference((yyvsp[0].strval).value, (yyvsp[0].strval).length);
      if (node == nullptr) {
        ABORT_OOM
      }

      // indicate the this node is a reference to the variable name, not the variable value
      node->setFlag(FLAG_KEEP_VARIABLENAME);
      parser->pushArrayElement(node);
    }
#line 2323 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 46:
#line 536 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[0].strval).value, "KEEP")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'KEEP'", (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2336 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 543 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2345 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 48:
#line 550 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2354 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 553 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      auto node = parser->ast()->createNodeSort(list);
      parser->ast()->addOperation(node);
    }
#line 2364 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 561 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2372 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 564 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2380 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 52:
#line 570 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeSortElement((yyvsp[-1].node), (yyvsp[0].node));
    }
#line 2388 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 576 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2396 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 579 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2404 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 582 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 2412 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 585 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2420 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 591 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto offset = parser->ast()->createNodeValueInt(0);
      auto node = parser->ast()->createNodeLimit(offset, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2430 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 596 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLimit((yyvsp[-2].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2439 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 603 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeReturn((yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2449 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 611 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2457 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 614 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
       (yyval.node) = (yyvsp[0].node);
     }
#line 2465 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 620 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2477 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 630 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2489 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 640 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2502 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 648 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2515 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 659 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2522 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 664 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2535 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 672 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2548 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 683 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2555 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 688 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_UPDATE);
    }
#line 2563 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 691 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_REPLACE);
    }
#line 2571 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 697 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    { 
      // reserve a variable named "$OLD", we might need it in the update expression
      // and in a later return thing
      parser->pushStack(parser->ast()->createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD), true));
    }
#line 2581 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 701 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* variableNode = static_cast<AstNode*>(parser->popStack());
      
      auto scopes = parser->ast()->scopes();
      
      scopes->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
      
      scopes->start(triagens::aql::AQL_SCOPE_FOR);
      std::string const variableName = std::move(parser->ast()->variables()->nextName());
      auto forNode = parser->ast()->createNodeFor(variableName.c_str(), variableName.size(), (yyvsp[-1].node), false);
      parser->ast()->addOperation(forNode);

      auto filterNode = parser->ast()->createNodeUpsertFilter(parser->ast()->createNodeReference(variableName), (yyvsp[-6].node));
      parser->ast()->addOperation(filterNode);
      
      auto offsetValue = parser->ast()->createNodeValueInt(0);
      auto limitValue = parser->ast()->createNodeValueInt(1);
      auto limitNode = parser->ast()->createNodeLimit(offsetValue, limitValue);
      parser->ast()->addOperation(limitNode);
      
      auto refNode = parser->ast()->createNodeReference(variableName);
      auto returnNode = parser->ast()->createNodeReturn(refNode);
      parser->ast()->addOperation(returnNode);
      scopes->endNested();

      AstNode* subqueryNode = parser->ast()->endSubQuery();
      scopes->endCurrent();
      
      std::string const subqueryName = std::move(parser->ast()->variables()->nextName());
      auto subQuery = parser->ast()->createNodeLet(subqueryName.c_str(), subqueryName.size(), subqueryNode, false);
      parser->ast()->addOperation(subQuery);
      
      auto index = parser->ast()->createNodeValueInt(0);
      auto firstDoc = parser->ast()->createNodeLet(variableNode, parser->ast()->createNodeIndexedAccess(parser->ast()->createNodeReference(subqueryName), index));
      parser->ast()->addOperation(firstDoc);

      auto node = parser->ast()->createNodeUpsert(static_cast<AstNodeType>((yyvsp[-3].intval)), parser->ast()->createNodeReference(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD)), (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2630 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 74:
#line 748 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto const scopeType = parser->ast()->scopes()->type();

      if (scopeType == AQL_SCOPE_MAIN ||
          scopeType == AQL_SCOPE_SUBQUERY) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "cannot use DISTINCT modifier on top-level query element", yylloc.first_line, yylloc.first_column);
      }
    }
#line 2643 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 75:
#line 755 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDistinct((yyvsp[0].node));
    }
#line 2651 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 76:
#line 758 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2659 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 77:
#line 764 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2667 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 767 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2675 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 770 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2683 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 773 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2691 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 776 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2699 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 82:
#line 779 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeRange((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2707 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 83:
#line 785 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2715 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 788 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      std::string temp((yyvsp[-2].strval).value, (yyvsp[-2].strval).length);
      temp.append("::");
      temp.append((yyvsp[0].strval).value, (yyvsp[0].strval).length);
      auto p = parser->query()->registerString(temp);

      if (p == nullptr) {
        ABORT_OOM
      }

      (yyval.strval).value = p;
      (yyval.strval).length = temp.size();
    }
#line 2733 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 85:
#line 804 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushStack((yyvsp[0].strval).value);

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2744 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 86:
#line 809 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall(static_cast<char const*>(parser->popStack()), list);
    }
#line 2753 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 87:
#line 816 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_PLUS, (yyvsp[0].node));
    }
#line 2761 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 88:
#line 819 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_MINUS, (yyvsp[0].node));
    }
#line 2769 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 89:
#line 822 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    { 
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, (yyvsp[0].node));
    }
#line 2777 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 90:
#line 828 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2785 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 91:
#line 831 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2793 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 92:
#line 834 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_PLUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2801 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 93:
#line 837 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MINUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2809 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 94:
#line 840 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_TIMES, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2817 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 95:
#line 843 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_DIV, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2825 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 96:
#line 846 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MOD, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2833 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 97:
#line 849 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2841 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 98:
#line 852 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2849 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 99:
#line 855 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2857 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 100:
#line 858 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2865 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 101:
#line 861 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2873 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 102:
#line 864 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2881 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 103:
#line 867 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2889 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 104:
#line 870 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, (yyvsp[-3].node), (yyvsp[0].node));
    }
#line 2897 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 105:
#line 876 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2905 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 106:
#line 882 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2912 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 107:
#line 884 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2919 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 108:
#line 889 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2927 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 109:
#line 892 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 2936 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 110:
#line 895 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = std::move(parser->ast()->variables()->nextName());
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 2951 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 111:
#line 908 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2959 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 112:
#line 911 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2967 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 113:
#line 917 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2975 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 114:
#line 920 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2983 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 115:
#line 926 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2992 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 116:
#line 929 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3000 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 117:
#line 935 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3007 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 118:
#line 937 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3014 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 119:
#line 942 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3022 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 120:
#line 945 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3030 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 121:
#line 951 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3038 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 122:
#line 954 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[-1].strval).value, "OPTIONS")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'OPTIONS'", (yyvsp[-1].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3054 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 123:
#line 968 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeObject();
      parser->pushStack(node);
    }
#line 3063 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 124:
#line 971 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3071 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 125:
#line 977 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3078 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 126:
#line 979 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3085 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 127:
#line 984 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3092 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 128:
#line 986 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3099 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 129:
#line 991 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // attribute-name-only (comparable to JS enhanced object literals, e.g. { foo, bar })
      auto ast = parser->ast();
      auto variable = ast->scopes()->getVariable((yyvsp[0].strval).value, (yyvsp[0].strval).length, true);
      
      if (variable == nullptr) {
        // variable does not exist
        parser->registerParseError(TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN, "use of unknown variable '%s' in object literal", (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      // create a reference to the variable
      auto node = ast->createNodeReference(variable);
      parser->pushObjectElement((yyvsp[0].strval).value, (yyvsp[0].strval).length, node);
    }
#line 3118 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 130:
#line 1005 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // attribute-name : attribute-value
      parser->pushObjectElement((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
    }
#line 3127 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 131:
#line 1009 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // bind-parameter : attribute-value
      if ((yyvsp[-2].strval).length < 1 || (yyvsp[-2].strval).value[0] == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto param = parser->ast()->createNodeParameter((yyvsp[-2].strval).value, (yyvsp[-2].strval).length);
      parser->pushObjectElement(param, (yyvsp[0].node));
    }
#line 3141 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 132:
#line 1018 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // [ attribute-name-expression ] : attribute-value
      parser->pushObjectElement((yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3150 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 133:
#line 1025 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3158 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 134:
#line 1028 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = (yyvsp[-1].intval) + 1;
    }
#line 3166 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 135:
#line 1034 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3174 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 136:
#line 1037 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3182 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 137:
#line 1043 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3190 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 138:
#line 1046 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit(nullptr, (yyvsp[0].node));
    }
#line 3198 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 139:
#line 1049 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3206 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 140:
#line 1055 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3214 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 141:
#line 1058 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3222 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 142:
#line 1064 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3230 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 143:
#line 1067 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // TODO FIXME check @s
      (yyval.node) = (yyvsp[0].node);
    }
#line 3239 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 144:
#line 1074 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3248 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 145:
#line 1078 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3257 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 146:
#line 1085 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3267 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 147:
#line 1091 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
      node->addMember((yyvsp[-1].node));
    }
#line 3277 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 148:
#line 1095 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3286 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 149:
#line 1099 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      (yyval.node) = (yyvsp[0].node);
    }
#line 3295 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 150:
#line 1103 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3304 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 151:
#line 1112 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 2;
    }
#line 3312 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 152:
#line 1115 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3320 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 153:
#line 1118 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 0; 
    }
#line 3328 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 154:
#line 1124 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), 1);
    }
#line 3336 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 155:
#line 1127 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), (yyvsp[-1].node));
    }
#line 3344 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 156:
#line 1133 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // variable or collection
      auto ast = parser->ast();
      AstNode* node = nullptr;

      auto variable = ast->scopes()->getVariable((yyvsp[0].strval).value, (yyvsp[0].strval).length, true);
      
      if (variable == nullptr) {
        // variable does not exist
        // now try special variables
        if (ast->scopes()->canUseCurrentVariable() && strcmp((yyvsp[0].strval).value, "CURRENT") == 0) {
          variable = ast->scopes()->getCurrentVariable();
        }
        else if (strcmp((yyvsp[0].strval).value, Variable::NAME_CURRENT) == 0) {
          variable = ast->scopes()->getCurrentVariable();
        }
      }
        
      if (variable != nullptr) {
        // variable alias exists, now use it
        node = ast->createNodeReference(variable);
      }

      if (node == nullptr) {
        // variable not found. so it must have been a collection
        node = ast->createNodeCollection((yyvsp[0].strval).value, TRI_TRANSACTION_READ);
      }

      TRI_ASSERT(node != nullptr);

      (yyval.node) = node;
    }
#line 3381 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 157:
#line 1165 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3389 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 158:
#line 1168 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3397 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 159:
#line 1171 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
      
      if ((yyval.node) == nullptr) {
        ABORT_OOM
      }
    }
#line 3409 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 160:
#line 1178 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[-1].node)->type == NODE_TYPE_EXPANSION) {
        // create a dummy passthru node that reduces and evaluates the expansion first
        // and the expansion on top of the stack won't be chained with any other expansions
        (yyval.node) = parser->ast()->createNodePassthru((yyvsp[-1].node));
      }
      else {
        (yyval.node) = (yyvsp[-1].node);
      }
    }
#line 3424 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 161:
#line 1188 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 3433 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 162:
#line 1191 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = std::move(parser->ast()->variables()->nextName());
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 3448 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 163:
#line 1201 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // named variable access, e.g. variable.reference
      if ((yyvsp[-2].node)->type == NODE_TYPE_EXPANSION) {
        // if left operand is an expansion already...
        // dive into the expansion's right-hand child nodes for further expansion and
        // patch the bottom-most one
        auto current = const_cast<AstNode*>(parser->ast()->findExpansionSubNode((yyvsp[-2].node)));
        TRI_ASSERT(current->type == NODE_TYPE_EXPANSION);
        current->changeMember(1, parser->ast()->createNodeAttributeAccess(current->getMember(1), (yyvsp[0].strval).value, (yyvsp[0].strval).length));
        (yyval.node) = (yyvsp[-2].node);
      }
      else {
        (yyval.node) = parser->ast()->createNodeAttributeAccess((yyvsp[-2].node), (yyvsp[0].strval).value, (yyvsp[0].strval).length);
      }
    }
#line 3468 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 164:
#line 1216 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // named variable access, e.g. variable.@reference
      if ((yyvsp[-2].node)->type == NODE_TYPE_EXPANSION) {
        // if left operand is an expansion already...
        // patch the existing expansion
        auto current = const_cast<AstNode*>(parser->ast()->findExpansionSubNode((yyvsp[-2].node)));
        TRI_ASSERT(current->type == NODE_TYPE_EXPANSION);
        current->changeMember(1, parser->ast()->createNodeBoundAttributeAccess(current->getMember(1), (yyvsp[0].node)));
        (yyval.node) = (yyvsp[-2].node);
      }
      else {
        (yyval.node) = parser->ast()->createNodeBoundAttributeAccess((yyvsp[-2].node), (yyvsp[0].node));
      }
    }
#line 3487 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 165:
#line 1230 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // indexed variable access, e.g. variable[index]
      if ((yyvsp[-3].node)->type == NODE_TYPE_EXPANSION) {
        // if left operand is an expansion already...
        // patch the existing expansion
        auto current = const_cast<AstNode*>(parser->ast()->findExpansionSubNode((yyvsp[-3].node)));
        TRI_ASSERT(current->type == NODE_TYPE_EXPANSION);
        current->changeMember(1, parser->ast()->createNodeIndexedAccess(current->getMember(1), (yyvsp[-1].node)));
        (yyval.node) = (yyvsp[-3].node);
      }
      else {
        (yyval.node) = parser->ast()->createNodeIndexedAccess((yyvsp[-3].node), (yyvsp[-1].node));
      }
    }
#line 3506 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 166:
#line 1244 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // variable expansion, e.g. variable[*], with optional FILTER, LIMIT and RETURN clauses
      if ((yyvsp[0].intval) > 1 && (yyvsp[-2].node)->type == NODE_TYPE_EXPANSION) {
        // create a dummy passthru node that reduces and evaluates the expansion first
        // and the expansion on top of the stack won't be chained with any other expansions
        (yyvsp[-2].node) = parser->ast()->createNodePassthru((yyvsp[-2].node));
      }

      // create a temporary iterator variable
      std::string const nextName = parser->ast()->variables()->nextName() + "_";

      if ((yyvsp[-2].node)->type == NODE_TYPE_EXPANSION) {
        auto iterator = parser->ast()->createNodeIterator(nextName.c_str(), nextName.size(), (yyvsp[-2].node)->getMember(1));
        parser->pushStack(iterator);
      }
      else {
        auto iterator = parser->ast()->createNodeIterator(nextName.c_str(), nextName.size(), (yyvsp[-2].node));
        parser->pushStack(iterator);
      }

      auto scopes = parser->ast()->scopes();
      scopes->stackCurrentVariable(scopes->getVariable(nextName));
    }
#line 3534 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 167:
#line 1266 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto scopes = parser->ast()->scopes();
      scopes->unstackCurrentVariable();

      auto iterator = static_cast<AstNode const*>(parser->popStack());
      auto variableNode = iterator->getMember(0);
      TRI_ASSERT(variableNode->type == NODE_TYPE_VARIABLE);
      auto variable = static_cast<Variable const*>(variableNode->getData());

      if ((yyvsp[-7].node)->type == NODE_TYPE_EXPANSION) {
        auto expand = parser->ast()->createNodeExpansion((yyvsp[-5].intval), iterator, parser->ast()->createNodeReference(variable->name), (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[-1].node));
        (yyvsp[-7].node)->changeMember(1, expand);
        (yyval.node) = (yyvsp[-7].node);
      }
      else {
        (yyval.node) = parser->ast()->createNodeExpansion((yyvsp[-5].intval), iterator, parser->ast()->createNodeReference(variable->name), (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[-1].node));
      }
    }
#line 3557 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 168:
#line 1287 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3565 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 169:
#line 1290 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3573 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 170:
#line 1296 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }
      
      (yyval.node) = (yyvsp[0].node);
    }
#line 3585 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 171:
#line 1303 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3597 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 172:
#line 1313 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3605 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 173:
#line 1316 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3613 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 174:
#line 1319 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueNull();
    }
#line 3621 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 175:
#line 1322 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 3629 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 176:
#line 1325 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 3637 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 177:
#line 1331 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, TRI_TRANSACTION_WRITE);
    }
#line 3645 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 178:
#line 1334 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, TRI_TRANSACTION_WRITE);
    }
#line 3653 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 179:
#line 1337 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval).length < 2 || (yyvsp[0].strval).value[0] != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3665 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 180:
#line 1347 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3673 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 181:
#line 1353 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3681 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 182:
#line 1356 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3689 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 183:
#line 1361 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3697 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;


#line 3701 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
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
  *++yylsp = yyloc;

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
      yyerror (&yylloc, parser, YY_("syntax error"));
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
        yyerror (&yylloc, parser, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

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
                      yytoken, &yylval, &yylloc, parser);
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

  yyerror_range[1] = yylsp[1-yylen];
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

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, yylsp, parser);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
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

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, parser, YY_("memory exhausted"));
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
                  yytoken, &yylval, &yylloc, parser);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, yylsp, parser);
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
