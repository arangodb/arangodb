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
#define yyparse         Ahuacatlparse
#define yylex           Ahuacatllex
#define yyerror         Ahuacatlerror
#define yydebug         Ahuacatldebug
#define yynerrs         Ahuacatlnerrs


/* Copy the first part of user declarations.  */
#line 10 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:339  */

#include <stdio.h>
#include <stdlib.h>

#include <Basics/Common.h>
#include <Basics/conversions.h>
#include <Basics/tri-strings.h>

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-error.h"
#include "Ahuacatl/ahuacatl-parser.h"
#include "Ahuacatl/ahuacatl-parser-functions.h"
#include "Ahuacatl/ahuacatl-scope.h"

#line 88 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:339  */

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
   by #include "ahuacatl-grammar.hpp".  */
#ifndef YY_AHUACATL_ARANGOD_AHUACATL_AHUACATL_GRAMMAR_HPP_INCLUDED
# define YY_AHUACATL_ARANGOD_AHUACATL_AHUACATL_GRAMMAR_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int Ahuacatldebug;
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
#line 26 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:355  */

  TRI_aql_node_t* node;
  char* strval;
  bool boolval;
  int64_t intval;

#line 195 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:355  */
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



int Ahuacatlparse (TRI_aql_context_t* const context);

#endif /* !YY_AHUACATL_ARANGOD_AHUACATL_AHUACATL_GRAMMAR_HPP_INCLUDED  */

/* Copy the second part of user declarations.  */
#line 33 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:358  */


////////////////////////////////////////////////////////////////////////////////
/// @brief forward for lexer function defined in ahuacatl-tokens.l
////////////////////////////////////////////////////////////////////////////////

int Ahuacatllex (YYSTYPE*, YYLTYPE*, void*);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief register parse error
////////////////////////////////////////////////////////////////////////////////

void Ahuacatlerror (YYLTYPE* locp, TRI_aql_context_t* const context, const char* err) {
  TRI_SetErrorParseAql(context, err, locp->first_line, locp->first_column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro for signalling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM                                                                     \
  TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_OUT_OF_MEMORY, NULL); \
  YYABORT;

#define scanner context->_parser->_scanner


#line 250 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:358  */

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
#define YYLAST   518

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  60
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  59
/* YYNRULES -- Number of rules.  */
#define YYNRULES  130
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  216

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
       0,   193,   193,   196,   199,   202,   205,   211,   213,   218,
     220,   222,   224,   226,   228,   233,   252,   265,   270,   272,
     277,   291,   291,   316,   318,   323,   336,   339,   345,   345,
     368,   373,   381,   392,   395,   398,   404,   415,   428,   445,
     448,   454,   473,   492,   508,   527,   543,   562,   565,   565,
     606,   609,   612,   615,   618,   621,   624,   655,   662,   676,
     676,   705,   713,   721,   732,   740,   748,   756,   764,   772,
     780,   788,   796,   804,   812,   820,   828,   836,   847,   859,
     861,   866,   871,   879,   882,   888,   888,   901,   903,   908,
     913,   921,   924,   939,   939,   952,   954,   959,   961,   966,
     974,   978,   978,  1036,  1053,  1060,  1068,  1076,  1087,  1097,
    1107,  1117,  1125,  1133,  1144,  1147,  1153,  1156,  1181,  1190,
    1193,  1202,  1211,  1223,  1237,  1251,  1274,  1286,  1293,  1302,
    1308
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
  "collect_list", "collect_element", "optional_into", "sort_statement",
  "$@2", "sort_list", "sort_element", "sort_direction", "limit_statement",
  "return_statement", "in_or_into_collection", "remove_statement",
  "insert_statement", "update_statement", "replace_statement",
  "expression", "$@3", "function_name", "function_call", "$@4",
  "operator_unary", "operator_binary", "operator_ternary",
  "optional_function_call_arguments", "function_arguments_list",
  "compound_type", "list", "$@5", "optional_list_elements",
  "list_elements_list", "query_options", "array", "$@6",
  "optional_array_elements", "array_elements_list", "array_element",
  "reference", "$@7", "single_reference", "expansion", "atomic_value",
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

#define YYTABLE_NINF -126

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     -99,    24,   252,   -99,     3,     3,   430,   430,   -99,   -99,
     203,   430,   430,   430,   430,   -99,   -99,   -99,   -99,   -99,
     -99,   -99,   -99,   -99,   -99,   -99,   -99,   -99,    27,     1,
     -99,    14,   -99,   -99,   -99,   -15,   -99,   -99,   -99,   -99,
     430,   430,   430,   430,   -99,   -99,   402,    12,   -99,   -99,
     -99,   -99,   -99,   -99,   -99,    20,   -32,   -99,   -99,   -99,
     -99,   -99,   402,     3,   430,    11,   342,   342,   277,   312,
     430,     3,   430,   -99,   -99,   -99,   202,   -99,     9,   430,
     430,   430,   430,   430,   430,   430,   430,   430,   430,   430,
     430,   430,   430,   430,   430,   430,    41,    23,    30,   430,
      -3,     0,   -99,    64,    45,   -99,   242,   203,   451,    28,
      72,    72,   430,    72,   430,    72,   402,   -99,   402,   -99,
      46,   -99,   -99,    48,    49,   -99,    56,   402,    50,    57,
     259,   328,   154,   358,   358,     5,     5,     5,     5,    29,
      29,   -99,   -99,   -99,   372,   475,   -99,   430,   -31,    89,
     -99,   -99,     3,     3,   -99,   430,   430,   -99,   -99,   -99,
     -99,     4,    16,    18,   -99,   -99,   -99,   -99,   -99,    55,
     -99,   -99,   342,   -99,   342,   -99,   -99,   -99,     9,   430,
     -99,   430,   430,   402,    58,    65,   430,    33,   -30,   -99,
     -99,   -99,   402,   -99,   -99,    72,    72,   -99,   402,   402,
     475,   -99,   430,   124,   -99,   -99,   430,    34,   -99,   -99,
     402,   -99,   172,   -99,   -99,   -99
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       7,     0,     0,     1,     0,     0,     0,     0,    21,    28,
       0,     0,     0,     0,     0,     8,     9,    11,    10,    12,
      13,    14,     2,     3,     4,     5,     6,   129,     0,    17,
      18,     0,   120,   121,   122,   103,   118,   130,   117,   126,
       0,     0,     0,    48,    93,    85,    16,    59,   104,    50,
      51,    52,    53,    83,    84,    55,   100,    54,   119,   114,
     115,   116,    38,     0,     0,    36,     0,     0,     0,     0,
       0,     0,     0,    63,    61,    62,     0,     7,    95,    87,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    26,    23,     0,    29,    30,    33,     0,     0,     0,
      91,    91,     0,    91,     0,    91,    15,    19,    20,    47,
       0,   127,   128,     0,    96,    97,     0,    89,     0,    88,
      77,    65,    64,    71,    72,    73,    74,    75,    76,    66,
      67,    68,    69,    70,     0,    56,    58,    79,     0,     0,
     105,   106,     0,     0,    22,     0,     0,    34,    35,    32,
      37,   103,   118,   126,    39,   123,   124,   125,    40,     0,
      41,    42,     0,    43,     0,    45,    49,    94,     0,     0,
      86,     0,     0,    81,     0,    80,     0,     0,   102,   107,
      27,    24,    25,    31,    92,    91,    91,    98,    99,    90,
      78,    60,     0,     0,   108,   109,     0,     0,    44,    46,
      82,   110,     0,   111,   112,   113
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -99,    36,   -99,   -99,   -99,   -99,   -99,   -99,    40,   -99,
     -99,   -99,   -38,   -99,   -99,   -99,   -99,   -39,   -99,   -99,
     -99,   -58,   -99,   -99,   -99,   -99,    -6,   -99,   -99,   -99,
     -99,   -99,   -99,   -99,   -99,   -99,   -99,   -99,   -99,   -99,
     -99,   -98,   -36,   -99,   -99,   -99,   -47,   -99,   -99,   -99,
     -99,     2,   -99,   -99,    25,   -97,   -99,    -2,   -99
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    15,    16,    17,    18,    29,    30,    19,
      63,   101,   102,   154,    20,    64,   104,   105,   159,    21,
      22,   110,    23,    24,    25,    26,   106,    77,    47,    48,
      97,    49,    50,    51,   184,   185,    52,    53,    79,   128,
     129,   170,    54,    78,   123,   124,   125,    55,    98,    56,
     188,    57,    58,    59,   164,    60,   126,    31,    61
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      46,    62,    28,   151,  -123,    66,    67,    68,    69,   111,
     113,   115,    65,   171,   152,   173,  -124,   175,  -125,   150,
      99,   186,   206,    39,     3,    27,  -123,   100,   187,   207,
     -57,   121,   122,   -57,    73,    74,    75,    76,  -124,    70,
    -125,    72,    89,    90,    91,    92,    93,   153,    71,   -57,
     165,   166,   -57,  -123,   167,   204,   213,    96,   107,    39,
      39,   103,  -101,   146,   116,  -124,   118,  -125,    91,    92,
      93,   147,   148,   127,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     205,   155,   156,   149,   169,   176,   178,   208,   209,   177,
     179,    80,   130,   180,   181,    44,   172,   201,   174,   160,
     214,   117,   202,   120,   195,   191,   196,   193,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,   197,    94,   194,   168,    95,    80,     0,     0,     0,
       0,   183,   189,     0,     0,     0,     0,     0,     0,   192,
     190,   103,     0,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    80,    94,     0,     0,
      95,     0,     0,   198,     0,   199,   200,   211,     0,     0,
     203,     0,     0,    81,    80,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,   210,     0,     0,     0,
     212,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    80,    94,     0,     0,    95,     0,
       0,     0,    32,    33,    34,   215,    36,    37,    38,    39,
       0,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,     0,    94,     0,     0,    95,     0,
       0,   119,   157,   158,    80,     4,     5,     6,     7,     8,
       9,    10,     0,     0,     0,     0,     0,    11,    12,    13,
      14,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,     0,    94,     0,     0,    95,   108,
     112,   109,    85,    86,    87,    88,    89,    90,    91,    92,
      93,     0,     0,     0,     0,     0,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,     0,
      94,     0,     0,    95,   108,   114,   109,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,   108,    94,   109,     0,    95,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    80,    94,     0,     0,    95,     0,
       0,    85,    86,    87,    88,    89,    90,    91,    92,    93,
       0,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    80,    94,   182,     0,    95,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,     0,    94,     0,     0,    95,    32,
      33,    34,    35,    36,    37,    38,    39,     0,    40,     0,
       0,     0,     0,     0,     0,     0,     0,    41,    42,     0,
      32,    33,    34,   161,   162,    37,    38,   163,    43,    40,
      44,     0,    45,     0,     0,     0,     0,    80,    41,    42,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    43,
       0,    44,     0,    45,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,     0,    94
};

static const yytype_int16 yycheck[] =
{
       6,     7,     4,   100,     0,    11,    12,    13,    14,    67,
      68,    69,    10,   111,    14,   113,     0,   115,     0,    22,
      52,    52,    52,    26,     0,    22,    22,    59,    59,    59,
      45,    22,    23,    48,    40,    41,    42,    43,    22,    12,
      22,    27,    37,    38,    39,    40,    41,    47,    47,    45,
      22,    23,    48,    49,    26,    22,    22,    45,    47,    26,
      26,    63,    42,    22,    70,    49,    72,    49,    39,    40,
      41,    48,    42,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
     187,    27,    47,    99,    22,    49,    47,   195,   196,    51,
      44,    12,   108,    53,    47,    50,   112,    49,   114,   107,
     207,    71,    47,    77,   172,   153,   174,   156,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,   178,    43,   169,   109,    46,    12,    -1,    -1,    -1,
      -1,   147,    53,    -1,    -1,    -1,    -1,    -1,    -1,   155,
     152,   153,    -1,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    12,    43,    -1,    -1,
      46,    -1,    -1,   179,    -1,   181,   182,    53,    -1,    -1,
     186,    -1,    -1,    29,    12,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,   202,    -1,    -1,    -1,
     206,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    12,    43,    -1,    -1,    46,    -1,
      -1,    -1,    19,    20,    21,    53,    23,    24,    25,    26,
      -1,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    -1,    43,    -1,    -1,    46,    -1,
      -1,    49,    10,    11,    12,     3,     4,     5,     6,     7,
       8,     9,    -1,    -1,    -1,    -1,    -1,    15,    16,    17,
      18,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    -1,    43,    -1,    -1,    46,    12,
      13,    14,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    -1,    -1,    -1,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    -1,
      43,    -1,    -1,    46,    12,    13,    14,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      12,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    12,    43,    14,    -1,    46,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    12,    43,    -1,    -1,    46,    -1,
      -1,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      -1,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    12,    43,    44,    -1,    46,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    -1,    43,    -1,    -1,    46,    19,
      20,    21,    22,    23,    24,    25,    26,    -1,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,
      19,    20,    21,    22,    23,    24,    25,    26,    48,    28,
      50,    -1,    52,    -1,    -1,    -1,    -1,    12,    37,    38,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      -1,    50,    -1,    52,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    -1,    43
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    61,    62,     0,     3,     4,     5,     6,     7,     8,
       9,    15,    16,    17,    18,    63,    64,    65,    66,    69,
      74,    79,    80,    82,    83,    84,    85,    22,   117,    67,
      68,   117,    19,    20,    21,    22,    23,    24,    25,    26,
      28,    37,    38,    48,    50,    52,    86,    88,    89,    91,
      92,    93,    96,    97,   102,   107,   109,   111,   112,   113,
     115,   118,    86,    70,    75,   111,    86,    86,    86,    86,
      12,    47,    27,    86,    86,    86,    86,    87,   103,    98,
      12,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    43,    46,    45,    90,   108,    52,
      59,    71,    72,   117,    76,    77,    86,    47,    12,    14,
      81,    81,    13,    81,    13,    81,    86,    68,    86,    49,
      61,    22,    23,   104,   105,   106,   116,    86,    99,   100,
      86,    86,    86,    86,    86,    86,    86,    86,    86,    86,
      86,    86,    86,    86,    86,    86,    22,    48,    42,    86,
      22,   115,    14,    47,    73,    27,    47,    10,    11,    78,
     111,    22,    23,    26,   114,    22,    23,    26,   114,    22,
     101,   101,    86,   101,    86,   101,    49,    51,    47,    44,
      53,    47,    44,    86,    94,    95,    52,    59,   110,    53,
     117,    72,    86,    77,   102,    81,    81,   106,    86,    86,
      86,    49,    47,    86,    22,   115,    52,    59,   101,   101,
      86,    53,    86,    22,   115,    53
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    60,    61,    61,    61,    61,    61,    62,    62,    63,
      63,    63,    63,    63,    63,    64,    65,    66,    67,    67,
      68,    70,    69,    71,    71,    72,    73,    73,    75,    74,
      76,    76,    77,    78,    78,    78,    79,    79,    80,    81,
      81,    82,    83,    84,    84,    85,    85,    86,    87,    86,
      86,    86,    86,    86,    86,    86,    86,    88,    88,    90,
      89,    91,    91,    91,    92,    92,    92,    92,    92,    92,
      92,    92,    92,    92,    92,    92,    92,    92,    93,    94,
      94,    95,    95,    96,    96,    98,    97,    99,    99,   100,
     100,   101,   101,   103,   102,   104,   104,   105,   105,   106,
     107,   108,   107,   109,   109,   109,   109,   109,   110,   110,
     110,   110,   110,   110,   111,   111,   112,   112,   113,   113,
     113,   113,   113,   114,   114,   114,   115,   116,   116,   117,
     118
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     2,     2,     2,     0,     2,     1,
       1,     1,     1,     1,     1,     4,     2,     2,     1,     3,
       3,     0,     4,     1,     3,     3,     0,     2,     0,     3,
       1,     3,     2,     0,     1,     1,     2,     4,     2,     2,
       2,     4,     4,     4,     6,     4,     6,     3,     0,     4,
       1,     1,     1,     1,     1,     1,     3,     1,     3,     0,
       5,     2,     2,     2,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     5,     0,
       1,     1,     3,     1,     1,     0,     4,     0,     1,     1,
       3,     0,     2,     0,     4,     0,     1,     1,     3,     3,
       1,     0,     4,     1,     1,     3,     3,     4,     2,     2,
       3,     3,     3,     4,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1
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
      yyerror (&yylloc, context, YY_("syntax error: cannot back up")); \
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
                  Type, Value, Location, context); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, TRI_aql_context_t* const context)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (yylocationp);
  YYUSE (context);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, TRI_aql_context_t* const context)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, context);
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, TRI_aql_context_t* const context)
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
                       , &(yylsp[(yyi + 1) - (yynrhs)])                       , context);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, context); \
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, TRI_aql_context_t* const context)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (context);
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
yyparse (TRI_aql_context_t* const context)
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
#line 193 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      context->_type = TRI_AQL_QUERY_READ;
    }
#line 1674 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 3:
#line 196 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      context->_type = TRI_AQL_QUERY_REMOVE;
    }
#line 1682 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 4:
#line 199 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      context->_type = TRI_AQL_QUERY_INSERT;
    }
#line 1690 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 202 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      context->_type = TRI_AQL_QUERY_UPDATE;
    }
#line 1698 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 205 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      context->_type = TRI_AQL_QUERY_REPLACE;
    }
#line 1706 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 211 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1713 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 8:
#line 213 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1720 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 218 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1727 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 220 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1734 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 222 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1741 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 224 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1748 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 13:
#line 226 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1755 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 14:
#line 228 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1762 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 233 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;
      
      if (! TRI_StartScopeAql(context, TRI_AQL_SCOPE_FOR)) {
        ABORT_OOM
      }
      
      node = TRI_CreateNodeForAql(context, (yyvsp[-2].strval), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
#line 1783 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 252 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeFilterAql(context, (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
#line 1798 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 265 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1805 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 270 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1812 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 272 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1819 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 277 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeLetAql(context, (yyvsp[-2].strval), (yyvsp[0].node));

      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
#line 1835 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 291 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
#line 1849 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 299 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeCollectAql(
                context, 
                static_cast<const TRI_aql_node_t* const>
                           (TRI_PopStackParseAql(context)), 
                (yyvsp[0].strval));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
#line 1868 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 316 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1875 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 318 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 1882 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 323 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeAssignAql(context, (yyvsp[-2].strval), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      if (! TRI_PushListAql(context, node)) {
        ABORT_OOM
      }
    }
#line 1897 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 336 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = NULL;
    }
#line 1905 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 339 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 1913 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 345 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
#line 1927 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 353 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* list 
          = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));
      TRI_aql_node_t* node = TRI_CreateNodeSortAql(context, list);
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
#line 1944 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 368 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_PushListAql(context, (yyvsp[0].node))) {
        ABORT_OOM
      }
    }
#line 1954 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 373 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_PushListAql(context, (yyvsp[0].node))) {
        ABORT_OOM
      }
    }
#line 1964 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 381 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeSortElementAql(context, (yyvsp[-1].node), (yyvsp[0].boolval));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 1977 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 392 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.boolval) = true;
    }
#line 1985 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 395 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.boolval) = true;
    }
#line 1993 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 398 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.boolval) = false;
    }
#line 2001 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 404 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, TRI_CreateNodeValueIntAql(context, 0), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
    }
#line 2017 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 415 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
#line 2032 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 428 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeReturnAql(context, (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      if (! TRI_EndScopeByReturnAql(context)) {
        ABORT_OOM
      }
    }
#line 2051 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 445 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2059 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 448 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2067 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 454 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeRemoveAql(context, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      if (! TRI_EndScopeByReturnAql(context)) {
        ABORT_OOM
      }
    }
#line 2088 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 473 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeInsertAql(context, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      if (! TRI_EndScopeByReturnAql(context)) {
        ABORT_OOM
      }
    }
#line 2109 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 492 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeUpdateAql(context, NULL, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      if (! TRI_EndScopeByReturnAql(context)) {
        ABORT_OOM
      }
    }
#line 2130 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 508 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeUpdateAql(context, (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      if (! TRI_EndScopeByReturnAql(context)) {
        ABORT_OOM
      }
    }
#line 2151 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 527 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeReplaceAql(context, NULL, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      if (! TRI_EndScopeByReturnAql(context)) {
        ABORT_OOM
      }
    }
#line 2172 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 46:
#line 543 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeReplaceAql(context, (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      if (! TRI_EndScopeByReturnAql(context)) {
        ABORT_OOM
      }
    }
#line 2193 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 562 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[-1].node);
    }
#line 2201 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 48:
#line 565 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_StartScopeAql(context, TRI_AQL_SCOPE_SUBQUERY)) {
        ABORT_OOM
      }

      context->_subQueries++;

    }
#line 2214 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 572 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* result;
      TRI_aql_node_t* subQuery;
      TRI_aql_node_t* nameNode;
      
      context->_subQueries--;

      if (! TRI_EndScopeAql(context)) {
        ABORT_OOM
      }

      subQuery = TRI_CreateNodeSubqueryAql(context);

      if (subQuery == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, subQuery)) {
        ABORT_OOM
      }

      nameNode = TRI_AQL_NODE_MEMBER(subQuery, 0);
      if (nameNode == NULL) {
        ABORT_OOM
      }

      result = TRI_CreateNodeReferenceAql(context, TRI_AQL_NODE_STRING(nameNode));
      if (result == NULL) {
        ABORT_OOM
      }

      // return the result
      (yyval.node) = result;
    }
#line 2253 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 606 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2261 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 609 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2269 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 52:
#line 612 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2277 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 615 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2285 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 618 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2293 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 621 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2301 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 624 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;
      TRI_aql_node_t* list;

      if ((yyvsp[-2].node) == NULL || (yyvsp[0].node) == NULL) {
        ABORT_OOM
      }
      
      list = TRI_CreateNodeListAql(context);
      if (list == NULL) {
        ABORT_OOM
      }
       
      if (TRI_ERROR_NO_ERROR != TRI_PushBackVectorPointer(&list->_members, (void*) (yyvsp[-2].node))) {
        ABORT_OOM
      }
      if (TRI_ERROR_NO_ERROR != TRI_PushBackVectorPointer(&list->_members, (void*) (yyvsp[0].node))) {
        ABORT_OOM
      }
      
      node = TRI_CreateNodeFcallAql(context, "RANGE", list);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2334 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 655 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);

      if ((yyval.strval) == NULL) {
        ABORT_OOM
      }
    }
#line 2346 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 662 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[-2].strval) == NULL || (yyvsp[0].strval) == NULL) {
        ABORT_OOM
      }

      (yyval.strval) = TRI_RegisterString3Aql(context, (yyvsp[-2].strval), "::", (yyvsp[0].strval));

      if ((yyval.strval) == NULL) {
        ABORT_OOM
      }
    }
#line 2362 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 676 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      if (! TRI_PushStackParseAql(context, (yyvsp[0].strval))) {
        ABORT_OOM
      }

      node = TRI_CreateNodeListAql(context);
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
#line 2381 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 689 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* list 
        = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));
      TRI_aql_node_t* node = TRI_CreateNodeFcallAql(context, 
                                    static_cast<char const* const>
                                               (TRI_PopStackParseAql(context)),
                                    list);
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2399 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 705 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryPlusAql(context, (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2412 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 713 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryMinusAql(context, (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2425 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 721 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    { 
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryNotAql(context, (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2438 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 732 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryOrAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2451 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 740 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryAndAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2464 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 748 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryPlusAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2477 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 756 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryMinusAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2490 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 764 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryTimesAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2503 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 772 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryDivAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2516 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 780 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryModAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2529 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 788 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryEqAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2542 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 796 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryNeAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2555 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 804 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLtAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2568 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 74:
#line 812 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGtAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2581 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 75:
#line 820 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLeAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2594 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 76:
#line 828 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGeAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2607 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 77:
#line 836 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryInAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2620 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 847 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorTernaryAql(context, (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2634 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 859 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 2641 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 861 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 2648 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 866 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_PushListAql(context, (yyvsp[0].node))) {
        ABORT_OOM
      }
    }
#line 2658 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 82:
#line 871 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_PushListAql(context, (yyvsp[0].node))) {
        ABORT_OOM
      }
    }
#line 2668 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 83:
#line 879 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2676 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 882 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2684 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 85:
#line 888 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
#line 2697 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 86:
#line 895 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));
    }
#line 2705 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 87:
#line 901 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 2712 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 88:
#line 903 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 2719 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 89:
#line 908 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_PushListAql(context, (yyvsp[0].node))) {
        ABORT_OOM
      }
    }
#line 2729 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 90:
#line 913 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_PushListAql(context, (yyvsp[0].node))) {
        ABORT_OOM
      }
    }
#line 2739 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 91:
#line 921 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = NULL;
    }
#line 2747 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 92:
#line 924 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[-1].strval) == NULL || (yyvsp[0].node) == NULL) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[-1].strval), "OPTIONS")) {
        TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
        YYABORT;
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 2764 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 93:
#line 939 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeArrayAql(context);
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
#line 2777 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 94:
#line 946 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));
    }
#line 2785 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 95:
#line 952 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 2792 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 96:
#line 954 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 2799 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 97:
#line 959 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 2806 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 98:
#line 961 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
    }
#line 2813 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 99:
#line 966 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_PushArrayAql(context, (yyvsp[-2].strval), (yyvsp[0].node))) {
        ABORT_OOM
      }
    }
#line 2823 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 100:
#line 974 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // start of reference (collection or variable name)
      (yyval.node) = (yyvsp[0].node);
    }
#line 2832 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 101:
#line 978 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // expanded variable access, e.g. variable[*]
      TRI_aql_node_t* node;
      char* varname = TRI_GetNameParseAql(context);

      if (varname == NULL) {
        ABORT_OOM
      }
      
      // push the varname onto the stack
      TRI_PushStackParseAql(context, varname);
      
      // push on the stack what's going to be expanded (will be popped when we come back) 
      TRI_PushStackParseAql(context, (yyvsp[0].node));

      // create a temporary variable for the row iterator (will be popped by "expansion" rule")
      node = TRI_CreateNodeReferenceAql(context, varname);

      if (node == NULL) {
        ABORT_OOM
      }

      // push the variable
      TRI_PushStackParseAql(context, node);
    }
#line 2862 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 102:
#line 1002 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // return from the "expansion" subrule
      TRI_aql_node_t* expanded = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));
      TRI_aql_node_t* expand;
      TRI_aql_node_t* nameNode;
      char* varname = static_cast<char*>(TRI_PopStackParseAql(context));

      // push the actual expand node into the statement list
      expand = TRI_CreateNodeExpandAql(context, varname, expanded, (yyvsp[0].node));

      if (expand == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, expand)) {
        ABORT_OOM
      }

      nameNode = TRI_AQL_NODE_MEMBER(expand, 1);

      if (nameNode == NULL) {
        ABORT_OOM
      }

      // return a reference only
      (yyval.node) = TRI_CreateNodeReferenceAql(context, TRI_AQL_NODE_STRING(nameNode));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 2898 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 103:
#line 1036 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // variable or collection
      TRI_aql_node_t* node;
      
      if (TRI_VariableExistsScopeAql(context, (yyvsp[0].strval))) {
        node = TRI_CreateNodeReferenceAql(context, (yyvsp[0].strval));
      }
      else {
        node = TRI_CreateNodeCollectionAql(context, (yyvsp[0].strval));
      }

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 2920 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 104:
#line 1053 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 2932 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 105:
#line 1060 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // named variable access, e.g. variable.reference
      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, (yyvsp[-2].node), (yyvsp[0].strval));
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 2945 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 106:
#line 1068 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // named variable access, e.g. variable.@reference
      (yyval.node) = TRI_CreateNodeBoundAttributeAccessAql(context, (yyvsp[-2].node), (yyvsp[0].node));
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 2958 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 107:
#line 1076 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // indexed variable access, e.g. variable[index]
      (yyval.node) = TRI_CreateNodeIndexedAql(context, (yyvsp[-3].node), (yyvsp[-1].node));
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 2971 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 108:
#line 1087 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // named variable access, continuation from * expansion, e.g. [*].variable.reference
      TRI_aql_node_t* node = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));

      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, node, (yyvsp[0].strval));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 2986 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 109:
#line 1097 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.@reference
      TRI_aql_node_t* node = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));

      (yyval.node) = TRI_CreateNodeBoundAttributeAccessAql(context, node, (yyvsp[0].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 3001 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 110:
#line 1107 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable[index]
      TRI_aql_node_t* node = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));

      (yyval.node) = TRI_CreateNodeIndexedAql(context, node, (yyvsp[-1].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 3016 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 111:
#line 1117 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // named variable access, continuation from * expansion, e.g. [*].variable.xx.reference
      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, (yyvsp[-2].node), (yyvsp[0].strval));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 3029 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 112:
#line 1125 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.xx.@reference
      (yyval.node) = TRI_CreateNodeBoundAttributeAccessAql(context, (yyvsp[-2].node), (yyvsp[0].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 3042 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 113:
#line 1133 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable.xx.[index]
      (yyval.node) = TRI_CreateNodeIndexedAql(context, (yyvsp[-3].node), (yyvsp[-1].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
#line 3055 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 114:
#line 1144 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3063 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 115:
#line 1147 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3071 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 116:
#line 1153 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3079 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 117:
#line 1156 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;
      double value;

      if ((yyvsp[0].strval) == NULL) {
        ABORT_OOM
      }
      
      value = TRI_DoubleString((yyvsp[0].strval));

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
        YYABORT;
      }
      
      node = TRI_CreateNodeValueDoubleAql(context, value);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3107 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 118:
#line 1181 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueStringAql(context, (yyvsp[0].strval));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3121 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 119:
#line 1190 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3129 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 120:
#line 1193 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueNullAql(context);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3143 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 121:
#line 1202 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, true);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3157 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 122:
#line 1211 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, false);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3171 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 123:
#line 1223 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      if ((yyvsp[0].strval) == NULL) {
        ABORT_OOM
      }

      node = TRI_CreateNodeCollectionAql(context, (yyvsp[0].strval));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3190 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 124:
#line 1237 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      if ((yyvsp[0].strval) == NULL) {
        ABORT_OOM
      }

      node = TRI_CreateNodeCollectionAql(context, (yyvsp[0].strval));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3209 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 125:
#line 1251 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;

      if ((yyvsp[0].strval) == NULL) {
        ABORT_OOM
      }
      
      if (strlen((yyvsp[0].strval)) < 2 || (yyvsp[0].strval)[0] != '@') {
        TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, (yyvsp[0].strval));
        YYABORT;
      }

      node = TRI_CreateNodeParameterAql(context, (yyvsp[0].strval));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3234 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 126:
#line 1274 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node = TRI_CreateNodeParameterAql(context, (yyvsp[0].strval));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3248 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 127:
#line 1286 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval) == NULL) {
        ABORT_OOM
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3260 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 128:
#line 1293 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval) == NULL) {
        ABORT_OOM
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3272 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 129:
#line 1302 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3280 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;

  case 130:
#line 1308 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1646  */
    {
      TRI_aql_node_t* node;
      int64_t value;

      if ((yyvsp[0].strval) == NULL) {
        ABORT_OOM
      }

      value = TRI_Int64String((yyvsp[0].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
        YYABORT;
      }

      node = TRI_CreateNodeValueIntAql(context, value);
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
#line 3306 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
    break;


#line 3310 "arangod/Ahuacatl/ahuacatl-grammar.cpp" /* yacc.c:1646  */
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
      yyerror (&yylloc, context, YY_("syntax error"));
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
        yyerror (&yylloc, context, yymsgp);
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
                  yystos[yystate], yyvsp, yylsp, context);
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
  yyerror (&yylloc, context, YY_("memory exhausted"));
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
                  yytoken, &yylval, &yylloc, context);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
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
  return yyresult;
}
