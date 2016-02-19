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
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 18 "arangod/Aql/grammar.y" /* yacc.c:355  */

  triagens::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 201 "arangod/Aql/grammar.cpp" /* yacc.c:355  */
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
#line 28 "arangod/Aql/grammar.y" /* yacc.c:358  */


using namespace triagens::aql;

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
               triagens::aql::Parser* parser,
               char const* message) {
  parser->registerParseError(TRI_ERROR_QUERY_PARSE, message, locp->first_line, locp->first_column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register variables in the scope
////////////////////////////////////////////////////////////////////////////////

static void RegisterAssignVariables(triagens::aql::Scopes* scopes, AstNode const* vars) { 
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
        auto f = static_cast<triagens::aql::Function*>(func->getData());
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

static bool StartCollectScope(triagens::aql::Scopes* scopes) { 
  // check if we are in the main scope
  if (scopes->type() == triagens::aql::AQL_SCOPE_MAIN) {
    return false;
  } 

  // end the active scopes
  scopes->endNested();
  // start a new scope
  scopes->start(triagens::aql::AQL_SCOPE_COLLECT);
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


#line 372 "arangod/Aql/grammar.cpp" /* yacc.c:358  */

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
#define YYLAST   960

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  70
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  88
/* YYNRULES -- Number of rules.  */
#define YYNRULES  198
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  326

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
       0,   331,   331,   334,   345,   349,   353,   360,   362,   362,
     373,   378,   383,   385,   388,   391,   394,   397,   403,   405,
     410,   412,   414,   416,   418,   420,   422,   424,   426,   428,
     430,   435,   441,   446,   451,   459,   467,   472,   474,   479,
     486,   496,   496,   510,   519,   530,   549,   600,   614,   636,
     638,   643,   650,   653,   656,   665,   679,   696,   696,   710,
     710,   720,   720,   731,   734,   740,   746,   749,   752,   755,
     761,   766,   773,   781,   784,   790,   800,   810,   818,   829,
     834,   842,   853,   858,   861,   867,   867,   918,   918,   928,
     934,   937,   940,   943,   946,   949,   955,   958,   974,   974,
     986,   989,   992,   998,  1001,  1004,  1007,  1010,  1013,  1016,
    1019,  1022,  1025,  1028,  1031,  1034,  1037,  1040,  1046,  1052,
    1054,  1059,  1062,  1062,  1078,  1081,  1087,  1090,  1096,  1096,
    1105,  1107,  1112,  1115,  1121,  1124,  1138,  1138,  1147,  1149,
    1154,  1156,  1161,  1175,  1179,  1188,  1195,  1198,  1204,  1207,
    1213,  1216,  1219,  1225,  1228,  1234,  1237,  1245,  1249,  1260,
    1264,  1271,  1277,  1276,  1285,  1294,  1303,  1306,  1309,  1315,
    1318,  1324,  1356,  1359,  1362,  1369,  1379,  1379,  1392,  1407,
    1421,  1435,  1435,  1478,  1481,  1487,  1494,  1504,  1507,  1510,
    1513,  1516,  1522,  1525,  1528,  1538,  1544,  1547,  1552
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
  "with_collection", "with_collection_list", "optional_with", "$@1",
  "queryStart", "query", "final_statement",
  "optional_statement_block_statements", "statement_block_statement",
  "for_statement", "filter_statement", "let_statement", "let_list",
  "let_element", "count_into", "collect_variable_list", "$@2",
  "collect_statement", "collect_list", "collect_element",
  "collect_optional_into", "variable_list", "keep", "$@3", "aggregate",
  "$@4", "sort_statement", "$@5", "sort_list", "sort_element",
  "sort_direction", "limit_statement", "return_statement",
  "in_or_into_collection", "remove_statement", "insert_statement",
  "update_parameters", "update_statement", "replace_parameters",
  "replace_statement", "update_or_replace", "upsert_statement", "$@6",
  "distinct_expression", "$@7", "expression", "function_name",
  "function_call", "$@8", "operator_unary", "operator_binary",
  "operator_ternary", "optional_function_call_arguments",
  "expression_or_query", "$@9", "function_arguments_list",
  "compound_value", "array", "$@10", "optional_array_elements",
  "array_elements_list", "options", "object", "$@11",
  "optional_object_elements", "object_elements_list", "object_element",
  "array_filter_operator", "optional_array_filter", "optional_array_limit",
  "optional_array_return", "graph_collection", "graph_collection_list",
  "graph_subject", "$@12", "graph_direction", "graph_direction_steps",
  "reference", "$@13", "$@14", "simple_value", "numeric_value",
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

#define YYPACT_NINF -259

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-259)))

#define YYTABLE_NINF -197

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      44,  -259,  -259,    65,    66,  -259,   168,  -259,  -259,  -259,
    -259,     9,  -259,    59,    59,   885,   224,   134,  -259,   263,
     885,   885,   885,   885,  -259,  -259,  -259,  -259,  -259,  -259,
     139,  -259,  -259,  -259,  -259,     6,    38,    41,    47,    51,
      66,  -259,  -259,    11,    52,  -259,    63,  -259,  -259,  -259,
      27,  -259,  -259,  -259,   885,   885,   885,   885,  -259,  -259,
     736,    85,  -259,  -259,  -259,  -259,  -259,  -259,  -259,   -29,
    -259,  -259,  -259,  -259,  -259,   736,    81,  -259,   132,    59,
     145,   885,   111,  -259,  -259,   665,   665,  -259,   556,  -259,
     594,   885,    59,   132,   136,   145,  -259,   799,    59,    59,
     885,   131,   131,   131,   455,  -259,   102,   885,   885,   152,
     885,   885,   885,   885,   885,   885,   885,   885,   885,   885,
     885,   885,   885,   885,   885,   142,   119,   819,    74,   885,
     166,    59,   128,  -259,   133,  -259,   151,   132,   141,  -259,
     498,   263,   905,   115,   132,   132,   885,   132,   885,   132,
     700,   163,  -259,   128,   132,  -259,   132,  -259,  -259,  -259,
     350,  -259,   885,    13,  -259,   736,  -259,   155,   156,  -259,
     157,   885,   154,   159,  -259,   169,   736,   161,   160,   644,
     885,   772,   754,   572,   572,   401,   401,   401,   401,    39,
      39,   131,   131,   131,   718,   334,  -259,   852,  -259,   385,
     175,  -259,  -259,   736,    59,   133,  -259,    59,   885,  -259,
     885,  -259,  -259,  -259,  -259,  -259,   317,    24,   193,  -259,
    -259,  -259,  -259,  -259,  -259,  -259,   665,  -259,   665,  -259,
     885,   885,    59,  -259,  -259,  -259,   315,   799,    59,  -259,
     885,   420,  -259,   102,   885,  -259,   885,   644,   885,   736,
     170,  -259,  -259,   171,  -259,  -259,   218,  -259,  -259,   736,
    -259,   132,   132,   629,   736,   174,  -259,    96,  -259,   180,
    -259,   109,  -259,   350,   885,   213,   736,   184,  -259,   736,
     736,   736,  -259,  -259,   885,   885,   225,  -259,  -259,  -259,
    -259,   885,    59,  -259,  -259,  -259,  -259,  -259,   315,   799,
     885,  -259,   736,   885,   227,   665,  -259,   108,  -259,   885,
     736,   517,   885,   183,   132,  -259,   186,   315,   885,   736,
    -259,  -259,   108,  -259,   736,  -259
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       7,     8,    18,     0,     0,    10,     0,     1,     2,   195,
       4,     9,     3,     0,     0,     0,     0,    41,    61,     0,
       0,     0,     0,     0,    85,    11,    19,    20,    22,    21,
      52,    23,    24,    25,    12,    26,    27,    28,    29,    30,
       0,     6,   198,     0,    36,    37,     0,   189,   190,   191,
     171,   187,   185,   186,     0,     0,     0,   176,   136,   128,
      35,    98,   174,    90,    91,    92,   172,   126,   127,    94,
     188,    93,   173,    87,    72,    89,     0,    59,   134,     0,
      52,     0,    70,   183,   184,     0,     0,    79,     0,    82,
       0,     0,     0,   134,   134,    52,     5,     0,     0,     0,
       0,   102,   100,   101,     0,    18,   138,   130,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    43,    42,    49,     0,   134,    62,    63,
      66,     0,     0,     0,   134,   134,     0,   134,     0,   134,
       0,    53,    44,    57,   134,    47,   134,   166,   167,   168,
      31,   169,     0,     0,    38,    39,   175,     0,   142,   197,
       0,     0,     0,   139,   140,     0,   132,     0,   131,   116,
       0,   104,   103,   110,   111,   112,   113,   114,   115,   105,
     106,   107,   108,   109,     0,    95,    97,   122,   146,     0,
     181,   178,   179,    88,     0,    60,   135,     0,     0,    45,
       0,    67,    68,    65,    69,    71,   171,   187,   195,    73,
     192,   193,   194,    74,    75,    76,     0,    77,     0,    80,
       0,     0,     0,    48,    46,   170,     0,     0,     0,   177,
       0,     0,   137,     0,     0,   129,     0,   117,     0,   121,
       0,   124,    18,   120,   180,   147,   148,    40,    50,    51,
      64,   134,   134,     0,    54,    58,    55,     0,   155,   161,
      32,     0,   156,     0,     0,     0,   144,     0,   141,   143,
     133,   118,    99,   123,   122,     0,   150,    78,    81,    83,
      84,     0,     0,   165,   164,   162,   157,   158,     0,     0,
       0,   125,   149,     0,   153,     0,    56,     0,    33,     0,
     145,   151,     0,     0,   134,   159,   163,     0,     0,   154,
     182,    86,     0,    34,   152,   160
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -259,    -6,  -259,  -259,  -259,  -259,  -101,  -259,  -259,  -259,
    -259,  -259,  -259,  -259,   158,   210,  -259,  -259,  -259,   124,
      53,   -58,  -259,  -259,  -259,   228,  -259,  -259,  -259,  -259,
      49,  -259,  -259,  -259,   -78,  -259,  -259,  -259,  -259,  -259,
    -259,  -259,  -259,  -259,  -259,  -259,    -2,  -259,  -259,  -259,
    -259,  -259,  -259,  -259,   -23,  -259,  -259,  -259,  -259,  -259,
    -259,  -259,   -77,   -84,  -259,  -259,  -259,    19,  -259,  -259,
    -259,  -259,  -258,  -259,  -236,  -259,   -94,  -226,  -259,  -259,
    -259,    15,  -259,   -10,   121,    -4,  -259,   -12
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    10,    11,     2,     4,     3,     5,    25,     6,    26,
      27,    28,    29,    44,    45,    78,    30,    79,    31,   134,
     135,    94,   265,   154,   232,    80,   131,    32,    81,   138,
     139,   213,    33,    34,   144,    35,    36,    87,    37,    89,
      38,   291,    39,    91,    74,   129,   140,    61,    62,   126,
      63,    64,    65,   250,   251,   252,   253,    66,    67,   107,
     177,   178,   133,    68,   106,   172,   173,   174,   200,   286,
     304,   313,   269,   316,   270,   307,   271,   162,    69,   105,
     256,    82,    70,    71,   219,    72,   175,   136
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      12,    43,    46,   161,   167,    41,   -13,    12,   145,    83,
     147,   274,   149,    60,    75,    84,   152,   155,    85,    86,
      88,    90,   137,    97,  -193,   237,   127,  -193,  -193,  -193,
    -193,  -193,  -193,  -193,    96,     8,    12,   156,   -14,     9,
     128,   -15,  -193,  -193,  -193,  -193,  -193,   -16,   206,   315,
    -193,   -17,   101,   102,   103,   104,   -13,     1,   -13,    40,
     209,    98,   308,   238,   325,     7,   235,   224,   225,   206,
     227,   109,   229,   309,  -193,   -96,  -193,   233,   -96,   234,
     151,   323,   120,   121,   122,    42,   163,    46,   -14,   150,
     -14,   -15,     8,   -15,   100,   160,     9,   -16,   165,   -16,
     201,   -17,    99,   -17,     9,   176,   179,   130,   181,   182,
     183,   184,   185,   186,   187,   188,   189,   190,   191,   192,
     193,   194,   195,   293,   202,   199,     9,   203,   168,   169,
      83,    83,   170,   125,   268,   296,    84,    84,     9,     9,
     179,   220,   221,   161,   226,   222,   228,    76,   261,    77,
     262,   283,    76,    92,    77,   214,   215,   171,   132,    92,
     236,   141,   153,   109,   180,   157,   158,   159,   196,   241,
     197,    13,    14,    15,    16,    17,    18,    19,   247,   235,
     204,    58,   208,   207,   287,   288,    20,    21,    22,    23,
      24,   210,   257,  -194,   231,   249,  -194,  -194,  -194,  -194,
    -194,  -194,  -194,  -196,   240,   161,   259,   239,   242,   243,
     246,  -194,  -194,  -194,  -194,  -194,   244,   245,   255,  -194,
     266,   284,   282,   285,   292,   299,   275,   314,   263,   264,
     295,   300,   272,   312,   303,   273,   322,   321,   276,   320,
      93,    73,   279,  -194,   280,  -194,   281,    47,    48,    49,
      50,    51,    52,    53,     9,   205,    54,   164,    95,   260,
     258,   301,   278,   294,   223,    55,    56,   297,     0,     0,
       0,     0,   298,     0,     0,    57,     0,    58,     0,    59,
     306,     0,   249,   302,     0,     0,    47,    48,    49,   305,
      51,    52,    53,     9,   272,     0,     0,   273,   310,     0,
       0,   311,     0,   272,     0,     0,     0,   317,     0,     0,
     319,     0,     0,   272,     0,     0,   324,  -192,   272,     0,
    -192,  -192,  -192,  -192,  -192,  -192,  -192,   108,     0,     0,
       0,   267,     0,     0,     0,  -192,  -192,  -192,  -192,  -192,
       0,   268,     0,  -192,     0,     9,     0,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   108,     0,   124,   -96,   109,  -192,   -96,  -192,
       0,     0,   157,   158,   159,   118,   119,   120,   121,   122,
       0,     0,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   108,     0,   124,
       0,     0,     0,     0,     0,     0,     0,   157,   158,   159,
       0,     0,     0,     0,     0,     0,     0,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   108,   109,   124,     0,     0,     0,     0,     0,
       0,   254,   118,   119,   120,   121,   122,     0,     0,     0,
     124,     0,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   108,     0,   124,
       0,     0,     0,     0,     0,     0,   277,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,     0,     0,   124,     0,     0,   166,   211,   212,
     108,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    47,    48,    49,     0,    51,    52,    53,     9,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,     0,     0,   124,     0,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,     0,     0,   124,   318,   142,   146,
     143,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   108,     0,     0,     0,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,     0,   109,   124,   142,   148,   143,   114,
     115,   116,   117,   118,   119,   120,   121,   122,     0,     0,
       0,   124,     0,     0,     0,     0,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   108,     0,   124,     0,     0,     0,     0,     0,   289,
     290,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   109,   142,   124,   143,
       0,   114,   115,   116,   117,   118,   119,   120,   121,   122,
       0,     0,     0,   124,     0,     0,     0,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   108,     0,   124,     0,     0,     0,     0,   230,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     108,     0,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,     0,   108,   124,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   248,   108,   124,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,     0,   108,   124,   109,   110,     0,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
       0,     0,     0,   124,   109,     0,     0,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,     0,     0,
       0,   124,    47,    48,    49,    50,    51,    52,    53,     9,
       0,    54,     0,     0,     0,     0,     0,     0,     0,     0,
      55,    56,    47,    48,    49,    50,    51,    52,    53,     9,
      57,    54,    58,     0,    59,     0,   157,   158,   159,     0,
      55,    56,   198,     0,     0,     0,     0,     0,     0,     0,
      57,     0,    58,     0,    59,    47,    48,    49,    50,    51,
      52,    53,     9,     0,    54,     0,     0,     0,     0,     0,
       0,     0,     0,    55,    56,     0,     0,     0,     0,     0,
       0,     0,     0,    57,  -119,    58,     0,    59,    47,    48,
      49,    50,    51,    52,    53,     9,     0,    54,     0,     0,
       0,     0,     0,     0,     0,     0,    55,    56,    47,    48,
      49,   216,   217,    52,    53,   218,    57,    54,    58,     0,
      59,     0,     0,     0,     0,     0,    55,    56,     0,     0,
       0,     0,     0,     0,     0,     0,    57,     0,    58,     0,
      59
};

static const yytype_int16 yycheck[] =
{
       4,    13,    14,    97,   105,    11,     0,    11,    86,    19,
      88,   237,    90,    15,    16,    19,    93,    94,    20,    21,
      22,    23,    80,    12,     0,    12,    55,     3,     4,     5,
       6,     7,     8,     9,    40,    26,    40,    95,     0,    30,
      69,     0,    18,    19,    20,    21,    22,     0,   132,   307,
      26,     0,    54,    55,    56,    57,    50,    13,    52,    50,
     137,    50,   298,    50,   322,     0,   160,   144,   145,   153,
     147,    32,   149,   299,    50,    48,    52,   154,    51,   156,
      92,   317,    43,    44,    45,    26,    98,    99,    50,    91,
      52,    50,    26,    52,    31,    97,    30,    50,   100,    52,
      26,    50,    50,    52,    30,   107,   108,    26,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,    27,   128,   127,    30,   129,    26,    27,
     140,   141,    30,    48,    26,    26,   140,   141,    30,    30,
     142,    26,    27,   237,   146,    30,   148,    13,   226,    15,
     228,   252,    13,    14,    15,   140,   141,    55,    26,    14,
     162,    50,    26,    32,    12,    57,    58,    59,    26,   171,
      51,     3,     4,     5,     6,     7,     8,     9,   180,   273,
      14,    53,    31,    50,   261,   262,    18,    19,    20,    21,
      22,    50,   204,     0,    31,   197,     3,     4,     5,     6,
       7,     8,     9,    47,    47,   299,   208,    52,    54,    50,
      50,    18,    19,    20,    21,    22,    47,    56,    43,    26,
     232,    50,    52,     5,    50,    12,   238,   305,   230,   231,
      50,    47,   236,     6,     9,   237,    50,   314,   240,    56,
      30,    17,   244,    50,   246,    52,   248,    23,    24,    25,
      26,    27,    28,    29,    30,   131,    32,    99,    30,   210,
     207,   284,   243,   267,   143,    41,    42,   271,    -1,    -1,
      -1,    -1,   274,    -1,    -1,    51,    -1,    53,    -1,    55,
     292,    -1,   284,   285,    -1,    -1,    23,    24,    25,   291,
      27,    28,    29,    30,   298,    -1,    -1,   299,   300,    -1,
      -1,   303,    -1,   307,    -1,    -1,    -1,   309,    -1,    -1,
     312,    -1,    -1,   317,    -1,    -1,   318,     0,   322,    -1,
       3,     4,     5,     6,     7,     8,     9,    12,    -1,    -1,
      -1,    16,    -1,    -1,    -1,    18,    19,    20,    21,    22,
      -1,    26,    -1,    26,    -1,    30,    -1,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    12,    -1,    49,    48,    32,    50,    51,    52,
      -1,    -1,    57,    58,    59,    41,    42,    43,    44,    45,
      -1,    -1,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    12,    -1,    49,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    57,    58,    59,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    12,    32,    49,    -1,    -1,    -1,    -1,    -1,
      -1,    56,    41,    42,    43,    44,    45,    -1,    -1,    -1,
      49,    -1,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    12,    -1,    49,
      -1,    -1,    -1,    -1,    -1,    -1,    56,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    -1,    -1,    49,    -1,    -1,    52,    10,    11,
      12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    23,    24,    25,    -1,    27,    28,    29,    30,    12,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    -1,    -1,    49,    -1,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    -1,    -1,    49,    50,    12,    13,
      14,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    12,    -1,    -1,    -1,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    -1,    32,    49,    12,    13,    14,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    -1,    -1,
      -1,    49,    -1,    -1,    -1,    -1,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    12,    -1,    49,    -1,    -1,    -1,    -1,    -1,    20,
      21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    32,    12,    49,    14,
      -1,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      -1,    -1,    -1,    49,    -1,    -1,    -1,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    12,    -1,    49,    -1,    -1,    -1,    -1,    19,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      12,    -1,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    -1,    12,    49,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    12,    49,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    -1,    12,    49,    32,    33,    -1,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      -1,    -1,    -1,    49,    32,    -1,    -1,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    -1,    -1,
      -1,    49,    23,    24,    25,    26,    27,    28,    29,    30,
      -1,    32,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      41,    42,    23,    24,    25,    26,    27,    28,    29,    30,
      51,    32,    53,    -1,    55,    -1,    57,    58,    59,    -1,
      41,    42,    43,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      51,    -1,    53,    -1,    55,    23,    24,    25,    26,    27,
      28,    29,    30,    -1,    32,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    41,    42,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    51,    52,    53,    -1,    55,    23,    24,
      25,    26,    27,    28,    29,    30,    -1,    32,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    41,    42,    23,    24,
      25,    26,    27,    28,    29,    30,    51,    32,    53,    -1,
      55,    -1,    -1,    -1,    -1,    -1,    41,    42,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    51,    -1,    53,    -1,
      55
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    13,    73,    75,    74,    76,    78,     0,    26,    30,
      71,    72,   155,     3,     4,     5,     6,     7,     8,     9,
      18,    19,    20,    21,    22,    77,    79,    80,    81,    82,
      86,    88,    97,   102,   103,   105,   106,   108,   110,   112,
      50,    71,    26,   157,    83,    84,   157,    23,    24,    25,
      26,    27,    28,    29,    32,    41,    42,    51,    53,    55,
     116,   117,   118,   120,   121,   122,   127,   128,   133,   148,
     152,   153,   155,    17,   114,   116,    13,    15,    85,    87,
      95,    98,   151,   153,   155,   116,   116,   107,   116,   109,
     116,   113,    14,    85,    91,    95,    71,    12,    50,    50,
      31,   116,   116,   116,   116,   149,   134,   129,    12,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    49,    48,   119,    55,    69,   115,
      26,    96,    26,   132,    89,    90,   157,    91,    99,   100,
     116,    50,    12,    14,   104,   104,    13,   104,    13,   104,
     116,   157,   132,    26,    93,   132,    91,    57,    58,    59,
     116,   146,   147,   157,    84,   116,    52,    76,    26,    27,
      30,    55,   135,   136,   137,   156,   116,   130,   131,   116,
      12,   116,   116,   116,   116,   116,   116,   116,   116,   116,
     116,   116,   116,   116,   116,   116,    26,    51,    43,   116,
     138,    26,   155,   116,    14,    89,   133,    50,    31,   132,
      50,    10,    11,   101,   151,   151,    26,    27,    30,   154,
      26,    27,    30,   154,   132,   132,   116,   132,   116,   132,
      19,    31,    94,   132,   132,   146,   116,    12,    50,    52,
      47,   116,    54,    50,    47,    56,    50,   116,    47,   116,
     123,   124,   125,   126,    56,    43,   150,   157,    90,   116,
     100,   104,   104,   116,   116,    92,   157,    16,    26,   142,
     144,   146,   155,   116,   147,   157,   116,    56,   137,   116,
     116,   116,    52,    76,    50,     5,   139,   132,   132,    20,
      21,   111,    50,    27,   155,    50,    26,   155,   116,    12,
      47,   124,   116,     9,   140,   116,   157,   145,   144,   147,
     116,   116,     6,   141,   104,   142,   143,   116,    50,   116,
      56,   132,    50,   144,   116,   142
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    70,    71,    71,    72,    72,    72,    73,    74,    73,
      75,    76,    77,    77,    77,    77,    77,    77,    78,    78,
      79,    79,    79,    79,    79,    79,    79,    79,    79,    79,
      79,    80,    80,    80,    80,    81,    82,    83,    83,    84,
      85,    87,    86,    88,    88,    88,    88,    88,    88,    89,
      89,    90,    91,    91,    91,    92,    92,    94,    93,    96,
      95,    98,    97,    99,    99,   100,   101,   101,   101,   101,
     102,   102,   103,   104,   104,   105,   106,   107,   107,   108,
     109,   109,   110,   111,   111,   113,   112,   115,   114,   114,
     116,   116,   116,   116,   116,   116,   117,   117,   119,   118,
     120,   120,   120,   121,   121,   121,   121,   121,   121,   121,
     121,   121,   121,   121,   121,   121,   121,   121,   122,   123,
     123,   124,   125,   124,   126,   126,   127,   127,   129,   128,
     130,   130,   131,   131,   132,   132,   134,   133,   135,   135,
     136,   136,   137,   137,   137,   137,   138,   138,   139,   139,
     140,   140,   140,   141,   141,   142,   142,   142,   142,   143,
     143,   144,   145,   144,   144,   144,   146,   146,   146,   147,
     147,   148,   148,   148,   148,   148,   149,   148,   148,   148,
     148,   150,   148,   151,   151,   152,   152,   153,   153,   153,
     153,   153,   154,   154,   154,   155,   156,   156,   157
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
       3,     5,     2,     1,     1,     0,     9,     0,     3,     1,
       1,     1,     1,     1,     1,     3,     1,     3,     0,     5,
       2,     2,     2,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     4,     5,     0,
       1,     1,     0,     2,     1,     3,     1,     1,     0,     4,
       0,     1,     1,     3,     0,     2,     0,     4,     0,     1,
       1,     3,     1,     3,     3,     5,     1,     2,     0,     2,
       0,     2,     4,     0,     2,     1,     1,     2,     2,     1,
       3,     1,     0,     4,     2,     2,     1,     1,     1,     1,
       2,     1,     1,     1,     1,     3,     0,     4,     3,     3,
       4,     0,     8,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1
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
#line 331 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 1958 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 3:
#line 334 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 1971 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 4:
#line 345 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 1980 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 349 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 1989 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 353 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 1998 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 360 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
     }
#line 2005 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 8:
#line 362 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
     }
#line 2014 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 365 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      auto withNode = parser->ast()->createNodeWithCollections(node);
      parser->ast()->addOperation(withNode);
     }
#line 2024 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 373 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2031 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 378 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2038 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 383 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2045 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 13:
#line 385 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2053 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 14:
#line 388 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2061 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 391 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2069 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 394 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2077 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 397 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->endNested();
    }
#line 2085 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 403 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2092 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 405 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2099 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 410 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2106 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 412 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2113 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 414 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2120 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 416 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2127 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 418 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2134 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 420 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2141 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 422 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2148 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 424 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2155 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 426 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2162 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 428 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2169 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 430 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2176 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 435 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
     
      auto node = parser->ast()->createNodeFor((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2187 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 441 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2197 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 446 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-6].strval).value, (yyvsp[-6].strval).length, (yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2207 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 451 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
      auto node = parser->ast()->createNodeTraversal((yyvsp[-8].strval).value, (yyvsp[-8].strval).length, (yyvsp[-6].strval).value, (yyvsp[-6].strval).length, (yyvsp[-4].strval).value, (yyvsp[-4].strval).length, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2217 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 459 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // operand is a reference. can use it directly
      auto node = parser->ast()->createNodeFilter((yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2227 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 467 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2234 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 472 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2241 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 474 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2248 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 479 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLet((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node), true);
      parser->ast()->addOperation(node);
    }
#line 2257 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 486 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[-2].strval).value, "COUNT")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'COUNT'", (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2269 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 496 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2278 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 499 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    { 
      auto list = static_cast<AstNode*>(parser->popStack());

      if (list == nullptr) {
        ABORT_OOM
      }
      (yyval.node) = list;
    }
#line 2291 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 510 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      /* COLLECT WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      StartCollectScope(scopes);

      auto node = parser->ast()->createNodeCollectCount(parser->ast()->createNodeArray(), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2305 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 519 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      /* COLLECT var = expr WITH COUNT INTO var OPTIONS ... */
      auto scopes = parser->ast()->scopes();

      if (StartCollectScope(scopes)) {
        RegisterAssignVariables(scopes, (yyvsp[-2].node));
      }

      auto node = parser->ast()->createNodeCollectCount((yyvsp[-2].node), (yyvsp[-1].strval).value, (yyvsp[-1].strval).length, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2321 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 530 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 2345 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 46:
#line 549 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 2401 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 600 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 2420 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 48:
#line 614 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 2444 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 636 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2451 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 638 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2458 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 643 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeAssign((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
      parser->pushArrayElement(node);
    }
#line 2467 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 52:
#line 650 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 2475 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 653 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 2483 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 656 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember(parser->ast()->createNodeValueString((yyvsp[-2].strval).value, (yyvsp[-2].strval).length));
      node->addMember((yyvsp[0].node));
      (yyval.node) = node;
    }
#line 2494 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 665 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 2513 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 679 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 2532 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 696 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! TRI_CaseEqualString((yyvsp[0].strval).value, "KEEP")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'KEEP'", (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2545 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 703 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2554 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 710 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2563 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 713 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = list;
    }
#line 2572 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 720 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2581 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 723 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      auto node = parser->ast()->createNodeSort(list);
      parser->ast()->addOperation(node);
    }
#line 2591 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 731 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2599 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 734 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 2607 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 740 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeSortElement((yyvsp[-1].node), (yyvsp[0].node));
    }
#line 2615 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 746 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2623 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 749 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 2631 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 752 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 2639 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 755 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2647 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 761 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto offset = parser->ast()->createNodeValueInt(0);
      auto node = parser->ast()->createNodeLimit(offset, (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2657 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 766 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeLimit((yyvsp[-2].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2666 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 773 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeReturn((yyvsp[0].node));
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
#line 2676 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 781 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2684 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 74:
#line 784 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
       (yyval.node) = (yyvsp[0].node);
     }
#line 2692 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 75:
#line 790 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2704 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 76:
#line 800 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert((yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2716 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 77:
#line 810 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2729 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 818 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeUpdate((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2742 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 829 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2749 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 834 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace(nullptr, (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2762 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 842 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if (! parser->configureWriteQuery((yyvsp[-1].node), (yyvsp[0].node))) {
        YYABORT;
      }

      AstNode* node = parser->ast()->createNodeReplace((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyvsp[0].node));
      parser->ast()->addOperation(node);
    }
#line 2775 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 82:
#line 853 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 2782 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 83:
#line 858 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_UPDATE);
    }
#line 2790 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 861 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = static_cast<int64_t>(NODE_TYPE_REPLACE);
    }
#line 2798 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 85:
#line 867 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    { 
      // reserve a variable named "$OLD", we might need it in the update expression
      // and in a later return thing
      parser->pushStack(parser->ast()->createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD), true));
    }
#line 2808 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 86:
#line 871 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 2857 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 87:
#line 918 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto const scopeType = parser->ast()->scopes()->type();

      if (scopeType == AQL_SCOPE_MAIN ||
          scopeType == AQL_SCOPE_SUBQUERY) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "cannot use DISTINCT modifier on top-level query element", yylloc.first_line, yylloc.first_column);
      }
    }
#line 2870 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 88:
#line 925 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDistinct((yyvsp[0].node));
    }
#line 2878 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 89:
#line 928 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2886 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 90:
#line 934 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2894 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 91:
#line 937 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2902 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 92:
#line 940 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2910 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 93:
#line 943 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2918 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 94:
#line 946 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 2926 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 95:
#line 949 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeRange((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2934 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 96:
#line 955 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 2942 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 97:
#line 958 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 2960 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 98:
#line 974 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushStack((yyvsp[0].strval).value);

      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 2971 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 99:
#line 979 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto list = static_cast<AstNode const*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeFunctionCall(static_cast<char const*>(parser->popStack()), list);
    }
#line 2980 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 100:
#line 986 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_PLUS, (yyvsp[0].node));
    }
#line 2988 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 101:
#line 989 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_MINUS, (yyvsp[0].node));
    }
#line 2996 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 102:
#line 992 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    { 
      (yyval.node) = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, (yyvsp[0].node));
    }
#line 3004 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 103:
#line 998 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3012 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 104:
#line 1001 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3020 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 105:
#line 1004 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_PLUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3028 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 106:
#line 1007 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MINUS, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3036 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 107:
#line 1010 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_TIMES, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3044 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 108:
#line 1013 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_DIV, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3052 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 109:
#line 1016 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MOD, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3060 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 110:
#line 1019 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3068 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 111:
#line 1022 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3076 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 112:
#line 1025 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3084 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 113:
#line 1028 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GT, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3092 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 114:
#line 1031 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3100 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 115:
#line 1034 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GE, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3108 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 116:
#line 1037 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3116 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 117:
#line 1040 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, (yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3124 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 118:
#line 1046 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeTernaryOperator((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3132 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 119:
#line 1052 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3139 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 120:
#line 1054 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3146 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 121:
#line 1059 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3154 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 122:
#line 1062 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 3163 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 123:
#line 1065 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = std::move(parser->ast()->variables()->nextName());
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 3178 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 124:
#line 1078 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3186 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 125:
#line 1081 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3194 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 126:
#line 1087 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3202 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 127:
#line 1090 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3210 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 128:
#line 1096 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    }
#line 3219 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 129:
#line 1099 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3227 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 130:
#line 1105 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3234 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 131:
#line 1107 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3241 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 132:
#line 1112 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3249 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 133:
#line 1115 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->pushArrayElement((yyvsp[0].node));
    }
#line 3257 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 134:
#line 1121 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3265 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 135:
#line 1124 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[-1].strval).value, "OPTIONS")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'OPTIONS'", (yyvsp[-1].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3281 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 136:
#line 1138 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeObject();
      parser->pushStack(node);
    }
#line 3290 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 137:
#line 1141 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = static_cast<AstNode*>(parser->popStack());
    }
#line 3298 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 138:
#line 1147 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3305 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 139:
#line 1149 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3312 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 140:
#line 1154 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3319 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 141:
#line 1156 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
    }
#line 3326 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 142:
#line 1161 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 3345 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 143:
#line 1175 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // attribute-name : attribute-value
      parser->pushObjectElement((yyvsp[-2].strval).value, (yyvsp[-2].strval).length, (yyvsp[0].node));
    }
#line 3354 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 144:
#line 1179 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // bind-parameter : attribute-value
      if ((yyvsp[-2].strval).length < 1 || (yyvsp[-2].strval).value[0] == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[-2].strval).value, yylloc.first_line, yylloc.first_column);
      }

      auto param = parser->ast()->createNodeParameter((yyvsp[-2].strval).value, (yyvsp[-2].strval).length);
      parser->pushObjectElement(param, (yyvsp[0].node));
    }
#line 3368 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 145:
#line 1188 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // [ attribute-name-expression ] : attribute-value
      parser->pushObjectElement((yyvsp[-3].node), (yyvsp[0].node));
    }
#line 3377 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 146:
#line 1195 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3385 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 147:
#line 1198 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = (yyvsp[-1].intval) + 1;
    }
#line 3393 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 148:
#line 1204 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3401 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 149:
#line 1207 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3409 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 150:
#line 1213 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3417 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 151:
#line 1216 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit(nullptr, (yyvsp[0].node));
    }
#line 3425 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 152:
#line 1219 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeArrayLimit((yyvsp[-2].node), (yyvsp[0].node));
    }
#line 3433 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 153:
#line 1225 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = nullptr;
    }
#line 3441 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 154:
#line 1228 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3449 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 155:
#line 1234 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3457 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 156:
#line 1237 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3470 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 157:
#line 1245 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto tmp = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), tmp);
    }
#line 3479 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 158:
#line 1249 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = parser->ast()->createNodeCollectionDirection((yyvsp[-1].intval), (yyvsp[0].node));
    }
#line 3492 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 159:
#line 1260 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3501 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 160:
#line 1264 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
       auto node = static_cast<AstNode*>(parser->peekStack());
       node->addMember((yyvsp[0].node));
     }
#line 3510 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 161:
#line 1271 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      node->addMember((yyvsp[0].node));
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3520 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 162:
#line 1277 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
      node->addMember((yyvsp[-1].node));
    }
#line 3530 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 163:
#line 1281 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      auto node = static_cast<AstNode*>(parser->popStack());
      (yyval.node) = parser->ast()->createNodeCollectionList(node);
    }
#line 3539 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 164:
#line 1285 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      char const* p = (yyvsp[0].node)->getStringValue();
      size_t const len = (yyvsp[0].node)->getStringLength();
      if (len < 1 || *p == '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), p, yylloc.first_line, yylloc.first_column);
      }
      (yyval.node) = (yyvsp[0].node);
    }
#line 3553 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 165:
#line 1294 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      // graph name
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3562 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 166:
#line 1303 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 2;
    }
#line 3570 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 167:
#line 1306 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 1;
    }
#line 3578 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 168:
#line 1309 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.intval) = 0; 
    }
#line 3586 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 169:
#line 1315 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), 1);
    }
#line 3594 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 170:
#line 1318 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeDirection((yyvsp[0].intval), (yyvsp[-1].node));
    }
#line 3602 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 171:
#line 1324 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 3639 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 172:
#line 1356 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3647 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 173:
#line 1359 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3655 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 174:
#line 1362 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
      
      if ((yyval.node) == nullptr) {
        ABORT_OOM
      }
    }
#line 3667 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 175:
#line 1369 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 3682 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 176:
#line 1379 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    }
#line 3691 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 177:
#line 1382 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = std::move(parser->ast()->variables()->nextName());
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), variableName.size(), node, false);
      parser->ast()->addOperation(subQuery);

      (yyval.node) = parser->ast()->createNodeReference(variableName);
    }
#line 3706 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 178:
#line 1392 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 3726 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 179:
#line 1407 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 3745 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 180:
#line 1421 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 3764 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 181:
#line 1435 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 3792 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 182:
#line 1457 "arangod/Aql/grammar.y" /* yacc.c:1646  */
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
#line 3815 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 183:
#line 1478 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3823 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 184:
#line 1481 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3831 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 185:
#line 1487 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }
      
      (yyval.node) = (yyvsp[0].node);
    }
#line 3843 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 186:
#line 1494 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].node) == nullptr) {
        ABORT_OOM
      }

      (yyval.node) = (yyvsp[0].node);
    }
#line 3855 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 187:
#line 1504 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueString((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3863 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 188:
#line 1507 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = (yyvsp[0].node);
    }
#line 3871 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 189:
#line 1510 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueNull();
    }
#line 3879 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 190:
#line 1513 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(true);
    }
#line 3887 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 191:
#line 1516 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeValueBool(false);
    }
#line 3895 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 192:
#line 1522 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, TRI_TRANSACTION_WRITE);
    }
#line 3903 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 193:
#line 1525 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeCollection((yyvsp[0].strval).value, TRI_TRANSACTION_WRITE);
    }
#line 3911 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 194:
#line 1528 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      if ((yyvsp[0].strval).length < 2 || (yyvsp[0].strval).value[0] != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), (yyvsp[0].strval).value, yylloc.first_line, yylloc.first_column);
      }

      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3923 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 195:
#line 1538 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.node) = parser->ast()->createNodeParameter((yyvsp[0].strval).value, (yyvsp[0].strval).length);
    }
#line 3931 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 196:
#line 1544 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3939 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 197:
#line 1547 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3947 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;

  case 198:
#line 1552 "arangod/Aql/grammar.y" /* yacc.c:1646  */
    {
      (yyval.strval) = (yyvsp[0].strval);
    }
#line 3955 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
    break;


#line 3959 "arangod/Aql/grammar.cpp" /* yacc.c:1646  */
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
