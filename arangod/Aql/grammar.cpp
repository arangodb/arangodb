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

#include <stdio.h>
#include <stdlib.h>

#include <Basics/Common.h>
#include <Basics/conversions.h>
#include <Basics/tri-strings.h>

#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/Parser.h"

#line 85 "arangod/Aql/grammar.cpp" /* yacc.c:339  */

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
    T_UPSERT = 274,
    T_NULL = 275,
    T_TRUE = 276,
    T_FALSE = 277,
    T_STRING = 278,
    T_QUOTED_STRING = 279,
    T_INTEGER = 280,
    T_DOUBLE = 281,
    T_PARAMETER = 282,
    T_ASSIGN = 283,
    T_NOT = 284,
    T_AND = 285,
    T_OR = 286,
    T_EQ = 287,
    T_NE = 288,
    T_LT = 289,
    T_GT = 290,
    T_LE = 291,
    T_GE = 292,
    T_PLUS = 293,
    T_MINUS = 294,
    T_TIMES = 295,
    T_DIV = 296,
    T_MOD = 297,
    T_EXPAND = 298,
    T_QUESTION = 299,
    T_COLON = 300,
    T_SCOPE = 301,
    T_RANGE = 302,
    T_COMMA = 303,
    T_OPEN = 304,
    T_CLOSE = 305,
    T_OBJECT_OPEN = 306,
    T_OBJECT_CLOSE = 307,
    T_ARRAY_OPEN = 308,
    T_ARRAY_CLOSE = 309,
    T_NIN = 310,
    UMINUS = 311,
    UPLUS = 312,
    FUNCCALL = 313,
    REFERENCE = 314,
    INDEXED = 315
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

#line 194 "arangod/Aql/grammar.cpp" /* yacc.c:355  */
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


#line 255 "arangod/Aql/grammar.cpp" /* yacc.c:358  */

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
#define YYLAST   743

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  62
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  72
/* YYNRULES -- Number of rules.  */
#define YYNRULES  157
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  268

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   315

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
       2,     2,     2,     2,     2,     2,    61,     2,     2,     2,
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
      55,    56,    57,    58,    59,    60
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   203,   203,   205,   207,   209,   211,   213,   218,   220,
     225,   229,   235,   237,   242,   244,   246,   248,   250,   252,
     257,   266,   274,   279,   281,   286,   293,   303,   303,   317,
     333,   360,   392,   422,   424,   429,   436,   439,   445,   459,
     476,   479,   479,   493,   493,   504,   507,   513,   519,   522,
     525,   528,   534,   539,   546,   554,   557,   563,   574,   585,
     594,   606,   611,   620,   632,   637,   640,   646,   646,   702,
     705,   705,   721,   724,   727,   730,   733,   736,   739,   745,
     752,   769,   769,   781,   784,   787,   793,   796,   799,   802,
     805,   808,   811,   814,   817,   820,   823,   826,   829,   832,
     835,   841,   847,   849,   854,   857,   857,   876,   879,   885,
     888,   894,   894,   903,   905,   910,   913,   919,   922,   936,
     936,   945,   947,   952,   954,   959,   962,   965,   980,   984,
     984,  1008,  1044,  1051,  1055,  1059,  1066,  1071,  1076,  1081,
    1085,  1089,  1096,  1099,  1105,  1112,  1122,  1125,  1128,  1131,
    1134,  1140,  1147,  1154,  1168,  1174,  1181,  1190
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
  "\"UPDATE command\"", "\"REPLACE command\"", "\"UPSERT command\"",
  "\"null\"", "\"true\"", "\"false\"", "\"identifier\"",
  "\"quoted string\"", "\"integer number\"", "\"number\"",
  "\"bind parameter\"", "\"assignment\"", "\"not operator\"",
  "\"and operator\"", "\"or operator\"", "\"== operator\"",
  "\"!= operator\"", "\"< operator\"", "\"> operator\"", "\"<= operator\"",
  "\">= operator\"", "\"+ operator\"", "\"- operator\"", "\"* operator\"",
  "\"/ operator\"", "\"% operator\"", "\"[*] operator\"", "\"?\"", "\":\"",
  "\"::\"", "\"..\"", "\",\"", "\"(\"", "\")\"", "\"{\"", "\"}\"", "\"[\"",
  "\"]\"", "T_NIN", "UMINUS", "UPLUS", "FUNCCALL", "REFERENCE", "INDEXED",
  "'.'", "$accept", "query", "optional_post_modification_lets",
  "optional_post_modification_block",
  "optional_statement_block_statements", "statement_block_statement",
  "for_statement", "filter_statement", "let_statement", "let_list",
  "let_element", "count_into", "collect_variable_list", "$@1",
  "collect_statement", "collect_list", "collect_element", "optional_into",
  "variable_list", "optional_keep", "$@2", "sort_statement", "$@3",
  "sort_list", "sort_element", "sort_direction", "limit_statement",
  "return_statement", "in_or_into_collection", "remove_statement",
  "insert_statement", "update_parameters", "update_statement",
  "replace_parameters", "replace_statement", "update_or_replace",
  "upsert_statement", "$@4", "expression", "$@5", "function_name",
  "function_call", "$@6", "operator_unary", "operator_binary",
  "operator_ternary", "optional_function_call_arguments",
  "expression_or_query", "$@7", "function_arguments_list", "compound_type",
  "array", "$@8", "optional_array_elements", "array_elements_list",
  "query_options", "object", "$@9", "optional_object_elements",
  "object_elements_list", "object_element", "reference", "$@10",
  "single_reference", "expansion", "atomic_value", "numeric_value",
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
     315,    46
};
# endif

#define YYPACT_NINF -115

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-115)))

#define YYTABLE_NINF -154

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -115,     8,   662,  -115,   -14,   -14,   669,   669,    16,  -115,
      45,   669,   669,   669,   669,  -115,  -115,  -115,  -115,  -115,
     -11,  -115,  -115,  -115,  -115,    56,    56,    56,    56,    56,
    -115,    24,     5,  -115,    19,  -115,  -115,  -115,   -23,  -115,
    -115,  -115,  -115,   669,   669,   669,   669,  -115,  -115,   560,
      15,  -115,  -115,  -115,  -115,  -115,  -115,  -115,    53,   -31,
    -115,  -115,  -115,  -115,   560,    90,  -115,   -14,   669,    20,
     465,   465,  -115,   367,  -115,   403,   669,   -14,  -115,    91,
     117,  -115,  -115,  -115,  -115,  -115,   669,   -14,   669,    93,
      93,    93,   290,  -115,    -3,   669,   669,   118,   669,   669,
     669,   669,   669,   669,   669,   669,   669,   669,   669,   669,
     669,   669,   669,   114,    89,    97,   669,    25,   127,    98,
    -115,   121,   100,  -115,   331,    45,   690,    59,   122,   122,
     669,   122,   669,   122,   498,   123,  -115,  -115,  -115,  -115,
     560,  -115,   560,  -115,   103,  -115,  -115,   109,   669,   106,
     102,  -115,   110,   560,   108,   111,   348,   669,   607,   576,
     591,   591,    17,    17,    17,    17,    -2,    -2,    93,    93,
      93,   529,    51,  -115,   635,   -28,   153,  -115,  -115,   -14,
     -14,   669,   669,  -115,  -115,  -115,  -115,  -115,    28,    31,
     120,  -115,  -115,  -115,  -115,  -115,   112,  -115,  -115,   465,
    -115,   465,  -115,   669,   669,   -14,  -115,   669,   192,  -115,
      -3,   669,  -115,   669,   348,   669,   560,   116,  -115,  -115,
     113,   669,    61,   -12,  -115,  -115,  -115,   560,  -115,  -115,
     122,   122,   434,   560,   119,  -115,   560,   115,  -115,   560,
     560,   560,  -115,  -115,   669,   228,  -115,  -115,   669,   105,
    -115,  -115,  -115,  -115,   669,   -14,   669,  -115,  -115,   259,
    -115,  -115,   465,  -115,   560,  -115,   122,  -115
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
      12,     0,     0,     1,     0,     0,     0,     0,    27,    43,
       0,     0,     0,     0,     0,    67,    13,    14,    16,    15,
      36,    17,    18,    19,     2,    10,    10,    10,    10,    10,
     157,     0,    22,    23,     0,   148,   149,   150,   131,   146,
     144,   145,   154,     0,     0,     0,    70,   119,   111,    21,
      81,   132,    72,    73,    74,    75,   109,   110,    77,   128,
      76,   147,   142,   143,    54,     0,    29,     0,     0,    52,
       0,     0,    61,     0,    64,     0,     0,     0,    30,    40,
       0,     3,     4,     5,     6,     7,     0,     0,     0,    85,
      83,    84,     0,    12,   121,   113,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    28,
      33,     0,    44,    45,    48,     0,     0,     0,   117,   117,
       0,   117,     0,   117,     0,    37,    41,    31,     9,    11,
      20,    24,    25,    69,     0,   155,   156,     0,     0,     0,
     122,   123,     0,   115,     0,   114,    99,     0,    87,    86,
      93,    94,    95,    96,    97,    98,    88,    89,    90,    91,
      92,     0,    78,    80,   105,     0,     0,   133,   134,     0,
       0,     0,     0,    49,    50,    47,    51,    53,   131,   146,
     154,    55,   151,   152,   153,    56,     0,    57,    58,     0,
      59,     0,    62,     0,     0,     0,    71,     0,     0,   120,
       0,     0,   112,     0,   100,     0,   104,     0,   107,    12,
     103,     0,     0,   130,   135,    26,    34,    35,    46,   118,
     117,   117,     0,    32,    42,    38,   127,     0,   124,   125,
     116,   101,    82,   106,   105,     0,   136,   137,     0,     0,
      60,    63,    65,    66,     0,     0,     0,   108,   138,     0,
     139,   140,     0,    39,   126,   141,   117,    68
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -115,   -92,  -115,   107,  -115,  -115,  -115,  -115,    77,  -115,
      82,   144,  -115,  -115,  -115,  -115,    -9,  -115,  -115,  -115,
    -115,  -115,  -115,  -115,   -10,  -115,  -115,    94,   -57,  -115,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,  -115,    -1,  -115,
    -115,  -115,  -115,  -115,  -115,  -115,  -115,   -67,  -115,  -115,
    -115,  -115,  -115,  -115,  -115,  -114,   -18,  -115,  -115,  -115,
     -29,  -115,  -115,  -115,  -115,    -6,  -115,  -115,    52,  -110,
    -115,    -4
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,    80,    81,     2,    16,    17,    18,    19,    32,
      33,    66,    20,    67,    21,   119,   120,    79,   234,   137,
     205,    22,    68,   122,   123,   185,    23,    24,   128,    25,
      26,    72,    27,    74,    28,   254,    29,    76,   124,    93,
      50,    51,   114,    52,    53,    54,   217,   218,   219,   220,
      55,    56,    95,   154,   155,   197,    57,    94,   149,   150,
     151,    58,   115,    59,   223,    60,    61,    62,   191,    63,
     152,    34
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      31,   144,    65,    77,    69,    49,    64,   178,     3,    30,
      70,    71,    73,    75,   129,   198,   131,   200,   133,   202,
     145,   146,   116,   -79,   147,   221,   -79,    97,  -151,    65,
     117,  -152,  -151,   222,  -151,  -152,    86,  -152,   108,   109,
     110,   248,    89,    90,    91,    92,    97,    88,   177,   249,
     148,  -151,    42,    87,  -152,   106,   107,   108,   109,   110,
      -8,   113,    -8,   121,   112,    35,    36,    37,   125,    39,
      40,    41,    42,   135,   -79,   134,  -151,   -79,  -151,  -152,
      97,  -152,   192,   193,   246,   140,   194,   142,    42,   106,
     107,   108,   109,   110,   153,   156,  -129,   158,   159,   160,
     161,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172,   247,   118,   136,   176,   250,   251,   186,   187,
    -153,     5,    97,     7,  -153,   156,  -153,   243,   260,   199,
     157,   201,    42,    82,    83,    84,    85,   173,   174,   261,
     175,   179,   230,  -153,   231,   196,   180,   208,   182,   181,
     210,   204,   267,   206,   207,   211,   214,   138,   209,   213,
     256,   244,   212,    47,    78,    96,   242,   255,  -153,   141,
    -153,   226,   228,   216,   139,   225,   121,   257,   229,   195,
     227,   238,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,     0,   111,     0,     0,
     112,   235,   232,   233,    96,   266,   236,   224,     0,     0,
     239,     0,   240,     0,   241,     0,     0,     0,     0,     0,
     245,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,     0,   111,     0,     0,   112,
      96,     0,     0,   216,     0,     0,   237,   259,     0,     0,
       0,   263,     0,   262,     0,   264,     0,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,    96,   111,     0,     0,   112,     0,     0,     0,     0,
       0,     0,   258,     0,     0,     0,     0,     0,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,    96,   111,     0,     0,   112,     0,     0,     0,
       0,     0,     0,   265,     0,     0,     0,     0,     0,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,     0,   111,     0,     0,   112,     0,     0,
     143,   183,   184,    96,     0,     0,     0,     0,     0,     0,
       0,    35,    36,    37,     0,    39,    40,    41,    42,     0,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,     0,   111,     0,    97,   112,   126,
     130,   127,   102,   103,   104,   105,   106,   107,   108,   109,
     110,     0,     0,     0,     0,   112,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
       0,   111,     0,     0,   112,   126,   132,   127,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,    96,   111,     0,     0,
     112,   252,   253,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   126,   111,   127,
       0,   112,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,     0,   111,
      96,     0,   112,     0,   203,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,    96,   111,     0,     0,   112,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,    96,   111,   215,     0,   112,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,    96,   111,    97,    98,   112,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,    96,
      97,     0,     0,   112,     0,   102,   103,   104,   105,   106,
     107,   108,   109,   110,     0,     0,    97,     0,   112,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
       0,     0,     0,     0,   112,    35,    36,    37,    38,    39,
      40,    41,    42,     0,    43,     4,     5,     6,     7,     8,
       9,    10,     0,    44,    45,     0,     0,    11,    12,    13,
      14,    15,     0,     0,    46,  -102,    47,     0,    48,    35,
      36,    37,    38,    39,    40,    41,    42,     0,    43,     0,
       0,     0,     0,     0,     0,     0,     0,    44,    45,     0,
      35,    36,    37,   188,   189,    40,    41,   190,    46,    43,
      47,     0,    48,     0,     0,     0,     0,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    46,
       0,    47,     0,    48
};

static const yytype_int16 yycheck[] =
{
       4,    93,    13,    14,    10,     6,     7,   117,     0,    23,
      11,    12,    13,    14,    71,   129,    73,   131,    75,   133,
      23,    24,    53,    46,    27,    53,    49,    29,     0,    13,
      61,     0,     4,    61,     6,     4,    12,     6,    40,    41,
      42,    53,    43,    44,    45,    46,    29,    28,    23,    61,
      53,    23,    27,    48,    23,    38,    39,    40,    41,    42,
       4,    46,     6,    67,    47,    20,    21,    22,    48,    24,
      25,    26,    27,    77,    46,    76,    48,    49,    50,    48,
      29,    50,    23,    24,    23,    86,    27,    88,    27,    38,
      39,    40,    41,    42,    95,    96,    43,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   222,    23,    23,   116,   230,   231,   124,   125,
       0,     4,    29,     6,     4,   126,     6,   219,    23,   130,
      12,   132,    27,    26,    27,    28,    29,    23,    49,   249,
      43,    14,   199,    23,   201,    23,    48,   148,    48,    28,
      48,    28,   266,    50,    45,    45,   157,    80,    52,    48,
      45,    48,    54,    51,    20,    12,    50,    48,    48,    87,
      50,   180,   182,   174,    80,   179,   180,   244,   196,   127,
     181,   210,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    -1,    44,    -1,    -1,
      47,   205,   203,   204,    12,   262,   207,    54,    -1,    -1,
     211,    -1,   213,    -1,   215,    -1,    -1,    -1,    -1,    -1,
     221,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    -1,    44,    -1,    -1,    47,
      12,    -1,    -1,   244,    -1,    -1,    54,   248,    -1,    -1,
      -1,   255,    -1,   254,    -1,   256,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    12,    44,    -1,    -1,    47,    -1,    -1,    -1,    -1,
      -1,    -1,    54,    -1,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    12,    44,    -1,    -1,    47,    -1,    -1,    -1,
      -1,    -1,    -1,    54,    -1,    -1,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    -1,    44,    -1,    -1,    47,    -1,    -1,
      50,    10,    11,    12,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    20,    21,    22,    -1,    24,    25,    26,    27,    -1,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    -1,    44,    -1,    29,    47,    12,
      13,    14,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    -1,    -1,    -1,    -1,    47,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      -1,    44,    -1,    -1,    47,    12,    13,    14,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    12,    44,    -1,    -1,
      47,    17,    18,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    12,    44,    14,
      -1,    47,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    -1,    44,
      12,    -1,    47,    -1,    16,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    12,    44,    -1,    -1,    47,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    12,    44,    45,    -1,    47,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    12,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    12,    44,    29,    30,    47,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    12,
      29,    -1,    -1,    47,    -1,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    -1,    -1,    29,    -1,    47,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      -1,    -1,    -1,    -1,    47,    20,    21,    22,    23,    24,
      25,    26,    27,    -1,    29,     3,     4,     5,     6,     7,
       8,     9,    -1,    38,    39,    -1,    -1,    15,    16,    17,
      18,    19,    -1,    -1,    49,    50,    51,    -1,    53,    20,
      21,    22,    23,    24,    25,    26,    27,    -1,    29,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    38,    39,    -1,
      20,    21,    22,    23,    24,    25,    26,    27,    49,    29,
      51,    -1,    53,    -1,    -1,    -1,    -1,    -1,    38,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    49,
      -1,    51,    -1,    53
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    63,    66,     0,     3,     4,     5,     6,     7,     8,
       9,    15,    16,    17,    18,    19,    67,    68,    69,    70,
      74,    76,    83,    88,    89,    91,    92,    94,    96,    98,
      23,   133,    71,    72,   133,    20,    21,    22,    23,    24,
      25,    26,    27,    29,    38,    39,    49,    51,    53,   100,
     102,   103,   105,   106,   107,   112,   113,   118,   123,   125,
     127,   128,   129,   131,   100,    13,    73,    75,    84,   127,
     100,   100,    93,   100,    95,   100,    99,    14,    73,    79,
      64,    65,    65,    65,    65,    65,    12,    48,    28,   100,
     100,   100,   100,   101,   119,   114,    12,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    44,    47,    46,   104,   124,    53,    61,    23,    77,
      78,   133,    85,    86,   100,    48,    12,    14,    90,    90,
      13,    90,    13,    90,   100,   133,    23,    81,    70,    89,
     100,    72,   100,    50,    63,    23,    24,    27,    53,   120,
     121,   122,   132,   100,   115,   116,   100,    12,   100,   100,
     100,   100,   100,   100,   100,   100,   100,   100,   100,   100,
     100,   100,   100,    23,    49,    43,   100,    23,   131,    14,
      48,    28,    48,    10,    11,    87,   127,   127,    23,    24,
      27,   130,    23,    24,    27,   130,    23,   117,   117,   100,
     117,   100,   117,    16,    28,    82,    50,    45,   100,    52,
      48,    45,    54,    48,   100,    45,   100,   108,   109,   110,
     111,    53,    61,   126,    54,   133,    78,   100,    86,   118,
      90,    90,   100,   100,    80,   133,   100,    54,   122,   100,
     100,   100,    50,    63,    48,   100,    23,   131,    53,    61,
     117,   117,    17,    18,    97,    48,    45,   109,    54,   100,
      23,   131,   100,   133,   100,    54,    90,   117
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    62,    63,    63,    63,    63,    63,    63,    64,    64,
      65,    65,    66,    66,    67,    67,    67,    67,    67,    67,
      68,    69,    70,    71,    71,    72,    73,    75,    74,    76,
      76,    76,    76,    77,    77,    78,    79,    79,    80,    80,
      81,    82,    81,    84,    83,    85,    85,    86,    87,    87,
      87,    87,    88,    88,    89,    90,    90,    91,    92,    93,
      93,    94,    95,    95,    96,    97,    97,    99,    98,   100,
     101,   100,   100,   100,   100,   100,   100,   100,   100,   102,
     102,   104,   103,   105,   105,   105,   106,   106,   106,   106,
     106,   106,   106,   106,   106,   106,   106,   106,   106,   106,
     106,   107,   108,   108,   109,   110,   109,   111,   111,   112,
     112,   114,   113,   115,   115,   116,   116,   117,   117,   119,
     118,   120,   120,   121,   121,   122,   122,   122,   123,   124,
     123,   125,   125,   125,   125,   125,   126,   126,   126,   126,
     126,   126,   127,   127,   128,   128,   129,   129,   129,   129,
     129,   130,   130,   130,   131,   132,   132,   133
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     3,     3,     3,     3,     3,     0,     2,
       0,     2,     0,     2,     1,     1,     1,     1,     1,     1,
       4,     2,     2,     1,     3,     3,     4,     0,     3,     2,
       2,     3,     5,     1,     3,     3,     0,     2,     1,     3,
       0,     0,     3,     0,     3,     1,     3,     2,     0,     1,
       1,     1,     2,     4,     2,     2,     2,     4,     4,     3,
       5,     2,     3,     5,     2,     1,     1,     0,     9,     3,
       0,     4,     1,     1,     1,     1,     1,     1,     3,     1,
       3,     0,     5,     2,     2,     2,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       4,     5,     0,     1,     1,     0,     2,     1,     3,     1,
       1,     0,     4,     0,     1,     1,     3,     0,     2,     0,
       4,     0,     1,     1,     3,     3,     5,     3,     1,     0,
       4,     1,     1,     3,     3,     4,     2,     2,     3,     3,
       3,     4,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1
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
#line 203 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1756 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 3:
#line 205 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1763 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 4:
#line 207 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1770 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 5:
#line 209 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1777 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 6:
#line 211 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1784 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 7:
#line 213 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1791 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 8:
#line 218 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1798 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 9:
#line 220 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1805 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 10:
#line 225 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // still need to close the scope opened by the data-modification statement
      parser->ast()->scopes()->endNested();
    }
#line 1814 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 11:
#line 229 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // the RETURN statement will close the scope opened by the data-modification statement
    }
#line 1822 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 12:
#line 235 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1829 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 13:
#line 237 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1836 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 14:
#line 242 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1843 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 15:
#line 244 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1850 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 16:
#line 246 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1857 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 17:
#line 248 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1864 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 18:
#line 250 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1871 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 19:
#line 252 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1878 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 20:
#line 257 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
     
      auto node = parser->ast()->createNodeFor((yyvsp[-2].strval), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 1889 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 21:
#line 266 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // operand is a reference. can use it directly
      auto node = parser->ast()->createNodeFilter((yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 1899 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 22:
#line 274 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1906 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 23:
#line 279 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1913 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 24:
#line 281 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1920 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 25:
#line 286 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeLet((yyvsp[-2].strval), (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 1929 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 26:
#line 293 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! TRI_CaseEqualString((yyvsp[-2].strval), "COUNT")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'COUNT'", (yyvsp[-2].strval), yylloc.first_line, yylloc.first_column);
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 1941 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 27:
#line 303 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 1950 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 28:
#line 306 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    { 
      auto list = static_cast<AstNode*>(parser->popStack());

      if (list == nullptr) {
        ABORT_OOM
      }
      (yyval.node) = list;
    }
#line 1963 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 29:
#line 317 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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

      auto node = parser->ast()->createNodeCollectCount(parser->ast()->createNodeArray(), (yyvsp[0].strval));
      parser->ast()->addOperation(node);
    }
#line 1984 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 30:
#line 333 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = (yyvsp[-1].node)->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = (yyvsp[-1].node)->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      auto node = parser->ast()->createNodeCollectCount((yyvsp[-1].node), (yyvsp[0].strval));
      parser->ast()->addOperation(node);
    }
#line 2016 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 31:
#line 360 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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

      if ((yyvsp[-1].strval) == nullptr && 
          (yyvsp[0].node) != nullptr) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of 'KEEP' without 'INTO'", yylloc.first_line, yylloc.first_column);
      } 

      auto node = parser->ast()->createNodeCollect((yyvsp[-2].node), (yyvsp[-1].strval), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2053 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 32:
#line 392 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = (yyvsp[-4].node)->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = (yyvsp[-4].node)->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      auto node = parser->ast()->createNodeCollectExpression((yyvsp[-4].node), (yyvsp[-2].strval), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2085 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 33:
#line 422 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2092 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 34:
#line 424 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2099 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 35:
#line 429 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeAssign((yyvsp[-2].strval), (yyvsp[0].node));
      parser->pushArrayElement(node);
    }
#line 2108 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 36:
#line 436 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.strval) = nullptr;
    }
#line 2116 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 37:
#line 439 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2124 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 38:
#line 445 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
      parser->pushArrayElement(node);
    }
#line 2143 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 39:
#line 459 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
      parser->pushArrayElement(node);
    }
#line 2162 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 40:
#line 476 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = nullptr;
    }
#line 2170 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 41:
#line 479 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! TRI_CaseEqualString((yyvsp[0].strval), "KEEP")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'KEEP'", (yyvsp[0].strval), yylloc.first_line, yylloc.first_column);
      }

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2183 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 42:
#line 486 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2192 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 43:
#line 493 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2201 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 44:
#line 496 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      auto node = parser->ast()->createNodeSort(list);
      parser->ast()->addOperation(node);
    }
#line 2211 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 45:
#line 504 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2219 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 46:
#line 507 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2227 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 47:
#line 513 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeSortElement((yyvsp[-1].node), (yyvsp[0].node));
    }
#line 2235 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 48:
#line 519 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2243 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 49:
#line 522 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2251 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 50:
#line 525 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 2259 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 51:
#line 528 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2267 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 52:
#line 534 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto offset = parser->ast()->createNodeValueInt(0);
      auto node = parser->ast()->createNodeLimit(offset, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2277 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 53:
#line 539 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeLimit((yyvsp[-2].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2286 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 54:
#line 546 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeReturn((yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2296 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 55:
#line 554 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2304 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 56:
#line 557 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
       (yyval.node) = (yyvsp[0].node);
     }
#line 2312 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 57:
#line 563 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_REMOVE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->setWriteNode(node);
    }
#line 2325 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 58:
#line 574 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_INSERT, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->setWriteNode(node);
    }
#line 2338 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 59:
#line 585 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_UPDATE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->setWriteNode(node);
    }
#line 2352 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 60:
#line 594 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_UPDATE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->setWriteNode(node);
    }
#line 2366 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 61:
#line 606 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2373 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 62:
#line 611 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_REPLACE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->setWriteNode(node);
    }
#line 2387 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 63:
#line 620 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_REPLACE, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->setWriteNode(node);
    }
#line 2401 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 64:
#line 632 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2408 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 65:
#line 637 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_UPDATE);
    }
#line 2416 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 66:
#line 640 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_REPLACE);
    }
#line 2424 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 67:
#line 646 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    { 
      // reserve a variable named "$OLD", we might need it in the update expression
      // and in a later return thing
      parser->pushStack(parser->ast()->createNodeVariable("$OLD", true));
    }
#line 2434 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 68:
#line 650 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery(AQL_QUERY_UPSERT, (yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* variableNode = static_cast<AstNode*>(parser->popStack());
      
      auto scopes = parser->ast()->scopes();
      
      scopes->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
      
      scopes->start(triagens::aql::AQL_SCOPE_FOR);
      std::string const variableName = parser->ast()->variables()->nextName();
      auto forNode = parser->ast()->createNodeFor(variableName.c_str(), (yyvsp[-1].node), false);
      parser->ast()->addOperation(forNode);

      if ((yyvsp[-6].node) == nullptr || (yyvsp[-6].node)->type != NODE_TYPE_OBJECT) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "expecting object literal for upsert search document", yylloc.first_line, yylloc.first_column);
      }

      auto filterNode = parser->ast()->createNodeUpsertFilter(parser->ast()->createNodeReference(variableName.c_str()), (yyvsp[-6].node));
      parser->ast()->addOperation(filterNode);
      
      auto offsetValue = parser->ast()->createNodeValueInt(0);
      auto limitValue = parser->ast()->createNodeValueInt(1);
      auto limitNode = parser->ast()->createNodeLimit(offsetValue, limitValue);
      parser->ast()->addOperation(limitNode);
      
      auto refNode = parser->ast()->createNodeReference(variableName.c_str());
      auto returnNode = parser->ast()->createNodeReturn(refNode);
      parser->ast()->addOperation(returnNode);
      scopes->endNested();

      AstNode* subqueryNode = parser->ast()->endSubQuery();
      scopes->endCurrent();
      
      std::string const subqueryName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(subqueryName.c_str(), subqueryNode, false);
      parser->ast()->addOperation(subQuery);
      
      auto index = parser->ast()->createNodeValueInt(0);
      auto firstDoc = parser->ast()->createNodeLet(variableNode, parser->ast()->createNodeIndexedAccess(parser->ast()->createNodeReference(subqueryName.c_str()), index));
      parser->ast()->addOperation(firstDoc);

      auto node = parser->ast()->createNodeUpsert(static_cast<AstNodeType>((yyvsp[-3].intval)), parser->ast()->createNodeReference("$OLD"), (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->setWriteNode(node);
    }
#line 2488 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 69:
#line 702 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[-1].node);
    }
#line 2496 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 70:
#line 705 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (parser->isModificationQuery()) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected subquery after data-modification operation", yylloc.first_line, yylloc.first_column);
      }
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 2508 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 71:
#line 711 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName.c_str());
    }
#line 2523 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 72:
#line 721 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2531 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 73:
#line 724 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2539 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 74:
#line 727 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2547 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 75:
#line 730 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2555 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 76:
#line 733 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2563 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 77:
#line 736 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2571 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 78:
#line 739 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeRange((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2579 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 79:
#line 745 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.strval) = (yyvsp[0].strval);

      if ((yyval.strval) == nullptr) {
        ABORT_OOM
      }
    }
#line 2591 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 80:
#line 752 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 2610 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 81:
#line 769 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushStack((yyvsp[0].strval));

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2621 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 82:
#line 774 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall(static_cast<char const*>(parser->popStack()), list);
    }
#line 2630 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 83:
#line 781 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_PLUS, (yyvsp[0].node));
    }
#line 2638 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 84:
#line 784 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_MINUS, (yyvsp[0].node));
    }
#line 2646 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 85:
#line 787 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    { 
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, (yyvsp[0].node));
    }
#line 2654 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 86:
#line 793 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2662 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 87:
#line 796 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2670 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 88:
#line 799 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_PLUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2678 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 89:
#line 802 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MINUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2686 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 90:
#line 805 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_TIMES, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2694 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 91:
#line 808 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_DIV, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2702 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 92:
#line 811 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MOD, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2710 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 93:
#line 814 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2718 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 94:
#line 817 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2726 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 95:
#line 820 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2734 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 96:
#line 823 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2742 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 97:
#line 826 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2750 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 98:
#line 829 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2758 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 99:
#line 832 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2766 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 100:
#line 835 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, (yyvsp[-3].node), (yyvsp[0].node));
    }
#line 2774 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 101:
#line 841 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2782 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 102:
#line 847 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2789 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 103:
#line 849 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2796 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 104:
#line 854 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2804 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 105:
#line 857 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (parser->isModificationQuery()) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected subquery after data-modification operation", yylloc.first_line, yylloc.first_column);
      }
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 2816 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 106:
#line 863 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName.c_str());
    }
#line 2831 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 107:
#line 876 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2839 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 108:
#line 879 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2847 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 109:
#line 885 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2855 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 110:
#line 888 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2863 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 111:
#line 894 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2872 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 112:
#line 897 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 2880 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 113:
#line 903 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2887 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 114:
#line 905 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2894 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 115:
#line 910 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2902 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 116:
#line 913 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2910 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 117:
#line 919 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = nullptr;
    }
#line 2918 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 118:
#line 922 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[-1].strval) == nullptr || (yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[-1].strval), "OPTIONS")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'OPTIONS'", (yyvsp[-1].strval), yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 2934 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 119:
#line 936 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeObject();
      parser->pushStack(node);
    }
#line 2943 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 120:
#line 939 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 2951 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 121:
#line 945 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2958 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 122:
#line 947 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2965 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 123:
#line 952 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2972 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 124:
#line 954 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2979 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 125:
#line 959 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushObjectElement((yyvsp[-2].strval), (yyvsp[0].node));
    }
#line 2987 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 126:
#line 962 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushObjectElement((yyvsp[-3].node), (yyvsp[0].node));
    }
#line 2995 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 127:
#line 965 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[-2].strval) == nullptr) {
        ABORT_OOM
      }
      
      if (strlen((yyvsp[-2].strval)) < 1 || (yyvsp[-2].strval)[0] == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[-2].strval), yylloc.first_line, yylloc.first_column);
      }

      auto param = parser->ast()->createNodeParameter((yyvsp[-2].strval));
      parser->pushObjectElement(param, (yyvsp[0].node));
    }
#line 3012 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 128:
#line 980 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // start of reference (collection or variable name)
      (yyval.node) = (yyvsp[0].node);
    }
#line 3021 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 129:
#line 984 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // expanded variable access, e.g. variable[*]

      // create a temporary iterator variable
      std::string const nextName = parser->ast()->variables()->nextName() + "_";
      char const* iteratorName = nextName.c_str();
      auto iterator = parser->ast()->createNodeIterator(iteratorName, (yyvsp[0].node));

      parser->pushStack(iterator);
      parser->pushStack(parser->ast()->createNodeReference(iteratorName));
    }
#line 3037 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 130:
#line 994 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // return from the "expansion" subrule

      // push the expand node into the statement list
      auto iterator = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeExpand(iterator, (yyvsp[0].node));

      if ((yyval.node) == nullptr) {
        ABORT_OOM
      }
    }
#line 3053 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 131:
#line 1008 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // variable or collection
      auto ast = parser->ast();
      AstNode* node = nullptr;

      auto variable = ast->scopes()->getVariable((yyvsp[0].strval));
      
      if (variable != nullptr) {
        // variable exists, now use it
        node = ast->createNodeReference(variable);
      }
      else {
        // variable does not exist
        // now try variable aliases OLD (= $OLD) and NEW (= $NEW)
        if (strcmp((yyvsp[0].strval), "OLD") == 0) {
          variable = ast->scopes()->getVariable("$OLD");
        }
        else if (strcmp((yyvsp[0].strval), "NEW") == 0) {
          variable = ast->scopes()->getVariable("$NEW");
        }
        
        if (variable != nullptr) {
          // variable alias exists, now use it
          node = ast->createNodeReference(variable);
        }
      }

      if (node == nullptr) {
        // variable not found. so it must have been a collection
        node = ast->createNodeCollection((yyvsp[0].strval), TRI_TRANSACTION_READ);
      }

      TRI_ASSERT(node != nullptr);

      (yyval.node) = node;
    }
#line 3094 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 132:
#line 1044 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
      
      if ((yyval.node) == nullptr) {
        ABORT_OOM
      }
    }
#line 3106 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 133:
#line 1051 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // named variable access, e.g. variable.reference
      (yyval.node) = parser->ast()->createNodeAttributeAccess((yyvsp[-2].node), (yyvsp[0].strval));
    }
#line 3115 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 134:
#line 1055 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // named variable access, e.g. variable.@reference
      (yyval.node) = parser->ast()->createNodeBoundAttributeAccess((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3124 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 135:
#line 1059 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // indexed variable access, e.g. variable[index]
      (yyval.node) = parser->ast()->createNodeIndexedAccess((yyvsp[-3].node), (yyvsp[-1].node));
    }
#line 3133 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 136:
#line 1066 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // named variable access, continuation from * expansion, e.g. [*].variable.reference
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeAttributeAccess(node, (yyvsp[0].strval));
    }
#line 3143 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 137:
#line 1071 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.@reference
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeBoundAttributeAccess(node, (yyvsp[0].node));
    }
#line 3153 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 138:
#line 1076 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable[index]
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeIndexedAccess(node, (yyvsp[-1].node));
    }
#line 3163 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 139:
#line 1081 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // named variable access, continuation from * expansion, e.g. [*].variable.xx.reference
      (yyval.node) = parser->ast()->createNodeAttributeAccess((yyvsp[-2].node), (yyvsp[0].strval));
    }
#line 3172 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 140:
#line 1085 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.xx.@reference
      (yyval.node) = parser->ast()->createNodeBoundAttributeAccess((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3181 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 141:
#line 1089 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable.xx.[index]
      (yyval.node) = parser->ast()->createNodeIndexedAccess((yyvsp[-3].node), (yyvsp[-1].node));
    }
#line 3190 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 142:
#line 1096 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3198 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 143:
#line 1099 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3206 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 144:
#line 1105 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }
      
      (yyval.node) = (yyvsp[0].node);
    }
#line 3218 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 145:
#line 1112 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3230 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 146:
#line 1122 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval)); 
    }
#line 3238 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 147:
#line 1125 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3246 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 148:
#line 1128 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueNull();
    }
#line 3254 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 149:
#line 1131 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 3262 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 150:
#line 1134 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 3270 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 151:
#line 1140 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval), TRI_TRANSACTION_WRITE);
    }
#line 3282 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 152:
#line 1147 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval), TRI_TRANSACTION_WRITE);
    }
#line 3294 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 153:
#line 1154 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }
      
      if (strlen((yyvsp[0].strval)) < 2 || (yyvsp[0].strval)[0] != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[0].strval), yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval));
    }
#line 3310 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 154:
#line 1168 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval));
    }
#line 3318 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 155:
#line 1174 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3330 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 156:
#line 1181 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].strval) == nullptr) {
        ABORT_OOM
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3342 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 157:
#line 1190 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3350 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;


#line 3354 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
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
