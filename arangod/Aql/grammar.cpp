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
#include "Aql/CollectNode.h"
#include "Aql/Function.h"
#include "Aql/Parser.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"

#line 81 "arangod/Aql/grammar.cpp" /* yacc.c:339  */

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
    T_AGGREGATE = 270,
    T_GRAPH = 271,
    T_DISTINCT = 272,
    T_REMOVE = 273,
    T_INSERT = 274,
    T_UPDATE = 275,
    T_REPLACE = 276,
    T_UPSERT = 277,
    T_NULL = 278,
    T_TRUE = 279,
    T_FALSE = 280,
    T_STRING = 281,
    T_QUOTED_STRING = 282,
    T_INTEGER = 283,
    T_DOUBLE = 284,
    T_PARAMETER = 285,
    T_ASSIGN = 286,
    T_NOT = 287,
    T_AND = 288,
    T_OR = 289,
    T_EQ = 290,
    T_NE = 291,
    T_LT = 292,
    T_GT = 293,
    T_LE = 294,
    T_GE = 295,
    T_PLUS = 296,
    T_MINUS = 297,
    T_TIMES = 298,
    T_DIV = 299,
    T_MOD = 300,
    T_QUESTION = 301,
    T_COLON = 302,
    T_SCOPE = 303,
    T_RANGE = 304,
    T_COMMA = 305,
    T_OPEN = 306,
    T_CLOSE = 307,
    T_OBJECT_OPEN = 308,
    T_OBJECT_CLOSE = 309,
    T_ARRAY_OPEN = 310,
    T_ARRAY_CLOSE = 311,
    T_OUTBOUND = 312,
    T_INBOUND = 313,
    T_ANY = 314,
    T_ALL = 315,
    T_NONE = 316,
    T_NIN = 317,
    UMINUS = 318,
    UPLUS = 319,
    FUNCCALL = 320,
    REFERENCE = 321,
    INDEXED = 322,
    EXPANSION = 323
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 18 "arangod/Aql/grammar.y" /* yacc.c:355  */

  arangodb::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 201 "arangod/Aql/grammar.cpp" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
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



int Aqlparse (arangodb::aql::Parser* parser);

#endif /* !YY_AQL_ARANGOD_AQL_GRAMMAR_HPP_INCLUDED  */

/* Copy the second part of user declarations.  */
#line 28 "arangod/Aql/grammar.y" /* yacc.c:358  */


using namespace arangodb::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro for signaling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM                                   \
  parser->registerError(TRI_ERROR_OUT_OF_MEMORY);   \
  YYABORT;

#define scanner parser->scanner()

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
               arangodb::aql::Parser* parser,
               char const* message) {
  parser->registerParseError(TRI_ERROR_QUERY_PARSE, message, locp->first_line, locp->first_column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register variables in the scope
////////////////////////////////////////////////////////////////////////////////

static void RegisterAssignVariables(arangodb::aql::Scopes* scopes, AstNode const* vars) { 
  size_t const n = vars->numMembers();
  for (size_t i = 0; i < n; ++i) {
    auto member = vars->getMember(i);

    if (member != nullptr) {
      TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
      auto v = static_cast<Variable*>(member->getMember(0)->getData());
      scopes->addVariable(v);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate the aggregate variables expressions
////////////////////////////////////////////////////////////////////////////////

static bool ValidateAggregates(Parser* parser, AstNode const* aggregates) {
  size_t const n = aggregates->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = aggregates->getMember(i);

    if (member != nullptr) {
      TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
      auto func = member->getMember(1);

      bool isValid = true;
      if (func->type != NODE_TYPE_FCALL) {
        // aggregate expression must be a function call
        isValid = false;
      }
      else {
        auto f = static_cast<arangodb::aql::Function*>(func->getData());
        if (! Aggregator::isSupported(f->externalName)) {
          // aggregate expression must be a call to MIN|MAX|LENGTH...
          isValid = false;
        }
      }

      if (! isValid) {
        parser->registerError(TRI_ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION);
        return false;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a new scope for the collect
////////////////////////////////////////////////////////////////////////////////

static bool StartCollectScope(arangodb::aql::Scopes* scopes) { 
  // check if we are in the main scope
  if (scopes->type() == arangodb::aql::AQL_SCOPE_MAIN) {
    return false;
  } 

  // end the active scopes
  scopes->endNested();
  // start a new scope
  scopes->start(arangodb::aql::AQL_SCOPE_COLLECT);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the INTO variable stored in a node (may not exist)
////////////////////////////////////////////////////////////////////////////////

static AstNode const* GetIntoVariable(Parser* parser, AstNode const* node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (node->type == NODE_TYPE_VALUE) {
    // node is a string containing the variable name
    return parser->ast()->createNodeVariable(node->getStringValue(), node->getStringLength(), true);
  }

  // node is an array with the variable name as the first member
  TRI_ASSERT(node->type == NODE_TYPE_ARRAY);
  TRI_ASSERT(node->numMembers() == 2);

  auto v = node->getMember(0);
  TRI_ASSERT(v->type == NODE_TYPE_VALUE);
  return parser->ast()->createNodeVariable(v->getStringValue(), v->getStringLength(), true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the INTO variable = expression stored in a node (may not exist)
////////////////////////////////////////////////////////////////////////////////

static AstNode const* GetIntoExpression(AstNode const* node) {
  if (node == nullptr || node->type == NODE_TYPE_VALUE) {
    return nullptr;
  }

  // node is an array with the expression as the second member
  TRI_ASSERT(node->type == NODE_TYPE_ARRAY);
  TRI_ASSERT(node->numMembers() == 2);

  return node->getMember(1);
}


#line 374 "arangod/Aql/grammar.cpp" /* yacc.c:358  */

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
#define YYLAST   995

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  70
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  83
/* YYNRULES -- Number of rules.  */
#define YYNRULES  189
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  315

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   323

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
       2,     2,     2,     2,     2,     2,    69,     2,     2,     2,
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
      65,    66,    67,    68
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   330,   330,   335,   337,   340,   343,   346,   349,   355,
     357,   362,   364,   366,   368,   370,   372,   374,   376,   378,
     380,   382,   387,   393,   398,   403,   411,   419,   424,   426,
     431,   438,   448,   448,   462,   471,   482,   501,   552,   566,
     588,   590,   595,   602,   605,   608,   617,   631,   648,   648,
     662,   662,   672,   672,   683,   686,   692,   698,   701,   704,
     707,   713,   718,   725,   733,   736,   742,   752,   762,   770,
     781,   786,   794,   805,   810,   813,   819,   819,   870,   870,
     880,   886,   889,   892,   895,   898,   901,   907,   910,   926,
     926,   938,   941,   944,   950,   953,   956,   959,   962,   965,
     968,   971,   974,   977,   980,   983,   986,   989,   992,   998,
    1004,  1006,  1011,  1014,  1014,  1030,  1033,  1039,  1042,  1048,
    1048,  1057,  1059,  1064,  1067,  1073,  1076,  1090,  1090,  1099,
    1101,  1106,  1108,  1113,  1127,  1131,  1140,  1147,  1150,  1156,
    1159,  1165,  1168,  1171,  1177,  1180,  1186,  1189,  1197,  1201,
    1212,  1216,  1223,  1229,  1228,  1237,  1246,  1255,  1258,  1261,
    1267,  1270,  1276,  1308,  1311,  1314,  1321,  1331,  1331,  1344,
    1359,  1373,  1387,  1387,  1430,  1433,  1439,  1446,  1456,  1459,
    1462,  1465,  1468,  1474,  1477,  1480,  1490,  1496,  1499,  1504
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
  "\"INTO keyword\"", "\"AGGREGATE keyword\"", "\"GRAPH keyword\"",
  "\"DISTINCT modifier\"", "\"REMOVE command\"", "\"INSERT command\"",
  "\"UPDATE command\"", "\"REPLACE command\"", "\"UPSERT command\"",
  "\"null\"", "\"true\"", "\"false\"", "\"identifier\"",
  "\"quoted string\"", "\"integer number\"", "\"number\"",
  "\"bind parameter\"", "\"assignment\"", "\"not operator\"",
  "\"and operator\"", "\"or operator\"", "\"== operator\"",
  "\"!= operator\"", "\"< operator\"", "\"> operator\"", "\"<= operator\"",
  "\">= operator\"", "\"+ operator\"", "\"- operator\"", "\"* operator\"",
  "\"/ operator\"", "\"% operator\"", "\"?\"", "\":\"", "\"::\"", "\"..\"",
  "\",\"", "\"(\"", "\")\"", "\"{\"", "\"}\"", "\"[\"", "\"]\"",
  "\"outbound modifier\"", "\"inbound modifier\"", "\"any modifier\"",
  "\"all modifier\"", "\"none modifier\"", "T_NIN", "UMINUS", "UPLUS",
  "FUNCCALL", "REFERENCE", "INDEXED", "EXPANSION", "'.'", "$accept",
  "query", "final_statement", "optional_statement_block_statements",
  "statement_block_statement", "for_statement", "filter_statement",
  "let_statement", "let_list", "let_element", "count_into",
  "collect_variable_list", "$@1", "collect_statement", "collect_list",
  "collect_element", "collect_optional_into", "variable_list", "keep",
  "$@2", "aggregate", "$@3", "sort_statement", "$@4", "sort_list",
  "sort_element", "sort_direction", "limit_statement", "return_statement",
  "in_or_into_collection", "remove_statement", "insert_statement",
  "update_parameters", "update_statement", "replace_parameters",
  "replace_statement", "update_or_replace", "upsert_statement", "$@5",
  "distinct_expression", "$@6", "expression", "function_name",
  "function_call", "$@7", "operator_unary", "operator_binary",
  "operator_ternary", "optional_function_call_arguments",
  "expression_or_query", "$@8", "function_arguments_list",
  "compound_value", "array", "$@9", "optional_array_elements",
  "array_elements_list", "options", "object", "$@10",
  "optional_object_elements", "object_elements_list", "object_element",
  "array_filter_operator", "optional_array_filter", "optional_array_limit",
  "optional_array_return", "graph_collection", "graph_collection_list",
  "graph_subject", "$@11", "graph_direction", "graph_direction_steps",
  "reference", "$@12", "$@13", "simple_value", "numeric_value",
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
     315,   316,   317,   318,   319,   320,   321,   322,   323,    46
};
# endif

#define YYPACT_NINF -265

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-265)))

#define YYTABLE_NINF -188

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -265,    28,   973,  -265,    42,    42,   913,   560,    41,  -265,
     158,   913,   913,   913,   913,  -265,  -265,  -265,  -265,  -265,
    -265,   121,  -265,  -265,  -265,  -265,    12,    17,    24,    27,
      32,  -265,     7,    23,  -265,    47,  -265,  -265,  -265,   -22,
    -265,  -265,  -265,  -265,   913,   913,   913,   913,  -265,  -265,
     764,    63,  -265,  -265,  -265,  -265,  -265,  -265,  -265,   -20,
    -265,  -265,  -265,  -265,  -265,   764,    92,  -265,    98,    42,
     116,   913,    88,  -265,  -265,   693,   693,  -265,   584,  -265,
     622,   913,    42,    98,   118,   116,   827,    42,    42,   913,
     110,   110,   110,   468,  -265,    -5,   913,   913,   140,   913,
     913,   913,   913,   913,   913,   913,   913,   913,   913,   913,
     913,   913,   913,   913,   130,   106,   847,    55,   913,   144,
      42,   107,  -265,   111,  -265,   131,    98,   123,  -265,   511,
     158,   933,    90,    98,    98,   913,    98,   913,    98,   728,
     143,  -265,   107,    98,  -265,    98,  -265,  -265,  -265,   363,
    -265,   913,     8,  -265,   764,  -265,   132,   128,  -265,   152,
     913,   136,   150,  -265,   154,   764,   146,   153,   600,   913,
     800,   782,   127,   127,     2,     2,     2,     2,    83,    83,
     110,   110,   110,   746,   105,  -265,   880,  -265,   398,   161,
    -265,  -265,   764,    42,   111,  -265,    42,   913,  -265,   913,
    -265,  -265,  -265,  -265,  -265,   189,   305,   330,  -265,  -265,
    -265,  -265,  -265,  -265,  -265,   693,  -265,   693,  -265,   913,
     913,    42,  -265,  -265,  -265,   328,   827,    42,  -265,   913,
     433,  -265,    -5,   913,  -265,   913,   600,   913,   764,   160,
    -265,  -265,   156,  -265,  -265,   211,  -265,  -265,   764,  -265,
      98,    98,   657,   764,   168,  -265,    62,  -265,   171,  -265,
      93,  -265,   363,   913,   210,   764,   177,  -265,   764,   764,
     764,  -265,  -265,   913,   913,   216,  -265,  -265,  -265,  -265,
     913,    42,  -265,  -265,  -265,  -265,  -265,   328,   827,   913,
    -265,   764,   913,   220,   693,  -265,    29,  -265,   913,   764,
     530,   913,   172,    98,  -265,   183,   328,   913,   764,  -265,
    -265,    29,  -265,   764,  -265
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       9,     0,     0,     1,     0,     0,     0,     0,    32,    52,
       0,     0,     0,     0,     0,    76,     2,    10,    11,    13,
      12,    43,    14,    15,    16,     3,    17,    18,    19,    20,
      21,   189,     0,    27,    28,     0,   180,   181,   182,   162,
     178,   176,   177,   186,     0,     0,     0,   167,   127,   119,
      26,    89,   165,    81,    82,    83,   163,   117,   118,    85,
     179,    84,   164,    78,    63,    80,     0,    50,   125,     0,
      43,     0,    61,   174,   175,     0,     0,    70,     0,    73,
       0,     0,     0,   125,   125,    43,     0,     0,     0,     0,
      93,    91,    92,     0,     9,   129,   121,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    34,    33,    40,     0,   125,    53,    54,    57,
       0,     0,     0,   125,   125,     0,   125,     0,   125,     0,
      44,    35,    48,   125,    38,   125,   157,   158,   159,    22,
     160,     0,     0,    29,    30,   166,     0,   133,   188,     0,
       0,     0,   130,   131,     0,   123,     0,   122,   107,     0,
      95,    94,   101,   102,   103,   104,   105,   106,    96,    97,
      98,    99,   100,     0,    86,    88,   113,   137,     0,   172,
     169,   170,    79,     0,    51,   126,     0,     0,    36,     0,
      58,    59,    56,    60,    62,   162,   178,   186,    64,   183,
     184,   185,    65,    66,    67,     0,    68,     0,    71,     0,
       0,     0,    39,    37,   161,     0,     0,     0,   168,     0,
       0,   128,     0,     0,   120,     0,   108,     0,   112,     0,
     115,     9,   111,   171,   138,   139,    31,    41,    42,    55,
     125,   125,     0,    45,    49,    46,     0,   146,   152,    23,
       0,   147,     0,     0,     0,   135,     0,   132,   134,   124,
     109,    90,   114,   113,     0,   141,    69,    72,    74,    75,
       0,     0,   156,   155,   153,   148,   149,     0,     0,     0,
     116,   140,     0,   144,     0,    47,     0,    24,     0,   136,
     142,     0,     0,   125,   150,   154,     0,     0,   145,   173,
      77,     0,    25,   143,   151
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -265,   -90,  -265,  -265,  -265,  -265,  -265,  -265,  -265,   147,
     213,  -265,  -265,  -265,   122,    40,   -37,  -265,  -265,  -265,
     217,  -265,  -265,  -265,  -265,    44,  -265,  -265,  -265,   -62,
    -265,  -265,  -265,  -265,  -265,  -265,  -265,  -265,  -265,  -265,
    -265,    -6,  -265,  -265,  -265,  -265,  -265,  -265,  -265,   -29,
    -265,  -265,  -265,  -265,  -265,  -265,  -265,   -73,  -106,  -265,
    -265,  -265,    13,  -265,  -265,  -265,  -265,  -259,  -265,  -264,
    -265,   -83,  -217,  -265,  -265,  -265,    11,  -265,     3,   114,
      -8,  -265,    26
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,    16,     2,    17,    18,    19,    20,    33,    34,
      68,    21,    69,    22,   123,   124,    84,   254,   143,   221,
      70,   120,    23,    71,   127,   128,   202,    24,    25,   133,
      26,    27,    77,    28,    79,    29,   280,    30,    81,    64,
     118,   129,    51,    52,   115,    53,    54,    55,   239,   240,
     241,   242,    56,    57,    96,   166,   167,   122,    58,    95,
     161,   162,   163,   189,   275,   293,   302,   258,   305,   259,
     296,   260,   151,    59,    94,   245,    72,    60,    61,   208,
      62,   164,   125
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      50,    65,    74,   150,   156,    75,    76,    78,    80,   263,
     141,   144,    -4,    73,   134,   195,   136,    -5,   138,    86,
     226,   157,   158,   297,    -6,   159,   -87,    -7,     3,   -87,
      32,    35,    -8,   126,    98,   116,   195,   304,    90,    91,
      92,    93,   312,   107,   108,   109,   110,   111,   145,   117,
     160,   113,   314,   198,    66,   257,    67,    87,   227,    43,
     213,   214,    -4,   216,    -4,   218,   224,    -5,    31,    -5,
     222,   298,   223,    88,    -6,   139,    -6,    -7,    89,    -7,
     149,   190,    -8,   154,    -8,    43,   146,   147,   148,   282,
     165,   168,    43,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   140,   191,
     188,   114,   192,   152,    35,    98,   209,   210,   119,   285,
     211,    74,    74,    43,   121,   168,   109,   110,   111,   215,
      82,   217,    73,    73,    66,    82,    67,    98,   130,    97,
     203,   204,    98,   150,   142,   225,   107,   108,   109,   110,
     111,   272,   169,   250,   230,   251,   185,   186,   193,    98,
      48,   196,   197,   236,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   199,   220,  -187,   113,   276,   277,   224,
     238,    36,    37,    38,   228,    40,    41,    42,    43,  -183,
     231,   248,  -183,  -183,  -183,  -183,  -183,  -183,  -183,   229,
     232,   233,   234,   235,   244,   150,   273,  -183,  -183,  -183,
    -183,  -183,   271,   252,   253,  -183,   274,   261,   281,   246,
     262,   284,   288,   265,   289,   292,   301,   268,   309,   269,
     310,   270,   303,   311,    83,   153,   247,   -87,    85,  -183,
     -87,  -183,   194,   249,   290,   267,   212,   255,   283,     0,
       0,     0,   286,   264,     0,     0,     0,   287,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   238,   291,     0,
       0,     0,     0,     0,   294,     0,     0,     0,     0,   261,
       0,     0,   262,   299,     0,     0,   300,     0,   261,     0,
       0,     0,   306,     0,     0,   308,     0,     0,   261,     0,
       0,   313,     0,   261,     0,  -184,     0,   295,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,     0,     0,     0,     0,     0,
       0,     0,     0,  -184,  -184,  -184,  -184,  -184,     0,     0,
    -185,  -184,     0,  -185,  -185,  -185,  -185,  -185,  -185,  -185,
      97,     0,     0,     0,   256,     0,     0,     0,  -185,  -185,
    -185,  -185,  -185,     0,   257,  -184,  -185,  -184,    43,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,    97,     0,   113,     0,     0,
    -185,     0,  -185,     0,     0,   146,   147,   148,     0,     0,
       0,     0,     0,     0,     0,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
      97,     0,   113,     0,     0,     0,     0,     0,     0,     0,
     146,   147,   148,     0,     0,     0,     0,     0,     0,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,    97,     0,   113,     0,     0,
       0,     0,     0,     0,   243,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
      97,     0,   113,     0,     0,     0,     0,     0,     0,   266,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,     0,     0,   113,     0,     0,
     155,   200,   201,    97,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    36,    37,    38,     0,    40,    41,
      42,    43,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,     0,     0,
     113,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,    63,     0,   113,
     307,     0,     0,    36,    37,    38,    39,    40,    41,    42,
      43,     0,    44,     0,     0,     0,   131,   135,   132,     0,
       0,    45,    46,     0,     0,     0,     0,     0,     0,     0,
       0,    47,     0,    48,     0,    49,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,     0,    98,   113,   131,   137,   132,   103,   104,   105,
     106,   107,   108,   109,   110,   111,     0,     0,     0,   113,
       0,     0,     0,     0,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,    97,
       0,   113,     0,     0,     0,     0,     0,   278,   279,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,     0,   131,   113,   132,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
      97,     0,   113,     0,     0,     0,     0,   219,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    97,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,     0,    97,   113,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   237,    97,   113,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,     0,    97,   113,    98,    99,     0,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,     0,     0,
       0,   113,    98,     0,     0,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,     0,     0,     0,   113,
      36,    37,    38,    39,    40,    41,    42,    43,     0,    44,
       0,     0,     0,     0,     0,     0,     0,     0,    45,    46,
      36,    37,    38,    39,    40,    41,    42,    43,    47,    44,
      48,     0,    49,     0,   146,   147,   148,     0,    45,    46,
     187,     0,     0,     0,     0,     0,     0,     0,    47,     0,
      48,     0,    49,    36,    37,    38,    39,    40,    41,    42,
      43,     0,    44,     0,     0,     0,     0,     0,     0,     0,
       0,    45,    46,     0,     0,     0,     0,     0,     0,     0,
       0,    47,  -110,    48,     0,    49,    36,    37,    38,    39,
      40,    41,    42,    43,     0,    44,     0,     0,     0,     0,
       0,     0,     0,     0,    45,    46,    36,    37,    38,   205,
     206,    41,    42,   207,    47,    44,    48,     0,    49,     0,
       0,     0,     0,     0,    45,    46,     4,     5,     6,     7,
       8,     9,    10,     0,    47,     0,    48,     0,    49,     0,
       0,    11,    12,    13,    14,    15
};

static const yytype_int16 yycheck[] =
{
       6,     7,    10,    86,    94,    11,    12,    13,    14,   226,
      83,    84,     0,    10,    76,   121,    78,     0,    80,    12,
      12,    26,    27,   287,     0,    30,    48,     0,     0,    51,
       4,     5,     0,    70,    32,    55,   142,   296,    44,    45,
      46,    47,   306,    41,    42,    43,    44,    45,    85,    69,
      55,    49,   311,   126,    13,    26,    15,    50,    50,    30,
     133,   134,    50,   136,    52,   138,   149,    50,    26,    52,
     143,   288,   145,    50,    50,    81,    52,    50,    31,    52,
      86,    26,    50,    89,    52,    30,    57,    58,    59,    27,
      96,    97,    30,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,    82,   117,
     116,    48,   118,    87,    88,    32,    26,    27,    26,    26,
      30,   129,   130,    30,    26,   131,    43,    44,    45,   135,
      14,   137,   129,   130,    13,    14,    15,    32,    50,    12,
     129,   130,    32,   226,    26,   151,    41,    42,    43,    44,
      45,   241,    12,   215,   160,   217,    26,    51,    14,    32,
      53,    50,    31,   169,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    50,    31,    47,    49,   250,   251,   262,
     186,    23,    24,    25,    52,    27,    28,    29,    30,     0,
      54,   197,     3,     4,     5,     6,     7,     8,     9,    47,
      50,    47,    56,    50,    43,   288,    50,    18,    19,    20,
      21,    22,    52,   219,   220,    26,     5,   225,    50,   193,
     226,    50,    12,   229,    47,     9,     6,   233,    56,   235,
     303,   237,   294,    50,    21,    88,   196,    48,    21,    50,
      51,    52,   120,   199,   273,   232,   132,   221,   256,    -1,
      -1,    -1,   260,   227,    -1,    -1,    -1,   263,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   273,   274,    -1,
      -1,    -1,    -1,    -1,   280,    -1,    -1,    -1,    -1,   287,
      -1,    -1,   288,   289,    -1,    -1,   292,    -1,   296,    -1,
      -1,    -1,   298,    -1,    -1,   301,    -1,    -1,   306,    -1,
      -1,   307,    -1,   311,    -1,     0,    -1,   281,     3,     4,
       5,     6,     7,     8,     9,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    18,    19,    20,    21,    22,    -1,    -1,
       0,    26,    -1,     3,     4,     5,     6,     7,     8,     9,
      12,    -1,    -1,    -1,    16,    -1,    -1,    -1,    18,    19,
      20,    21,    22,    -1,    26,    50,    26,    52,    30,    -1,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    12,    -1,    49,    -1,    -1,
      50,    -1,    52,    -1,    -1,    57,    58,    59,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      12,    -1,    49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      57,    58,    59,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    12,    -1,    49,    -1,    -1,
      -1,    -1,    -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      12,    -1,    49,    -1,    -1,    -1,    -1,    -1,    -1,    56,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    -1,    -1,    49,    -1,    -1,
      52,    10,    11,    12,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    23,    24,    25,    -1,    27,    28,
      29,    30,    12,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    -1,    -1,
      49,    -1,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    17,    -1,    49,
      50,    -1,    -1,    23,    24,    25,    26,    27,    28,    29,
      30,    -1,    32,    -1,    -1,    -1,    12,    13,    14,    -1,
      -1,    41,    42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    51,    -1,    53,    -1,    55,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    -1,    32,    49,    12,    13,    14,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    -1,    -1,    -1,    49,
      -1,    -1,    -1,    -1,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    12,
      -1,    49,    -1,    -1,    -1,    -1,    -1,    20,    21,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    -1,    12,    49,    14,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      12,    -1,    49,    -1,    -1,    -1,    -1,    19,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    12,    -1,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    -1,    12,    49,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    12,    49,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    -1,    12,    49,    32,    33,    -1,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    -1,    -1,
      -1,    49,    32,    -1,    -1,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    -1,    -1,    -1,    49,
      23,    24,    25,    26,    27,    28,    29,    30,    -1,    32,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    41,    42,
      23,    24,    25,    26,    27,    28,    29,    30,    51,    32,
      53,    -1,    55,    -1,    57,    58,    59,    -1,    41,    42,
      43,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    51,    -1,
      53,    -1,    55,    23,    24,    25,    26,    27,    28,    29,
      30,    -1,    32,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    41,    42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    51,    52,    53,    -1,    55,    23,    24,    25,    26,
      27,    28,    29,    30,    -1,    32,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    41,    42,    23,    24,    25,    26,
      27,    28,    29,    30,    51,    32,    53,    -1,    55,    -1,
      -1,    -1,    -1,    -1,    41,    42,     3,     4,     5,     6,
       7,     8,     9,    -1,    51,    -1,    53,    -1,    55,    -1,
      -1,    18,    19,    20,    21,    22
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    71,    73,     0,     3,     4,     5,     6,     7,     8,
       9,    18,    19,    20,    21,    22,    72,    74,    75,    76,
      77,    81,    83,    92,    97,    98,   100,   101,   103,   105,
     107,    26,   152,    78,    79,   152,    23,    24,    25,    26,
      27,    28,    29,    30,    32,    41,    42,    51,    53,    55,
     111,   112,   113,   115,   116,   117,   122,   123,   128,   143,
     147,   148,   150,    17,   109,   111,    13,    15,    80,    82,
      90,    93,   146,   148,   150,   111,   111,   102,   111,   104,
     111,   108,    14,    80,    86,    90,    12,    50,    50,    31,
     111,   111,   111,   111,   144,   129,   124,    12,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    49,    48,   114,    55,    69,   110,    26,
      91,    26,   127,    84,    85,   152,    86,    94,    95,   111,
      50,    12,    14,    99,    99,    13,    99,    13,    99,   111,
     152,   127,    26,    88,   127,    86,    57,    58,    59,   111,
     141,   142,   152,    79,   111,    52,    71,    26,    27,    30,
      55,   130,   131,   132,   151,   111,   125,   126,   111,    12,
     111,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,    26,    51,    43,   111,   133,
      26,   150,   111,    14,    84,   128,    50,    31,   127,    50,
      10,    11,    96,   146,   146,    26,    27,    30,   149,    26,
      27,    30,   149,   127,   127,   111,   127,   111,   127,    19,
      31,    89,   127,   127,   141,   111,    12,    50,    52,    47,
     111,    54,    50,    47,    56,    50,   111,    47,   111,   118,
     119,   120,   121,    56,    43,   145,   152,    85,   111,    95,
      99,    99,   111,   111,    87,   152,    16,    26,   137,   139,
     141,   150,   111,   142,   152,   111,    56,   132,   111,   111,
     111,    52,    71,    50,     5,   134,   127,   127,    20,    21,
     106,    50,    27,   150,    50,    26,   150,   111,    12,    47,
     119,   111,     9,   135,   111,   152,   140,   139,   142,   111,
     111,     6,   136,    99,   137,   138,   111,    50,   111,    56,
     127,    50,   139,   111,   137
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    70,    71,    72,    72,    72,    72,    72,    72,    73,
      73,    74,    74,    74,    74,    74,    74,    74,    74,    74,
      74,    74,    75,    75,    75,    75,    76,    77,    78,    78,
      79,    80,    82,    81,    83,    83,    83,    83,    83,    83,
      84,    84,    85,    86,    86,    86,    87,    87,    89,    88,
      91,    90,    93,    92,    94,    94,    95,    96,    96,    96,
      96,    97,    97,    98,    99,    99,   100,   101,   102,   102,
     103,   104,   104,   105,   106,   106,   108,   107,   110,   109,
     109,   111,   111,   111,   111,   111,   111,   112,   112,   114,
     113,   115,   115,   115,   116,   116,   116,   116,   116,   116,
     116,   116,   116,   116,   116,   116,   116,   116,   116,   117,
     118,   118,   119,   120,   119,   121,   121,   122,   122,   124,
     123,   125,   125,   126,   126,   127,   127,   129,   128,   130,
     130,   131,   131,   132,   132,   132,   132,   133,   133,   134,
     134,   135,   135,   135,   136,   136,   137,   137,   137,   137,
     138,   138,   139,   140,   139,   139,   139,   141,   141,   141,
     142,   142,   143,   143,   143,   143,   143,   144,   143,   143,
     143,   143,   145,   143,   146,   146,   147,   147,   148,   148,
     148,   148,   148,   149,   149,   149,   150,   151,   151,   152
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     1,     1,     1,     1,     1,     0,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     4,     6,     8,    10,     2,     2,     1,     3,
       3,     4,     0,     3,     3,     3,     4,     4,     3,     4,
       1,     3,     3,     0,     2,     4,     1,     3,     0,     3,
       0,     3,     0,     3,     1,     3,     2,     0,     1,     1,
       1,     2,     4,     2,     2,     2,     4,     4,     3,     5,
       2,     3,     5,     2,     1,     1,     0,     9,     0,     3,
       1,     1,     1,     1,     1,     1,     3,     1,     3,     0,
       5,     2,     2,     2,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     4,     5,
       0,     1,     1,     0,     2,     1,     3,     1,     1,     0,
       4,     0,     1,     1,     3,     0,     2,     0,     4,     0,
       1,     1,     3,     1,     3,     3,     5,     1,     2,     0,
       2,     0,     2,     4,     0,     2,     1,     1,     2,     2,
       1,     3,     1,     0,     4,     2,     2,     1,     1,     1,
       1,     2,     1,     1,     1,     1,     3,     0,     4,     3,
       3,     4,     0,     8,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1
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
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, arangodb::aql::Parser* parser)
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, arangodb::aql::Parser* parser)
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, arangodb::aql::Parser* parser)
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, arangodb::aql::Parser* parser)
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
yyparse (arangodb::aql::Parser* parser)
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
#line 330 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1957 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 3:
#line 335 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 1964 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 4:
#line 337 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 1972 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 5:
#line 340 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 1980 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 6:
#line 343 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 1988 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 7:
#line 346 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 1996 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 8:
#line 349 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2004 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 9:
#line 355 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2011 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 10:
#line 357 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2018 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 11:
#line 362 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2025 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 12:
#line 364 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2032 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 13:
#line 366 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2039 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 14:
#line 368 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2046 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 15:
#line 370 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2053 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 16:
#line 372 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2060 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 17:
#line 374 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2067 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 18:
#line 376 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2074 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 19:
#line 378 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2081 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 20:
#line 380 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2088 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 21:
#line 382 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2095 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 22:
#line 387 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
     
      auto node = parser->ast()->createNodeFor((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2106 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 23:
#line 393 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2116 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 24:
#line 398 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-6].strval).value, (yyvsp[-6].strval).length, (yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2126 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 25:
#line 403 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-8].strval).value, (yyvsp[-8].strval).length, (yyvsp[-6].strval).value, (yyvsp[-6].strval).length, (yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2136 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 26:
#line 411 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // operand is a reference. can use it directly
      auto node = parser->ast()->createNodeFilter((yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2146 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 27:
#line 419 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2153 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 28:
#line 424 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2160 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 29:
#line 426 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2167 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 30:
#line 431 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeLet((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2176 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 31:
#line 438 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! TRI_CaseEqualString((yyvsp[-2].strval).value, "COUNT")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'COUNT'", (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2188 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 32:
#line 448 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2197 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 33:
#line 451 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    { 
      auto list = static_cast<AstNode*>(parser->popStack());

      if (list == nullptr) {
        ABORT_OOM
      }
      (yyval.node) = list;
    }
#line 2210 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 34:
#line 462 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      /* COLLECT WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      StartCollectScope(scopes);

      auto node = parser->ast()->createNodeCollectCount(parser->ast()->createNodeArray(), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2224 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 35:
#line 471 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      /* COLLECT var = expr WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-2].node));
      }

      auto node = parser->ast()->createNodeCollectCount((yyvsp[-2].node), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2240 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 36:
#line 482 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      /* AGGREGATE var = expr OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-2].node));
      }

      // validate aggregates
      if (! ValidateAggregates(parser, (yyvsp[-2].node))) {
        YYABORT;
      }

      AstNode const* into = GetIntoVariable(parser, (yyvsp[-1].node));
      AstNode const* intoExpression = GetIntoExpression((yyvsp[-1].node));

      auto node = parser->ast()->createNodeCollect(parser->ast()->createNodeArray(), (yyvsp[-2].node), into, intoExpression, nullptr, (yyvsp[-1].node));
      parser->ast()->addOperation(node);
    }
#line 2264 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 37:
#line 501 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      /* COLLECT var = expr AGGREGATE var = expr OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-3].node));
        RegisterAssignVariables(scopes, (yyvsp[-2].node));
      }

      if (! ValidateAggregates(parser, (yyvsp[-2].node))) {
        YYABORT;
      }

      // note all group variables
      std::unordered_set<Variable const*> groupVars;
      size_t n = (yyvsp[-3].node)->numMembers();
      for (size_t i = 0; i < n; ++i) {
        auto member = (yyvsp[-3].node)->getMember(i);

        if (member != nullptr) {
          TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
          groupVars.emplace(static_cast<Variable const*>(member->getMember(0)->getData()));
        }
      }

      // now validate if any aggregate refers to one of the group variables
      n = (yyvsp[-2].node)->numMembers();
      for (size_t i = 0; i < n; ++i) {
        auto member = (yyvsp[-2].node)->getMember(i);

        if (member != nullptr) {
          TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
          std::unordered_set<Variable const*> variablesUsed;
          Ast::getReferencedVariables(member->getMember(1), variablesUsed);

          for (auto& it : groupVars) {
            if (variablesUsed.find(it) != variablesUsed.end()) {
              parser->registerParseError(TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN, 
                "use of unknown variable '%s' in aggregate expression", it->name.c_str(), yylloc.first_line, yylloc.first_column);
              break;
            }
          }
        }
      }

      AstNode const* into = GetIntoVariable(parser, (yyvsp[-1].node));
      AstNode const* intoExpression = GetIntoExpression((yyvsp[-1].node));

      auto node = parser->ast()->createNodeCollect((yyvsp[-3].node), (yyvsp[-2].node), into, intoExpression, nullptr, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2320 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 38:
#line 552 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      /* COLLECT var = expr INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-2].node));
      }

      AstNode const* into = GetIntoVariable(parser, (yyvsp[-1].node));
      AstNode const* intoExpression = GetIntoExpression((yyvsp[-1].node));

      auto node = parser->ast()->createNodeCollect((yyvsp[-2].node), parser->ast()->createNodeArray(), into, intoExpression, nullptr, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2339 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 39:
#line 566 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      /* COLLECT var = expr INTO var KEEP ... OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-3].node));
      }

      if ((yyvsp[-2].node) == nullptr && 
          (yyvsp[-1].node) != nullptr) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of 'KEEP' without 'INTO'", yylloc.first_line, yylloc.first_column);
      }
 
      AstNode const* into = GetIntoVariable(parser, (yyvsp[-2].node));
      AstNode const* intoExpression = GetIntoExpression((yyvsp[-2].node));

      auto node = parser->ast()->createNodeCollect((yyvsp[-3].node), parser->ast()->createNodeArray(), into, intoExpression, (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2363 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 40:
#line 588 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2370 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 41:
#line 590 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2377 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 42:
#line 595 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeAssign((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
      parser->pushArrayElement(node);
    }
#line 2386 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 43:
#line 602 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = nullptr;
    }
#line 2394 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 44:
#line 605 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 2402 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 45:
#line 608 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember(parser->ast()->createNodeValueString((yyvsp[-2].strval).value, (yyvsp[-2].strval).length));
      node->addMember((yyvsp[0].node));
      (yyval.node) = node;
    }
#line 2413 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 46:
#line 617 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 2432 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 47:
#line 631 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 2451 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 48:
#line 648 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! TRI_CaseEqualString((yyvsp[0].strval).value, "KEEP")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'KEEP'", (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2464 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 49:
#line 655 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2473 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 50:
#line 662 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2482 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 51:
#line 665 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2491 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 52:
#line 672 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2500 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 53:
#line 675 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      auto node = parser->ast()->createNodeSort(list);
      parser->ast()->addOperation(node);
    }
#line 2510 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 54:
#line 683 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2518 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 55:
#line 686 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2526 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 56:
#line 692 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeSortElement((yyvsp[-1].node), (yyvsp[0].node));
    }
#line 2534 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 57:
#line 698 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2542 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 58:
#line 701 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2550 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 59:
#line 704 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 2558 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 60:
#line 707 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2566 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 61:
#line 713 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto offset = parser->ast()->createNodeValueInt(0);
      auto node = parser->ast()->createNodeLimit(offset, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2576 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 62:
#line 718 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeLimit((yyvsp[-2].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2585 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 63:
#line 725 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeReturn((yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2595 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 64:
#line 733 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2603 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 65:
#line 736 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
       (yyval.node) = (yyvsp[0].node);
     }
#line 2611 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 66:
#line 742 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2623 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 67:
#line 752 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2635 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 68:
#line 762 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2648 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 69:
#line 770 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2661 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 70:
#line 781 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2668 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 71:
#line 786 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2681 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 72:
#line 794 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2694 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 73:
#line 805 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 2701 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 74:
#line 810 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_UPDATE);
    }
#line 2709 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 75:
#line 813 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_REPLACE);
    }
#line 2717 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 76:
#line 819 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    { 
      // reserve a variable named "$OLD", we might need it in the update expression
      // and in a later return thing
      parser->pushStack(parser->ast()->createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD), true));
    }
#line 2727 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 77:
#line 823 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* variableNode = static_cast<AstNode*>(parser->popStack());
      
      auto scopes = parser->ast()->scopes();
      
      scopes->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
      
      scopes->start(arangodb::aql::AQL_SCOPE_FOR);
      std::string const variableName = parser->ast()->variables()->nextName();
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
      
      std::string const subqueryName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(subqueryName.c_str(), subqueryName.size(), subqueryNode, false);
      parser->ast()->addOperation(subQuery);
      
      auto index = parser->ast()->createNodeValueInt(0);
      auto firstDoc = parser->ast()->createNodeLet(variableNode, parser->ast()->createNodeIndexedAccess(parser->ast()->createNodeReference(subqueryName), index));
      parser->ast()->addOperation(firstDoc);

      auto node = parser->ast()->createNodeUpsert(static_cast<AstNodeType>((yyvsp[-3].intval)), parser->ast()->createNodeReference(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD)), (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2776 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 78:
#line 870 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto const scopeType = parser->ast()->scopes()->type();

      if (scopeType == AQL_SCOPE_MAIN ||
          scopeType == AQL_SCOPE_SUBQUERY) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "cannot use DISTINCT modifier on top-level query element", yylloc.first_line, yylloc.first_column);
      }
    }
#line 2789 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 79:
#line 877 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeDistinct((yyvsp[0].node));
    }
#line 2797 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 80:
#line 880 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2805 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 81:
#line 886 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2813 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 82:
#line 889 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2821 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 83:
#line 892 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2829 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 84:
#line 895 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2837 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 85:
#line 898 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2845 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 86:
#line 901 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeRange((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2853 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 87:
#line 907 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2861 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 88:
#line 910 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 2879 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 89:
#line 926 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushStack((yyvsp[0].strval).value);

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2890 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 90:
#line 931 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall(static_cast<char const*>(parser->popStack()), list);
    }
#line 2899 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 91:
#line 938 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_PLUS, (yyvsp[0].node));
    }
#line 2907 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 92:
#line 941 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_MINUS, (yyvsp[0].node));
    }
#line 2915 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 93:
#line 944 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    { 
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, (yyvsp[0].node));
    }
#line 2923 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 94:
#line 950 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2931 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 95:
#line 953 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2939 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 96:
#line 956 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_PLUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2947 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 97:
#line 959 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MINUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2955 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 98:
#line 962 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_TIMES, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2963 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 99:
#line 965 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_DIV, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2971 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 100:
#line 968 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MOD, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2979 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 101:
#line 971 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2987 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 102:
#line 974 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2995 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 103:
#line 977 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3003 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 104:
#line 980 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3011 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 105:
#line 983 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3019 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 106:
#line 986 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3027 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 107:
#line 989 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3035 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 108:
#line 992 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, (yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3043 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 109:
#line 998 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3051 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 110:
#line 1004 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 3058 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 111:
#line 1006 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 3065 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 112:
#line 1011 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3073 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 113:
#line 1014 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 3082 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 114:
#line 1017 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 3097 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 115:
#line 1030 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3105 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 116:
#line 1033 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3113 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 117:
#line 1039 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3121 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 118:
#line 1042 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3129 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 119:
#line 1048 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3138 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 120:
#line 1051 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3146 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 121:
#line 1057 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 3153 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 122:
#line 1059 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 3160 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 123:
#line 1064 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3168 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 124:
#line 1067 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3176 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 125:
#line 1073 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = nullptr;
    }
#line 3184 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 126:
#line 1076 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[-1].strval).value, "OPTIONS")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'OPTIONS'", (yyvsp[-1].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3200 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 127:
#line 1090 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeObject();
      parser->pushStack(node);
    }
#line 3209 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 128:
#line 1093 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3217 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 129:
#line 1099 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 3224 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 130:
#line 1101 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 3231 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 131:
#line 1106 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 3238 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 132:
#line 1108 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
    }
#line 3245 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 133:
#line 1113 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 3264 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 134:
#line 1127 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // attribute-name : attribute-value
      parser->pushObjectElement((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
    }
#line 3273 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 135:
#line 1131 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // bind-parameter : attribute-value
      if ((yyvsp[-2].strval).length < 1 || (yyvsp[-2].strval).value[0] == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto param = parser->ast()->createNodeParameter((yyvsp[-2].strval).value, (yyvsp[-2].strval).length);
      parser->pushObjectElement(param, (yyvsp[0].node));
    }
#line 3287 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 136:
#line 1140 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // [ attribute-name-expression ] : attribute-value
      parser->pushObjectElement((yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3296 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 137:
#line 1147 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.intval) = 1;
    }
#line 3304 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 138:
#line 1150 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.intval) = (yyvsp[-1].intval) + 1;
    }
#line 3312 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 139:
#line 1156 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = nullptr;
    }
#line 3320 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 140:
#line 1159 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3328 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 141:
#line 1165 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = nullptr;
    }
#line 3336 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 142:
#line 1168 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit(nullptr, (yyvsp[0].node));
    }
#line 3344 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 143:
#line 1171 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3352 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 144:
#line 1177 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = nullptr;
    }
#line 3360 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 145:
#line 1180 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3368 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 146:
#line 1186 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3376 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 147:
#line 1189 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3389 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 148:
#line 1197 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto tmp = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), tmp);
    }
#line 3398 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 149:
#line 1201 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), (yyvsp[0].node));
    }
#line 3411 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 150:
#line 1212 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3420 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 151:
#line 1216 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3429 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 152:
#line 1223 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3439 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 153:
#line 1229 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
      node->addMember((yyvsp[-1].node));
    }
#line 3449 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 154:
#line 1233 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3458 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 155:
#line 1237 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // graph name
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3472 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 156:
#line 1246 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      // graph name
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3481 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 157:
#line 1255 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.intval) = 2;
    }
#line 3489 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 158:
#line 1258 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.intval) = 1;
    }
#line 3497 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 159:
#line 1261 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.intval) = 0; 
    }
#line 3505 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 160:
#line 1267 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), 1);
    }
#line 3513 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 161:
#line 1270 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), (yyvsp[-1].node));
    }
#line 3521 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 162:
#line 1276 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 3558 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 163:
#line 1308 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3566 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 164:
#line 1311 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3574 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 165:
#line 1314 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
      
      if ((yyval.node) == nullptr) {
        ABORT_OOM
      }
    }
#line 3586 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 166:
#line 1321 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 3601 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 167:
#line 1331 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 3610 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 168:
#line 1334 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 3625 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 169:
#line 1344 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 3645 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 170:
#line 1359 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 3664 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 171:
#line 1373 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 3683 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 172:
#line 1387 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 3711 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 173:
#line 1409 "arangod/Aql/grammar.y" /* yacc.c:1661  */
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
#line 3734 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 174:
#line 1430 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3742 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 175:
#line 1433 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3750 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 176:
#line 1439 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }
      
      (yyval.node) = (yyvsp[0].node);
    }
#line 3762 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 177:
#line 1446 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3774 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 178:
#line 1456 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3782 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 179:
#line 1459 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3790 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 180:
#line 1462 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueNull();
    }
#line 3798 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 181:
#line 1465 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 3806 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 182:
#line 1468 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 3814 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 183:
#line 1474 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, TRI_TRANSACTION_WRITE);
    }
#line 3822 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 184:
#line 1477 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, TRI_TRANSACTION_WRITE);
    }
#line 3830 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 185:
#line 1480 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      if ((yyvsp[0].strval).length < 2 || (yyvsp[0].strval).value[0] != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3842 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 186:
#line 1490 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3850 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 187:
#line 1496 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3858 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 188:
#line 1499 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3866 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;

  case 189:
#line 1504 "arangod/Aql/grammar.y" /* yacc.c:1661  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3874 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
    break;


#line 3878 "arangod/Aql/grammar.cpp" /* yacc.c:1661  */
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
