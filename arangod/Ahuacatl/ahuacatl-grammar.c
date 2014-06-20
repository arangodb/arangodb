/* A Bison parser, made by GNU Bison 2.7.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2012 Free Software Foundation, Inc.
   
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
#define YYBISON_VERSION "2.7"

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
#define yylval          Ahuacatllval
#define yychar          Ahuacatlchar
#define yydebug         Ahuacatldebug
#define yynerrs         Ahuacatlnerrs
#define yylloc          Ahuacatllloc

/* Copy the first part of user declarations.  */
/* Line 371 of yacc.c  */
#line 10 "arangod/Ahuacatl/ahuacatl-grammar.y"

#include <stdio.h>
#include <stdlib.h>

#include <Basics/Common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/tri-strings.h>

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-error.h"
#include "Ahuacatl/ahuacatl-parser.h"
#include "Ahuacatl/ahuacatl-parser-functions.h"
#include "Ahuacatl/ahuacatl-scope.h"

/* Line 371 of yacc.c  */
#line 92 "arangod/Ahuacatl/ahuacatl-grammar.c"

# ifndef YY_NULL
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULL nullptr
#  else
#   define YY_NULL 0
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
   by #include "ahuacatl-grammar.h".  */
#ifndef YY_AHUACATL_ARANGOD_AHUACATL_AHUACATL_GRAMMAR_H_INCLUDED
# define YY_AHUACATL_ARANGOD_AHUACATL_AHUACATL_GRAMMAR_H_INCLUDED
/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int Ahuacatldebug;
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
     T_WITH = 269,
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
     UPLUS = 309,
     UMINUS = 310,
     FUNCCALL = 311,
     REFERENCE = 312,
     INDEXED = 313
   };
#endif


#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{
/* Line 387 of yacc.c  */
#line 26 "arangod/Ahuacatl/ahuacatl-grammar.y"

  TRI_aql_node_t* node;
  char* strval;
  bool boolval;
  int64_t intval;


/* Line 387 of yacc.c  */
#line 202 "arangod/Ahuacatl/ahuacatl-grammar.c"
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


#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int Ahuacatlparse (void *YYPARSE_PARAM);
#else
int Ahuacatlparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int Ahuacatlparse (TRI_aql_context_t* const context);
#else
int Ahuacatlparse ();
#endif
#endif /* ! YYPARSE_PARAM */

#endif /* !YY_AHUACATL_ARANGOD_AHUACATL_AHUACATL_GRAMMAR_H_INCLUDED  */

/* Copy the second part of user declarations.  */
/* Line 390 of yacc.c  */
#line 33 "arangod/Ahuacatl/ahuacatl-grammar.y"


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


/* Line 390 of yacc.c  */
#line 270 "arangod/Ahuacatl/ahuacatl-grammar.c"

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

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(N) (N)
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
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

# define YYCOPY_NEEDED 1

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
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   610

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  60
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  58
/* YYNRULES -- Number of rules.  */
#define YYNRULES  128
/* YYNRULES -- Number of states.  */
#define YYNSTATES  215

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   313

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
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
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     6,     9,    12,    15,    18,    19,    22,
      24,    26,    28,    30,    32,    34,    39,    42,    45,    47,
      51,    55,    56,    61,    63,    67,    71,    72,    75,    76,
      80,    82,    86,    89,    90,    92,    94,    97,   102,   105,
     111,   117,   123,   131,   137,   145,   149,   150,   155,   157,
     159,   161,   163,   165,   167,   171,   173,   177,   178,   184,
     187,   190,   193,   197,   201,   205,   209,   213,   217,   221,
     225,   229,   233,   237,   241,   245,   249,   255,   256,   258,
     260,   264,   266,   268,   269,   274,   275,   277,   279,   283,
     284,   287,   288,   293,   294,   296,   298,   302,   306,   308,
     309,   314,   316,   318,   322,   326,   331,   334,   337,   341,
     345,   349,   354,   356,   358,   360,   362,   364,   366,   368,
     370,   372,   374,   376,   378,   380,   382,   384,   386
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      61,     0,    -1,    62,    80,    -1,    62,    81,    -1,    62,
      82,    -1,    62,    83,    -1,    62,    84,    -1,    -1,    62,
      63,    -1,    64,    -1,    66,    -1,    65,    -1,    69,    -1,
      74,    -1,    79,    -1,     3,   116,    12,    85,    -1,     5,
      85,    -1,     4,    67,    -1,    68,    -1,    67,    47,    68,
      -1,   116,    27,    85,    -1,    -1,     7,    70,    71,    73,
      -1,    72,    -1,    71,    47,    72,    -1,   116,    27,    85,
      -1,    -1,    13,   116,    -1,    -1,     8,    75,    76,    -1,
      77,    -1,    76,    47,    77,    -1,    85,    78,    -1,    -1,
      10,    -1,    11,    -1,     9,   110,    -1,     9,   110,    47,
     110,    -1,     6,    85,    -1,    15,    85,    12,   113,   100,
      -1,    16,    85,    12,   113,   100,    -1,    17,    85,    12,
     113,   100,    -1,    17,    85,    14,    85,    12,   113,   100,
      -1,    18,    85,    12,   113,   100,    -1,    18,    85,    14,
      85,    12,   113,   100,    -1,    48,    85,    49,    -1,    -1,
      48,    86,    61,    49,    -1,    90,    -1,    91,    -1,    92,
      -1,    95,    -1,   110,    -1,   106,    -1,    85,    46,    85,
      -1,    22,    -1,    87,    45,    22,    -1,    -1,    87,    89,
      48,    93,    49,    -1,    37,    85,    -1,    38,    85,    -1,
      28,    85,    -1,    85,    30,    85,    -1,    85,    29,    85,
      -1,    85,    37,    85,    -1,    85,    38,    85,    -1,    85,
      39,    85,    -1,    85,    40,    85,    -1,    85,    41,    85,
      -1,    85,    31,    85,    -1,    85,    32,    85,    -1,    85,
      33,    85,    -1,    85,    34,    85,    -1,    85,    35,    85,
      -1,    85,    36,    85,    -1,    85,    12,    85,    -1,    85,
      43,    85,    44,    85,    -1,    -1,    94,    -1,    85,    -1,
      94,    47,    85,    -1,    96,    -1,   101,    -1,    -1,    52,
      97,    98,    53,    -1,    -1,    99,    -1,    85,    -1,    99,
      47,    85,    -1,    -1,    22,   101,    -1,    -1,    50,   102,
     103,    51,    -1,    -1,   104,    -1,   105,    -1,   104,    47,
     105,    -1,   115,    44,    85,    -1,   108,    -1,    -1,   106,
     107,    42,   109,    -1,    22,    -1,    88,    -1,   108,    59,
      22,    -1,   108,    59,   114,    -1,   108,    52,    85,    53,
      -1,    59,    22,    -1,    59,   114,    -1,    52,    85,    53,
      -1,   109,    59,    22,    -1,   109,    59,   114,    -1,   109,
      52,    85,    53,    -1,   112,    -1,   114,    -1,   117,    -1,
      25,    -1,    23,    -1,   111,    -1,    19,    -1,    20,    -1,
      21,    -1,    22,    -1,    23,    -1,    26,    -1,    26,    -1,
      22,    -1,    23,    -1,    22,    -1,    24,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   193,   193,   196,   199,   202,   205,   211,   213,   218,
     220,   222,   224,   226,   228,   233,   252,   265,   270,   272,
     277,   291,   291,   316,   318,   323,   336,   339,   345,   345,
     368,   373,   381,   392,   395,   398,   404,   415,   428,   445,
     464,   483,   499,   518,   534,   553,   556,   556,   597,   600,
     603,   606,   609,   612,   615,   646,   653,   667,   667,   696,
     704,   712,   723,   731,   739,   747,   755,   763,   771,   779,
     787,   795,   803,   811,   819,   827,   838,   850,   852,   857,
     862,   870,   873,   879,   879,   892,   894,   899,   904,   912,
     915,   930,   930,   943,   945,   950,   952,   957,   965,   969,
     969,  1027,  1044,  1051,  1059,  1067,  1078,  1088,  1098,  1108,
    1116,  1124,  1135,  1138,  1144,  1147,  1172,  1181,  1184,  1193,
    1202,  1214,  1228,  1242,  1265,  1277,  1284,  1293,  1299
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
  "\"DESC keyword\"", "\"IN keyword\"", "\"INTO keyword\"",
  "\"WITH keyword\"", "\"REMOVE command\"", "\"INSERT command\"",
  "\"UPDATE command\"", "\"REPLACE command\"", "\"null\"", "\"true\"",
  "\"false\"", "\"identifier\"", "\"quoted string\"", "\"integer number\"",
  "\"number\"", "\"bind parameter\"", "\"assignment\"", "\"not operator\"",
  "\"and operator\"", "\"or operator\"", "\"== operator\"",
  "\"!= operator\"", "\"< operator\"", "\"> operator\"", "\"<= operator\"",
  "\">= operator\"", "\"+ operator\"", "\"- operator\"", "\"* operator\"",
  "\"/ operator\"", "\"% operator\"", "\"[*] operator\"", "\"?\"", "\":\"",
  "\"::\"", "\"..\"", "\",\"", "\"(\"", "\")\"", "\"{\"", "\"}\"", "\"[\"",
  "\"]\"", "UPLUS", "UMINUS", "FUNCCALL", "REFERENCE", "INDEXED", "'.'",
  "$accept", "query", "optional_statement_block_statements",
  "statement_block_statement", "for_statement", "filter_statement",
  "let_statement", "let_list", "let_element", "collect_statement", "$@1",
  "collect_list", "collect_element", "optional_into", "sort_statement",
  "$@2", "sort_list", "sort_element", "sort_direction", "limit_statement",
  "return_statement", "remove_statement", "insert_statement",
  "update_statement", "replace_statement", "expression", "$@3",
  "function_name", "function_call", "$@4", "operator_unary",
  "operator_binary", "operator_ternary",
  "optional_function_call_arguments", "function_arguments_list",
  "compound_type", "list", "$@5", "optional_list_elements",
  "list_elements_list", "query_options", "array", "$@6",
  "optional_array_elements", "array_elements_list", "array_element",
  "reference", "$@7", "single_reference", "expansion", "atomic_value",
  "numeric_value", "value_literal", "collection_name", "bind_parameter",
  "array_element_name", "variable_name", "integer_value", YY_NULL
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
     305,   306,   307,   308,   309,   310,   311,   312,   313,    46
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    60,    61,    61,    61,    61,    61,    62,    62,    63,
      63,    63,    63,    63,    63,    64,    65,    66,    67,    67,
      68,    70,    69,    71,    71,    72,    73,    73,    75,    74,
      76,    76,    77,    78,    78,    78,    79,    79,    80,    81,
      82,    83,    83,    84,    84,    85,    86,    85,    85,    85,
      85,    85,    85,    85,    85,    87,    87,    89,    88,    90,
      90,    90,    91,    91,    91,    91,    91,    91,    91,    91,
      91,    91,    91,    91,    91,    91,    92,    93,    93,    94,
      94,    95,    95,    97,    96,    98,    98,    99,    99,   100,
     100,   102,   101,   103,   103,   104,   104,   105,   106,   107,
     106,   108,   108,   108,   108,   108,   109,   109,   109,   109,
     109,   109,   110,   110,   111,   111,   112,   112,   112,   112,
     112,   113,   113,   113,   114,   115,   115,   116,   117
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     2,     2,     2,     0,     2,     1,
       1,     1,     1,     1,     1,     4,     2,     2,     1,     3,
       3,     0,     4,     1,     3,     3,     0,     2,     0,     3,
       1,     3,     2,     0,     1,     1,     2,     4,     2,     5,
       5,     5,     7,     5,     7,     3,     0,     4,     1,     1,
       1,     1,     1,     1,     3,     1,     3,     0,     5,     2,
       2,     2,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     5,     0,     1,     1,
       3,     1,     1,     0,     4,     0,     1,     1,     3,     0,
       2,     0,     4,     0,     1,     1,     3,     3,     1,     0,
       4,     1,     1,     3,     3,     4,     2,     2,     3,     3,
       3,     4,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       7,     0,     0,     1,     0,     0,     0,     0,    21,    28,
       0,     0,     0,     0,     0,     8,     9,    11,    10,    12,
      13,    14,     2,     3,     4,     5,     6,   127,     0,    17,
      18,     0,   118,   119,   120,   101,   116,   128,   115,   124,
       0,     0,     0,    46,    91,    83,    16,    57,   102,    48,
      49,    50,    51,    81,    82,    53,    98,    52,   117,   112,
     113,   114,    38,     0,     0,    36,     0,     0,     0,     0,
       0,     0,     0,    61,    59,    60,     0,     7,    93,    85,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    26,    23,     0,    29,    30,    33,     0,     0,     0,
       0,     0,     0,     0,    15,    19,    20,    45,     0,   125,
     126,     0,    94,    95,     0,    87,     0,    86,    75,    63,
      62,    69,    70,    71,    72,    73,    74,    64,    65,    66,
      67,    68,     0,    54,    56,    77,     0,     0,   103,   104,
       0,     0,    22,     0,     0,    34,    35,    32,    37,   101,
     116,   124,    89,    89,    89,     0,    89,     0,    47,    92,
       0,     0,    84,     0,     0,    79,     0,    78,     0,     0,
     100,   105,    27,    24,    25,    31,     0,    39,    40,    41,
       0,    43,     0,    96,    97,    88,    76,    58,     0,     0,
     106,   107,     0,     0,    90,    89,    89,    80,   108,     0,
     109,   110,    42,    44,   111
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    15,    16,    17,    18,    29,    30,    19,
      63,   101,   102,   152,    20,    64,   104,   105,   157,    21,
      22,    23,    24,    25,    26,   128,    77,    47,    48,    97,
      49,    50,    51,   176,   177,    52,    53,    79,   126,   127,
     187,    54,    78,   121,   122,   123,    55,    98,    56,   180,
      57,    58,    59,   162,    60,   124,    31,    61
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -137
static const yytype_int16 yypact[] =
{
    -137,    21,   592,  -137,     9,     9,   506,   506,  -137,  -137,
     199,   506,   506,   506,   506,  -137,  -137,  -137,  -137,  -137,
    -137,  -137,  -137,  -137,  -137,  -137,  -137,  -137,    27,   -14,
    -137,    14,  -137,  -137,  -137,   -25,  -137,  -137,  -137,  -137,
     506,   506,   506,   506,  -137,  -137,   358,     6,  -137,  -137,
    -137,  -137,  -137,  -137,  -137,    15,   -40,  -137,  -137,  -137,
    -137,  -137,   358,     9,   506,    13,   388,   418,   268,   298,
     506,     9,   506,  -137,  -137,  -137,   198,  -137,    33,   506,
     506,   506,   506,   506,   506,   506,   506,   506,   506,   506,
     506,   506,   506,   506,   506,   506,    39,    17,    26,   506,
       3,    -4,  -137,    36,    24,  -137,   238,   199,   527,   527,
     527,   506,   527,   506,   358,  -137,   358,  -137,    23,  -137,
    -137,    40,    45,  -137,    30,   358,    42,    50,   148,   284,
     254,   314,   314,     7,     7,     7,     7,    62,    62,  -137,
    -137,  -137,   328,   551,  -137,   506,   -37,    88,  -137,  -137,
       9,     9,  -137,   506,   506,  -137,  -137,  -137,  -137,     4,
      10,    18,    76,    76,    76,   448,    76,   478,  -137,  -137,
      33,   506,  -137,   506,   506,   358,    55,    59,   506,    16,
     -35,  -137,  -137,  -137,   358,  -137,    58,  -137,  -137,  -137,
     527,  -137,   527,  -137,   358,   358,   551,  -137,   506,   123,
    -137,  -137,   506,    28,  -137,    76,    76,   358,  -137,   168,
    -137,  -137,  -137,  -137,  -137
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -137,    32,  -137,  -137,  -137,  -137,  -137,  -137,    41,  -137,
    -137,  -137,   -41,  -137,  -137,  -137,  -137,   -43,  -137,  -137,
    -137,  -137,  -137,  -137,  -137,    -6,  -137,  -137,  -137,  -137,
    -137,  -137,  -137,  -137,  -137,  -137,  -137,  -137,  -137,  -137,
    -136,   -73,  -137,  -137,  -137,   -55,  -137,  -137,  -137,  -137,
      -8,  -137,  -137,   -96,   -89,  -137,    -1,  -137
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -124
static const yytype_int16 yytable[] =
{
      46,    62,    65,    28,  -121,    66,    67,    68,    69,   150,
    -122,   149,    99,   163,   164,   178,   166,   202,  -123,   100,
     -55,     3,   179,   -55,   203,   148,  -121,   188,   189,    39,
     191,    27,  -122,    71,    73,    74,    75,    76,   200,    70,
    -123,    72,    39,   151,    89,    90,    91,    92,    93,   -55,
     210,    96,   -55,  -121,    39,   119,   120,   -99,   106,  -122,
     107,   144,   103,   153,   114,   145,   116,  -123,   146,   212,
     213,   154,   168,   125,   171,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     201,   169,   170,   147,   205,   172,   206,   173,   186,   158,
      80,    91,    92,    93,   197,   165,   198,   167,    44,   118,
     183,   185,   115,   204,   211,   193,     0,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
       0,    94,     0,     0,    95,    80,     0,     0,     0,   175,
       0,   181,     0,     0,     0,     0,     0,   184,   106,   182,
     103,     0,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,   194,    94,   195,   196,    95,
       0,     0,   199,     0,     0,     0,   208,     0,     0,     0,
      80,    85,    86,    87,    88,    89,    90,    91,    92,    93,
       0,     0,   207,     0,     0,     0,   209,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      80,    94,     0,     0,    95,     0,     0,     0,    32,    33,
      34,   214,    36,    37,    38,    39,     0,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
       0,    94,     0,     0,    95,     0,     0,   117,   155,   156,
      80,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
     110,    94,   111,    81,    95,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
     112,    94,   113,     0,    95,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      80,    94,     0,     0,    95,     0,     0,    85,    86,    87,
      88,    89,    90,    91,    92,    93,     0,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      80,    94,   174,     0,    95,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
     108,    94,     0,     0,    95,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
     109,    94,     0,     0,    95,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
     190,    94,     0,     0,    95,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
     192,    94,     0,     0,    95,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
       0,    94,     0,     0,    95,    32,    33,    34,    35,    36,
      37,    38,    39,     0,    40,     0,     0,     0,     0,     0,
       0,     0,     0,    41,    42,     0,    32,    33,    34,   159,
     160,    37,    38,   161,    43,    40,    44,     0,    45,     0,
       0,     0,     0,    80,    41,    42,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    43,     0,    44,     0,    45,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,     0,    94,     4,     5,     6,     7,     8,
       9,    10,     0,     0,     0,     0,     0,    11,    12,    13,
      14
};

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-137)))

#define yytable_value_is_error(Yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
       6,     7,    10,     4,     0,    11,    12,    13,    14,    13,
       0,   100,    52,   109,   110,    52,   112,    52,     0,    59,
      45,     0,    59,    48,    59,    22,    22,   163,   164,    26,
     166,    22,    22,    47,    40,    41,    42,    43,    22,    12,
      22,    27,    26,    47,    37,    38,    39,    40,    41,    45,
      22,    45,    48,    49,    26,    22,    23,    42,    64,    49,
      47,    22,    63,    27,    70,    48,    72,    49,    42,   205,
     206,    47,    49,    79,    44,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
     179,    51,    47,    99,   190,    53,   192,    47,    22,   107,
      12,    39,    40,    41,    49,   111,    47,   113,    50,    77,
     151,   154,    71,   186,   203,   170,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      -1,    43,    -1,    -1,    46,    12,    -1,    -1,    -1,   145,
      -1,    53,    -1,    -1,    -1,    -1,    -1,   153,   154,   150,
     151,    -1,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,   171,    43,   173,   174,    46,
      -1,    -1,   178,    -1,    -1,    -1,    53,    -1,    -1,    -1,
      12,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      -1,    -1,   198,    -1,    -1,    -1,   202,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    43,    -1,    -1,    46,    -1,    -1,    -1,    19,    20,
      21,    53,    23,    24,    25,    26,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      -1,    43,    -1,    -1,    46,    -1,    -1,    49,    10,    11,
      12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    12,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    43,    14,    29,    46,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    12,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    43,    14,    -1,    46,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    12,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    43,    -1,    -1,    46,    -1,    -1,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    43,    44,    -1,    46,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    43,    -1,    -1,    46,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    43,    -1,    -1,    46,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    43,    -1,    -1,    46,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      12,    43,    -1,    -1,    46,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      -1,    43,    -1,    -1,    46,    19,    20,    21,    22,    23,
      24,    25,    26,    -1,    28,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    48,    28,    50,    -1,    52,    -1,
      -1,    -1,    -1,    12,    37,    38,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    48,    -1,    50,    -1,    52,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    -1,    43,     3,     4,     5,     6,     7,
       8,     9,    -1,    -1,    -1,    -1,    -1,    15,    16,    17,
      18
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    61,    62,     0,     3,     4,     5,     6,     7,     8,
       9,    15,    16,    17,    18,    63,    64,    65,    66,    69,
      74,    79,    80,    81,    82,    83,    84,    22,   116,    67,
      68,   116,    19,    20,    21,    22,    23,    24,    25,    26,
      28,    37,    38,    48,    50,    52,    85,    87,    88,    90,
      91,    92,    95,    96,   101,   106,   108,   110,   111,   112,
     114,   117,    85,    70,    75,   110,    85,    85,    85,    85,
      12,    47,    27,    85,    85,    85,    85,    86,   102,    97,
      12,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    43,    46,    45,    89,   107,    52,
      59,    71,    72,   116,    76,    77,    85,    47,    12,    12,
      12,    14,    12,    14,    85,    68,    85,    49,    61,    22,
      23,   103,   104,   105,   115,    85,    98,    99,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    22,    48,    42,    85,    22,   114,
      13,    47,    73,    27,    47,    10,    11,    78,   110,    22,
      23,    26,   113,   113,   113,    85,   113,    85,    49,    51,
      47,    44,    53,    47,    44,    85,    93,    94,    52,    59,
     109,    53,   116,    72,    85,    77,    22,   100,   100,   100,
      12,   100,    12,   105,    85,    85,    85,    49,    47,    85,
      22,   114,    52,    59,   101,   113,   113,    85,    53,    85,
      22,   114,   100,   100,    53
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
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

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
      YYERROR;							\
    }								\
while (YYID (0))

/* Error token number */
#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (YYID (N))                                                     \
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
    while (YYID (0))
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

__attribute__((__unused__))
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static unsigned
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
#else
static unsigned
yy_location_print_ (yyo, yylocp)
    FILE *yyo;
    YYLTYPE const * const yylocp;
#endif
{
  unsigned res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += fprintf (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += fprintf (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += fprintf (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += fprintf (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += fprintf (yyo, "-%d", end_col);
    }
  return res;
 }

#  define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

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
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, TRI_aql_context_t* const context)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    TRI_aql_context_t* const context;
#endif
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, TRI_aql_context_t* const context)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    TRI_aql_context_t* const context;
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
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, TRI_aql_context_t* const context)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule, context)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
    TRI_aql_context_t* const context;
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
  YYSIZE_T yysize0 = yytnamerr (YY_NULL, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULL;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
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
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULL, yytname[yyx]);
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

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, TRI_aql_context_t* const context)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp, context)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
    TRI_aql_context_t* const context;
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




/*----------.
| yyparse.  |
`----------*/

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
yyparse (TRI_aql_context_t* const context)
#else
int
yyparse (context)
    TRI_aql_context_t* const context;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;


#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
static YYSTYPE yyval_default;
# define YY_INITIAL_VALUE(Value) = Value
#endif
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval YY_INITIAL_VALUE(yyval_default);

/* Location data for the lookahead symbol.  */
YYLTYPE yylloc = yyloc_default;


    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.
       `yyls': related to locations.

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
/* Line 1792 of yacc.c  */
#line 193 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      context->_type = TRI_AQL_QUERY_READ;
    }
    break;

  case 3:
/* Line 1792 of yacc.c  */
#line 196 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      context->_type = TRI_AQL_QUERY_REMOVE;
    }
    break;

  case 4:
/* Line 1792 of yacc.c  */
#line 199 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      context->_type = TRI_AQL_QUERY_INSERT;
    }
    break;

  case 5:
/* Line 1792 of yacc.c  */
#line 202 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      context->_type = TRI_AQL_QUERY_UPDATE;
    }
    break;

  case 6:
/* Line 1792 of yacc.c  */
#line 205 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      context->_type = TRI_AQL_QUERY_REPLACE;
    }
    break;

  case 7:
/* Line 1792 of yacc.c  */
#line 211 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 8:
/* Line 1792 of yacc.c  */
#line 213 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 9:
/* Line 1792 of yacc.c  */
#line 218 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 10:
/* Line 1792 of yacc.c  */
#line 220 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 11:
/* Line 1792 of yacc.c  */
#line 222 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 12:
/* Line 1792 of yacc.c  */
#line 224 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 13:
/* Line 1792 of yacc.c  */
#line 226 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 14:
/* Line 1792 of yacc.c  */
#line 228 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 15:
/* Line 1792 of yacc.c  */
#line 233 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;
      
      if (! TRI_StartScopeAql(context, TRI_AQL_SCOPE_FOR)) {
        ABORT_OOM
      }
      
      node = TRI_CreateNodeForAql(context, (yyvsp[(2) - (4)].strval), (yyvsp[(4) - (4)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 16:
/* Line 1792 of yacc.c  */
#line 252 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeFilterAql(context, (yyvsp[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 17:
/* Line 1792 of yacc.c  */
#line 265 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 18:
/* Line 1792 of yacc.c  */
#line 270 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 19:
/* Line 1792 of yacc.c  */
#line 272 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 20:
/* Line 1792 of yacc.c  */
#line 277 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeLetAql(context, (yyvsp[(1) - (3)].strval), (yyvsp[(3) - (3)].node));

      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 21:
/* Line 1792 of yacc.c  */
#line 291 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 22:
/* Line 1792 of yacc.c  */
#line 299 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeCollectAql(
                context, 
                static_cast<const TRI_aql_node_t* const>
                           (TRI_PopStackParseAql(context)), 
                (yyvsp[(4) - (4)].strval));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 23:
/* Line 1792 of yacc.c  */
#line 316 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 24:
/* Line 1792 of yacc.c  */
#line 318 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 25:
/* Line 1792 of yacc.c  */
#line 323 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeAssignAql(context, (yyvsp[(1) - (3)].strval), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      if (! TRI_PushListAql(context, node)) {
        ABORT_OOM
      }
    }
    break;

  case 26:
/* Line 1792 of yacc.c  */
#line 336 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.strval) = NULL;
    }
    break;

  case 27:
/* Line 1792 of yacc.c  */
#line 339 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.strval) = (yyvsp[(2) - (2)].strval);
    }
    break;

  case 28:
/* Line 1792 of yacc.c  */
#line 345 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 29:
/* Line 1792 of yacc.c  */
#line 353 "arangod/Ahuacatl/ahuacatl-grammar.y"
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
    break;

  case 30:
/* Line 1792 of yacc.c  */
#line 368 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if (! TRI_PushListAql(context, (yyvsp[(1) - (1)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 31:
/* Line 1792 of yacc.c  */
#line 373 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if (! TRI_PushListAql(context, (yyvsp[(3) - (3)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 32:
/* Line 1792 of yacc.c  */
#line 381 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeSortElementAql(context, (yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].boolval));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 33:
/* Line 1792 of yacc.c  */
#line 392 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.boolval) = true;
    }
    break;

  case 34:
/* Line 1792 of yacc.c  */
#line 395 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.boolval) = true;
    }
    break;

  case 35:
/* Line 1792 of yacc.c  */
#line 398 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.boolval) = false;
    }
    break;

  case 36:
/* Line 1792 of yacc.c  */
#line 404 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, TRI_CreateNodeValueIntAql(context, 0), (yyvsp[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
    }
    break;

  case 37:
/* Line 1792 of yacc.c  */
#line 415 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, (yyvsp[(2) - (4)].node), (yyvsp[(4) - (4)].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 38:
/* Line 1792 of yacc.c  */
#line 428 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeReturnAql(context, (yyvsp[(2) - (2)].node));
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
    break;

  case 39:
/* Line 1792 of yacc.c  */
#line 445 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeRemoveAql(context, (yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
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
    break;

  case 40:
/* Line 1792 of yacc.c  */
#line 464 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeInsertAql(context, (yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
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
    break;

  case 41:
/* Line 1792 of yacc.c  */
#line 483 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeUpdateAql(context, NULL, (yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
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
    break;

  case 42:
/* Line 1792 of yacc.c  */
#line 499 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeUpdateAql(context, (yyvsp[(2) - (7)].node), (yyvsp[(4) - (7)].node), (yyvsp[(6) - (7)].node), (yyvsp[(7) - (7)].node));
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
    break;

  case 43:
/* Line 1792 of yacc.c  */
#line 518 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeReplaceAql(context, NULL, (yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
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
    break;

  case 44:
/* Line 1792 of yacc.c  */
#line 534 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      node = TRI_CreateNodeReplaceAql(context, (yyvsp[(2) - (7)].node), (yyvsp[(4) - (7)].node), (yyvsp[(6) - (7)].node), (yyvsp[(7) - (7)].node));
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
    break;

  case 45:
/* Line 1792 of yacc.c  */
#line 553 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(2) - (3)].node);
    }
    break;

  case 46:
/* Line 1792 of yacc.c  */
#line 556 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if (! TRI_StartScopeAql(context, TRI_AQL_SCOPE_SUBQUERY)) {
        ABORT_OOM
      }

      context->_subQueries++;

    }
    break;

  case 47:
/* Line 1792 of yacc.c  */
#line 563 "arangod/Ahuacatl/ahuacatl-grammar.y"
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
    break;

  case 48:
/* Line 1792 of yacc.c  */
#line 597 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 49:
/* Line 1792 of yacc.c  */
#line 600 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 50:
/* Line 1792 of yacc.c  */
#line 603 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 51:
/* Line 1792 of yacc.c  */
#line 606 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 52:
/* Line 1792 of yacc.c  */
#line 609 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 53:
/* Line 1792 of yacc.c  */
#line 612 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 54:
/* Line 1792 of yacc.c  */
#line 615 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;
      TRI_aql_node_t* list;

      if ((yyvsp[(1) - (3)].node) == NULL || (yyvsp[(3) - (3)].node) == NULL) {
        ABORT_OOM
      }
      
      list = TRI_CreateNodeListAql(context);
      if (list == NULL) {
        ABORT_OOM
      }
       
      if (TRI_ERROR_NO_ERROR != TRI_PushBackVectorPointer(&list->_members, (void*) (yyvsp[(1) - (3)].node))) {
        ABORT_OOM
      }
      if (TRI_ERROR_NO_ERROR != TRI_PushBackVectorPointer(&list->_members, (void*) (yyvsp[(3) - (3)].node))) {
        ABORT_OOM
      }
      
      node = TRI_CreateNodeFcallAql(context, "RANGE", list);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 55:
/* Line 1792 of yacc.c  */
#line 646 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.strval) = (yyvsp[(1) - (1)].strval);

      if ((yyval.strval) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 56:
/* Line 1792 of yacc.c  */
#line 653 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if ((yyvsp[(1) - (3)].strval) == NULL || (yyvsp[(3) - (3)].strval) == NULL) {
        ABORT_OOM
      }

      (yyval.strval) = TRI_RegisterString3Aql(context, (yyvsp[(1) - (3)].strval), "::", (yyvsp[(3) - (3)].strval));

      if ((yyval.strval) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 57:
/* Line 1792 of yacc.c  */
#line 667 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      if (! TRI_PushStackParseAql(context, (yyvsp[(1) - (1)].strval))) {
        ABORT_OOM
      }

      node = TRI_CreateNodeListAql(context);
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 58:
/* Line 1792 of yacc.c  */
#line 680 "arangod/Ahuacatl/ahuacatl-grammar.y"
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
    break;

  case 59:
/* Line 1792 of yacc.c  */
#line 696 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryPlusAql(context, (yyvsp[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 60:
/* Line 1792 of yacc.c  */
#line 704 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryMinusAql(context, (yyvsp[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 61:
/* Line 1792 of yacc.c  */
#line 712 "arangod/Ahuacatl/ahuacatl-grammar.y"
    { 
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryNotAql(context, (yyvsp[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 62:
/* Line 1792 of yacc.c  */
#line 723 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryOrAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 63:
/* Line 1792 of yacc.c  */
#line 731 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryAndAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 64:
/* Line 1792 of yacc.c  */
#line 739 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryPlusAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 65:
/* Line 1792 of yacc.c  */
#line 747 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryMinusAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 66:
/* Line 1792 of yacc.c  */
#line 755 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryTimesAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 67:
/* Line 1792 of yacc.c  */
#line 763 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryDivAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 68:
/* Line 1792 of yacc.c  */
#line 771 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryModAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 69:
/* Line 1792 of yacc.c  */
#line 779 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryEqAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 70:
/* Line 1792 of yacc.c  */
#line 787 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryNeAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 71:
/* Line 1792 of yacc.c  */
#line 795 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLtAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 72:
/* Line 1792 of yacc.c  */
#line 803 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGtAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 73:
/* Line 1792 of yacc.c  */
#line 811 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLeAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 74:
/* Line 1792 of yacc.c  */
#line 819 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGeAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 75:
/* Line 1792 of yacc.c  */
#line 827 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryInAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 76:
/* Line 1792 of yacc.c  */
#line 838 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorTernaryAql(context, (yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].node), (yyvsp[(5) - (5)].node));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 77:
/* Line 1792 of yacc.c  */
#line 850 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 78:
/* Line 1792 of yacc.c  */
#line 852 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 79:
/* Line 1792 of yacc.c  */
#line 857 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if (! TRI_PushListAql(context, (yyvsp[(1) - (1)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 80:
/* Line 1792 of yacc.c  */
#line 862 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if (! TRI_PushListAql(context, (yyvsp[(3) - (3)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 81:
/* Line 1792 of yacc.c  */
#line 870 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 82:
/* Line 1792 of yacc.c  */
#line 873 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 83:
/* Line 1792 of yacc.c  */
#line 879 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 84:
/* Line 1792 of yacc.c  */
#line 886 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));
    }
    break;

  case 85:
/* Line 1792 of yacc.c  */
#line 892 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 86:
/* Line 1792 of yacc.c  */
#line 894 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 87:
/* Line 1792 of yacc.c  */
#line 899 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if (! TRI_PushListAql(context, (yyvsp[(1) - (1)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 88:
/* Line 1792 of yacc.c  */
#line 904 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if (! TRI_PushListAql(context, (yyvsp[(3) - (3)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 89:
/* Line 1792 of yacc.c  */
#line 912 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = NULL;
    }
    break;

  case 90:
/* Line 1792 of yacc.c  */
#line 915 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if ((yyvsp[(1) - (2)].strval) == NULL || (yyvsp[(2) - (2)].node) == NULL) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString((yyvsp[(1) - (2)].strval), "OPTIONS")) {
        TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
        YYABORT;
      }

      (yyval.node) = (yyvsp[(2) - (2)].node);
    }
    break;

  case 91:
/* Line 1792 of yacc.c  */
#line 930 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeArrayAql(context);
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 92:
/* Line 1792 of yacc.c  */
#line 937 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));
    }
    break;

  case 93:
/* Line 1792 of yacc.c  */
#line 943 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 94:
/* Line 1792 of yacc.c  */
#line 945 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 95:
/* Line 1792 of yacc.c  */
#line 950 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 96:
/* Line 1792 of yacc.c  */
#line 952 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
    }
    break;

  case 97:
/* Line 1792 of yacc.c  */
#line 957 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if (! TRI_PushArrayAql(context, (yyvsp[(1) - (3)].strval), (yyvsp[(3) - (3)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 98:
/* Line 1792 of yacc.c  */
#line 965 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // start of reference (collection or variable name)
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 99:
/* Line 1792 of yacc.c  */
#line 969 "arangod/Ahuacatl/ahuacatl-grammar.y"
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
      TRI_PushStackParseAql(context, (yyvsp[(1) - (1)].node));

      // create a temporary variable for the row iterator (will be popped by "expansion" rule")
      node = TRI_CreateNodeReferenceAql(context, varname);

      if (node == NULL) {
        ABORT_OOM
      }

      // push the variable
      TRI_PushStackParseAql(context, node);
    }
    break;

  case 100:
/* Line 1792 of yacc.c  */
#line 993 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // return from the "expansion" subrule
      TRI_aql_node_t* expanded = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));
      TRI_aql_node_t* expand;
      TRI_aql_node_t* nameNode;
      char* varname = static_cast<char*>(TRI_PopStackParseAql(context));

      // push the actual expand node into the statement list
      expand = TRI_CreateNodeExpandAql(context, varname, expanded, (yyvsp[(4) - (4)].node));

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
    break;

  case 101:
/* Line 1792 of yacc.c  */
#line 1027 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // variable or collection
      TRI_aql_node_t* node;
      
      if (TRI_VariableExistsScopeAql(context, (yyvsp[(1) - (1)].strval))) {
        node = TRI_CreateNodeReferenceAql(context, (yyvsp[(1) - (1)].strval));
      }
      else {
        node = TRI_CreateNodeCollectionAql(context, (yyvsp[(1) - (1)].strval));
      }

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 102:
/* Line 1792 of yacc.c  */
#line 1044 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 103:
/* Line 1792 of yacc.c  */
#line 1051 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // named variable access, e.g. variable.reference
      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].strval));
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 104:
/* Line 1792 of yacc.c  */
#line 1059 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // named variable access, e.g. variable.@reference
      (yyval.node) = TRI_CreateNodeBoundAttributeAccessAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 105:
/* Line 1792 of yacc.c  */
#line 1067 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // indexed variable access, e.g. variable[index]
      (yyval.node) = TRI_CreateNodeIndexedAql(context, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 106:
/* Line 1792 of yacc.c  */
#line 1078 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // named variable access, continuation from * expansion, e.g. [*].variable.reference
      TRI_aql_node_t* node = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));

      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, node, (yyvsp[(2) - (2)].strval));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 107:
/* Line 1792 of yacc.c  */
#line 1088 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.@reference
      TRI_aql_node_t* node = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));

      (yyval.node) = TRI_CreateNodeBoundAttributeAccessAql(context, node, (yyvsp[(2) - (2)].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 108:
/* Line 1792 of yacc.c  */
#line 1098 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable[index]
      TRI_aql_node_t* node = static_cast<TRI_aql_node_t*>(TRI_PopStackParseAql(context));

      (yyval.node) = TRI_CreateNodeIndexedAql(context, node, (yyvsp[(2) - (3)].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 109:
/* Line 1792 of yacc.c  */
#line 1108 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // named variable access, continuation from * expansion, e.g. [*].variable.xx.reference
      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].strval));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 110:
/* Line 1792 of yacc.c  */
#line 1116 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.xx.@reference
      (yyval.node) = TRI_CreateNodeBoundAttributeAccessAql(context, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 111:
/* Line 1792 of yacc.c  */
#line 1124 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable.xx.[index]
      (yyval.node) = TRI_CreateNodeIndexedAql(context, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 112:
/* Line 1792 of yacc.c  */
#line 1135 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 113:
/* Line 1792 of yacc.c  */
#line 1138 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 114:
/* Line 1792 of yacc.c  */
#line 1144 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 115:
/* Line 1792 of yacc.c  */
#line 1147 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;
      double value;

      if ((yyvsp[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }
      
      value = TRI_DoubleString((yyvsp[(1) - (1)].strval));

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
    break;

  case 116:
/* Line 1792 of yacc.c  */
#line 1172 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueStringAql(context, (yyvsp[(1) - (1)].strval));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 117:
/* Line 1792 of yacc.c  */
#line 1181 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.node) = (yyvsp[(1) - (1)].node);
    }
    break;

  case 118:
/* Line 1792 of yacc.c  */
#line 1184 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueNullAql(context);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 119:
/* Line 1792 of yacc.c  */
#line 1193 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, true);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 120:
/* Line 1792 of yacc.c  */
#line 1202 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, false);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 121:
/* Line 1792 of yacc.c  */
#line 1214 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      if ((yyvsp[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }

      node = TRI_CreateNodeCollectionAql(context, (yyvsp[(1) - (1)].strval));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 122:
/* Line 1792 of yacc.c  */
#line 1228 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      if ((yyvsp[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }

      node = TRI_CreateNodeCollectionAql(context, (yyvsp[(1) - (1)].strval));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 123:
/* Line 1792 of yacc.c  */
#line 1242 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;

      if ((yyvsp[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }
      
      if (strlen((yyvsp[(1) - (1)].strval)) < 2 || (yyvsp[(1) - (1)].strval)[0] != '@') {
        TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, (yyvsp[(1) - (1)].strval));
        YYABORT;
      }

      node = TRI_CreateNodeParameterAql(context, (yyvsp[(1) - (1)].strval));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 124:
/* Line 1792 of yacc.c  */
#line 1265 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node = TRI_CreateNodeParameterAql(context, (yyvsp[(1) - (1)].strval));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 125:
/* Line 1792 of yacc.c  */
#line 1277 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if ((yyvsp[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }

      (yyval.strval) = (yyvsp[(1) - (1)].strval);
    }
    break;

  case 126:
/* Line 1792 of yacc.c  */
#line 1284 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      if ((yyvsp[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }

      (yyval.strval) = (yyvsp[(1) - (1)].strval);
    }
    break;

  case 127:
/* Line 1792 of yacc.c  */
#line 1293 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      (yyval.strval) = (yyvsp[(1) - (1)].strval);
    }
    break;

  case 128:
/* Line 1792 of yacc.c  */
#line 1299 "arangod/Ahuacatl/ahuacatl-grammar.y"
    {
      TRI_aql_node_t* node;
      int64_t value;

      if ((yyvsp[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }

      value = TRI_Int64String((yyvsp[(1) - (1)].strval));
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
    break;


/* Line 1792 of yacc.c  */
#line 3519 "arangod/Ahuacatl/ahuacatl-grammar.c"
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


