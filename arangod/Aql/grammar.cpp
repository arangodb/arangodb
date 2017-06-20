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
#line 9 "Aql/grammar.y" /* yacc.c:339  */

#include "Aql/Aggregator.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/Parser.h"
#include "Aql/Quantifier.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "VocBase/AccessMode.h"

#line 83 "Aql/grammar.cpp" /* yacc.c:339  */

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
#ifndef YY_AQL_AQL_GRAMMAR_HPP_INCLUDED
# define YY_AQL_AQL_GRAMMAR_HPP_INCLUDED
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
    T_VIEW = 271,
    T_GRAPH = 272,
    T_SHORTEST_PATH = 273,
    T_DISTINCT = 274,
    T_REMOVE = 275,
    T_INSERT = 276,
    T_UPDATE = 277,
    T_REPLACE = 278,
    T_UPSERT = 279,
    T_NULL = 280,
    T_TRUE = 281,
    T_FALSE = 282,
    T_STRING = 283,
    T_QUOTED_STRING = 284,
    T_INTEGER = 285,
    T_DOUBLE = 286,
    T_PARAMETER = 287,
    T_ASSIGN = 288,
    T_NOT = 289,
    T_AND = 290,
    T_OR = 291,
    T_NIN = 292,
    T_REGEX_MATCH = 293,
    T_REGEX_NON_MATCH = 294,
    T_EQ = 295,
    T_NE = 296,
    T_LT = 297,
    T_GT = 298,
    T_LE = 299,
    T_GE = 300,
    T_LIKE = 301,
    T_PLUS = 302,
    T_MINUS = 303,
    T_TIMES = 304,
    T_DIV = 305,
    T_MOD = 306,
    T_QUESTION = 307,
    T_COLON = 308,
    T_SCOPE = 309,
    T_RANGE = 310,
    T_COMMA = 311,
    T_OPEN = 312,
    T_CLOSE = 313,
    T_OBJECT_OPEN = 314,
    T_OBJECT_CLOSE = 315,
    T_ARRAY_OPEN = 316,
    T_ARRAY_CLOSE = 317,
    T_OUTBOUND = 318,
    T_INBOUND = 319,
    T_ANY = 320,
    T_ALL = 321,
    T_NONE = 322,
    UMINUS = 323,
    UPLUS = 324,
    FUNCCALL = 325,
    REFERENCE = 326,
    INDEXED = 327,
    EXPANSION = 328
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 20 "Aql/grammar.y" /* yacc.c:355  */

  arangodb::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 208 "Aql/grammar.cpp" /* yacc.c:355  */
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

#endif /* !YY_AQL_AQL_GRAMMAR_HPP_INCLUDED  */

/* Copy the second part of user declarations.  */
#line 30 "Aql/grammar.y" /* yacc.c:358  */


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
/// @brief check if any of the variables used in the INTO expression were
/// introduced by the COLLECT itself, in which case it would fail
////////////////////////////////////////////////////////////////////////////////

static Variable const* CheckIntoVariables(AstNode const* collectVars,
                                          std::unordered_set<Variable const*> const& vars) {
  if (collectVars == nullptr || collectVars->type != NODE_TYPE_ARRAY) {
    return nullptr;
  }

  size_t const n = collectVars->numMembers();
  for (size_t i = 0; i < n; ++i) {
    auto member = collectVars->getMember(i);

    if (member != nullptr) {
      TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
      auto v = static_cast<Variable*>(member->getMember(0)->getData());
      if (vars.find(v) != vars.end()) {
        return v;
      }
    }
  }

  return nullptr;
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


#line 408 "Aql/grammar.cpp" /* yacc.c:358  */

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
#define YYFINAL  7
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1449

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  92
/* YYNRULES -- Number of rules.  */
#define YYNRULES  220
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  380

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   328

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
       2,     2,     2,     2,     2,     2,    74,     2,     2,     2,
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
      65,    66,    67,    68,    69,    70,    71,    72,    73
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   371,   371,   374,   385,   389,   393,   400,   402,   402,
     413,   418,   423,   425,   428,   431,   434,   437,   443,   445,
     450,   452,   454,   456,   458,   460,   462,   464,   466,   468,
     470,   475,   482,   489,   491,   496,   501,   506,   514,   522,
     533,   541,   546,   548,   553,   560,   570,   570,   584,   593,
     604,   634,   701,   726,   759,   761,   766,   773,   776,   779,
     788,   802,   819,   819,   833,   833,   843,   843,   854,   857,
     863,   869,   872,   875,   878,   884,   889,   896,   904,   907,
     913,   923,   933,   941,   952,   957,   965,   976,   981,   984,
     990,   990,  1041,  1044,  1047,  1053,  1053,  1063,  1069,  1072,
    1075,  1078,  1081,  1084,  1090,  1093,  1109,  1109,  1118,  1118,
    1128,  1131,  1134,  1140,  1143,  1146,  1149,  1152,  1155,  1158,
    1161,  1164,  1167,  1170,  1173,  1176,  1179,  1182,  1185,  1191,
    1197,  1204,  1207,  1210,  1213,  1216,  1219,  1222,  1225,  1231,
    1234,  1240,  1242,  1247,  1250,  1250,  1266,  1269,  1275,  1278,
    1284,  1284,  1293,  1295,  1300,  1303,  1309,  1312,  1326,  1326,
    1335,  1337,  1342,  1344,  1349,  1363,  1367,  1376,  1383,  1386,
    1392,  1395,  1401,  1404,  1407,  1413,  1416,  1422,  1425,  1433,
    1437,  1448,  1452,  1459,  1464,  1464,  1472,  1481,  1490,  1493,
    1496,  1502,  1505,  1511,  1543,  1546,  1549,  1556,  1566,  1566,
    1579,  1594,  1608,  1622,  1622,  1665,  1668,  1674,  1681,  1691,
    1694,  1697,  1700,  1703,  1709,  1712,  1715,  1725,  1731,  1734,
    1739
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
  "\"INTO keyword\"", "\"AGGREGATE keyword\"", "\"VIEW keyword\"",
  "\"GRAPH keyword\"", "\"SHORTEST_PATH keyword\"",
  "\"DISTINCT modifier\"", "\"REMOVE command\"", "\"INSERT command\"",
  "\"UPDATE command\"", "\"REPLACE command\"", "\"UPSERT command\"",
  "\"null\"", "\"true\"", "\"false\"", "\"identifier\"",
  "\"quoted string\"", "\"integer number\"", "\"number\"",
  "\"bind parameter\"", "\"assignment\"", "\"not operator\"",
  "\"and operator\"", "\"or operator\"", "\"not in operator\"",
  "\"~= operator\"", "\"~! operator\"", "\"== operator\"",
  "\"!= operator\"", "\"< operator\"", "\"> operator\"", "\"<= operator\"",
  "\">= operator\"", "\"like operator\"", "\"+ operator\"",
  "\"- operator\"", "\"* operator\"", "\"/ operator\"", "\"% operator\"",
  "\"?\"", "\":\"", "\"::\"", "\"..\"", "\",\"", "\"(\"", "\")\"", "\"{\"",
  "\"}\"", "\"[\"", "\"]\"", "\"outbound modifier\"",
  "\"inbound modifier\"", "\"any modifier\"", "\"all modifier\"",
  "\"none modifier\"", "UMINUS", "UPLUS", "FUNCCALL", "REFERENCE",
  "INDEXED", "EXPANSION", "'.'", "$accept", "with_collection",
  "with_collection_list", "optional_with", "$@1", "queryStart", "query",
  "final_statement", "optional_statement_block_statements",
  "statement_block_statement", "for_statement", "traversal_statement",
  "shortest_path_statement", "filter_statement", "let_statement",
  "let_list", "let_element", "count_into", "collect_variable_list", "$@2",
  "collect_statement", "collect_list", "collect_element",
  "collect_optional_into", "variable_list", "keep", "$@3", "aggregate",
  "$@4", "sort_statement", "$@5", "sort_list", "sort_element",
  "sort_direction", "limit_statement", "return_statement",
  "in_or_into_collection", "remove_statement", "insert_statement",
  "update_parameters", "update_statement", "replace_parameters",
  "replace_statement", "update_or_replace", "upsert_statement", "$@6",
  "quantifier", "distinct_expression", "$@7", "expression",
  "function_name", "function_call", "$@8", "$@9", "operator_unary",
  "operator_binary", "operator_ternary",
  "optional_function_call_arguments", "expression_or_query", "$@10",
  "function_arguments_list", "compound_value", "array", "$@11",
  "optional_array_elements", "array_elements_list", "options", "object",
  "$@12", "optional_object_elements", "object_elements_list",
  "object_element", "array_filter_operator", "optional_array_filter",
  "optional_array_limit", "optional_array_return", "graph_collection",
  "graph_collection_list", "graph_subject", "$@13", "graph_direction",
  "graph_direction_steps", "reference", "$@14", "$@15", "simple_value",
  "numeric_value", "value_literal", "collection_name", "bind_parameter",
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
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,    46
};
# endif

#define YYPACT_NINF -329

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-329)))

#define YYTABLE_NINF -219

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      -8,  -329,  -329,    27,    78,  -329,  1322,  -329,  -329,  -329,
    -329,    14,  -329,     5,     5,  1364,   296,    90,  -329,   208,
    1364,  1364,  1364,  1364,  -329,  -329,  -329,  -329,  -329,  -329,
      43,  -329,  -329,  -329,  -329,     9,    10,    17,    28,    29,
      78,  -329,  -329,  -329,  -329,   -11,   -13,  -329,    36,  -329,
    -329,  -329,    23,  -329,  -329,  -329,  1364,    54,  1364,  1364,
    1364,  -329,  -329,  1081,    89,  -329,  -329,  -329,  -329,  -329,
    -329,  -329,   -22,  -329,  -329,  -329,  -329,  -329,  1081,    88,
    -329,    94,     5,   110,  1364,    75,  -329,  -329,   711,   711,
    -329,   538,  -329,   582,  1364,     5,    94,   104,   110,  -329,
    1205,     5,     5,  1364,  -329,  -329,  -329,  -329,   752,  -329,
       2,  1364,  1364,  1364,  1364,  1364,  1364,  1364,  1364,  1364,
    1364,  1364,  1364,  1364,  1364,  1364,  1364,  1364,  1364,  1364,
    1253,  1364,  -329,  -329,  -329,   347,   113,  -329,  1290,   106,
    1364,   131,     5,   117,  -329,    93,  -329,   145,    94,   125,
    -329,   453,   208,  1388,    72,    94,    94,  1364,    94,  1364,
      94,   793,   147,  -329,   117,    94,  -329,    94,  1364,  -329,
    -329,  -329,   623,   164,  1364,    -9,  -329,  1081,  1327,  -329,
     127,   130,  -329,   134,  1364,   128,   133,  -329,   137,  1081,
     129,   138,   161,  1162,  1122,   161,    70,    70,    70,    70,
      41,    41,    41,    41,    70,   124,   124,  -329,  -329,  -329,
    1364,   834,    79,  1364,  1364,  1364,  1364,  1364,  1364,  1364,
    1364,  -329,  1327,  -329,   876,   144,  -329,  -329,  1081,     5,
      93,  -329,     5,  1364,  -329,  1364,  -329,  -329,  -329,  -329,
    -329,   404,   222,   414,  -329,  -329,  -329,  -329,  -329,  -329,
    -329,   711,  -329,   711,  -329,  1364,  1364,     5,  -329,  -329,
    1081,   403,  -329,  1364,   494,  1229,     5,  1081,   139,  -329,
    -329,   143,  -329,  1364,   917,  -329,     2,  1364,  -329,  1364,
    1081,  1364,   161,   161,    70,    70,    41,    41,    41,    41,
     142,  -329,  -329,   190,  -329,  -329,  1081,  -329,    94,    94,
     669,  1081,   146,  -329,   958,     3,  -329,   162,    94,   112,
    -329,   623,   183,  1364,   207,  -329,  -329,  1364,  1081,   167,
    -329,  1081,  1081,  1081,  -329,  1364,   212,  -329,  -329,  -329,
    -329,  1364,     5,  1364,  -329,  -329,  -329,  -329,  -329,  -329,
    1364,   494,  1229,  -329,  1364,  1081,  1364,   226,   711,  -329,
     494,   -14,   999,    94,  -329,  1364,  1081,  1040,  1364,   179,
      94,    94,  -329,   191,  1364,  -329,   494,  1364,  1081,  -329,
    -329,  -329,   -14,   494,    94,  1081,  -329,    94,  -329,  -329
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       7,     8,    18,     0,     0,    10,     0,     1,     2,   217,
       4,     9,     3,     0,     0,     0,     0,    46,    66,     0,
       0,     0,     0,     0,    90,    11,    19,    20,    22,    21,
      57,    23,    24,    25,    12,    26,    27,    28,    29,    30,
       0,     6,   220,    33,    34,     0,    41,    42,     0,   211,
     212,   213,   193,   209,   207,   208,     0,     0,     0,     0,
     198,   158,   150,    40,     0,   196,    98,    99,   100,   194,
     148,   149,   102,   210,   101,   195,    95,    77,    97,     0,
      64,   156,     0,    57,     0,    75,   205,   206,     0,     0,
      84,     0,    87,     0,     0,     0,   156,   156,    57,     5,
       0,     0,     0,     0,   112,   108,   110,   111,     0,    18,
     160,   152,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    93,    92,    94,     0,     0,   106,     0,     0,
       0,     0,     0,     0,    48,    47,    54,     0,   156,    67,
      68,    71,     0,     0,     0,   156,   156,     0,   156,     0,
     156,     0,    58,    49,    62,   156,    52,   156,     0,   188,
     189,   190,    32,   191,     0,     0,    43,    44,   144,   197,
       0,   164,   219,     0,     0,     0,   161,   162,     0,   154,
       0,   153,   126,   114,   113,   127,   129,   130,   120,   121,
     122,   123,   124,   125,   128,   115,   116,   117,   118,   119,
       0,     0,   103,     0,     0,     0,     0,     0,     0,     0,
       0,   105,   144,   168,     0,   203,   200,   201,    96,     0,
      65,   157,     0,     0,    50,     0,    72,    73,    70,    74,
      76,   193,   209,   217,    78,   214,   215,   216,    79,    80,
      81,     0,    82,     0,    85,     0,     0,     0,    53,    51,
      31,   190,   192,     0,     0,     0,     0,   143,     0,   146,
      18,   142,   199,     0,     0,   159,     0,     0,   151,     0,
     140,     0,   137,   138,   131,   132,   133,   134,   135,   136,
       0,   202,   169,   170,    45,    55,    56,    69,   156,   156,
       0,    59,    63,    60,     0,     0,   177,   183,   156,     0,
     178,     0,   191,     0,     0,   109,   145,   144,   166,     0,
     163,   165,   155,   139,   107,     0,   172,    83,    86,    88,
      89,     0,     0,     0,   187,   186,   184,    35,   179,   180,
       0,     0,     0,   147,     0,   171,     0,   175,     0,    61,
       0,     0,     0,   156,   191,     0,   167,   173,     0,     0,
     156,   156,   181,   185,     0,    36,     0,     0,   176,   204,
      91,    38,     0,     0,   156,   174,   182,   156,    37,    39
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -329,    -3,  -329,  -329,  -329,  -329,   -98,  -329,  -329,  -329,
    -329,  -329,  -329,  -329,  -329,  -329,   149,   231,  -329,  -329,
    -329,   120,    31,   -45,  -329,  -329,  -329,   235,  -329,  -329,
    -329,  -329,    32,  -329,  -329,  -329,   -67,  -329,  -329,  -329,
    -329,  -329,  -329,  -329,  -329,  -329,  -329,  -329,  -329,    39,
    -329,  -329,  -329,  -329,  -329,  -329,  -329,    44,   -49,  -329,
    -329,  -329,  -329,  -329,  -329,  -329,   -84,  -124,  -329,  -329,
    -329,    -6,  -329,  -329,  -329,  -329,  -328,  -329,  -325,  -329,
     -94,  -263,  -329,  -329,  -329,   -58,  -329,   -15,   121,    -4,
    -329,     7
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    10,    11,     2,     4,     3,     5,    25,     6,    26,
      27,    43,    44,    28,    29,    46,    47,    81,    30,    82,
      31,   145,   146,    97,   302,   165,   257,    83,   142,    32,
      84,   149,   150,   238,    33,    34,   155,    35,    36,    90,
      37,    92,    38,   331,    39,    94,   135,    77,   140,   267,
      64,    65,   222,   178,    66,    67,    68,   268,   269,   270,
     271,    69,    70,   111,   190,   191,   144,    71,   110,   185,
     186,   187,   225,   326,   347,   359,   307,   363,   308,   351,
     309,   174,    72,   109,   293,    85,    73,    74,   244,    75,
     188,   147
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      12,   100,   313,   265,    86,     1,   173,    12,    41,   -13,
     -14,   180,   163,   166,   306,    87,   353,   -15,     9,   231,
      45,    48,   156,   362,   158,   361,   160,     7,   -16,   -17,
     181,   182,   334,    42,   183,     9,    12,    99,   148,   138,
     231,   374,     8,   102,   376,   101,     9,   266,   377,   169,
     170,   171,   139,   167,    63,    78,    79,    95,    80,    88,
      89,    91,    93,   184,   234,   -13,   -14,   -13,   -14,   103,
      40,   249,   250,   -15,   252,   -15,   254,  -104,   262,   355,
    -104,   258,   112,   259,   -16,   -17,   -16,   -17,   125,   126,
     127,   128,   129,   239,   240,   104,   131,   106,   107,   108,
     245,   246,   162,    79,   247,    80,     8,   115,   175,    48,
       9,   105,   120,   121,   122,   123,   141,   125,   126,   127,
     128,   129,   143,   151,    95,   131,   125,   126,   127,   128,
     129,   152,   164,   161,   226,   227,    86,    86,     9,   172,
     338,   221,   177,   136,     9,   229,   137,    87,    87,   232,
     189,   192,   193,   194,   195,   196,   197,   198,   199,   200,
     201,   202,   203,   204,   205,   206,   207,   208,   209,   211,
     212,   312,   316,   127,   128,   129,    61,   224,   233,   228,
     256,   235,   263,  -218,   298,   272,   299,   273,   275,   276,
     277,   278,   192,   292,   279,   325,   251,   315,   253,   317,
     324,   340,   332,   120,   121,   122,   123,   260,   125,   126,
     127,   128,   129,   264,   327,   328,   131,   262,   336,   342,
     344,   346,  -215,   274,   337,  -215,  -215,  -215,  -215,  -215,
    -215,  -215,   358,    49,    50,    51,   294,    53,    54,    55,
       9,   369,  -215,  -215,  -215,  -215,  -215,   372,   354,   280,
    -215,   176,   282,   283,   284,   285,   286,   287,   288,   289,
     310,    96,   230,   295,   303,    98,   290,   297,   343,   365,
     320,     0,   296,   314,   151,   248,   370,   371,  -215,     0,
    -215,   360,     0,     0,     0,     0,     0,     0,     0,     0,
     378,     0,     0,   379,   300,   301,     0,     0,     0,     0,
       0,   335,   304,     0,   311,   339,     0,     0,     0,     0,
       0,     0,   318,     0,     0,    76,   321,     0,   322,     0,
     323,    49,    50,    51,    52,    53,    54,    55,     9,     0,
      56,     0,     0,     0,     0,     0,     0,   310,     0,   349,
       0,     0,    57,    58,    59,     0,   310,   310,     0,     0,
       0,     0,   341,    60,     0,    61,     0,    62,     0,   213,
       0,     0,   310,     0,   345,     0,     0,     0,   310,   310,
     348,     0,   350,     0,     0,     0,     0,     0,     0,   352,
       0,   311,     0,   356,   214,   357,     0,   215,   216,   217,
     218,   219,   220,     0,   366,     0,     0,   368,     0,     0,
       0,     0,     0,   373,  -214,     0,   375,  -214,  -214,  -214,
    -214,  -214,  -214,  -214,  -216,   -93,     0,  -216,  -216,  -216,
    -216,  -216,  -216,  -216,  -214,  -214,  -214,  -214,  -214,     0,
       0,     0,  -214,     0,  -216,  -216,  -216,  -216,  -216,     0,
     -93,     0,  -216,   -93,   -93,   -93,   -93,   -93,   -93,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -104,     0,
    -214,  -104,  -214,   236,   237,   112,     0,     0,     0,     0,
    -216,     0,  -216,     0,     0,     0,     0,     0,    49,    50,
      51,     0,    53,    54,    55,     9,     0,     0,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   112,     0,   131,     0,
       0,   305,     0,     0,     0,     0,     0,     0,   132,   133,
     134,     0,   306,     0,     0,     0,     9,     0,     0,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,     0,     0,   131,
     153,   157,   154,     0,     0,     0,     0,   169,   170,   261,
     133,   134,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,     0,     0,   131,   153,   159,   154,     0,     0,     0,
       0,     0,     0,   132,   133,   134,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   112,     0,   131,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   132,   133,   134,
       0,     0,     0,     0,     0,     0,     0,     0,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,     0,     0,   131,     0,
       0,   112,     0,     0,     0,     0,   169,   170,   261,   133,
     134,   329,   330,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,     0,   153,   131,   154,     0,     0,     0,     0,
       0,     0,     0,     0,   132,   133,   134,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   112,     0,   131,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   132,   133,   134,     0,
       0,     0,     0,     0,     0,     0,     0,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   112,     0,   131,     0,     0,
     179,     0,     0,     0,   255,     0,     0,   132,   133,   134,
       0,     0,     0,     0,     0,     0,     0,     0,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   112,     0,   131,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   132,   133,
     134,     0,     0,     0,     0,     0,     0,     0,     0,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   281,   112,   131,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   132,
     133,   134,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   112,
       0,   131,     0,     0,     0,     0,     0,     0,   291,     0,
       0,   132,   133,   134,     0,     0,     0,     0,     0,     0,
       0,     0,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     112,     0,   131,     0,     0,     0,     0,     0,     0,   319,
       0,     0,   132,   133,   134,     0,   333,     0,     0,     0,
       0,     0,     0,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   112,     0,   131,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   132,   133,   134,     0,   364,     0,     0,
       0,     0,     0,     0,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   112,     0,   131,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   132,   133,   134,     0,     0,     0,
       0,     0,     0,     0,     0,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   112,     0,   131,   367,     0,     0,     0,
       0,     0,     0,     0,     0,   132,   133,   134,     0,     0,
       0,     0,     0,     0,     0,     0,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   112,     0,   131,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   132,   133,   134,     0,
       0,     0,     0,     0,     0,     0,     0,   113,     0,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   112,     0,     0,   131,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   132,   133,   134,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,     0,     0,     0,   131,     0,     0,
       0,   168,     0,     0,     0,     0,     0,   132,   133,   134,
      49,    50,    51,    52,    53,    54,    55,     9,     0,    56,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    57,    58,    59,    49,    50,    51,    52,    53,    54,
      55,     9,    60,    56,    61,     0,    62,     0,   169,   170,
     171,     0,     0,     0,     0,    57,    58,    59,    49,    50,
      51,    52,    53,    54,    55,     9,    60,    56,    61,     0,
      62,     0,   169,   170,   171,     0,     0,     0,     0,    57,
      58,    59,     0,     0,     0,     0,   210,     0,     0,     0,
      60,     0,    61,     0,    62,    49,    50,    51,    52,    53,
      54,    55,     9,     0,    56,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,    57,    58,    59,   223,
       0,     0,    20,    21,    22,    23,    24,    60,     0,    61,
       0,    62,    49,    50,    51,    52,    53,    54,    55,     9,
       0,    56,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    57,    58,    59,     0,     0,     0,     0,
       0,     0,     0,     0,    60,  -141,    61,     0,    62,    49,
      50,    51,    52,    53,    54,    55,     9,     0,    56,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      57,    58,    59,    49,    50,    51,   241,   242,    54,    55,
     243,    60,    56,    61,     0,    62,     0,     0,     0,     0,
       0,     0,     0,     0,    57,    58,    59,     0,     0,     0,
       0,     0,     0,     0,     0,    60,     0,    61,     0,    62
};

static const yytype_int16 yycheck[] =
{
       4,    12,   265,    12,    19,    13,   100,    11,    11,     0,
       0,   109,    96,    97,    28,    19,   341,     0,    32,   143,
      13,    14,    89,   351,    91,   350,    93,     0,     0,     0,
      28,    29,    29,    28,    32,    32,    40,    40,    83,    61,
     164,   366,    28,    56,   372,    56,    32,    56,   373,    63,
      64,    65,    74,    98,    15,    16,    13,    14,    15,    20,
      21,    22,    23,    61,   148,    56,    56,    58,    58,    33,
      56,   155,   156,    56,   158,    58,   160,    54,   172,   342,
      57,   165,    12,   167,    56,    56,    58,    58,    47,    48,
      49,    50,    51,   151,   152,    56,    55,    58,    59,    60,
      28,    29,    95,    13,    32,    15,    28,    37,   101,   102,
      32,    57,    42,    43,    44,    45,    28,    47,    48,    49,
      50,    51,    28,    84,    14,    55,    47,    48,    49,    50,
      51,    56,    28,    94,    28,   139,   151,   152,    32,   100,
      28,    28,   103,    54,    32,    14,    57,   151,   152,    56,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   265,   270,    49,    50,    51,    59,   138,    33,   140,
      33,    56,    18,    53,   251,    58,   253,    53,    60,    56,
      53,    62,   153,    49,    56,     5,   157,    58,   159,    56,
      58,    18,    56,    42,    43,    44,    45,   168,    47,    48,
      49,    50,    51,   174,   298,   299,    55,   311,    56,    12,
      53,     9,     0,   184,   308,     3,     4,     5,     6,     7,
       8,     9,     6,    25,    26,    27,   229,    29,    30,    31,
      32,    62,    20,    21,    22,    23,    24,    56,   342,   210,
      28,   102,   213,   214,   215,   216,   217,   218,   219,   220,
     264,    30,   142,   232,   257,    30,   222,   235,   317,   353,
     276,    -1,   233,   266,   235,   154,   360,   361,    56,    -1,
      58,   348,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     374,    -1,    -1,   377,   255,   256,    -1,    -1,    -1,    -1,
      -1,   305,   263,    -1,   265,   309,    -1,    -1,    -1,    -1,
      -1,    -1,   273,    -1,    -1,    19,   277,    -1,   279,    -1,
     281,    25,    26,    27,    28,    29,    30,    31,    32,    -1,
      34,    -1,    -1,    -1,    -1,    -1,    -1,   341,    -1,   332,
      -1,    -1,    46,    47,    48,    -1,   350,   351,    -1,    -1,
      -1,    -1,   313,    57,    -1,    59,    -1,    61,    -1,    12,
      -1,    -1,   366,    -1,   325,    -1,    -1,    -1,   372,   373,
     331,    -1,   333,    -1,    -1,    -1,    -1,    -1,    -1,   340,
      -1,   342,    -1,   344,    37,   346,    -1,    40,    41,    42,
      43,    44,    45,    -1,   355,    -1,    -1,   358,    -1,    -1,
      -1,    -1,    -1,   364,     0,    -1,   367,     3,     4,     5,
       6,     7,     8,     9,     0,    12,    -1,     3,     4,     5,
       6,     7,     8,     9,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    28,    -1,    20,    21,    22,    23,    24,    -1,
      37,    -1,    28,    40,    41,    42,    43,    44,    45,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    -1,
      56,    57,    58,    10,    11,    12,    -1,    -1,    -1,    -1,
      56,    -1,    58,    -1,    -1,    -1,    -1,    -1,    25,    26,
      27,    -1,    29,    30,    31,    32,    -1,    -1,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    12,    -1,    55,    -1,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,
      67,    -1,    28,    -1,    -1,    -1,    32,    -1,    -1,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    -1,    -1,    55,
      12,    13,    14,    -1,    -1,    -1,    -1,    63,    64,    65,
      66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    -1,    -1,    55,    12,    13,    14,    -1,    -1,    -1,
      -1,    -1,    -1,    65,    66,    67,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    12,    -1,    55,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    -1,    -1,    55,    -1,
      -1,    12,    -1,    -1,    -1,    -1,    63,    64,    65,    66,
      67,    22,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    -1,    12,    55,    14,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    65,    66,    67,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    12,    -1,    55,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    12,    -1,    55,    -1,    -1,
      58,    -1,    -1,    -1,    21,    -1,    -1,    65,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    12,    -1,    55,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,
      67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    12,    55,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,
      66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    12,
      -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,    62,    -1,
      -1,    65,    66,    67,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      12,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,    62,
      -1,    -1,    65,    66,    67,    -1,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    12,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    65,    66,    67,    -1,    28,    -1,    -1,
      -1,    -1,    -1,    -1,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    12,    -1,    55,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    65,    66,    67,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    12,    -1,    55,    56,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    65,    66,    67,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    12,    -1,    55,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    35,    -1,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    12,    -1,    -1,    55,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    -1,    -1,    -1,    55,    -1,    -1,
      -1,    16,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,
      25,    26,    27,    28,    29,    30,    31,    32,    -1,    34,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    46,    47,    48,    25,    26,    27,    28,    29,    30,
      31,    32,    57,    34,    59,    -1,    61,    -1,    63,    64,
      65,    -1,    -1,    -1,    -1,    46,    47,    48,    25,    26,
      27,    28,    29,    30,    31,    32,    57,    34,    59,    -1,
      61,    -1,    63,    64,    65,    -1,    -1,    -1,    -1,    46,
      47,    48,    -1,    -1,    -1,    -1,    53,    -1,    -1,    -1,
      57,    -1,    59,    -1,    61,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    34,     3,     4,     5,     6,     7,
       8,     9,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      -1,    -1,    20,    21,    22,    23,    24,    57,    -1,    59,
      -1,    61,    25,    26,    27,    28,    29,    30,    31,    32,
      -1,    34,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    47,    48,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    57,    58,    59,    -1,    61,    25,
      26,    27,    28,    29,    30,    31,    32,    -1,    34,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    47,    48,    25,    26,    27,    28,    29,    30,    31,
      32,    57,    34,    59,    -1,    61,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    47,    48,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    57,    -1,    59,    -1,    61
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    13,    78,    80,    79,    81,    83,     0,    28,    32,
      76,    77,   164,     3,     4,     5,     6,     7,     8,     9,
      20,    21,    22,    23,    24,    82,    84,    85,    88,    89,
      93,    95,   104,   109,   110,   112,   113,   115,   117,   119,
      56,    76,    28,    86,    87,   166,    90,    91,   166,    25,
      26,    27,    28,    29,    30,    31,    34,    46,    47,    48,
      57,    59,    61,   124,   125,   126,   129,   130,   131,   136,
     137,   142,   157,   161,   162,   164,    19,   122,   124,    13,
      15,    92,    94,   102,   105,   160,   162,   164,   124,   124,
     114,   124,   116,   124,   120,    14,    92,    98,   102,    76,
      12,    56,    56,    33,   124,    57,   124,   124,   124,   158,
     143,   138,    12,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    55,    65,    66,    67,   121,    54,    57,    61,    74,
     123,    28,   103,    28,   141,    96,    97,   166,    98,   106,
     107,   124,    56,    12,    14,   111,   111,    13,   111,    13,
     111,   124,   166,   141,    28,   100,   141,    98,    16,    63,
      64,    65,   124,   155,   156,   166,    91,   124,   128,    58,
      81,    28,    29,    32,    61,   144,   145,   146,   165,   124,
     139,   140,   124,   124,   124,   124,   124,   124,   124,   124,
     124,   124,   124,   124,   124,   124,   124,   124,   124,   124,
      53,   124,   124,    12,    37,    40,    41,    42,    43,    44,
      45,    28,   127,    49,   124,   147,    28,   164,   124,    14,
      96,   142,    56,    33,   141,    56,    10,    11,   108,   160,
     160,    28,    29,    32,   163,    28,    29,    32,   163,   141,
     141,   124,   141,   124,   141,    21,    33,   101,   141,   141,
     124,    65,   155,    18,   124,    12,    56,   124,   132,   133,
     134,   135,    58,    53,   124,    60,    56,    53,    62,    56,
     124,    53,   124,   124,   124,   124,   124,   124,   124,   124,
     132,    62,    49,   159,   166,    97,   124,   107,   111,   111,
     124,   124,    99,   166,   124,    17,    28,   151,   153,   155,
     164,   124,   155,   156,   166,    58,    81,    56,   124,    62,
     146,   124,   124,   124,    58,     5,   148,   141,   141,    22,
      23,   118,    56,    28,    29,   164,    56,   141,    28,   164,
      18,   124,    12,   133,    53,   124,     9,   149,   124,   166,
     124,   154,   124,   153,   155,   156,   124,   124,     6,   150,
     111,   153,   151,   152,    28,   141,   124,    56,   124,    62,
     141,   141,    56,   124,   153,   124,   151,   153,   141,   141
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    75,    76,    76,    77,    77,    77,    78,    79,    78,
      80,    81,    82,    82,    82,    82,    82,    82,    83,    83,
      84,    84,    84,    84,    84,    84,    84,    84,    84,    84,
      84,    85,    85,    85,    85,    86,    86,    86,    87,    87,
      88,    89,    90,    90,    91,    92,    94,    93,    95,    95,
      95,    95,    95,    95,    96,    96,    97,    98,    98,    98,
      99,    99,   101,   100,   103,   102,   105,   104,   106,   106,
     107,   108,   108,   108,   108,   109,   109,   110,   111,   111,
     112,   113,   114,   114,   115,   116,   116,   117,   118,   118,
     120,   119,   121,   121,   121,   123,   122,   122,   124,   124,
     124,   124,   124,   124,   125,   125,   127,   126,   128,   126,
     129,   129,   129,   130,   130,   130,   130,   130,   130,   130,
     130,   130,   130,   130,   130,   130,   130,   130,   130,   130,
     130,   130,   130,   130,   130,   130,   130,   130,   130,   131,
     131,   132,   132,   133,   134,   133,   135,   135,   136,   136,
     138,   137,   139,   139,   140,   140,   141,   141,   143,   142,
     144,   144,   145,   145,   146,   146,   146,   146,   147,   147,
     148,   148,   149,   149,   149,   150,   150,   151,   151,   151,
     151,   152,   152,   153,   154,   153,   153,   153,   155,   155,
     155,   156,   156,   157,   157,   157,   157,   157,   158,   157,
     157,   157,   157,   159,   157,   160,   160,   161,   161,   162,
     162,   162,   162,   162,   163,   163,   163,   164,   165,   165,
     166
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     1,     3,     2,     0,     0,     3,
       2,     2,     1,     1,     1,     1,     1,     1,     0,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     5,     4,     2,     2,     6,     8,    10,     9,    11,
       2,     2,     1,     3,     3,     4,     0,     3,     3,     3,
       4,     4,     3,     4,     1,     3,     3,     0,     2,     4,
       1,     3,     0,     3,     0,     3,     0,     3,     1,     3,
       2,     0,     1,     1,     1,     2,     4,     2,     2,     2,
       4,     4,     3,     5,     2,     3,     5,     2,     1,     1,
       0,     9,     1,     1,     1,     0,     3,     1,     1,     1,
       1,     1,     1,     3,     1,     3,     0,     5,     0,     5,
       2,     2,     2,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     4,     4,     4,     4,     4,     4,     4,     4,     5,
       4,     0,     1,     1,     0,     2,     1,     3,     1,     1,
       0,     4,     0,     1,     1,     3,     0,     2,     0,     4,
       0,     1,     1,     3,     1,     3,     3,     5,     1,     2,
       0,     2,     0,     2,     4,     0,     2,     1,     1,     2,
       2,     1,     3,     1,     0,     4,     2,     2,     1,     1,
       1,     1,     2,     1,     1,     1,     1,     3,     0,     4,
       3,     3,     4,     0,     8,     1,     1,     1,     1,     1,
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
#line 371 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 2120 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 3:
#line 374 "Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 2133 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 4:
#line 385 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 2142 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 389 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 2151 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 393 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 2160 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 400 "Aql/grammar.y" /* yacc.c:1646  */
    {
     }
#line 2167 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 8:
#line 402 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
     }
#line 2176 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 405 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      auto withNode = parser->ast()->createNodeWithCollections(node);
      parser->ast()->addOperation(withNode);
     }
#line 2186 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 413 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2193 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 418 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2200 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 423 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2207 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 13:
#line 425 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2215 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 14:
#line 428 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2223 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 431 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2231 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 434 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2239 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 437 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2247 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 443 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2254 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 445 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2261 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 450 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2268 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 452 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2275 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 454 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2282 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 456 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2289 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 458 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2296 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 460 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2303 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 462 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2310 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 464 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2317 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 466 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2324 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 468 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2331 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 470 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2338 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 475 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);

      auto node = parser->ast()->createNodeFor((yyvsp[-3].strval).value, (yyvsp[-3].strval).length, (yyvsp[0].node), true,
        true);
      parser->ast()->addOperation(node);
    }
#line 2350 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 482 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);

      auto node = parser->ast()->createNodeFor((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true,
        false);
      parser->ast()->addOperation(node);
    }
#line 2362 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 489 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2369 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 491 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2376 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 496 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-5].strval).value, (yyvsp[-5].strval).length, (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2386 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 501 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-7].strval).value, (yyvsp[-7].strval).length, (yyvsp[-5].strval).value, (yyvsp[-5].strval).length, (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2396 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 506 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-9].strval).value, (yyvsp[-9].strval).length, (yyvsp[-7].strval).value, (yyvsp[-7].strval).length, (yyvsp[-5].strval).value, (yyvsp[-5].strval).length, (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2406 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 514 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (!TRI_CaseEqualString((yyvsp[-3].strval).value, "TO")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'TO'", (yyvsp[-3].strval).value, yylloc.first_line, yylloc.first_column);
      }
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeShortestPath((yyvsp[-8].strval).value, (yyvsp[-8].strval).length, (yyvsp[-6].intval), (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2419 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 522 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (!TRI_CaseEqualString((yyvsp[-3].strval).value, "TO")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'TO'", (yyvsp[-3].strval).value, yylloc.first_line, yylloc.first_column);
      }
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeShortestPath((yyvsp[-10].strval).value, (yyvsp[-10].strval).length, (yyvsp[-8].strval).value, (yyvsp[-8].strval).length, (yyvsp[-6].intval), (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2432 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 533 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // operand is a reference. can use it directly
      auto node = parser->ast()->createNodeFilter((yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2442 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 541 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2449 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 546 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2456 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 548 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2463 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 553 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLet((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2472 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 560 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[-2].strval).value, "COUNT")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'COUNT'", (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2484 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 46:
#line 570 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2493 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 573 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());

      if (list == nullptr) {
        ABORT_OOM
      }
      (yyval.node) = list;
    }
#line 2506 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 48:
#line 584 "Aql/grammar.y" /* yacc.c:1646  */
    {
      /* COLLECT WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      StartCollectScope(scopes);

      auto node = parser->ast()->createNodeCollectCount(parser->ast()->createNodeArray(), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2520 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 593 "Aql/grammar.y" /* yacc.c:1646  */
    {
      /* COLLECT var = expr WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-2].node));
      }

      auto node = parser->ast()->createNodeCollectCount((yyvsp[-2].node), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2536 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 604 "Aql/grammar.y" /* yacc.c:1646  */
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

      if ((yyvsp[-1].node) != nullptr && (yyvsp[-1].node)->type == NODE_TYPE_ARRAY) {
        std::unordered_set<Variable const*> vars;
        Ast::getReferencedVariables((yyvsp[-1].node)->getMember(1), vars);

        Variable const* used = CheckIntoVariables((yyvsp[-2].node), vars);
        if (used != nullptr) {
          std::string msg("use of COLLECT variable '" + used->name + "' IN INTO expression");
          parser->registerParseError(TRI_ERROR_QUERY_PARSE, msg.c_str(), yylloc.first_line, yylloc.first_column);
        }
      }

      AstNode const* into = GetIntoVariable(parser, (yyvsp[-1].node));
      AstNode const* intoExpression = GetIntoExpression((yyvsp[-1].node));

      auto node = parser->ast()->createNodeCollect(parser->ast()->createNodeArray(), (yyvsp[-2].node), into, intoExpression, nullptr, (yyvsp[-1].node));
      parser->ast()->addOperation(node);
    }
#line 2571 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 634 "Aql/grammar.y" /* yacc.c:1646  */
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

      if ((yyvsp[-1].node) != nullptr && (yyvsp[-1].node)->type == NODE_TYPE_ARRAY) {
        std::unordered_set<Variable const*> vars;
        Ast::getReferencedVariables((yyvsp[-1].node)->getMember(1), vars);

        Variable const* used = CheckIntoVariables((yyvsp[-3].node), vars);
        if (used != nullptr) {
          std::string msg("use of COLLECT variable '" + used->name + "' IN INTO expression");
          parser->registerParseError(TRI_ERROR_QUERY_PARSE, msg.c_str(), yylloc.first_line, yylloc.first_column);
        }
        used = CheckIntoVariables((yyvsp[-2].node), vars);
        if (used != nullptr) {
          std::string msg("use of COLLECT variable '" + used->name + "' IN INTO expression");
          parser->registerParseError(TRI_ERROR_QUERY_PARSE, msg.c_str(), yylloc.first_line, yylloc.first_column);
        }
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
#line 2643 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 52:
#line 701 "Aql/grammar.y" /* yacc.c:1646  */
    {
      /* COLLECT var = expr INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-2].node));
      }

      if ((yyvsp[-1].node) != nullptr && (yyvsp[-1].node)->type == NODE_TYPE_ARRAY) {
        std::unordered_set<Variable const*> vars;
        Ast::getReferencedVariables((yyvsp[-1].node)->getMember(1), vars);

        Variable const* used = CheckIntoVariables((yyvsp[-2].node), vars);
        if (used != nullptr) {
          std::string msg("use of COLLECT variable '" + used->name + "' IN INTO expression");
          parser->registerParseError(TRI_ERROR_QUERY_PARSE, msg.c_str(), yylloc.first_line, yylloc.first_column);
        }
      }

      AstNode const* into = GetIntoVariable(parser, (yyvsp[-1].node));
      AstNode const* intoExpression = GetIntoExpression((yyvsp[-1].node));

      auto node = parser->ast()->createNodeCollect((yyvsp[-2].node), parser->ast()->createNodeArray(), into, intoExpression, nullptr, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2673 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 726 "Aql/grammar.y" /* yacc.c:1646  */
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

      if ((yyvsp[-2].node) != nullptr && (yyvsp[-2].node)->type == NODE_TYPE_ARRAY) {
        std::unordered_set<Variable const*> vars;
        Ast::getReferencedVariables((yyvsp[-2].node)->getMember(1), vars);

        Variable const* used = CheckIntoVariables((yyvsp[-3].node), vars);
        if (used != nullptr) {
          std::string msg("use of COLLECT variable '" + used->name + "' IN INTO expression");
          parser->registerParseError(TRI_ERROR_QUERY_PARSE, msg.c_str(), yylloc.first_line, yylloc.first_column);
        }
      }

      AstNode const* into = GetIntoVariable(parser, (yyvsp[-2].node));
      AstNode const* intoExpression = GetIntoExpression((yyvsp[-2].node));

      auto node = parser->ast()->createNodeCollect((yyvsp[-3].node), parser->ast()->createNodeArray(), into, intoExpression, (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2708 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 759 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2715 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 761 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2722 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 766 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeAssign((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
      parser->pushArrayElement(node);
    }
#line 2731 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 773 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 2739 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 776 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 2747 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 779 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember(parser->ast()->createNodeValueString((yyvsp[-2].strval).value, (yyvsp[-2].strval).length));
      node->addMember((yyvsp[0].node));
      (yyval.node) = node;
    }
#line 2758 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 788 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2777 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 802 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2796 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 819 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[0].strval).value, "KEEP")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'KEEP'", (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2809 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 826 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2818 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 833 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2827 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 836 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2836 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 843 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2845 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 846 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      auto node = parser->ast()->createNodeSort(list);
      parser->ast()->addOperation(node);
    }
#line 2855 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 854 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2863 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 857 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2871 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 863 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeSortElement((yyvsp[-1].node), (yyvsp[0].node));
    }
#line 2879 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 869 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2887 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 872 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2895 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 875 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 2903 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 74:
#line 878 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2911 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 75:
#line 884 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto offset = parser->ast()->createNodeValueInt(0);
      auto node = parser->ast()->createNodeLimit(offset, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2921 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 76:
#line 889 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLimit((yyvsp[-2].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2930 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 77:
#line 896 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeReturn((yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2940 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 904 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2948 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 907 "Aql/grammar.y" /* yacc.c:1646  */
    {
       (yyval.node) = (yyvsp[0].node);
     }
#line 2956 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 913 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2968 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 923 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2980 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 82:
#line 933 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2993 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 83:
#line 941 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 3006 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 952 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3013 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 85:
#line 957 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 3026 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 86:
#line 965 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 3039 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 87:
#line 976 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3046 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 88:
#line 981 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_UPDATE);
    }
#line 3054 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 89:
#line 984 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_REPLACE);
    }
#line 3062 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 90:
#line 990 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // reserve a variable named "$OLD", we might need it in the update expression
      // and in a later return thing
      parser->pushStack(parser->ast()->createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD), true));
    }
#line 3072 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 91:
#line 994 "Aql/grammar.y" /* yacc.c:1646  */
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
      auto forNode = parser->ast()->createNodeFor(variableName.c_str(), variableName.size(), (yyvsp[-1].node), false, false);
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
#line 3121 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 92:
#line 1041 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeQuantifier(Quantifier::ALL);
    }
#line 3129 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 93:
#line 1044 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeQuantifier(Quantifier::ANY);
    }
#line 3137 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 94:
#line 1047 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeQuantifier(Quantifier::NONE);
    }
#line 3145 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 95:
#line 1053 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto const scopeType = parser->ast()->scopes()->type();

      if (scopeType == AQL_SCOPE_MAIN ||
          scopeType == AQL_SCOPE_SUBQUERY) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "cannot use DISTINCT modifier on top-level query element", yylloc.first_line, yylloc.first_column);
      }
    }
#line 3158 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 96:
#line 1060 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDistinct((yyvsp[0].node));
    }
#line 3166 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 97:
#line 1063 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3174 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 98:
#line 1069 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3182 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 99:
#line 1072 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3190 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 100:
#line 1075 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3198 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 101:
#line 1078 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3206 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 102:
#line 1081 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3214 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 103:
#line 1084 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeRange((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3222 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 104:
#line 1090 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3230 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 105:
#line 1093 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 3248 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 106:
#line 1109 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushStack((yyvsp[-1].strval).value);

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3259 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 107:
#line 1114 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall(static_cast<char const*>(parser->popStack()), list);
    }
#line 3268 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 108:
#line 1118 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3277 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 109:
#line 1121 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall("LIKE", list);
    }
#line 3286 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 110:
#line 1128 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_PLUS, (yyvsp[0].node));
    }
#line 3294 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 111:
#line 1131 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_MINUS, (yyvsp[0].node));
    }
#line 3302 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 112:
#line 1134 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, (yyvsp[0].node));
    }
#line 3310 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 113:
#line 1140 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3318 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 114:
#line 1143 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3326 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 115:
#line 1146 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_PLUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3334 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 116:
#line 1149 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MINUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3342 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 117:
#line 1152 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_TIMES, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3350 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 118:
#line 1155 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_DIV, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3358 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 119:
#line 1158 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MOD, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3366 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 120:
#line 1161 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3374 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 121:
#line 1164 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3382 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 122:
#line 1167 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3390 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 123:
#line 1170 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3398 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 124:
#line 1173 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3406 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 125:
#line 1176 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3414 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 126:
#line 1179 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3422 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 127:
#line 1182 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3430 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 128:
#line 1185 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* arguments = parser->ast()->createNodeArray(2);
      arguments->addMember((yyvsp[-2].node));
      arguments->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeFunctionCall("LIKE", arguments);
    }
#line 3441 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 129:
#line 1191 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* arguments = parser->ast()->createNodeArray(2);
      arguments->addMember((yyvsp[-2].node));
      arguments->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeFunctionCall("REGEX_TEST", arguments);
    }
#line 3452 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 130:
#line 1197 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* arguments = parser->ast()->createNodeArray(2);
      arguments->addMember((yyvsp[-2].node));
      arguments->addMember((yyvsp[0].node));
      AstNode* node = parser->ast()->createNodeFunctionCall("REGEX_TEST", arguments);
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, node);
    }
#line 3464 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 131:
#line 1204 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3472 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 132:
#line 1207 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_NE, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3480 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 133:
#line 1210 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_LT, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3488 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 134:
#line 1213 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_GT, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3496 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 135:
#line 1216 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_LE, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3504 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 136:
#line 1219 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_GE, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3512 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 137:
#line 1222 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_IN, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3520 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 138:
#line 1225 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3528 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 139:
#line 1231 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3536 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 140:
#line 1234 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-3].node), (yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3544 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 141:
#line 1240 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3551 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 142:
#line 1242 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3558 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 143:
#line 1247 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3566 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 144:
#line 1250 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 3575 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 145:
#line 1253 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 3590 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 146:
#line 1266 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3598 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 147:
#line 1269 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3606 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 148:
#line 1275 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3614 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 149:
#line 1278 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3622 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 150:
#line 1284 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3631 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 151:
#line 1287 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3639 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 152:
#line 1293 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3646 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 153:
#line 1295 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3653 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 154:
#line 1300 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3661 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 155:
#line 1303 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3669 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 156:
#line 1309 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3677 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 157:
#line 1312 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[-1].strval).value, "OPTIONS")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'OPTIONS'", (yyvsp[-1].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3693 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 158:
#line 1326 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeObject();
      parser->pushStack(node);
    }
#line 3702 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 159:
#line 1329 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3710 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 160:
#line 1335 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3717 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 161:
#line 1337 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3724 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 162:
#line 1342 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3731 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 163:
#line 1344 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3738 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 164:
#line 1349 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 3757 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 165:
#line 1363 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // attribute-name : attribute-value
      parser->pushObjectElement((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
    }
#line 3766 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 166:
#line 1367 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // bind-parameter : attribute-value
      if ((yyvsp[-2].strval).length < 1 || (yyvsp[-2].strval).value[0] == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto param = parser->ast()->createNodeParameter((yyvsp[-2].strval).value, (yyvsp[-2].strval).length);
      parser->pushObjectElement(param, (yyvsp[0].node));
    }
#line 3780 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 167:
#line 1376 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // [ attribute-name-expression ] : attribute-value
      parser->pushObjectElement((yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3789 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 168:
#line 1383 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3797 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 169:
#line 1386 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = (yyvsp[-1].intval) + 1;
    }
#line 3805 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 170:
#line 1392 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3813 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 171:
#line 1395 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3821 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 172:
#line 1401 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3829 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 173:
#line 1404 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit(nullptr, (yyvsp[0].node));
    }
#line 3837 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 174:
#line 1407 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3845 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 175:
#line 1413 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3853 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 176:
#line 1416 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3861 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 177:
#line 1422 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3869 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 178:
#line 1425 "Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3882 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 179:
#line 1433 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto tmp = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), tmp);
    }
#line 3891 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 180:
#line 1437 "Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), (yyvsp[0].node));
    }
#line 3904 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 181:
#line 1448 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3913 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 182:
#line 1452 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3922 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 183:
#line 1459 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3932 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 184:
#line 1464 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
      node->addMember((yyvsp[-1].node));
    }
#line 3942 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 185:
#line 1468 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3951 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 186:
#line 1472 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3965 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 187:
#line 1481 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3974 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 188:
#line 1490 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 2;
    }
#line 3982 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 189:
#line 1493 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3990 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 190:
#line 1496 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 0;
    }
#line 3998 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 191:
#line 1502 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), 1);
    }
#line 4006 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 192:
#line 1505 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), (yyvsp[-1].node));
    }
#line 4014 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 193:
#line 1511 "Aql/grammar.y" /* yacc.c:1646  */
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
        node = ast->createNodeCollection((yyvsp[0].strval).value, arangodb::AccessMode::Type::READ);
      }

      TRI_ASSERT(node != nullptr);

      (yyval.node) = node;
    }
#line 4051 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 194:
#line 1543 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4059 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 195:
#line 1546 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4067 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 196:
#line 1549 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);

      if ((yyval.node) == nullptr) {
        ABORT_OOM
      }
    }
#line 4079 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 197:
#line 1556 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4094 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 198:
#line 1566 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 4103 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 199:
#line 1569 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 4118 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 200:
#line 1579 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4138 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 201:
#line 1594 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4157 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 202:
#line 1608 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4176 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 203:
#line 1622 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4204 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 204:
#line 1644 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4227 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 205:
#line 1665 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4235 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 206:
#line 1668 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4243 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 207:
#line 1674 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 4255 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 208:
#line 1681 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 4267 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 209:
#line 1691 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 4275 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 210:
#line 1694 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4283 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 211:
#line 1697 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueNull();
    }
#line 4291 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 212:
#line 1700 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 4299 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 213:
#line 1703 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 4307 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 214:
#line 1709 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, arangodb::AccessMode::Type::WRITE);
    }
#line 4315 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 215:
#line 1712 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, arangodb::AccessMode::Type::WRITE);
    }
#line 4323 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 216:
#line 1715 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval).length < 2 || (yyvsp[0].strval).value[0] != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 4335 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 217:
#line 1725 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 4343 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 218:
#line 1731 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 4351 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 219:
#line 1734 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 4359 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 220:
#line 1739 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 4367 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;


#line 4371 "Aql/grammar.cpp" /* yacc.c:1646  */
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
