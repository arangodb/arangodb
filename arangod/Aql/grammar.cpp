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

#include "Aql/AstNode.h"
#include "Aql/CollectNode.h"
#include "Aql/Function.h"
#include "Aql/Parser.h"
#include "Aql/Quantifier.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"

#line 82 "Aql/grammar.cpp" /* yacc.c:339  */

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
    T_NIN = 290,
    T_REGEX_MATCH = 291,
    T_REGEX_NON_MATCH = 292,
    T_EQ = 293,
    T_NE = 294,
    T_LT = 295,
    T_GT = 296,
    T_LE = 297,
    T_GE = 298,
    T_LIKE = 299,
    T_PLUS = 300,
    T_MINUS = 301,
    T_TIMES = 302,
    T_DIV = 303,
    T_MOD = 304,
    T_QUESTION = 305,
    T_COLON = 306,
    T_SCOPE = 307,
    T_RANGE = 308,
    T_COMMA = 309,
    T_OPEN = 310,
    T_CLOSE = 311,
    T_OBJECT_OPEN = 312,
    T_OBJECT_CLOSE = 313,
    T_ARRAY_OPEN = 314,
    T_ARRAY_CLOSE = 315,
    T_OUTBOUND = 316,
    T_INBOUND = 317,
    T_ANY = 318,
    T_ALL = 319,
    T_NONE = 320,
    UMINUS = 321,
    UPLUS = 322,
    FUNCCALL = 323,
    REFERENCE = 324,
    INDEXED = 325,
    EXPANSION = 326
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 19 "Aql/grammar.y" /* yacc.c:355  */

  arangodb::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 205 "Aql/grammar.cpp" /* yacc.c:355  */
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
#line 29 "Aql/grammar.y" /* yacc.c:358  */


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


#line 405 "Aql/grammar.cpp" /* yacc.c:358  */

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
#define YYLAST   1247

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  73
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  90
/* YYNRULES -- Number of rules.  */
#define YYNRULES  214
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  357

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   326

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
       2,     2,     2,     2,     2,     2,    72,     2,     2,     2,
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
      65,    66,    67,    68,    69,    70,    71
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   367,   367,   370,   381,   385,   389,   396,   398,   398,
     409,   414,   419,   421,   424,   427,   430,   433,   439,   441,
     446,   448,   450,   452,   454,   456,   458,   460,   462,   464,
     466,   471,   477,   482,   487,   495,   503,   508,   510,   515,
     522,   532,   532,   546,   555,   566,   596,   663,   688,   721,
     723,   728,   735,   738,   741,   750,   764,   781,   781,   795,
     795,   805,   805,   816,   819,   825,   831,   834,   837,   840,
     846,   851,   858,   866,   869,   875,   885,   895,   903,   914,
     919,   927,   938,   943,   946,   952,   952,  1003,  1006,  1009,
    1015,  1015,  1025,  1031,  1034,  1037,  1040,  1043,  1046,  1052,
    1055,  1071,  1071,  1080,  1080,  1090,  1093,  1096,  1102,  1105,
    1108,  1111,  1114,  1117,  1120,  1123,  1126,  1129,  1132,  1135,
    1138,  1141,  1144,  1147,  1153,  1159,  1166,  1169,  1172,  1175,
    1178,  1181,  1184,  1187,  1193,  1199,  1201,  1206,  1209,  1209,
    1225,  1228,  1234,  1237,  1243,  1243,  1252,  1254,  1259,  1262,
    1268,  1271,  1285,  1285,  1294,  1296,  1301,  1303,  1308,  1322,
    1326,  1335,  1342,  1345,  1351,  1354,  1360,  1363,  1366,  1372,
    1375,  1381,  1384,  1392,  1396,  1407,  1411,  1418,  1423,  1423,
    1431,  1440,  1449,  1452,  1455,  1461,  1464,  1470,  1502,  1505,
    1508,  1515,  1525,  1525,  1538,  1553,  1567,  1581,  1581,  1624,
    1627,  1633,  1640,  1650,  1653,  1656,  1659,  1662,  1668,  1671,
    1674,  1684,  1690,  1693,  1698
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
  "statement_block_statement", "for_statement", "filter_statement",
  "let_statement", "let_list", "let_element", "count_into",
  "collect_variable_list", "$@2", "collect_statement", "collect_list",
  "collect_element", "collect_optional_into", "variable_list", "keep",
  "$@3", "aggregate", "$@4", "sort_statement", "$@5", "sort_list",
  "sort_element", "sort_direction", "limit_statement", "return_statement",
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
     325,   326,    46
};
# endif

#define YYPACT_NINF -305

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-305)))

#define YYTABLE_NINF -213

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      38,  -305,  -305,    66,    14,  -305,   215,  -305,  -305,  -305,
    -305,    17,  -305,    43,    43,  1164,  1042,   213,  -305,   174,
    1164,  1164,  1164,  1164,  -305,  -305,  -305,  -305,  -305,  -305,
     137,  -305,  -305,  -305,  -305,     3,    22,    30,    33,    39,
      14,  -305,  -305,    -1,    34,  -305,    69,  -305,  -305,  -305,
     -20,  -305,  -305,  -305,  1164,    75,  1164,  1164,  1164,  -305,
    -305,   922,    42,  -305,  -305,  -305,  -305,  -305,  -305,  -305,
     -22,  -305,  -305,  -305,  -305,  -305,   922,   109,  -305,   130,
      43,   143,  1164,   127,  -305,  -305,   648,   648,  -305,   527,
    -305,   569,  1164,    43,   130,   156,   143,  -305,  1066,    43,
      43,  1164,  -305,  -305,  -305,  -305,   687,  -305,    15,  1164,
    1164,  1164,  1164,  1164,  1164,  1164,  1164,  1164,  1164,  1164,
    1164,  1164,  1164,  1164,  1164,  1164,  1164,  1164,  1164,  1164,
    -305,  -305,  -305,    63,   159,  -305,  1090,    35,  1164,   149,
      43,   134,  -305,   162,  -305,   194,   130,   184,  -305,   445,
     174,  1188,   102,   130,   130,  1164,   130,  1164,   130,   726,
     208,  -305,   134,   130,  -305,   130,  -305,  -305,  -305,   608,
    -305,  1164,     4,  -305,   922,  1127,  -305,   185,   189,  -305,
     192,  1164,   186,   193,  -305,   195,   922,   188,   197,   147,
     999,   961,   147,   119,   119,   119,   119,   263,   263,   263,
     263,   119,   183,   183,  -305,  -305,  -305,   765,    92,  1164,
    1164,  1164,  1164,  1164,  1164,  1164,  1164,  -305,  1127,  -305,
     805,   209,  -305,  -305,   922,    43,   162,  -305,    43,  1164,
    -305,  1164,  -305,  -305,  -305,  -305,  -305,   348,   372,   411,
    -305,  -305,  -305,  -305,  -305,  -305,  -305,   648,  -305,   648,
    -305,  1164,  1164,    43,  -305,  -305,   135,  -305,   485,  1066,
      43,   922,   199,  -305,  -305,   204,  -305,  1164,   844,  -305,
      15,  1164,  -305,  1164,  1164,   147,   147,   119,   119,   263,
     263,   263,   263,   207,  -305,  -305,   255,  -305,  -305,   922,
    -305,   130,   130,   241,   922,   210,  -305,   153,  -305,   212,
    -305,   118,  -305,   608,  1164,   256,  -305,  -305,  1164,   922,
     216,  -305,   922,   922,   922,  -305,  1164,   261,  -305,  -305,
    -305,  -305,  1164,    43,  -305,  -305,  -305,  -305,  -305,   485,
    1066,  -305,  1164,   922,  1164,   286,   648,  -305,     1,  -305,
    1164,   922,   883,  1164,   235,   130,  -305,   242,   485,  1164,
     922,  -305,  -305,     1,  -305,   922,  -305
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       7,     8,    18,     0,     0,    10,     0,     1,     2,   211,
       4,     9,     3,     0,     0,     0,     0,    41,    61,     0,
       0,     0,     0,     0,    85,    11,    19,    20,    22,    21,
      52,    23,    24,    25,    12,    26,    27,    28,    29,    30,
       0,     6,   214,     0,    36,    37,     0,   205,   206,   207,
     187,   203,   201,   202,     0,     0,     0,     0,   192,   152,
     144,    35,     0,   190,    93,    94,    95,   188,   142,   143,
      97,   204,    96,   189,    90,    72,    92,     0,    59,   150,
       0,    52,     0,    70,   199,   200,     0,     0,    79,     0,
      82,     0,     0,     0,   150,   150,    52,     5,     0,     0,
       0,     0,   107,   103,   105,   106,     0,    18,   154,   146,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      88,    87,    89,     0,     0,   101,     0,     0,     0,     0,
       0,     0,    43,    42,    49,     0,   150,    62,    63,    66,
       0,     0,     0,   150,   150,     0,   150,     0,   150,     0,
      53,    44,    57,   150,    47,   150,   182,   183,   184,    31,
     185,     0,     0,    38,    39,   138,   191,     0,   158,   213,
       0,     0,     0,   155,   156,     0,   148,     0,   147,   121,
     109,   108,   122,   124,   125,   115,   116,   117,   118,   119,
     120,   123,   110,   111,   112,   113,   114,     0,    98,     0,
       0,     0,     0,     0,     0,     0,     0,   100,   138,   162,
       0,   197,   194,   195,    91,     0,    60,   151,     0,     0,
      45,     0,    67,    68,    65,    69,    71,   187,   203,   211,
      73,   208,   209,   210,    74,    75,    76,     0,    77,     0,
      80,     0,     0,     0,    48,    46,   184,   186,     0,     0,
       0,   137,     0,   140,    18,   136,   193,     0,     0,   153,
       0,     0,   145,     0,     0,   132,   133,   126,   127,   128,
     129,   130,   131,     0,   196,   163,   164,    40,    50,    51,
      64,   150,   150,     0,    54,    58,    55,     0,   171,   177,
      32,     0,   172,     0,     0,     0,   104,   139,   138,   160,
       0,   157,   159,   149,   134,   102,     0,   166,    78,    81,
      83,    84,     0,     0,   181,   180,   178,   173,   174,     0,
       0,   141,     0,   165,     0,   169,     0,    56,     0,    33,
       0,   161,   167,     0,     0,   150,   175,   179,     0,     0,
     170,   198,    86,     0,    34,   168,   176
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -305,     6,  -305,  -305,  -305,  -305,  -106,  -305,  -305,  -305,
    -305,  -305,  -305,  -305,   198,   269,  -305,  -305,  -305,   160,
      73,   -58,  -305,  -305,  -305,   273,  -305,  -305,  -305,  -305,
      76,  -305,  -305,  -305,   -63,  -305,  -305,  -305,  -305,  -305,
    -305,  -305,  -305,  -305,  -305,  -305,  -305,    -2,  -305,  -305,
    -305,  -305,  -305,  -305,  -305,    95,     9,  -305,  -305,  -305,
    -305,  -305,  -305,  -305,   -86,  -137,  -305,  -305,  -305,    48,
    -305,  -305,  -305,  -305,  -304,  -305,  -300,  -305,   -88,  -257,
    -305,  -305,  -305,   -67,  -305,    -7,   167,    -4,  -305,    -8
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    10,    11,     2,     4,     3,     5,    25,     6,    26,
      27,    28,    29,    44,    45,    79,    30,    80,    31,   143,
     144,    95,   295,   163,   253,    81,   140,    32,    82,   147,
     148,   234,    33,    34,   153,    35,    36,    88,    37,    90,
      38,   322,    39,    92,   133,    75,   138,   261,    62,    63,
     218,   175,    64,    65,    66,   262,   263,   264,   265,    67,
      68,   109,   187,   188,   142,    69,   108,   182,   183,   184,
     221,   317,   335,   344,   299,   347,   300,   338,   301,   171,
      70,   107,   286,    83,    71,    72,   240,    73,   185,   145
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      12,   177,   304,   -13,   227,    43,    46,    12,   161,   164,
     170,    98,    84,    61,    76,    85,   259,    41,    86,    87,
      89,    91,   -14,   146,   154,   227,   156,   298,   158,   339,
     -15,     9,   -99,   -16,   346,   -99,    12,   136,   165,   -17,
       8,   178,   179,     8,     9,   180,    97,     9,   354,   356,
     137,     1,   102,    99,   104,   105,   106,   -13,   260,   -13,
     230,   222,   166,   167,   168,     9,     7,   245,   246,    42,
     248,    40,   250,   340,   181,   209,   -14,   254,   -14,   255,
     149,   257,   235,   236,   -15,   160,   -15,   -16,   100,   -16,
     159,   172,    46,   -17,   134,   -17,   169,   135,   210,   174,
     101,   211,   212,   213,   214,   215,   216,   186,   189,   190,
     191,   192,   193,   194,   195,   196,   197,   198,   199,   200,
     201,   202,   203,   204,   205,   206,   207,   208,   241,   242,
     103,   110,   243,   223,   220,   139,   224,   123,   124,   125,
     126,   127,    84,    84,   327,    85,    85,   -88,     9,   189,
      77,    93,    78,   247,   113,   249,   141,    93,   307,   118,
     119,   120,   121,   225,   123,   124,   125,   126,   127,   258,
     -88,   170,   129,   -88,   -88,   -88,   -88,   -88,   -88,   268,
     324,   150,   162,     9,   291,   217,   292,   118,   119,   120,
     121,    59,   123,   124,   125,   126,   127,    47,    48,    49,
     129,    51,    52,    53,     9,   318,   319,   275,   276,   277,
     278,   279,   280,   281,   282,   257,   228,   287,    13,    14,
      15,    16,    17,    18,    19,   229,    77,   289,    78,   149,
     125,   126,   127,    20,    21,    22,    23,    24,   231,   252,
    -212,   266,   170,   267,   269,   296,   271,   270,   272,   293,
     294,   273,   305,   110,   302,   306,   285,   303,   308,   352,
     316,   320,   321,   315,   323,   309,   326,   332,   330,   312,
     334,   313,   314,   345,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   343,   325,   129,   351,   353,   328,   173,    94,
     226,   288,   329,    96,   130,   131,   132,   290,   123,   124,
     125,   126,   127,   283,   333,   337,   129,   331,   311,   244,
     336,     0,     0,     0,     0,   302,     0,     0,   303,     0,
     341,     0,   342,     0,   302,     0,     0,     0,   348,     0,
       0,   350,     0,     0,   302,     0,     0,   355,  -208,   302,
       0,  -208,  -208,  -208,  -208,  -208,  -208,  -208,     0,     0,
       0,     0,     0,     0,     0,     0,  -208,  -208,  -208,  -208,
    -208,     0,  -209,     0,  -208,  -209,  -209,  -209,  -209,  -209,
    -209,  -209,     0,     0,     0,     0,     0,     0,     0,     0,
    -209,  -209,  -209,  -209,  -209,     0,     0,     0,  -209,     0,
     -99,     0,  -208,   -99,  -208,     0,     0,     0,     0,     0,
       0,  -210,     0,     0,  -210,  -210,  -210,  -210,  -210,  -210,
    -210,     0,     0,     0,     0,     0,  -209,     0,  -209,  -210,
    -210,  -210,  -210,  -210,     0,     0,     0,  -210,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   232,   233,   110,     0,     0,
       0,     0,     0,     0,     0,  -210,     0,  -210,    47,    48,
      49,     0,    51,    52,    53,     9,     0,     0,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,     0,   110,   129,     0,
       0,   297,     0,     0,     0,     0,     0,     0,   130,   131,
     132,   298,     0,     0,     0,     9,     0,     0,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,     0,     0,   129,   151,
     155,   152,     0,     0,     0,     0,   166,   167,   256,   131,
     132,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,     0,     0,
     129,   151,   157,   152,     0,     0,     0,     0,     0,     0,
     130,   131,   132,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     110,     0,   129,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   130,   131,   132,     0,     0,     0,     0,     0,
       0,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,     0,
     151,   129,   152,     0,     0,     0,     0,     0,     0,   166,
     167,   256,   131,   132,     0,     0,     0,     0,     0,     0,
       0,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   110,
       0,   129,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   130,   131,   132,     0,     0,     0,     0,     0,     0,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   110,     0,
     129,     0,     0,   176,     0,   251,     0,     0,     0,     0,
     130,   131,   132,     0,     0,     0,     0,     0,     0,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   110,     0,   129,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   130,
     131,   132,     0,     0,     0,     0,     0,     0,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   274,   110,   129,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   130,   131,
     132,     0,     0,     0,     0,     0,     0,     0,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   110,     0,   129,     0,
       0,     0,     0,     0,     0,   284,     0,     0,   130,   131,
     132,     0,     0,     0,     0,     0,     0,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   110,     0,   129,     0,     0,
       0,     0,     0,     0,   310,     0,     0,   130,   131,   132,
       0,     0,     0,     0,     0,     0,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   110,     0,   129,   349,     0,     0,
       0,     0,     0,     0,     0,     0,   130,   131,   132,     0,
       0,     0,     0,     0,     0,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   110,     0,   129,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   130,   131,   132,     0,     0,
       0,     0,     0,     0,   111,     0,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   110,     0,     0,   129,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   130,   131,   132,     0,     0,     0,
       0,     0,     0,     0,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,     0,
       0,     0,   129,     0,     0,     0,     0,     0,     0,    74,
       0,     0,   130,   131,   132,    47,    48,    49,    50,    51,
      52,    53,     9,     0,    54,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    55,    56,    57,    47,
      48,    49,    50,    51,    52,    53,     9,    58,    54,    59,
       0,    60,     0,     0,     0,     0,     0,     0,     0,     0,
      55,    56,    57,    47,    48,    49,    50,    51,    52,    53,
       9,    58,    54,    59,     0,    60,     0,   166,   167,   168,
       0,     0,     0,     0,    55,    56,    57,   219,     0,     0,
       0,     0,     0,     0,     0,    58,     0,    59,     0,    60,
      47,    48,    49,    50,    51,    52,    53,     9,     0,    54,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    55,    56,    57,     0,     0,     0,     0,     0,     0,
       0,     0,    58,  -135,    59,     0,    60,    47,    48,    49,
      50,    51,    52,    53,     9,     0,    54,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    55,    56,
      57,    47,    48,    49,   237,   238,    52,    53,   239,    58,
      54,    59,     0,    60,     0,     0,     0,     0,     0,     0,
       0,     0,    55,    56,    57,     0,     0,     0,     0,     0,
       0,     0,     0,    58,     0,    59,     0,    60
};

static const yytype_int16 yycheck[] =
{
       4,   107,   259,     0,   141,    13,    14,    11,    94,    95,
      98,    12,    19,    15,    16,    19,    12,    11,    20,    21,
      22,    23,     0,    81,    87,   162,    89,    26,    91,   329,
       0,    30,    52,     0,   338,    55,    40,    59,    96,     0,
      26,    26,    27,    26,    30,    30,    40,    30,   348,   353,
      72,    13,    54,    54,    56,    57,    58,    54,    54,    56,
     146,    26,    61,    62,    63,    30,     0,   153,   154,    26,
     156,    54,   158,   330,    59,    12,    54,   163,    56,   165,
      82,   169,   149,   150,    54,    93,    56,    54,    54,    56,
      92,    99,   100,    54,    52,    56,    98,    55,    35,   101,
      31,    38,    39,    40,    41,    42,    43,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,    26,    27,
      55,    12,    30,   137,   136,    26,   138,    45,    46,    47,
      48,    49,   149,   150,    26,   149,   150,    12,    30,   151,
      13,    14,    15,   155,    35,   157,    26,    14,   264,    40,
      41,    42,    43,    14,    45,    46,    47,    48,    49,   171,
      35,   259,    53,    38,    39,    40,    41,    42,    43,   181,
      27,    54,    26,    30,   247,    26,   249,    40,    41,    42,
      43,    57,    45,    46,    47,    48,    49,    23,    24,    25,
      53,    27,    28,    29,    30,   291,   292,   209,   210,   211,
     212,   213,   214,   215,   216,   303,    54,   225,     3,     4,
       5,     6,     7,     8,     9,    31,    13,   229,    15,   231,
      47,    48,    49,    18,    19,    20,    21,    22,    54,    31,
      51,    56,   330,    51,    58,   253,    51,    54,    60,   251,
     252,    54,   260,    12,   258,    56,    47,   259,    54,   345,
       5,    20,    21,    56,    54,   267,    54,    51,    12,   271,
       9,   273,   274,   336,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,     6,   297,    53,    60,    54,   301,   100,    30,
     140,   228,   304,    30,    63,    64,    65,   231,    45,    46,
      47,    48,    49,   218,   316,   323,    53,   308,   270,   152,
     322,    -1,    -1,    -1,    -1,   329,    -1,    -1,   330,    -1,
     332,    -1,   334,    -1,   338,    -1,    -1,    -1,   340,    -1,
      -1,   343,    -1,    -1,   348,    -1,    -1,   349,     0,   353,
      -1,     3,     4,     5,     6,     7,     8,     9,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    18,    19,    20,    21,
      22,    -1,     0,    -1,    26,     3,     4,     5,     6,     7,
       8,     9,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      18,    19,    20,    21,    22,    -1,    -1,    -1,    26,    -1,
      52,    -1,    54,    55,    56,    -1,    -1,    -1,    -1,    -1,
      -1,     0,    -1,    -1,     3,     4,     5,     6,     7,     8,
       9,    -1,    -1,    -1,    -1,    -1,    54,    -1,    56,    18,
      19,    20,    21,    22,    -1,    -1,    -1,    26,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    10,    11,    12,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    54,    -1,    56,    23,    24,
      25,    -1,    27,    28,    29,    30,    -1,    -1,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    -1,    12,    53,    -1,
      -1,    16,    -1,    -1,    -1,    -1,    -1,    -1,    63,    64,
      65,    26,    -1,    -1,    -1,    30,    -1,    -1,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    -1,    -1,    53,    12,
      13,    14,    -1,    -1,    -1,    -1,    61,    62,    63,    64,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    -1,    -1,
      53,    12,    13,    14,    -1,    -1,    -1,    -1,    -1,    -1,
      63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      12,    -1,    53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,
      -1,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    -1,
      12,    53,    14,    -1,    -1,    -1,    -1,    -1,    -1,    61,
      62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    12,
      -1,    53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    12,    -1,
      53,    -1,    -1,    56,    -1,    19,    -1,    -1,    -1,    -1,
      63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    12,    -1,    53,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    63,
      64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    12,    53,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    63,    64,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    12,    -1,    53,    -1,
      -1,    -1,    -1,    -1,    -1,    60,    -1,    -1,    63,    64,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    12,    -1,    53,    -1,    -1,
      -1,    -1,    -1,    -1,    60,    -1,    -1,    63,    64,    65,
      -1,    -1,    -1,    -1,    -1,    -1,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    12,    -1,    53,    54,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    63,    64,    65,    -1,
      -1,    -1,    -1,    -1,    -1,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    12,    -1,    53,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    63,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    33,    -1,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    12,    -1,    -1,    53,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    63,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    -1,
      -1,    -1,    53,    -1,    -1,    -1,    -1,    -1,    -1,    17,
      -1,    -1,    63,    64,    65,    23,    24,    25,    26,    27,
      28,    29,    30,    -1,    32,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    23,
      24,    25,    26,    27,    28,    29,    30,    55,    32,    57,
      -1,    59,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      44,    45,    46,    23,    24,    25,    26,    27,    28,    29,
      30,    55,    32,    57,    -1,    59,    -1,    61,    62,    63,
      -1,    -1,    -1,    -1,    44,    45,    46,    47,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    55,    -1,    57,    -1,    59,
      23,    24,    25,    26,    27,    28,    29,    30,    -1,    32,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    44,    45,    46,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    55,    56,    57,    -1,    59,    23,    24,    25,
      26,    27,    28,    29,    30,    -1,    32,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    45,
      46,    23,    24,    25,    26,    27,    28,    29,    30,    55,
      32,    57,    -1,    59,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    44,    45,    46,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    55,    -1,    57,    -1,    59
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    13,    76,    78,    77,    79,    81,     0,    26,    30,
      74,    75,   160,     3,     4,     5,     6,     7,     8,     9,
      18,    19,    20,    21,    22,    80,    82,    83,    84,    85,
      89,    91,   100,   105,   106,   108,   109,   111,   113,   115,
      54,    74,    26,   162,    86,    87,   162,    23,    24,    25,
      26,    27,    28,    29,    32,    44,    45,    46,    55,    57,
      59,   120,   121,   122,   125,   126,   127,   132,   133,   138,
     153,   157,   158,   160,    17,   118,   120,    13,    15,    88,
      90,    98,   101,   156,   158,   160,   120,   120,   110,   120,
     112,   120,   116,    14,    88,    94,    98,    74,    12,    54,
      54,    31,   120,    55,   120,   120,   120,   154,   139,   134,
      12,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    53,
      63,    64,    65,   117,    52,    55,    59,    72,   119,    26,
      99,    26,   137,    92,    93,   162,    94,   102,   103,   120,
      54,    12,    14,   107,   107,    13,   107,    13,   107,   120,
     162,   137,    26,    96,   137,    94,    61,    62,    63,   120,
     151,   152,   162,    87,   120,   124,    56,    79,    26,    27,
      30,    59,   140,   141,   142,   161,   120,   135,   136,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   120,    12,
      35,    38,    39,    40,    41,    42,    43,    26,   123,    47,
     120,   143,    26,   160,   120,    14,    92,   138,    54,    31,
     137,    54,    10,    11,   104,   156,   156,    26,    27,    30,
     159,    26,    27,    30,   159,   137,   137,   120,   137,   120,
     137,    19,    31,    97,   137,   137,    63,   151,   120,    12,
      54,   120,   128,   129,   130,   131,    56,    51,   120,    58,
      54,    51,    60,    54,    51,   120,   120,   120,   120,   120,
     120,   120,   120,   128,    60,    47,   155,   162,    93,   120,
     103,   107,   107,   120,   120,    95,   162,    16,    26,   147,
     149,   151,   160,   120,   152,   162,    56,    79,    54,   120,
      60,   142,   120,   120,   120,    56,     5,   144,   137,   137,
      20,    21,   114,    54,    27,   160,    54,    26,   160,   120,
      12,   129,    51,   120,     9,   145,   120,   162,   150,   149,
     152,   120,   120,     6,   146,   107,   147,   148,   120,    54,
     120,    60,   137,    54,   149,   120,   147
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    73,    74,    74,    75,    75,    75,    76,    77,    76,
      78,    79,    80,    80,    80,    80,    80,    80,    81,    81,
      82,    82,    82,    82,    82,    82,    82,    82,    82,    82,
      82,    83,    83,    83,    83,    84,    85,    86,    86,    87,
      88,    90,    89,    91,    91,    91,    91,    91,    91,    92,
      92,    93,    94,    94,    94,    95,    95,    97,    96,    99,
      98,   101,   100,   102,   102,   103,   104,   104,   104,   104,
     105,   105,   106,   107,   107,   108,   109,   110,   110,   111,
     112,   112,   113,   114,   114,   116,   115,   117,   117,   117,
     119,   118,   118,   120,   120,   120,   120,   120,   120,   121,
     121,   123,   122,   124,   122,   125,   125,   125,   126,   126,
     126,   126,   126,   126,   126,   126,   126,   126,   126,   126,
     126,   126,   126,   126,   126,   126,   126,   126,   126,   126,
     126,   126,   126,   126,   127,   128,   128,   129,   130,   129,
     131,   131,   132,   132,   134,   133,   135,   135,   136,   136,
     137,   137,   139,   138,   140,   140,   141,   141,   142,   142,
     142,   142,   143,   143,   144,   144,   145,   145,   145,   146,
     146,   147,   147,   147,   147,   148,   148,   149,   150,   149,
     149,   149,   151,   151,   151,   152,   152,   153,   153,   153,
     153,   153,   154,   153,   153,   153,   153,   155,   153,   156,
     156,   157,   157,   158,   158,   158,   158,   158,   159,   159,
     159,   160,   161,   161,   162
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     1,     3,     2,     0,     0,     3,
       2,     2,     1,     1,     1,     1,     1,     1,     0,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     4,     6,     8,    10,     2,     2,     1,     3,     3,
       4,     0,     3,     3,     3,     4,     4,     3,     4,     1,
       3,     3,     0,     2,     4,     1,     3,     0,     3,     0,
       3,     0,     3,     1,     3,     2,     0,     1,     1,     1,
       2,     4,     2,     2,     2,     4,     4,     3,     5,     2,
       3,     5,     2,     1,     1,     0,     9,     1,     1,     1,
       0,     3,     1,     1,     1,     1,     1,     1,     3,     1,
       3,     0,     5,     0,     5,     2,     2,     2,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     4,     4,     4,     4,
       4,     4,     4,     4,     5,     0,     1,     1,     0,     2,
       1,     3,     1,     1,     0,     4,     0,     1,     1,     3,
       0,     2,     0,     4,     0,     1,     1,     3,     1,     3,
       3,     5,     1,     2,     0,     2,     0,     2,     4,     0,
       2,     1,     1,     2,     2,     1,     3,     1,     0,     4,
       2,     2,     1,     1,     1,     1,     2,     1,     1,     1,
       1,     3,     0,     4,     3,     3,     4,     0,     8,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1
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
#line 367 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 2064 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 3:
#line 370 "Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 2077 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 4:
#line 381 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 2086 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 385 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 2095 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 389 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 2104 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 396 "Aql/grammar.y" /* yacc.c:1646  */
    {
     }
#line 2111 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 8:
#line 398 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
     }
#line 2120 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 401 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      auto withNode = parser->ast()->createNodeWithCollections(node);
      parser->ast()->addOperation(withNode);
     }
#line 2130 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 409 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2137 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 414 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2144 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 419 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2151 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 13:
#line 421 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2159 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 14:
#line 424 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2167 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 427 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2175 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 430 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2183 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 433 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2191 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 439 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2198 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 441 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2205 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 446 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2212 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 448 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2219 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 450 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2226 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 452 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2233 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 454 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2240 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 456 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2247 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 458 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2254 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 460 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2261 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 462 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2268 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 464 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2275 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 466 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2282 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 471 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
     
      auto node = parser->ast()->createNodeFor((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2293 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 477 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2303 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 482 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-6].strval).value, (yyvsp[-6].strval).length, (yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2313 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 487 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-8].strval).value, (yyvsp[-8].strval).length, (yyvsp[-6].strval).value, (yyvsp[-6].strval).length, (yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2323 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 495 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // operand is a reference. can use it directly
      auto node = parser->ast()->createNodeFilter((yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2333 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 503 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2340 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 508 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2347 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 510 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2354 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 515 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLet((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2363 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 522 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[-2].strval).value, "COUNT")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'COUNT'", (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2375 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 532 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2384 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 535 "Aql/grammar.y" /* yacc.c:1646  */
    { 
      auto list = static_cast<AstNode*>(parser->popStack());

      if (list == nullptr) {
        ABORT_OOM
      }
      (yyval.node) = list;
    }
#line 2397 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 546 "Aql/grammar.y" /* yacc.c:1646  */
    {
      /* COLLECT WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      StartCollectScope(scopes);

      auto node = parser->ast()->createNodeCollectCount(parser->ast()->createNodeArray(), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2411 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 555 "Aql/grammar.y" /* yacc.c:1646  */
    {
      /* COLLECT var = expr WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-2].node));
      }

      auto node = parser->ast()->createNodeCollectCount((yyvsp[-2].node), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2427 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 566 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2462 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 46:
#line 596 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2534 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 663 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2564 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 48:
#line 688 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2599 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 721 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2606 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 723 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2613 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 728 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeAssign((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
      parser->pushArrayElement(node);
    }
#line 2622 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 52:
#line 735 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 2630 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 738 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 2638 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 741 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember(parser->ast()->createNodeValueString((yyvsp[-2].strval).value, (yyvsp[-2].strval).length));
      node->addMember((yyvsp[0].node));
      (yyval.node) = node;
    }
#line 2649 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 750 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2668 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 764 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 2687 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 781 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[0].strval).value, "KEEP")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'KEEP'", (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2700 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 788 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2709 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 795 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2718 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 798 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2727 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 805 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2736 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 808 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      auto node = parser->ast()->createNodeSort(list);
      parser->ast()->addOperation(node);
    }
#line 2746 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 816 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2754 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 819 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2762 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 825 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeSortElement((yyvsp[-1].node), (yyvsp[0].node));
    }
#line 2770 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 831 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2778 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 834 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2786 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 837 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 2794 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 840 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2802 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 846 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto offset = parser->ast()->createNodeValueInt(0);
      auto node = parser->ast()->createNodeLimit(offset, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2812 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 851 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLimit((yyvsp[-2].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2821 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 858 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeReturn((yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2831 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 866 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2839 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 74:
#line 869 "Aql/grammar.y" /* yacc.c:1646  */
    {
       (yyval.node) = (yyvsp[0].node);
     }
#line 2847 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 75:
#line 875 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2859 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 76:
#line 885 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2871 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 77:
#line 895 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2884 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 903 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2897 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 914 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2904 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 919 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2917 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 927 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2930 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 82:
#line 938 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2937 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 83:
#line 943 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_UPDATE);
    }
#line 2945 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 946 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_REPLACE);
    }
#line 2953 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 85:
#line 952 "Aql/grammar.y" /* yacc.c:1646  */
    { 
      // reserve a variable named "$OLD", we might need it in the update expression
      // and in a later return thing
      parser->pushStack(parser->ast()->createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD), true));
    }
#line 2963 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 86:
#line 956 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 3012 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 87:
#line 1003 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeQuantifier(Quantifier::ALL);
    }
#line 3020 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 88:
#line 1006 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeQuantifier(Quantifier::ANY);
    }
#line 3028 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 89:
#line 1009 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeQuantifier(Quantifier::NONE);
    }
#line 3036 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 90:
#line 1015 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto const scopeType = parser->ast()->scopes()->type();

      if (scopeType == AQL_SCOPE_MAIN ||
          scopeType == AQL_SCOPE_SUBQUERY) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "cannot use DISTINCT modifier on top-level query element", yylloc.first_line, yylloc.first_column);
      }
    }
#line 3049 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 91:
#line 1022 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDistinct((yyvsp[0].node));
    }
#line 3057 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 92:
#line 1025 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3065 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 93:
#line 1031 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3073 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 94:
#line 1034 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3081 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 95:
#line 1037 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3089 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 96:
#line 1040 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3097 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 97:
#line 1043 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3105 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 98:
#line 1046 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeRange((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3113 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 99:
#line 1052 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3121 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 100:
#line 1055 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 3139 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 101:
#line 1071 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushStack((yyvsp[-1].strval).value);

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3150 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 102:
#line 1076 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall(static_cast<char const*>(parser->popStack()), list);
    }
#line 3159 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 103:
#line 1080 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3168 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 104:
#line 1083 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall("LIKE", list);
    }
#line 3177 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 105:
#line 1090 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_PLUS, (yyvsp[0].node));
    }
#line 3185 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 106:
#line 1093 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_MINUS, (yyvsp[0].node));
    }
#line 3193 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 107:
#line 1096 "Aql/grammar.y" /* yacc.c:1646  */
    { 
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, (yyvsp[0].node));
    }
#line 3201 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 108:
#line 1102 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3209 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 109:
#line 1105 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3217 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 110:
#line 1108 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_PLUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3225 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 111:
#line 1111 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MINUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3233 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 112:
#line 1114 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_TIMES, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3241 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 113:
#line 1117 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_DIV, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3249 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 114:
#line 1120 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MOD, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3257 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 115:
#line 1123 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3265 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 116:
#line 1126 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3273 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 117:
#line 1129 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3281 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 118:
#line 1132 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3289 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 119:
#line 1135 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3297 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 120:
#line 1138 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3305 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 121:
#line 1141 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3313 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 122:
#line 1144 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3321 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 123:
#line 1147 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* arguments = parser->ast()->createNodeArray(2);
      arguments->addMember((yyvsp[-2].node));
      arguments->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeFunctionCall("LIKE", arguments);
    }
#line 3332 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 124:
#line 1153 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* arguments = parser->ast()->createNodeArray(2);
      arguments->addMember((yyvsp[-2].node));
      arguments->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeFunctionCall("REGEX_TEST", arguments);
    }
#line 3343 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 125:
#line 1159 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* arguments = parser->ast()->createNodeArray(2);
      arguments->addMember((yyvsp[-2].node));
      arguments->addMember((yyvsp[0].node));
      AstNode* node = parser->ast()->createNodeFunctionCall("REGEX_TEST", arguments);
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, node);
    }
#line 3355 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 126:
#line 1166 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3363 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 127:
#line 1169 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_NE, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3371 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 128:
#line 1172 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_LT, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3379 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 129:
#line 1175 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_GT, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3387 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 130:
#line 1178 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_LE, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3395 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 131:
#line 1181 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_GE, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3403 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 132:
#line 1184 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_IN, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3411 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 133:
#line 1187 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryArrayOperator(NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-2].node));
    }
#line 3419 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 134:
#line 1193 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3427 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 135:
#line 1199 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3434 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 136:
#line 1201 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3441 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 137:
#line 1206 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3449 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 138:
#line 1209 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 3458 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 139:
#line 1212 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 3473 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 140:
#line 1225 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3481 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 141:
#line 1228 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3489 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 142:
#line 1234 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3497 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 143:
#line 1237 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3505 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 144:
#line 1243 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3514 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 145:
#line 1246 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3522 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 146:
#line 1252 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3529 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 147:
#line 1254 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3536 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 148:
#line 1259 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3544 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 149:
#line 1262 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3552 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 150:
#line 1268 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3560 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 151:
#line 1271 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[-1].strval).value, "OPTIONS")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'OPTIONS'", (yyvsp[-1].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3576 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 152:
#line 1285 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeObject();
      parser->pushStack(node);
    }
#line 3585 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 153:
#line 1288 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3593 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 154:
#line 1294 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3600 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 155:
#line 1296 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3607 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 156:
#line 1301 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3614 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 157:
#line 1303 "Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3621 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 158:
#line 1308 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 3640 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 159:
#line 1322 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // attribute-name : attribute-value
      parser->pushObjectElement((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
    }
#line 3649 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 160:
#line 1326 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // bind-parameter : attribute-value
      if ((yyvsp[-2].strval).length < 1 || (yyvsp[-2].strval).value[0] == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto param = parser->ast()->createNodeParameter((yyvsp[-2].strval).value, (yyvsp[-2].strval).length);
      parser->pushObjectElement(param, (yyvsp[0].node));
    }
#line 3663 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 161:
#line 1335 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // [ attribute-name-expression ] : attribute-value
      parser->pushObjectElement((yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3672 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 162:
#line 1342 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3680 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 163:
#line 1345 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = (yyvsp[-1].intval) + 1;
    }
#line 3688 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 164:
#line 1351 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3696 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 165:
#line 1354 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3704 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 166:
#line 1360 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3712 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 167:
#line 1363 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit(nullptr, (yyvsp[0].node));
    }
#line 3720 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 168:
#line 1366 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3728 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 169:
#line 1372 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3736 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 170:
#line 1375 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3744 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 171:
#line 1381 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3752 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 172:
#line 1384 "Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3765 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 173:
#line 1392 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto tmp = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), tmp);
    }
#line 3774 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 174:
#line 1396 "Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), (yyvsp[0].node));
    }
#line 3787 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 175:
#line 1407 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3796 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 176:
#line 1411 "Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3805 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 177:
#line 1418 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3815 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 178:
#line 1423 "Aql/grammar.y" /* yacc.c:1646  */
    { 
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
      node->addMember((yyvsp[-1].node));
    }
#line 3825 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 179:
#line 1427 "Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3834 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 180:
#line 1431 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3848 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 181:
#line 1440 "Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3857 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 182:
#line 1449 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 2;
    }
#line 3865 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 183:
#line 1452 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3873 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 184:
#line 1455 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 0; 
    }
#line 3881 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 185:
#line 1461 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), 1);
    }
#line 3889 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 186:
#line 1464 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), (yyvsp[-1].node));
    }
#line 3897 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 187:
#line 1470 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 3934 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 188:
#line 1502 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3942 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 189:
#line 1505 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3950 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 190:
#line 1508 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
      
      if ((yyval.node) == nullptr) {
        ABORT_OOM
      }
    }
#line 3962 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 191:
#line 1515 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 3977 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 192:
#line 1525 "Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(arangodb::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 3986 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 193:
#line 1528 "Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 4001 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 194:
#line 1538 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4021 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 195:
#line 1553 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4040 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 196:
#line 1567 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4059 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 197:
#line 1581 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4087 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 198:
#line 1603 "Aql/grammar.y" /* yacc.c:1646  */
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
#line 4110 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 199:
#line 1624 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4118 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 200:
#line 1627 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4126 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 201:
#line 1633 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }
      
      (yyval.node) = (yyvsp[0].node);
    }
#line 4138 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 202:
#line 1640 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 4150 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 203:
#line 1650 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 4158 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 204:
#line 1653 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 4166 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 205:
#line 1656 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueNull();
    }
#line 4174 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 206:
#line 1659 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 4182 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 207:
#line 1662 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 4190 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 208:
#line 1668 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, TRI_TRANSACTION_WRITE);
    }
#line 4198 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 209:
#line 1671 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, TRI_TRANSACTION_WRITE);
    }
#line 4206 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 210:
#line 1674 "Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval).length < 2 || (yyvsp[0].strval).value[0] != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 4218 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 211:
#line 1684 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 4226 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 212:
#line 1690 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 4234 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 213:
#line 1693 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 4242 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 214:
#line 1698 "Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 4250 "Aql/grammar.cpp" /* yacc.c:1646  */
    break;


#line 4254 "Aql/grammar.cpp" /* yacc.c:1646  */
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
