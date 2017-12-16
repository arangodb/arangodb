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
    T_GRAPH = 271,
    T_SHORTEST_PATH = 272,
    T_DISTINCT = 273,
    T_REMOVE = 274,
    T_INSERT = 275,
    T_UPDATE = 276,
    T_REPLACE = 277,
    T_UPSERT = 278,
    T_NULL = 279,
    T_TRUE = 280,
    T_FALSE = 281,
    T_STRING = 282,
    T_QUOTED_STRING = 283,
    T_INTEGER = 284,
    T_DOUBLE = 285,
    T_PARAMETER = 286,
    T_ASSIGN = 287,
    T_NOT = 288,
    T_AND = 289,
    T_OR = 290,
    T_NIN = 291,
    T_REGEX_MATCH = 292,
    T_REGEX_NON_MATCH = 293,
    T_EQ = 294,
    T_NE = 295,
    T_LT = 296,
    T_GT = 297,
    T_LE = 298,
    T_GE = 299,
    T_LIKE = 300,
    T_PLUS = 301,
    T_MINUS = 302,
    T_TIMES = 303,
    T_DIV = 304,
    T_MOD = 305,
    T_QUESTION = 306,
    T_COLON = 307,
    T_SCOPE = 308,
    T_RANGE = 309,
    T_COMMA = 310,
    T_OPEN = 311,
    T_CLOSE = 312,
    T_OBJECT_OPEN = 313,
    T_OBJECT_CLOSE = 314,
    T_ARRAY_OPEN = 315,
    T_ARRAY_CLOSE = 316,
    T_OUTBOUND = 317,
    T_INBOUND = 318,
    T_ANY = 319,
    T_ALL = 320,
    T_NONE = 321,
    UMINUS = 322,
    UPLUS = 323,
    FUNCCALL = 324,
    REFERENCE = 325,
    INDEXED = 326,
    EXPANSION = 327
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

#line 207 "Aql/grammar.cpp" /* yacc.c:355  */
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
        if (!Aggregator::isSupported(f->name)) {
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


#line 407 "Aql/grammar.cpp" /* yacc.c:358  */

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
#define YYLAST   1366

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  74
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  93
/* YYNRULES -- Number of rules.  */
#define YYNRULES  220
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  379

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   327

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
       2,     2,     2,     2,     2,     2,    73,     2,     2,     2,
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
      65,    66,    67,    68,    69,    70,    71,    72
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   369,   369,   372,   383,   387,   391,   398,   400,   400,
     411,   416,   421,   423,   426,   429,   432,   435,   441,   443,
     448,   450,   452,   454,   456,   458,   460,   462,   464,   466,
     468,   473,   479,   481,   486,   491,   496,   504,   512,   523,
     531,   536,   538,   543,   550,   560,   560,   574,   583,   594,
     624,   691,   716,   749,   751,   756,   763,   766,   769,   778,
     792,   809,   809,   823,   823,   833,   833,   844,   847,   853,
     859,   862,   865,   868,   874,   879,   886,   894,   897,   903,
     913,   923,   931,   942,   947,   955,   966,   971,   974,   980,
     984,   980,  1036,  1039,  1042,  1048,  1048,  1058,  1064,  1067,
    1070,  1073,  1076,  1079,  1085,  1088,  1104,  1104,  1113,  1113,
    1123,  1126,  1129,  1135,  1138,  1141,  1144,  1147,  1150,  1153,
    1156,  1159,  1162,  1165,  1168,  1171,  1174,  1177,  1180,  1186,
    1192,  1199,  1202,  1205,  1208,  1211,  1214,  1217,  1220,  1226,
    1229,  1235,  1237,  1242,  1245,  1245,  1261,  1264,  1270,  1273,
    1279,  1279,  1288,  1290,  1295,  1298,  1304,  1307,  1321,  1321,
    1330,  1332,  1337,  1339,  1344,  1358,  1362,  1371,  1378,  1381,
    1387,  1390,  1396,  1399,  1402,  1408,  1411,  1417,  1420,  1428,
    1432,  1443,  1447,  1454,  1459,  1459,  1467,  1476,  1485,  1488,
    1491,  1497,  1500,  1506,  1538,  1541,  1544,  1551,  1561,  1561,
    1574,  1589,  1603,  1617,  1617,  1660,  1663,  1669,  1676,  1686,
    1689,  1692,  1695,  1698,  1704,  1707,  1710,  1720,  1726,  1729,
    1734
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
  "\"SHORTEST_PATH keyword\"", "\"DISTINCT modifier\"",
  "\"REMOVE command\"", "\"INSERT command\"", "\"UPDATE command\"",
  "\"REPLACE command\"", "\"UPSERT command\"", "\"null\"", "\"true\"",
  "\"false\"", "\"identifier\"", "\"quoted string\"", "\"integer number\"",
  "\"number\"", "\"bind parameter\"", "\"assignment\"", "\"not operator\"",
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
  "$@7", "quantifier", "distinct_expression", "$@8", "expression",
  "function_name", "function_call", "$@9", "$@10", "operator_unary",
  "operator_binary", "operator_ternary",
  "optional_function_call_arguments", "expression_or_query", "$@11",
  "function_arguments_list", "compound_value", "array", "$@12",
  "optional_array_elements", "array_elements_list", "options", "object",
  "$@13", "optional_object_elements", "object_elements_list",
  "object_element", "array_filter_operator", "optional_array_filter",
  "optional_array_limit", "optional_array_return", "graph_collection",
  "graph_collection_list", "graph_subject", "$@14", "graph_direction",
  "graph_direction_steps", "reference", "$@15", "$@16", "simple_value",
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
     325,   326,   327,    46
};
# endif

#define YYPACT_NINF -338

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-338)))

#define YYTABLE_NINF -219

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
       6,  -338,  -338,    20,    28,  -338,  1239,  -338,  -338,  -338,
    -338,    74,  -338,    15,    15,  1282,  1093,    89,  -338,   369,
    1282,  1282,  1282,  1282,  -338,  -338,  -338,  -338,  -338,  -338,
     143,  -338,  -338,  -338,  -338,    27,    30,    56,    57,    67,
      28,  -338,  -338,  -338,  -338,    -3,    -1,  -338,    26,  -338,
    -338,  -338,   -25,  -338,  -338,  -338,  1282,    18,  1282,  1282,
    1282,  -338,  -338,   971,   -19,  -338,  -338,  -338,  -338,  -338,
    -338,  -338,   -22,  -338,  -338,  -338,  -338,  -338,   971,    48,
    -338,    53,    15,    78,  1282,    52,  -338,  -338,   650,   650,
    -338,   480,  -338,   523,  1282,    15,    53,    93,    78,  -338,
    1147,    15,    15,  1282,  -338,  -338,  -338,  -338,   690,  -338,
      72,  1282,  1282,  1282,  1282,  1282,  1282,  1282,  1282,  1282,
    1282,  1282,  1282,  1282,  1282,  1282,  1282,  1282,  1282,  1282,
    1171,  1282,  -338,  -338,  -338,   196,   103,  -338,  1208,    94,
    1282,   117,    15,    75,  -338,    88,  -338,   120,    53,    99,
    -338,   397,   369,  1306,   118,    53,    53,  1282,    53,  1282,
      53,   971,   127,  -338,    75,    53,  -338,    53,  -338,  -338,
    -338,   563,   169,  1282,    -2,  -338,   971,  1245,  -338,   130,
     108,  -338,   137,  1282,   132,   138,  -338,   142,   971,   141,
     155,   328,  1050,  1011,   328,  1116,  1116,  1116,  1116,    69,
      69,    69,    69,  1116,   156,   156,  -338,  -338,  -338,  1282,
     730,    92,  1282,  1282,  1282,  1282,  1282,  1282,  1282,  1282,
    -338,  1245,  -338,   771,   172,  -338,  -338,   971,    15,    88,
    -338,    15,  1282,  -338,  1282,  -338,  -338,  -338,  -338,  -338,
      41,   192,   332,  -338,  -338,  -338,  -338,  -338,  -338,  -338,
     650,  -338,   650,  -338,   201,  1282,    15,  -338,  -338,   234,
    -338,  1282,   437,  1147,    15,   971,   165,  -338,  -338,   170,
    -338,  1282,   811,  -338,    72,  1282,  -338,  1282,   971,  1282,
     328,   328,  1116,  1116,    69,    69,    69,    69,   167,  -338,
    -338,   221,  -338,  -338,   971,  -338,    53,    53,  1282,   971,
     173,  -338,   851,    12,  -338,   174,    53,   124,  -338,   563,
     213,  1282,   222,  -338,  -338,  1282,   971,   189,  -338,   971,
     971,   971,  -338,  1282,   233,  -338,  -338,   609,    15,  1282,
    -338,  -338,  -338,  -338,  -338,  -338,  1282,   437,  1147,  -338,
    1282,   971,  1282,   237,  -338,  -338,  1282,  -338,   437,    64,
     891,    53,  -338,  1282,   971,   931,  1282,   184,   650,    53,
    -338,   193,  1282,  -338,   437,  1282,   971,  -338,    53,  -338,
      64,   437,    53,   971,  -338,  -338,    53,  -338,  -338
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       7,     8,    18,     0,     0,    10,     0,     1,     2,   217,
       4,     9,     3,     0,     0,     0,     0,    45,    65,     0,
       0,     0,     0,     0,    89,    11,    19,    20,    22,    21,
      56,    23,    24,    25,    12,    26,    27,    28,    29,    30,
       0,     6,   220,    32,    33,     0,    40,    41,     0,   211,
     212,   213,   193,   209,   207,   208,     0,     0,     0,     0,
     198,   158,   150,    39,     0,   196,    98,    99,   100,   194,
     148,   149,   102,   210,   101,   195,    95,    76,    97,     0,
      63,   156,     0,    56,     0,    74,   205,   206,     0,     0,
      83,     0,    86,     0,     0,     0,   156,   156,    56,     5,
       0,     0,     0,     0,   112,   108,   110,   111,     0,    18,
     160,   152,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    93,    92,    94,     0,     0,   106,     0,     0,
       0,     0,     0,     0,    47,    46,    53,     0,   156,    66,
      67,    70,     0,     0,     0,   156,   156,     0,   156,     0,
     156,    90,    57,    48,    61,   156,    51,   156,   188,   189,
     190,    31,   191,     0,     0,    42,    43,   144,   197,     0,
     164,   219,     0,     0,     0,   161,   162,     0,   154,     0,
     153,   126,   114,   113,   127,   129,   130,   120,   121,   122,
     123,   124,   125,   128,   115,   116,   117,   118,   119,     0,
       0,   103,     0,     0,     0,     0,     0,     0,     0,     0,
     105,   144,   168,     0,   203,   200,   201,    96,     0,    64,
     157,     0,     0,    49,     0,    71,    72,    69,    73,    75,
     193,   209,   217,    77,   214,   215,   216,    78,    79,    80,
       0,    81,     0,    84,     0,     0,     0,    52,    50,   190,
     192,     0,     0,     0,     0,   143,     0,   146,    18,   142,
     199,     0,     0,   159,     0,     0,   151,     0,   140,     0,
     137,   138,   131,   132,   133,   134,   135,   136,     0,   202,
     169,   170,    44,    54,    55,    68,   156,   156,     0,    58,
      62,    59,     0,     0,   177,   183,   156,     0,   178,     0,
     191,     0,     0,   109,   145,   144,   166,     0,   163,   165,
     155,   139,   107,     0,   172,    82,    85,     0,     0,     0,
     187,   186,   184,    34,   179,   180,     0,     0,     0,   147,
       0,   171,     0,   175,    87,    88,     0,    60,     0,     0,
       0,   156,   191,     0,   167,   173,     0,     0,     0,   156,
     181,   185,     0,    35,     0,     0,   176,   204,   156,    37,
       0,     0,   156,   174,    91,   182,   156,    36,    38
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -338,    -5,  -338,  -338,  -338,  -338,   -85,  -338,  -338,  -338,
    -338,  -338,  -338,  -338,  -338,  -338,   148,   223,  -338,  -338,
    -338,   109,    23,   -72,  -338,  -338,  -338,   225,  -338,  -338,
    -338,  -338,    22,  -338,  -338,  -338,   -68,  -338,  -338,  -338,
    -338,  -338,  -338,  -338,  -338,  -338,  -338,  -338,  -338,  -338,
      50,  -338,  -338,  -338,  -338,  -338,  -338,  -338,    36,   -54,
    -338,  -338,  -338,  -338,  -338,  -338,  -338,   -79,  -135,  -338,
    -338,  -338,     5,  -338,  -338,  -338,  -338,  -337,  -338,  -332,
    -338,   -78,  -260,  -338,  -338,  -338,  -138,  -338,   -15,   129,
      -4,  -338,   -12
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    10,    11,     2,     4,     3,     5,    25,     6,    26,
      27,    43,    44,    28,    29,    46,    47,    81,    30,    82,
      31,   145,   146,    97,   300,   165,   256,    83,   142,    32,
      84,   149,   150,   237,    33,    34,   155,    35,    36,    90,
      37,    92,    38,   346,    39,    94,   254,   135,    77,   140,
     265,    64,    65,   221,   177,    66,    67,    68,   266,   267,
     268,   269,    69,    70,   111,   189,   190,   144,    71,   110,
     184,   185,   186,   224,   324,   343,   357,   305,   361,   306,
     349,   307,   173,    72,   109,   291,    85,    73,    74,   243,
      75,   187,   147
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      12,    45,    48,   311,    86,   351,    41,    12,   230,   100,
     263,   148,   360,   238,   239,    87,   359,   163,   166,     1,
       7,   156,   172,   158,   179,   160,   167,   -13,  -104,   230,
     -14,  -104,   372,   375,   136,    99,    12,   137,   138,   376,
     330,  -214,    42,     9,  -214,  -214,  -214,  -214,  -214,  -214,
    -214,   139,   101,   264,   102,     8,   -15,   -16,   103,     9,
    -214,  -214,  -214,  -214,  -214,    63,    78,   -17,  -214,   233,
      88,    89,    91,    93,   105,   141,   248,   249,   353,   251,
     143,   253,   -13,   162,   -13,   -14,   257,   -14,   258,   174,
      48,   304,    95,   260,  -104,     9,  -214,  -104,  -214,   180,
     181,     8,    79,   182,    80,     9,   104,   152,   106,   107,
     108,   -15,   -16,   -15,   -16,   125,   126,   127,   128,   129,
     164,   225,   -17,   131,   -17,     9,   168,   169,   170,    40,
     220,   228,   183,    61,   151,   226,    86,    86,   125,   126,
     127,   128,   129,   231,   161,   244,   245,    87,    87,   246,
     171,   334,   232,   176,   234,     9,    79,    95,    80,   255,
    -218,   188,   191,   192,   193,   194,   195,   196,   197,   198,
     199,   200,   201,   202,   203,   204,   205,   206,   207,   208,
     210,   211,   296,   314,   297,   310,   261,   270,   223,   271,
     227,   273,  -215,   274,   275,  -215,  -215,  -215,  -215,  -215,
    -215,  -215,   276,   191,   127,   128,   129,   250,   212,   252,
     277,  -215,  -215,  -215,  -215,  -215,   292,   325,   326,  -215,
     290,   298,   313,   262,   322,   315,   323,   333,   328,   332,
     336,   260,   213,   272,   338,   214,   215,   216,   217,   218,
     219,   340,   342,   356,   301,   367,   -93,  -215,   370,  -215,
     175,   229,   312,    96,   293,    98,   295,   288,   308,   278,
     352,   339,   280,   281,   282,   283,   284,   285,   286,   287,
     -93,     0,   363,   -93,   -93,   -93,   -93,   -93,   -93,   318,
     369,     0,   294,   247,   151,     0,     0,     0,     0,   374,
     368,     0,     0,   377,     0,     0,     0,   378,     0,   331,
       0,     0,     0,   335,     0,   299,     0,     0,     0,     0,
       0,   302,     0,   309,     0,     0,   347,     0,     0,     0,
       0,   316,     0,     0,     0,   319,     0,   320,     0,   321,
       0,     0,  -216,   308,     0,  -216,  -216,  -216,  -216,  -216,
    -216,  -216,     0,     0,   308,   308,     0,     0,   327,     0,
       0,  -216,  -216,  -216,  -216,  -216,     0,     0,     0,  -216,
     308,   337,     0,     0,     0,     0,   308,   308,     0,   120,
     121,   122,   123,   341,   125,   126,   127,   128,   129,   348,
       0,     0,   131,     0,     0,     0,   350,  -216,   309,  -216,
     354,     0,   355,    49,    50,    51,   358,    53,    54,    55,
       9,     0,     0,   364,     0,     0,   366,   235,   236,   112,
       0,     0,   371,     0,     0,   373,     0,     0,     0,     0,
       0,    49,    50,    51,     0,    53,    54,    55,     9,     0,
       0,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   112,
       0,   131,     0,   303,     0,     0,     0,     0,     0,     0,
       0,   132,   133,   134,   304,     0,     0,     0,     9,     0,
       0,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,     0,
       0,   131,   153,   157,   154,     0,     0,     0,     0,   168,
     169,   259,   133,   134,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,     0,     0,   131,   153,   159,   154,     0,     0,
       0,     0,     0,     0,   132,   133,   134,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   112,     0,   131,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   132,   133,   134,
       0,     0,     0,     0,     0,     0,     0,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,     0,     0,   131,     0,     0,
       0,   112,     0,     0,     0,   168,   169,   259,   133,   134,
     344,   345,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,     0,   153,   131,   154,     0,     0,     0,     0,     0,
       0,     0,     0,   132,   133,   134,     0,     0,     0,     0,
       0,     0,     0,     0,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   112,     0,   131,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   132,   133,   134,     0,     0,     0,
       0,     0,     0,     0,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   112,     0,   131,     0,     0,   178,     0,     0,
       0,     0,     0,     0,   132,   133,   134,     0,     0,     0,
       0,     0,     0,     0,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   279,   112,   131,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   132,   133,   134,     0,     0,     0,
       0,     0,     0,     0,     0,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   112,     0,   131,     0,     0,     0,     0,
       0,     0,   289,     0,     0,   132,   133,   134,     0,     0,
       0,     0,     0,     0,     0,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   112,     0,   131,     0,     0,     0,     0,
       0,     0,   317,     0,     0,   132,   133,   134,   329,     0,
       0,     0,     0,     0,     0,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   112,     0,   131,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   132,   133,   134,   362,     0,
       0,     0,     0,     0,     0,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   112,     0,   131,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   132,   133,   134,     0,     0,
       0,     0,     0,     0,     0,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   112,     0,   131,   365,     0,     0,     0,
       0,     0,     0,     0,     0,   132,   133,   134,     0,     0,
       0,     0,     0,     0,     0,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   112,     0,   131,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   132,   133,   134,     0,     0,
       0,     0,     0,     0,     0,   113,     0,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   112,     0,     0,   131,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   132,   133,   134,     0,     0,
       0,     0,     0,     0,     0,     0,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,     0,     0,     0,   131,     0,     0,     0,     0,     0,
       0,    76,     0,     0,   132,   133,   134,    49,    50,    51,
      52,    53,    54,    55,     9,     0,    56,     0,   112,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    57,    58,
      59,     0,     0,     0,     0,     0,     0,     0,     0,    60,
       0,    61,   115,    62,     0,     0,     0,   120,   121,   122,
     123,     0,   125,   126,   127,   128,   129,     0,     0,     0,
     131,    49,    50,    51,    52,    53,    54,    55,     9,     0,
      56,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    57,    58,    59,    49,    50,    51,    52,    53,
      54,    55,     9,    60,    56,    61,     0,    62,     0,   168,
     169,   170,     0,     0,     0,     0,    57,    58,    59,     0,
       0,     0,     0,   209,     0,     0,     0,    60,     0,    61,
       0,    62,    49,    50,    51,    52,    53,    54,    55,     9,
       0,    56,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,    57,    58,    59,   222,     0,    20,    21,
      22,    23,    24,     0,    60,     0,    61,     0,    62,    49,
      50,    51,    52,    53,    54,    55,     9,     0,    56,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      57,    58,    59,     0,     0,     0,     0,     0,     0,     0,
       0,    60,  -141,    61,     0,    62,    49,    50,    51,    52,
      53,    54,    55,     9,     0,    56,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    57,    58,    59,
      49,    50,    51,   240,   241,    54,    55,   242,    60,    56,
      61,     0,    62,     0,     0,     0,     0,     0,     0,     0,
       0,    57,    58,    59,     0,     0,     0,     0,     0,     0,
       0,     0,    60,     0,    61,     0,    62
};

static const yytype_int16 yycheck[] =
{
       4,    13,    14,   263,    19,   337,    11,    11,   143,    12,
      12,    83,   349,   151,   152,    19,   348,    96,    97,    13,
       0,    89,   100,    91,   109,    93,    98,     0,    53,   164,
       0,    56,   364,   370,    53,    40,    40,    56,    60,   371,
      28,     0,    27,    31,     3,     4,     5,     6,     7,     8,
       9,    73,    55,    55,    55,    27,     0,     0,    32,    31,
      19,    20,    21,    22,    23,    15,    16,     0,    27,   148,
      20,    21,    22,    23,    56,    27,   155,   156,   338,   158,
      27,   160,    55,    95,    57,    55,   165,    57,   167,   101,
     102,    27,    14,   171,    53,    31,    55,    56,    57,    27,
      28,    27,    13,    31,    15,    31,    56,    55,    58,    59,
      60,    55,    55,    57,    57,    46,    47,    48,    49,    50,
      27,    27,    55,    54,    57,    31,    62,    63,    64,    55,
      27,    14,    60,    58,    84,   139,   151,   152,    46,    47,
      48,    49,    50,    55,    94,    27,    28,   151,   152,    31,
     100,    27,    32,   103,    55,    31,    13,    14,    15,    32,
      52,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   250,   268,   252,   263,    17,    57,   138,    52,
     140,    59,     0,    55,    52,     3,     4,     5,     6,     7,
       8,     9,    61,   153,    48,    49,    50,   157,    12,   159,
      55,    19,    20,    21,    22,    23,   228,   296,   297,    27,
      48,    20,    57,   173,    57,    55,     5,   306,    55,    55,
      17,   309,    36,   183,    12,    39,    40,    41,    42,    43,
      44,    52,     9,     6,   256,    61,    12,    55,    55,    57,
     102,   142,   264,    30,   231,    30,   234,   221,   262,   209,
     338,   315,   212,   213,   214,   215,   216,   217,   218,   219,
      36,    -1,   351,    39,    40,    41,    42,    43,    44,   274,
     359,    -1,   232,   154,   234,    -1,    -1,    -1,    -1,   368,
     358,    -1,    -1,   372,    -1,    -1,    -1,   376,    -1,   303,
      -1,    -1,    -1,   307,    -1,   255,    -1,    -1,    -1,    -1,
      -1,   261,    -1,   263,    -1,    -1,   328,    -1,    -1,    -1,
      -1,   271,    -1,    -1,    -1,   275,    -1,   277,    -1,   279,
      -1,    -1,     0,   337,    -1,     3,     4,     5,     6,     7,
       8,     9,    -1,    -1,   348,   349,    -1,    -1,   298,    -1,
      -1,    19,    20,    21,    22,    23,    -1,    -1,    -1,    27,
     364,   311,    -1,    -1,    -1,    -1,   370,   371,    -1,    41,
      42,    43,    44,   323,    46,    47,    48,    49,    50,   329,
      -1,    -1,    54,    -1,    -1,    -1,   336,    55,   338,    57,
     340,    -1,   342,    24,    25,    26,   346,    28,    29,    30,
      31,    -1,    -1,   353,    -1,    -1,   356,    10,    11,    12,
      -1,    -1,   362,    -1,    -1,   365,    -1,    -1,    -1,    -1,
      -1,    24,    25,    26,    -1,    28,    29,    30,    31,    -1,
      -1,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    12,
      -1,    54,    -1,    16,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    64,    65,    66,    27,    -1,    -1,    -1,    31,    -1,
      -1,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    -1,
      -1,    54,    12,    13,    14,    -1,    -1,    -1,    -1,    62,
      63,    64,    65,    66,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    -1,    -1,    54,    12,    13,    14,    -1,    -1,
      -1,    -1,    -1,    -1,    64,    65,    66,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    12,    -1,    54,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    64,    65,    66,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    -1,    -1,    54,    -1,    -1,
      -1,    12,    -1,    -1,    -1,    62,    63,    64,    65,    66,
      21,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    -1,    12,    54,    14,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    64,    65,    66,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    12,    -1,    54,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    64,    65,    66,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    12,    -1,    54,    -1,    -1,    57,    -1,    -1,
      -1,    -1,    -1,    -1,    64,    65,    66,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    12,    54,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    64,    65,    66,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    12,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    -1,    61,    -1,    -1,    64,    65,    66,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    12,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    -1,    61,    -1,    -1,    64,    65,    66,    27,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    12,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    64,    65,    66,    27,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    12,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    64,    65,    66,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    12,    -1,    54,    55,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    64,    65,    66,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    12,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    64,    65,    66,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    -1,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    12,    -1,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    64,    65,    66,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    -1,    -1,    -1,    54,    -1,    -1,    -1,    -1,    -1,
      -1,    18,    -1,    -1,    64,    65,    66,    24,    25,    26,
      27,    28,    29,    30,    31,    -1,    33,    -1,    12,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    46,
      47,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    56,
      -1,    58,    36,    60,    -1,    -1,    -1,    41,    42,    43,
      44,    -1,    46,    47,    48,    49,    50,    -1,    -1,    -1,
      54,    24,    25,    26,    27,    28,    29,    30,    31,    -1,
      33,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    46,    47,    24,    25,    26,    27,    28,
      29,    30,    31,    56,    33,    58,    -1,    60,    -1,    62,
      63,    64,    -1,    -1,    -1,    -1,    45,    46,    47,    -1,
      -1,    -1,    -1,    52,    -1,    -1,    -1,    56,    -1,    58,
      -1,    60,    24,    25,    26,    27,    28,    29,    30,    31,
      -1,    33,     3,     4,     5,     6,     7,     8,     9,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    -1,    19,    20,
      21,    22,    23,    -1,    56,    -1,    58,    -1,    60,    24,
      25,    26,    27,    28,    29,    30,    31,    -1,    33,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    46,    47,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    56,    57,    58,    -1,    60,    24,    25,    26,    27,
      28,    29,    30,    31,    -1,    33,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      24,    25,    26,    27,    28,    29,    30,    31,    56,    33,
      58,    -1,    60,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    45,    46,    47,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    56,    -1,    58,    -1,    60
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    13,    77,    79,    78,    80,    82,     0,    27,    31,
      75,    76,   164,     3,     4,     5,     6,     7,     8,     9,
      19,    20,    21,    22,    23,    81,    83,    84,    87,    88,
      92,    94,   103,   108,   109,   111,   112,   114,   116,   118,
      55,    75,    27,    85,    86,   166,    89,    90,   166,    24,
      25,    26,    27,    28,    29,    30,    33,    45,    46,    47,
      56,    58,    60,   124,   125,   126,   129,   130,   131,   136,
     137,   142,   157,   161,   162,   164,    18,   122,   124,    13,
      15,    91,    93,   101,   104,   160,   162,   164,   124,   124,
     113,   124,   115,   124,   119,    14,    91,    97,   101,    75,
      12,    55,    55,    32,   124,    56,   124,   124,   124,   158,
     143,   138,    12,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    54,    64,    65,    66,   121,    53,    56,    60,    73,
     123,    27,   102,    27,   141,    95,    96,   166,    97,   105,
     106,   124,    55,    12,    14,   110,   110,    13,   110,    13,
     110,   124,   166,   141,    27,    99,   141,    97,    62,    63,
      64,   124,   155,   156,   166,    90,   124,   128,    57,    80,
      27,    28,    31,    60,   144,   145,   146,   165,   124,   139,
     140,   124,   124,   124,   124,   124,   124,   124,   124,   124,
     124,   124,   124,   124,   124,   124,   124,   124,   124,    52,
     124,   124,    12,    36,    39,    40,    41,    42,    43,    44,
      27,   127,    48,   124,   147,    27,   164,   124,    14,    95,
     142,    55,    32,   141,    55,    10,    11,   107,   160,   160,
      27,    28,    31,   163,    27,    28,    31,   163,   141,   141,
     124,   141,   124,   141,   120,    32,   100,   141,   141,    64,
     155,    17,   124,    12,    55,   124,   132,   133,   134,   135,
      57,    52,   124,    59,    55,    52,    61,    55,   124,    52,
     124,   124,   124,   124,   124,   124,   124,   124,   132,    61,
      48,   159,   166,    96,   124,   106,   110,   110,    20,   124,
      98,   166,   124,    16,    27,   151,   153,   155,   164,   124,
     155,   156,   166,    57,    80,    55,   124,    61,   146,   124,
     124,   124,    57,     5,   148,   141,   141,   124,    55,    27,
      28,   164,    55,   141,    27,   164,    17,   124,    12,   133,
      52,   124,     9,   149,    21,    22,   117,   166,   124,   154,
     124,   153,   155,   156,   124,   124,     6,   150,   124,   153,
     151,   152,    27,   141,   124,    55,   124,    61,   110,   141,
      55,   124,   153,   124,   141,   151,   153,   141,   141
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    74,    75,    75,    76,    76,    76,    77,    78,    77,
      79,    80,    81,    81,    81,    81,    81,    81,    82,    82,
      83,    83,    83,    83,    83,    83,    83,    83,    83,    83,
      83,    84,    84,    84,    85,    85,    85,    86,    86,    87,
      88,    89,    89,    90,    91,    93,    92,    94,    94,    94,
      94,    94,    94,    95,    95,    96,    97,    97,    97,    98,
      98,   100,    99,   102,   101,   104,   103,   105,   105,   106,
     107,   107,   107,   107,   108,   108,   109,   110,   110,   111,
     112,   113,   113,   114,   115,   115,   116,   117,   117,   119,
     120,   118,   121,   121,   121,   123,   122,   122,   124,   124,
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
       1,     4,     2,     2,     6,     8,    10,     9,    11,     2,
       2,     1,     3,     3,     4,     0,     3,     3,     3,     4,
       4,     3,     4,     1,     3,     3,     0,     2,     4,     1,
       3,     0,     3,     0,     3,     0,     3,     1,     3,     2,
       0,     1,     1,     1,     2,     4,     2,     2,     2,     4,
       4,     3,     5,     2,     3,     5,     2,     1,     1,     0,
       0,    10,     1,     1,     1,     0,     3,     1,     1,     1,
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
#line 369 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 2102 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 3:
#line 372 "Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 2115 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 4:
#line 383 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 2124 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 387 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 2133 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 391 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 2142 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 398 "Aql/grammar.y" /* yacc.c:1646  */
    {
     }
#line 2149 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 8:
#line 400 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
     }
#line 2158 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 403 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      auto withNode = parser->ast()->createNodeWithCollections(node);
      parser->ast()->addOperation(withNode);
     }
#line 2168 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 411 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2175 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 416 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2182 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 421 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2189 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 13:
#line 423 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2197 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 14:
#line 426 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2205 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 429 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2213 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 432 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2221 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 435 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2229 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 441 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2236 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 443 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2243 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 448 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2250 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 450 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2257 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 452 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2264 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 454 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2271 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 456 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2278 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 458 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2285 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 460 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2292 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 462 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2299 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 464 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2306 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 466 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2313 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 468 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2320 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 473 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
     
      auto node = parser->ast()->createNodeFor((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2331 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 479 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2338 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 481 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2345 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 486 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-5].strval).value, (yyvsp[-5].strval).length, (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2355 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 491 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-7].strval).value, (yyvsp[-7].strval).length, (yyvsp[-5].strval).value, (yyvsp[-5].strval).length, (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2365 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 496 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-9].strval).value, (yyvsp[-9].strval).length, (yyvsp[-7].strval).value, (yyvsp[-7].strval).length, (yyvsp[-5].strval).value, (yyvsp[-5].strval).length, (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2375 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 504 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (!TRI_CaseEqualString((yyvsp[-3].strval).value, "TO")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'TO'", (yyvsp[-3].strval).value, yylloc.first_line, yylloc.first_column);
      }
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeShortestPath((yyvsp[-8].strval).value, (yyvsp[-8].strval).length, (yyvsp[-6].intval), (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2388 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 512 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (!TRI_CaseEqualString((yyvsp[-3].strval).value, "TO")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'TO'", (yyvsp[-3].strval).value, yylloc.first_line, yylloc.first_column);
      }
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeShortestPath((yyvsp[-10].strval).value, (yyvsp[-10].strval).length, (yyvsp[-8].strval).value, (yyvsp[-8].strval).length, (yyvsp[-6].intval), (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2401 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 523 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // operand is a reference. can use it directly
      auto node = parser->ast()->createNodeFilter((yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2411 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 531 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2418 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 536 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2425 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 538 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2432 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 543 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLet((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2441 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 550 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[-2].strval).value, "COUNT")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'COUNT'", (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2453 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 560 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2462 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 46:
#line 563 "Aql/grammar.y" /* yacc.c:1646  */
    { 
      auto list = static_cast<AstNode*>(parser->popStack());

      if (list == nullptr) {
        ABORT_OOM
      }
      (yyval.node) = list;
    }
#line 2475 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 574 "Aql/grammar.y" /* yacc.c:1646  */
    {
      /* COLLECT WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      StartCollectScope(scopes);

      auto node = parser->ast()->createNodeCollectCount(parser->ast()->createNodeArray(), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2489 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 48:
#line 583 "Aql/grammar.y" /* yacc.c:1646  */
    {
      /* COLLECT var = expr WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-2].node));
      }

      auto node = parser->ast()->createNodeCollectCount((yyvsp[-2].node), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2505 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 594 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2540 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 624 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2612 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 691 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2642 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 52:
#line 716 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2677 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 749 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2684 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 751 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2691 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 756 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeAssign((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
      parser->pushArrayElement(node);
    }
#line 2700 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 763 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 2708 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 766 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 2716 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 769 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember(parser->ast()->createNodeValueString((yyvsp[-2].strval).value, (yyvsp[-2].strval).length));
      node->addMember((yyvsp[0].node));
      (yyval.node) = node;
    }
#line 2727 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 778 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2746 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 792 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2765 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 809 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[0].strval).value, "KEEP")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'KEEP'", (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2778 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 816 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2787 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 823 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2796 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 826 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2805 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 833 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2814 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 836 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      auto node = parser->ast()->createNodeSort(list);
      parser->ast()->addOperation(node);
    }
#line 2824 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 844 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2832 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 847 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2840 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 853 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeSortElement((yyvsp[-1].node), (yyvsp[0].node));
    }
#line 2848 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 859 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2856 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 862 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2864 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 865 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 2872 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 868 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2880 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 74:
#line 874 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto offset = parser->ast()->createNodeValueInt(0);
      auto node = parser->ast()->createNodeLimit(offset, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2890 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 75:
#line 879 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLimit((yyvsp[-2].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2899 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 76:
#line 886 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeReturn((yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2909 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 77:
#line 894 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2917 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 897 "Aql/grammar.y" /* yacc.c:1646  */
    {
       (yyval.node) = (yyvsp[0].node);
     }
#line 2925 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 903 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2937 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 913 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2949 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 923 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2962 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 82:
#line 931 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2975 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 83:
#line 942 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2982 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 947 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2995 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 85:
#line 955 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 3008 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 86:
#line 966 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3015 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 87:
#line 971 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_UPDATE);
    }
#line 3023 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 88:
#line 974 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_REPLACE);
    }
#line 3031 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 89:
#line 980 "Aql/grammar.y" /* yacc.c:1646  */
    { 
      // reserve a variable named "$OLD", we might need it in the update expression
      // and in a later return thing
      parser->pushStack(parser->ast()->createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD), true));
    }
#line 3041 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 90:
#line 984 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* variableNode = static_cast<AstNode*>(parser->popStack());
      
      auto scopes = parser->ast()->scopes();
      
      scopes->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
      
      scopes->start(arangodb::aql::AQL_SCOPE_FOR);
      std::string const variableName = parser->ast()->variables()->nextName();
      auto forNode = parser->ast()->createNodeFor(variableName.c_str(), variableName.size(), parser->ast()->createNodeArray(), false);
      parser->ast()->addOperation(forNode);

      auto filterNode = parser->ast()->createNodeUpsertFilter(parser->ast()->createNodeReference(variableName), (yyvsp[0].node));
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
      
      parser->pushStack(forNode);
    }
#line 3085 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 91:
#line 1022 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* forNode = static_cast<AstNode*>(parser->popStack());
      forNode->changeMember(1, (yyvsp[-1].node)); 

      if (!parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      auto node = parser->ast()->createNodeUpsert(static_cast<AstNodeType>((yyvsp[-3].intval)), parser->ast()->createNodeReference(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD)), (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 3101 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 92:
#line 1036 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeQuantifier(Quantifier::ALL);
    }
#line 3109 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 93:
#line 1039 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeQuantifier(Quantifier::ANY);
    }
#line 3117 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 94:
#line 1042 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeQuantifier(Quantifier::NONE);
    }
#line 3125 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 95:
#line 1048 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto const scopeType = parser->ast()->scopes()->type();

      if (scopeType == AQL_SCOPE_MAIN ||
          scopeType == AQL_SCOPE_SUBQUERY) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "cannot use DISTINCT modifier on top-level query element", yylloc.first_line, yylloc.first_column);
      }
    }
#line 3138 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 96:
#line 1055 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDistinct((yyvsp[0].node));
    }
#line 3146 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 97:
#line 1058 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3154 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 98:
#line 1064 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3162 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 99:
#line 1067 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3170 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 100:
#line 1070 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3178 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 101:
#line 1073 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3186 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 102:
#line 1076 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3194 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 103:
#line 1079 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeRange((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3202 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 104:
#line 1085 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3210 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 105:
#line 1088 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 3228 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 106:
#line 1104 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushStack((yyvsp[-1].strval).value);

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3239 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 107:
#line 1109 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall(static_cast<char const*>(parser->popStack()), list);
    }
#line 3248 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 108:
#line 1113 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3257 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 109:
#line 1116 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("LIKE"), list);
    }
#line 3266 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 110:
#line 1123 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_PLUS, (yyvsp[0].node));
    }
#line 3274 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 111:
#line 1126 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_MINUS, (yyvsp[0].node));
    }
#line 3282 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 112:
#line 1129 "Aql/grammar.y" /* yacc.c:1646  */
    { 
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, (yyvsp[0].node));
    }
#line 3290 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 113:
#line 1135 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3298 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 114:
#line 1138 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3306 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 115:
#line 1141 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_PLUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3314 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 116:
#line 1144 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MINUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3322 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 117:
#line 1147 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_TIMES, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3330 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 118:
#line 1150 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_DIV, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3338 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 119:
#line 1153 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MOD, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3346 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 120:
#line 1156 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3354 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 121:
#line 1159 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3362 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 122:
#line 1162 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3370 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 123:
#line 1165 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3378 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 124:
#line 1168 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3386 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 125:
#line 1171 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3394 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 126:
#line 1174 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3402 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 127:
#line 1177 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3410 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 128:
#line 1180 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* arguments = parser->ast()->createNodeArray(2);
      arguments->addMember((yyvsp[-2].node));
      arguments->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("LIKE"), arguments);
    }
#line 3421 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 129:
#line 1186 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* arguments = parser->ast()->createNodeArray(2);
      arguments->addMember((yyvsp[-2].node));
      arguments->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("REGEX_TEST"), arguments);
    }
#line 3432 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 130:
#line 1192 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* arguments = parser->ast()->createNodeArray(2);
      arguments->addMember((yyvsp[-2].node));
      arguments->addMember((yyvsp[0].node));
      AstNode* node = parser->ast()->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("REGEX_TEST"), arguments);
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, node);
    }
#line 3444 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 131:
#line 1199 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3452 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 132:
#line 1202 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_NE, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3460 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 133:
#line 1205 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_LT, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3468 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 134:
#line 1208 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_GT, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3476 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 135:
#line 1211 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_LE, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3484 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 136:
#line 1214 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_GE, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3492 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 137:
#line 1217 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_IN, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3500 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 138:
#line 1220 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3508 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 139:
#line 1226 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3516 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 140:
#line 1229 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-3].node), (yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3524 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 141:
#line 1235 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3531 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 142:
#line 1237 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3538 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 143:
#line 1242 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3546 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 144:
#line 1245 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 3555 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 145:
#line 1248 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 3570 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 146:
#line 1261 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3578 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 147:
#line 1264 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3586 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 148:
#line 1270 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3594 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 149:
#line 1273 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3602 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 150:
#line 1279 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3611 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 151:
#line 1282 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3619 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 152:
#line 1288 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3626 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 153:
#line 1290 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3633 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 154:
#line 1295 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3641 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 155:
#line 1298 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3649 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 156:
#line 1304 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3657 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 157:
#line 1307 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[-1].strval).value, "OPTIONS")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'OPTIONS'", (yyvsp[-1].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3673 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 158:
#line 1321 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeObject();
      parser->pushStack(node);
    }
#line 3682 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 159:
#line 1324 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3690 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 160:
#line 1330 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3697 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 161:
#line 1332 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3704 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 162:
#line 1337 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3711 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 163:
#line 1339 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3718 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 164:
#line 1344 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 3737 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 165:
#line 1358 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // attribute-name : attribute-value
      parser->pushObjectElement((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
    }
#line 3746 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 166:
#line 1362 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // bind-parameter : attribute-value
      if ((yyvsp[-2].strval).length < 1 || (yyvsp[-2].strval).value[0] == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto param = parser->ast()->createNodeParameter((yyvsp[-2].strval).value, (yyvsp[-2].strval).length);
      parser->pushObjectElement(param, (yyvsp[0].node));
    }
#line 3760 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 167:
#line 1371 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // [ attribute-name-expression ] : attribute-value
      parser->pushObjectElement((yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3769 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 168:
#line 1378 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3777 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 169:
#line 1381 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = (yyvsp[-1].intval) + 1;
    }
#line 3785 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 170:
#line 1387 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3793 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 171:
#line 1390 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3801 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 172:
#line 1396 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3809 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 173:
#line 1399 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit(nullptr, (yyvsp[0].node));
    }
#line 3817 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 174:
#line 1402 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3825 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 175:
#line 1408 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3833 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 176:
#line 1411 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3841 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 177:
#line 1417 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3849 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 178:
#line 1420 "Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3862 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 179:
#line 1428 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto tmp = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), tmp);
    }
#line 3871 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 180:
#line 1432 "Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), (yyvsp[0].node));
    }
#line 3884 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 181:
#line 1443 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3893 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 182:
#line 1447 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3902 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 183:
#line 1454 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3912 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 184:
#line 1459 "Aql/grammar.y" /* yacc.c:1646  */
    { 
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
      node->addMember((yyvsp[-1].node));
    }
#line 3922 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 185:
#line 1463 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3931 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 186:
#line 1467 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3945 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 187:
#line 1476 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3954 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 188:
#line 1485 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 2;
    }
#line 3962 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 189:
#line 1488 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3970 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 190:
#line 1491 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 0; 
    }
#line 3978 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 191:
#line 1497 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), 1);
    }
#line 3986 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 192:
#line 1500 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), (yyvsp[-1].node));
    }
#line 3994 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 193:
#line 1506 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4031 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 194:
#line 1538 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4039 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 195:
#line 1541 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4047 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 196:
#line 1544 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
      
      if ((yyval.node) == nullptr) {
        ABORT_OOM
      }
    }
#line 4059 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 197:
#line 1551 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4074 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 198:
#line 1561 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 4083 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 199:
#line 1564 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 4098 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 200:
#line 1574 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4118 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 201:
#line 1589 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4137 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 202:
#line 1603 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4156 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 203:
#line 1617 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4184 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 204:
#line 1639 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4207 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 205:
#line 1660 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4215 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 206:
#line 1663 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4223 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 207:
#line 1669 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }
      
      (yyval.node) = (yyvsp[0].node);
    }
#line 4235 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 208:
#line 1676 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 4247 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 209:
#line 1686 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 4255 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 210:
#line 1689 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4263 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 211:
#line 1692 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueNull();
    }
#line 4271 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 212:
#line 1695 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 4279 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 213:
#line 1698 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 4287 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 214:
#line 1704 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, arangodb::AccessMode::Type::WRITE);
    }
#line 4295 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 215:
#line 1707 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, arangodb::AccessMode::Type::WRITE);
    }
#line 4303 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 216:
#line 1710 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval).length < 2 || (yyvsp[0].strval).value[0] != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 4315 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 217:
#line 1720 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 4323 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 218:
#line 1726 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 4331 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 219:
#line 1729 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 4339 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 220:
#line 1734 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 4347 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;


#line 4351 "Aql/grammar.cpp" /* yacc.c:1646  */
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
