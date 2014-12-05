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
#line 10 "arangod/Aql/grammar.y" /* yacc.c:339  */

#include <stdio.h>
#include <stdlib.h>

#include <Basics/Common.h>
#include <Basics/conversions.h>
#include <Basics/tri-strings.h>

#include "Aql/AstNode.h"
#include "Aql/Parser.h"

#line 84 "arangod/Aql/grammar.cpp" /* yacc.c:339  */

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
    T_REMOVE = 270,
    T_INSERT = 271,
    T_UPDATE = 272,
    T_REPLACE = 273,
    T_NULL = 274,
    T_TRUE = 275,
    T_FALSE = 276,
    T_STRING = 277,
    T_QUOTED_STRING = 278,
    T_INTEGER = 279,
    T_DOUBLE = 280,
    T_PARAMETER = 281,
    T_ASSIGN = 282,
    T_NOT = 283,
    T_AND = 284,
    T_OR = 285,
    T_EQ = 286,
    T_NE = 287,
    T_LT = 288,
    T_GT = 289,
    T_LE = 290,
    T_GE = 291,
    T_PLUS = 292,
    T_MINUS = 293,
    T_TIMES = 294,
    T_DIV = 295,
    T_MOD = 296,
    T_EXPAND = 297,
    T_QUESTION = 298,
    T_COLON = 299,
    T_SCOPE = 300,
    T_RANGE = 301,
    T_COMMA = 302,
    T_OPEN = 303,
    T_CLOSE = 304,
    T_DOC_OPEN = 305,
    T_DOC_CLOSE = 306,
    T_LIST_OPEN = 307,
    T_LIST_CLOSE = 308,
    UMINUS = 309,
    UPLUS = 310,
    FUNCCALL = 311,
    REFERENCE = 312,
    INDEXED = 313
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 22 "arangod/Aql/grammar.y" /* yacc.c:355  */

  triagens::aql::AstNode*  node;
  char*                    strval;
  bool                     boolval;
  int64_t                  intval;

#line 191 "arangod/Aql/grammar.cpp" /* yacc.c:355  */
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
#line 29 "arangod/Aql/grammar.y" /* yacc.c:358  */


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
/// @brief shortcut macro for signalling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM                                   \
  parser->registerError(TRI_ERROR_OUT_OF_MEMORY);   \
  YYABORT;

#define scanner parser->scanner()


#line 252 "arangod/Aql/grammar.cpp" /* yacc.c:358  */

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
#define YYLAST   601

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  60
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  62
/* YYNRULES -- Number of rules.  */
#define YYNRULES  136
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  226

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   313

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
       2,     2,     2,     2,     2,     2,    59,     2,     2,     2,
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
      55,    56,    57,    58
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   196,   196,   198,   200,   202,   204,   209,   211,   216,
     218,   220,   222,   224,   226,   231,   240,   248,   253,   255,
     260,   267,   267,   310,   312,   317,   324,   327,   333,   347,
     364,   367,   367,   381,   381,   392,   395,   401,   407,   410,
     413,   419,   424,   431,   439,   442,   448,   459,   470,   478,
     489,   497,   508,   511,   511,   524,   527,   530,   533,   536,
     539,   542,   548,   555,   572,   572,   584,   587,   590,   596,
     599,   602,   605,   608,   611,   614,   617,   620,   623,   626,
     629,   632,   635,   638,   644,   650,   652,   657,   660,   666,
     669,   675,   675,   684,   686,   691,   694,   700,   703,   717,
     717,   726,   728,   733,   735,   740,   746,   750,   750,   778,
     791,   798,   802,   806,   813,   818,   823,   828,   832,   836,
     843,   846,   852,   855,   872,   875,   878,   881,   884,   890,
     897,   904,   918,   924,   931,   940,   946
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
  "\"INTO keyword\"", "\"REMOVE command\"", "\"INSERT command\"",
  "\"UPDATE command\"", "\"REPLACE command\"", "\"null\"", "\"true\"",
  "\"false\"", "\"identifier\"", "\"quoted string\"", "\"integer number\"",
  "\"number\"", "\"bind parameter\"", "\"assignment\"", "\"not operator\"",
  "\"and operator\"", "\"or operator\"", "\"== operator\"",
  "\"!= operator\"", "\"< operator\"", "\"> operator\"", "\"<= operator\"",
  "\">= operator\"", "\"+ operator\"", "\"- operator\"", "\"* operator\"",
  "\"/ operator\"", "\"% operator\"", "\"[*] operator\"", "\"?\"", "\":\"",
  "\"::\"", "\"..\"", "\",\"", "\"(\"", "\")\"", "\"{\"", "\"}\"", "\"[\"",
  "\"]\"", "UMINUS", "UPLUS", "FUNCCALL", "REFERENCE", "INDEXED", "'.'",
  "$accept", "query", "optional_statement_block_statements",
  "statement_block_statement", "for_statement", "filter_statement",
  "let_statement", "let_list", "let_element", "collect_statement", "$@1",
  "collect_list", "collect_element", "optional_into", "variable_list",
  "optional_keep", "$@2", "sort_statement", "$@3", "sort_list",
  "sort_element", "sort_direction", "limit_statement", "return_statement",
  "in_or_into_collection", "remove_statement", "insert_statement",
  "update_statement", "replace_statement", "expression", "$@4",
  "function_name", "function_call", "$@5", "operator_unary",
  "operator_binary", "operator_ternary",
  "optional_function_call_arguments", "function_arguments_list",
  "compound_type", "list", "$@6", "optional_list_elements",
  "list_elements_list", "query_options", "array", "$@7",
  "optional_array_elements", "array_elements_list", "array_element",
  "reference", "$@8", "single_reference", "expansion", "atomic_value",
  "numeric_value", "value_literal", "collection_name", "bind_parameter",
  "array_element_name", "variable_name", "integer_value", YY_NULLPTR
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
     305,   306,   307,   308,   309,   310,   311,   312,   313,    46
};
# endif

#define YYPACT_NINF -99

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-99)))

#define YYTABLE_NINF -132

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     -99,    34,   155,   -99,    25,    25,   451,   451,   -99,   -99,
     192,   451,   451,   451,   451,   -99,   -99,   -99,   -99,   -99,
     -99,   -99,   -99,   -99,   -99,   -99,   -99,   -99,    37,     9,
     -99,    30,   -99,   -99,   -99,    10,   -99,   -99,   -99,   -99,
     451,   451,   451,   451,   -99,   -99,   423,    21,   -99,   -99,
     -99,   -99,   -99,   -99,   -99,    28,   -36,   -99,   -99,   -99,
     -99,   -99,   423,    25,   451,    29,   363,   363,   298,   333,
     451,    25,   451,    40,    40,    40,   223,   -99,    -8,   451,
     451,    60,   451,   451,   451,   451,   451,   451,   451,   451,
     451,   451,   451,   451,   451,   451,   451,    51,    45,    52,
     451,     2,    -1,   -99,    69,    50,   -99,   263,   192,   472,
      17,    76,    76,   451,    76,   451,    76,   423,   -99,   423,
     -99,    54,   -99,   -99,    48,    53,   -99,    57,   423,    56,
      64,   243,   451,   543,   529,   560,   560,    23,    23,    23,
      23,   -10,   -10,    40,    40,    40,   393,   499,   -99,   451,
     -27,   100,   -99,   -99,    25,    25,    85,   451,   451,   -99,
     -99,   -99,   -99,     5,    20,    22,   -99,   -99,   -99,   -99,
     -99,    63,   -99,   -99,   363,   -99,   363,   -99,   -99,   -99,
      -8,   451,   -99,   451,   243,   451,   423,    66,    71,   451,
      19,   -26,   -99,   -99,   -99,   -99,   -99,   423,   -99,   -99,
      76,    76,   -99,   423,   423,   499,   -99,   451,   157,   -99,
     -99,   451,    26,    25,   -99,   -99,   423,   -99,   193,   -99,
     -99,    73,   -99,   -99,    25,   -99
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       7,     0,     0,     1,     0,     0,     0,     0,    21,    33,
       0,     0,     0,     0,     0,     8,     9,    11,    10,    12,
      13,    14,     2,     3,     4,     5,     6,   135,     0,    17,
      18,     0,   126,   127,   128,   109,   124,   136,   123,   132,
       0,     0,     0,    53,    99,    91,    16,    64,   110,    55,
      56,    57,    58,    89,    90,    60,   106,    59,   125,   120,
     121,   122,    43,     0,     0,    41,     0,     0,     0,     0,
       0,     0,     0,    68,    66,    67,     0,     7,   101,    93,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    26,    23,     0,    34,    35,    38,     0,     0,
       0,    97,    97,     0,    97,     0,    97,    15,    19,    20,
      52,     0,   133,   134,     0,   102,   103,     0,    95,     0,
      94,    82,     0,    70,    69,    76,    77,    78,    79,    80,
      81,    71,    72,    73,    74,    75,     0,    61,    63,    85,
       0,     0,   111,   112,     0,     0,    30,     0,     0,    39,
      40,    37,    42,   109,   124,   132,    44,   129,   130,   131,
      45,     0,    46,    47,     0,    48,     0,    50,    54,   100,
       0,     0,    92,     0,    83,     0,    87,     0,    86,     0,
       0,   108,   113,    27,    24,    31,    22,    25,    36,    98,
      97,    97,   104,   105,    96,    84,    65,     0,     0,   114,
     115,     0,     0,     0,    49,    51,    88,   116,     0,   117,
     118,    32,    28,   119,     0,    29
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -99,    39,   -99,   -99,   -99,   -99,   -99,   -99,    55,   -99,
     -99,   -99,   -34,   -99,   -99,   -99,   -99,   -99,   -99,   -99,
     -35,   -99,   -99,   -99,   -57,   -99,   -99,   -99,   -99,    -5,
     -99,   -99,   -99,   -99,   -99,   -99,   -99,   -99,   -99,   -99,
     -99,   -99,   -99,   -99,   -95,   -49,   -99,   -99,   -99,   -56,
     -99,   -99,   -99,   -99,    -6,   -99,   -99,    15,   -98,   -99,
      -4,   -99
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    15,    16,    17,    18,    29,    30,    19,
      63,   102,   103,   156,   221,   196,   213,    20,    64,   105,
     106,   161,    21,    22,   111,    23,    24,    25,    26,   107,
      77,    47,    48,    98,    49,    50,    51,   187,   188,    52,
      53,    79,   129,   130,   172,    54,    78,   124,   125,   126,
      55,    99,    56,   191,    57,    58,    59,   166,    60,   127,
      31,    61
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      28,    46,    62,   153,    65,  -129,    66,    67,    68,    69,
     112,   114,   116,   154,   122,   123,   100,   173,    81,   175,
    -130,   177,  -131,   101,   152,   189,   211,  -129,    39,    92,
      93,    94,   190,   212,     3,    73,    74,    75,    76,   167,
     168,   209,  -130,   169,  -131,    39,   155,    27,   219,    70,
     -62,    81,    39,   -62,  -129,   -62,    71,    72,   -62,   104,
      90,    91,    92,    93,    94,   117,    97,   119,    81,  -130,
    -107,  -131,   132,   148,   128,   131,   108,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   210,   149,   150,   151,   157,   158,   171,   179,
     180,   181,   162,   178,   131,   214,   215,   195,   174,   182,
     176,   183,    80,    44,   220,   206,   121,   200,   207,   201,
     224,   194,   199,   198,   202,   170,   118,   184,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,     0,    95,   186,     0,    96,     0,     0,     0,
     193,   104,   197,   192,     0,     0,     0,     0,     4,     5,
       6,     7,     8,     9,    10,     0,     0,     0,     0,    80,
      11,    12,    13,    14,     0,     0,   203,     0,   204,     0,
     205,     0,     0,     0,   208,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,     0,
      95,     0,   216,    96,     0,    80,   218,     0,     0,   222,
     217,    32,    33,    34,     0,    36,    37,    38,    39,     0,
     225,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    80,    95,     0,     0,    96,
       0,     0,     0,     0,     0,     0,   223,     0,     0,     0,
       0,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,     0,    95,     0,     0,    96,
       0,    81,   120,   159,   160,    80,    86,    87,    88,    89,
      90,    91,    92,    93,    94,     0,     0,     0,     0,     0,
       0,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,     0,    95,     0,     0,    96,
     109,   113,   110,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
       0,    95,     0,     0,    96,   109,   115,   110,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,   109,    95,   110,     0,    96,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    80,    95,     0,     0,    96,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    80,    95,   185,     0,    96,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,     0,    95,     0,     0,    96,
      32,    33,    34,    35,    36,    37,    38,    39,     0,    40,
       0,     0,     0,     0,     0,     0,     0,     0,    41,    42,
       0,    32,    33,    34,   163,   164,    37,    38,   165,    43,
      40,    44,     0,    45,     0,     0,     0,     0,     0,    41,
      42,    80,     0,     0,     0,     0,     0,     0,     0,     0,
      43,     0,    44,     0,    45,     0,     0,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    80,    95,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    80,     0,    81,    82,     0,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    81,    80,     0,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,     0,     0,     0,    81,     0,
       0,     0,     0,    86,    87,    88,    89,    90,    91,    92,
      93,    94
};

static const yytype_int16 yycheck[] =
{
       4,     6,     7,   101,    10,     0,    11,    12,    13,    14,
      67,    68,    69,    14,    22,    23,    52,   112,    28,   114,
       0,   116,     0,    59,    22,    52,    52,    22,    26,    39,
      40,    41,    59,    59,     0,    40,    41,    42,    43,    22,
      23,    22,    22,    26,    22,    26,    47,    22,    22,    12,
      45,    28,    26,    48,    49,    45,    47,    27,    48,    63,
      37,    38,    39,    40,    41,    70,    45,    72,    28,    49,
      42,    49,    12,    22,    79,    80,    47,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,   190,    48,    42,   100,    27,    47,    22,    51,
      47,    44,   108,    49,   109,   200,   201,    22,   113,    53,
     115,    47,    12,    50,   212,    49,    77,   174,    47,   176,
      47,   155,   171,   158,   180,   110,    71,   132,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    -1,    43,   149,    -1,    46,    -1,    -1,    -1,
     154,   155,   157,    53,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,     8,     9,    -1,    -1,    -1,    -1,    12,
      15,    16,    17,    18,    -1,    -1,   181,    -1,   183,    -1,
     185,    -1,    -1,    -1,   189,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    -1,
      43,    -1,   207,    46,    -1,    12,   211,    -1,    -1,   213,
      53,    19,    20,    21,    -1,    23,    24,    25,    26,    -1,
     224,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    12,    43,    -1,    -1,    46,
      -1,    -1,    -1,    -1,    -1,    -1,    53,    -1,    -1,    -1,
      -1,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    -1,    43,    -1,    -1,    46,
      -1,    28,    49,    10,    11,    12,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    -1,    -1,    -1,    -1,    -1,
      -1,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    -1,    43,    -1,    -1,    46,
      12,    13,    14,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      -1,    43,    -1,    -1,    46,    12,    13,    14,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    12,    43,    14,    -1,    46,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    12,    43,    -1,    -1,    46,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    12,    43,    44,    -1,    46,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    -1,    43,    -1,    -1,    46,
      19,    20,    21,    22,    23,    24,    25,    26,    -1,    28,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    48,
      28,    50,    -1,    52,    -1,    -1,    -1,    -1,    -1,    37,
      38,    12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      48,    -1,    50,    -1,    52,    -1,    -1,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    12,    43,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    12,    -1,    28,    29,    -1,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    28,    12,    -1,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    -1,    -1,    -1,    28,    -1,
      -1,    -1,    -1,    33,    34,    35,    36,    37,    38,    39,
      40,    41
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    61,    62,     0,     3,     4,     5,     6,     7,     8,
       9,    15,    16,    17,    18,    63,    64,    65,    66,    69,
      77,    82,    83,    85,    86,    87,    88,    22,   120,    67,
      68,   120,    19,    20,    21,    22,    23,    24,    25,    26,
      28,    37,    38,    48,    50,    52,    89,    91,    92,    94,
      95,    96,    99,   100,   105,   110,   112,   114,   115,   116,
     118,   121,    89,    70,    78,   114,    89,    89,    89,    89,
      12,    47,    27,    89,    89,    89,    89,    90,   106,   101,
      12,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    43,    46,    45,    93,   111,
      52,    59,    71,    72,   120,    79,    80,    89,    47,    12,
      14,    84,    84,    13,    84,    13,    84,    89,    68,    89,
      49,    61,    22,    23,   107,   108,   109,   119,    89,   102,
     103,    89,    12,    89,    89,    89,    89,    89,    89,    89,
      89,    89,    89,    89,    89,    89,    89,    89,    22,    48,
      42,    89,    22,   118,    14,    47,    73,    27,    47,    10,
      11,    81,   114,    22,    23,    26,   117,    22,    23,    26,
     117,    22,   104,   104,    89,   104,    89,   104,    49,    51,
      47,    44,    53,    47,    89,    44,    89,    97,    98,    52,
      59,   113,    53,   120,    72,    22,    75,    89,    80,   105,
      84,    84,   109,    89,    89,    89,    49,    47,    89,    22,
     118,    52,    59,    76,   104,   104,    89,    53,    89,    22,
     118,    74,   120,    53,    47,   120
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    60,    61,    61,    61,    61,    61,    62,    62,    63,
      63,    63,    63,    63,    63,    64,    65,    66,    67,    67,
      68,    70,    69,    71,    71,    72,    73,    73,    74,    74,
      75,    76,    75,    78,    77,    79,    79,    80,    81,    81,
      81,    82,    82,    83,    84,    84,    85,    86,    87,    87,
      88,    88,    89,    90,    89,    89,    89,    89,    89,    89,
      89,    89,    91,    91,    93,    92,    94,    94,    94,    95,
      95,    95,    95,    95,    95,    95,    95,    95,    95,    95,
      95,    95,    95,    95,    96,    97,    97,    98,    98,    99,
      99,   101,   100,   102,   102,   103,   103,   104,   104,   106,
     105,   107,   107,   108,   108,   109,   110,   111,   110,   112,
     112,   112,   112,   112,   113,   113,   113,   113,   113,   113,
     114,   114,   115,   115,   116,   116,   116,   116,   116,   117,
     117,   117,   118,   119,   119,   120,   121
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     2,     2,     2,     0,     2,     1,
       1,     1,     1,     1,     1,     4,     2,     2,     1,     3,
       3,     0,     5,     1,     3,     3,     0,     2,     1,     3,
       0,     0,     3,     0,     3,     1,     3,     2,     0,     1,
       1,     2,     4,     2,     2,     2,     4,     4,     4,     6,
       4,     6,     3,     0,     4,     1,     1,     1,     1,     1,
       1,     3,     1,     3,     0,     5,     2,     2,     2,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     4,     5,     0,     1,     1,     3,     1,
       1,     0,     4,     0,     1,     1,     3,     0,     2,     0,
       4,     0,     1,     1,     3,     3,     1,     0,     4,     1,
       1,     3,     3,     4,     2,     2,     3,     3,     3,     4,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1
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
#line 196 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1699 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 3:
#line 198 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1706 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 4:
#line 200 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1713 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 202 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1720 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 204 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1727 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 209 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1734 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 8:
#line 211 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1741 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 216 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1748 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 218 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1755 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 220 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1762 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 222 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1769 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 13:
#line 224 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1776 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 14:
#line 226 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1783 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 231 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
     
      auto node = parser->ast()->createNodeFor((yyvsp[-2].strval), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 1794 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 240 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // operand is a reference. can use it directly
      auto node = parser->ast()->createNodeFilter((yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 1804 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 248 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1811 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 253 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1818 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 255 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1825 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 260 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLet((yyvsp[-2].strval), (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 1834 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 267 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    }
#line 1843 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 270 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());

      if (list == nullptr) {
        ABORT_OOM
      }

      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = list->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = list->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      if ((yyvsp[-1].strval) == nullptr && (yyvsp[0].node) != nullptr) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of 'KEEP' without 'INTO'", yylloc.first_line, yylloc.first_column);
      } 

      auto node = parser->ast()->createNodeCollect(list, (yyvsp[-1].strval), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 1885 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 310 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1892 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 312 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 1899 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 317 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeAssign((yyvsp[-2].strval), (yyvsp[0].node));
      parser->pushList(node);
    }
#line 1908 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 324 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = nullptr;
    }
#line 1916 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 327 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 1924 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 333 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->ast()->scopes()->existsVariable((yyvsp[0].strval))) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of unknown variable '%s' for KEEP", (yyvsp[0].strval), yylloc.first_line, yylloc.first_column);
      }
        
      auto node = parser->ast()->createNodeReference((yyvsp[0].strval));
      if (node == nullptr) {
        ABORT_OOM
      }

      // indicate the this node is a reference to the variable name, not the variable value
      node->setFlag(FLAG_KEEP_VARIABLENAME);
      parser->pushList(node);
    }
#line 1943 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 347 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->ast()->scopes()->existsVariable((yyvsp[0].strval))) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of unknown variable '%s' for KEEP", (yyvsp[0].strval), yylloc.first_line, yylloc.first_column);
      }
        
      auto node = parser->ast()->createNodeReference((yyvsp[0].strval));
      if (node == nullptr) {
        ABORT_OOM
      }

      // indicate the this node is a reference to the variable name, not the variable value
      node->setFlag(FLAG_KEEP_VARIABLENAME);
      parser->pushList(node);
    }
#line 1962 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 364 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 1970 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 367 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[0].strval), "KEEP")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'KEEP'", (yyvsp[0].strval), yylloc.first_line, yylloc.first_column);
      }

      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    }
#line 1983 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 374 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 1992 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 381 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    }
#line 2001 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 384 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      auto node = parser->ast()->createNodeSort(list);
      parser->ast()->addOperation(node);
    }
#line 2011 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 392 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushList((yyvsp[0].node));
    }
#line 2019 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 395 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushList((yyvsp[0].node));
    }
#line 2027 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 401 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeSortElement((yyvsp[-1].node), (yyvsp[0].boolval));
    }
#line 2035 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 407 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.boolval) = true;
    }
#line 2043 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 410 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.boolval) = true;
    }
#line 2051 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 413 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.boolval) = false;
    }
#line 2059 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 419 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto offset = parser->ast()->createNodeValueInt(0);
      auto node = parser->ast()->createNodeLimit(offset, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2069 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 424 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLimit((yyvsp[-2].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2078 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 431 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeReturn((yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2088 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 439 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2096 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 442 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2104 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 46:
#line 448 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_REMOVE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2117 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 459 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_INSERT, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2130 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 48:
#line 470 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_UPDATE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeUpdate(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2143 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 478 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_UPDATE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeUpdate((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2156 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 489 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_REPLACE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeReplace(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2169 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 497 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_REPLACE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeReplace((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2182 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 52:
#line 508 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[-1].node);
    }
#line 2190 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 511 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 2199 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 514 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName.c_str());
    }
#line 2214 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 524 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2222 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 527 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2230 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 530 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2238 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 533 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2246 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 536 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2254 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 539 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2262 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 542 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeRange((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2270 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 548 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);

      if ((yyval.strval) == nullptr) {
        ABORT_OOM
      }
    }
#line 2282 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 555 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[-2].strval) == nullptr || (yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      std::string temp((yyvsp[-2].strval));
      temp.append("::");
      temp.append((yyvsp[0].strval));
      (yyval.strval) = parser->query()->registerString(temp.c_str(), temp.size(), false);

      if ((yyval.strval) == nullptr) {
        ABORT_OOM
      }
    }
#line 2301 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 572 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushStack((yyvsp[0].strval));

      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    }
#line 2312 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 577 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall(static_cast<char const*>(parser->popStack()), list);
    }
#line 2321 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 584 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_PLUS, (yyvsp[0].node));
    }
#line 2329 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 587 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_MINUS, (yyvsp[0].node));
    }
#line 2337 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 590 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    { 
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, (yyvsp[0].node));
    }
#line 2345 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 596 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2353 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 599 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2361 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 602 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_PLUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2369 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 605 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MINUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2377 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 608 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_TIMES, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2385 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 74:
#line 611 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_DIV, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2393 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 75:
#line 614 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MOD, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2401 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 76:
#line 617 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2409 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 77:
#line 620 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2417 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 623 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2425 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 626 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2433 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 629 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2441 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 632 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2449 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 82:
#line 635 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2457 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 83:
#line 638 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, (yyvsp[-3].node), (yyvsp[0].node));
    }
#line 2465 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 644 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2473 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 85:
#line 650 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2480 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 86:
#line 652 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2487 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 87:
#line 657 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushList((yyvsp[0].node));
    }
#line 2495 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 88:
#line 660 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushList((yyvsp[0].node));
    }
#line 2503 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 89:
#line 666 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2511 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 90:
#line 669 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2519 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 91:
#line 675 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    }
#line 2528 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 92:
#line 678 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 2536 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 93:
#line 684 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2543 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 94:
#line 686 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2550 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 95:
#line 691 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushList((yyvsp[0].node));
    }
#line 2558 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 96:
#line 694 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushList((yyvsp[0].node));
    }
#line 2566 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 97:
#line 700 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 2574 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 98:
#line 703 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[-1].strval) == nullptr || (yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[-1].strval), "OPTIONS")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'OPTIONS'", (yyvsp[-1].strval), yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 2590 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 99:
#line 717 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2599 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 100:
#line 720 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 2607 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 101:
#line 726 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2614 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 102:
#line 728 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2621 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 103:
#line 733 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2628 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 104:
#line 735 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2635 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 105:
#line 740 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArray((yyvsp[-2].strval), (yyvsp[0].node));
    }
#line 2643 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 106:
#line 746 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // start of reference (collection or variable name)
      (yyval.node) = (yyvsp[0].node);
    }
#line 2652 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 107:
#line 750 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // expanded variable access, e.g. variable[*]

      // create a temporary iterator variable
      std::string const nextName = parser->ast()->variables()->nextName() + "_";
      char const* iteratorName = nextName.c_str();
      auto iterator = parser->ast()->createNodeIterator(iteratorName, (yyvsp[0].node));

      parser->pushStack(iterator);
      parser->pushStack(parser->ast()->createNodeReference(iteratorName));
    }
#line 2668 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 108:
#line 760 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // return from the "expansion" subrule

      // push the expand node into the statement list
      auto iterator = static_cast<AstNode*>(parser->popStack());
      auto expand = parser->ast()->createNodeExpand(iterator, (yyvsp[0].node));
      
      std::string const nextName = parser->ast()->variables()->nextName();
      char const* variableName = nextName.c_str();
      auto let = parser->ast()->createNodeLet(variableName, expand, false);
      parser->ast()->addOperation(let);
      
      // return a reference only
      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 2688 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 109:
#line 778 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // variable or collection
      AstNode* node;
      
      if (parser->ast()->scopes()->existsVariable((yyvsp[0].strval))) {
        node = parser->ast()->createNodeReference((yyvsp[0].strval));
      }
      else {
        node = parser->ast()->createNodeCollection((yyvsp[0].strval));
      }

      (yyval.node) = node;
    }
#line 2706 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 110:
#line 791 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
      
      if ((yyval.node) == nullptr) {
        ABORT_OOM
      }
    }
#line 2718 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 111:
#line 798 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // named variable access, e.g. variable.reference
      (yyval.node) = parser->ast()->createNodeAttributeAccess((yyvsp[-2].node), (yyvsp[0].strval));
    }
#line 2727 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 112:
#line 802 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // named variable access, e.g. variable.@reference
      (yyval.node) = parser->ast()->createNodeBoundAttributeAccess((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2736 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 113:
#line 806 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // indexed variable access, e.g. variable[index]
      (yyval.node) = parser->ast()->createNodeIndexedAccess((yyvsp[-3].node), (yyvsp[-1].node));
    }
#line 2745 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 114:
#line 813 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // named variable access, continuation from * expansion, e.g. [*].variable.reference
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeAttributeAccess(node, (yyvsp[0].strval));
    }
#line 2755 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 115:
#line 818 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.@reference
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeBoundAttributeAccess(node, (yyvsp[0].node));
    }
#line 2765 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 116:
#line 823 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable[index]
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeIndexedAccess(node, (yyvsp[-1].node));
    }
#line 2775 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 117:
#line 828 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // named variable access, continuation from * expansion, e.g. [*].variable.xx.reference
      (yyval.node) = parser->ast()->createNodeAttributeAccess((yyvsp[-2].node), (yyvsp[0].strval));
    }
#line 2784 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 118:
#line 832 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.xx.@reference
      (yyval.node) = parser->ast()->createNodeBoundAttributeAccess((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2793 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 119:
#line 836 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable.xx.[index]
      (yyval.node) = parser->ast()->createNodeIndexedAccess((yyvsp[-3].node), (yyvsp[-1].node));
    }
#line 2802 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 120:
#line 843 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2810 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 121:
#line 846 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2818 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 122:
#line 852 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2826 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 123:
#line 855 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }
      
      double value = TRI_DoubleString((yyvsp[0].strval));

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        parser->registerWarning(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, TRI_errno_string(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE), yylloc.first_line, yylloc.first_column);
        (yyval.node) = parser->ast()->createNodeValueNull();
      }
      else {
        (yyval.node) = parser->ast()->createNodeValueDouble(value); 
      }
    }
#line 2846 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 124:
#line 872 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval)); 
    }
#line 2854 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 125:
#line 875 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2862 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 126:
#line 878 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueNull();
    }
#line 2870 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 127:
#line 881 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2878 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 128:
#line 884 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 2886 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 129:
#line 890 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval));
    }
#line 2898 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 130:
#line 897 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval));
    }
#line 2910 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 131:
#line 904 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }
      
      if (strlen((yyvsp[0].strval)) < 2 || (yyvsp[0].strval)[0] != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[0].strval), yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval));
    }
#line 2926 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 132:
#line 918 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval));
    }
#line 2934 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 133:
#line 924 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2946 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 134:
#line 931 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2958 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 135:
#line 940 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2966 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 136:
#line 946 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      int64_t value = TRI_Int64String((yyvsp[0].strval));
      if (TRI_errno() == TRI_ERROR_NO_ERROR) {
        (yyval.node) = parser->ast()->createNodeValueInt(value);
      }
      else {
        double value2 = TRI_DoubleString((yyvsp[0].strval));
        if (TRI_errno() == TRI_ERROR_NO_ERROR) {
          (yyval.node) = parser->ast()->createNodeValueDouble(value2);
        }
        else {
          parser->registerWarning(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, TRI_errno_string(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE), yylloc.first_line, yylloc.first_column);
          (yyval.node) = parser->ast()->createNodeValueNull();
        }
      }
    }
#line 2991 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;


#line 2995 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
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
