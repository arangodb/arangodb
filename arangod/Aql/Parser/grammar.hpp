/* clang-format off */
/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_AQL_GRAMMAR_HPP_INCLUDED
# define YY_AQL_GRAMMAR_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int Aqldebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    T_END = 0,                     /* "end of query string"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    T_FOR = 258,                   /* "FOR declaration"  */
    T_LET = 259,                   /* "LET declaration"  */
    T_FILTER = 260,                /* "FILTER declaration"  */
    T_RETURN = 261,                /* "RETURN declaration"  */
    T_COLLECT = 262,               /* "COLLECT declaration"  */
    T_SORT = 263,                  /* "SORT declaration"  */
    T_LIMIT = 264,                 /* "LIMIT declaration"  */
    T_WINDOW = 265,                /* "WINDOW declaration"  */
    T_MATCH = 266,                 /* "MATCH declaration"  */
    T_WHILE = 267,                 /* "WHILE declaration"  */
    T_UNTIL = 268,                 /* "UNTIL declaration"  */
    T_SCAN = 269,                  /* "SCAN declaration"  */
    T_FOLD = 270,                  /* "FOLD declaration"  */
    T_INNER_JOIN = 271,            /* "INNER_JOIN declaration"  */
    T_LEFT_JOIN = 272,             /* "LEFT_JOIN declaration"  */
    T_RIGHT_JOIN = 273,            /* "RIGHT_JOIN declaration"  */
    T_OUTER_JOIN = 274,            /* "OUTER_JOIN declaration"  */
    T_ASC = 275,                   /* "ASC keyword"  */
    T_DESC = 276,                  /* "DESC keyword"  */
    T_IN = 277,                    /* "IN keyword"  */
    T_WITH = 278,                  /* "WITH keyword"  */
    T_INTO = 279,                  /* "INTO keyword"  */
    T_AGGREGATE = 280,             /* "AGGREGATE keyword"  */
    T_GRAPH = 281,                 /* "GRAPH keyword"  */
    T_SHORTEST_PATH = 282,         /* "SHORTEST_PATH keyword"  */
    T_K_SHORTEST_PATHS = 283,      /* "K_SHORTEST_PATHS keyword"  */
    T_K_PATHS = 284,               /* "K_PATHS keyword"  */
    T_ALL_SHORTEST_PATHS = 285,    /* "ALL_SHORTEST_PATHS keyword"  */
    T_DISTINCT = 286,              /* "DISTINCT modifier"  */
    T_REMOVE = 287,                /* "REMOVE command"  */
    T_INSERT = 288,                /* "INSERT command"  */
    T_UPDATE = 289,                /* "UPDATE command"  */
    T_REPLACE = 290,               /* "REPLACE command"  */
    T_UPSERT = 291,                /* "UPSERT command"  */
    T_NULL = 292,                  /* "null"  */
    T_TRUE = 293,                  /* "true"  */
    T_FALSE = 294,                 /* "false"  */
    T_STRING = 295,                /* "identifier"  */
    T_QUOTED_STRING = 296,         /* "quoted string"  */
    T_INTEGER = 297,               /* "integer number"  */
    T_DOUBLE = 298,                /* "number"  */
    T_PARAMETER = 299,             /* "bind parameter"  */
    T_DATA_SOURCE_PARAMETER = 300, /* "bind data source parameter"  */
    T_ASSIGN = 301,                /* "assignment"  */
    T_NOT = 302,                   /* "not operator"  */
    T_AND = 303,                   /* "and operator"  */
    T_OR = 304,                    /* "or operator"  */
    T_NOT_IN = 305,                /* "not in operator"  */
    T_REGEX_MATCH = 306,           /* "~= operator"  */
    T_REGEX_NON_MATCH = 307,       /* "~! operator"  */
    T_EQ = 308,                    /* "== operator"  */
    T_NE = 309,                    /* "!= operator"  */
    T_LT = 310,                    /* "< operator"  */
    T_GT = 311,                    /* "> operator"  */
    T_LE = 312,                    /* "<= operator"  */
    T_GE = 313,                    /* ">= operator"  */
    T_LIKE = 314,                  /* "like operator"  */
    T_PLUS = 315,                  /* "+ operator"  */
    T_MINUS = 316,                 /* "- operator"  */
    T_TIMES = 317,                 /* "* operator"  */
    T_DIV = 318,                   /* "/ operator"  */
    T_MOD = 319,                   /* "% operator"  */
    T_QUESTION = 320,              /* "?"  */
    T_COLON = 321,                 /* ":"  */
    T_SCOPE = 322,                 /* "::"  */
    T_RANGE = 323,                 /* ".."  */
    T_ELLIPSIS = 324,              /* "..."  */
    T_COMMA = 325,                 /* ","  */
    T_OPEN = 326,                  /* "("  */
    T_CLOSE = 327,                 /* ")"  */
    T_OBJECT_OPEN = 328,           /* "{"  */
    T_OBJECT_CLOSE = 329,          /* "}"  */
    T_ARRAY_OPEN = 330,            /* "["  */
    T_ARRAY_CLOSE = 331,           /* "]"  */
    T_RELATION_OPEN = 332,         /* "-["  */
    T_RELATION_IN_OPEN = 333,      /* "<-["  */
    T_RELATION_CLOSE = 334,        /* "]-"  */
    T_RELATION_OUT_CLOSE = 335,    /* "]->"  */
    T_OUTBOUND = 336,              /* "outbound modifier"  */
    T_INBOUND = 337,               /* "inbound modifier"  */
    T_ANY = 338,                   /* "any modifier"  */
    T_ALL = 339,                   /* "all modifier"  */
    T_NONE = 340,                  /* "none modifier"  */
    T_AT_LEAST = 341,              /* "at least modifier"  */
    UMINUS = 342,                  /* UMINUS  */
    UPLUS = 343,                   /* UPLUS  */
    UNEGATION = 344,               /* UNEGATION  */
    FUNCCALL = 345,                /* FUNCCALL  */
    REFERENCE = 346,               /* REFERENCE  */
    INDEXED = 347,                 /* INDEXED  */
    EXPANSION = 348                /* EXPANSION  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define T_END 0
#define YYerror 256
#define YYUNDEF 257
#define T_FOR 258
#define T_LET 259
#define T_FILTER 260
#define T_RETURN 261
#define T_COLLECT 262
#define T_SORT 263
#define T_LIMIT 264
#define T_WINDOW 265
#define T_ASC 266
#define T_DESC 267
#define T_IN 268
#define T_WITH 269
#define T_INTO 270
#define T_AGGREGATE 271
#define T_GRAPH 272
#define T_SHORTEST_PATH 273
#define T_K_SHORTEST_PATHS 274
#define T_K_PATHS 275
#define T_ALL_SHORTEST_PATHS 276
#define T_DISTINCT 277
#define T_REMOVE 278
#define T_INSERT 279
#define T_UPDATE 280
#define T_REPLACE 281
#define T_UPSERT 282
#define T_NULL 283
#define T_TRUE 284
#define T_FALSE 285
#define T_STRING 286
#define T_QUOTED_STRING 287
#define T_INTEGER 288
#define T_DOUBLE 289
#define T_PARAMETER 290
#define T_DATA_SOURCE_PARAMETER 291
#define T_ASSIGN 292
#define T_NOT 293
#define T_AND 294
#define T_OR 295
#define T_NOT_IN 296
#define T_REGEX_MATCH 297
#define T_REGEX_NON_MATCH 298
#define T_EQ 299
#define T_NE 300
#define T_LT 301
#define T_GT 302
#define T_LE 303
#define T_GE 304
#define T_LIKE 305
#define T_PLUS 306
#define T_MINUS 307
#define T_TIMES 308
#define T_DIV 309
#define T_MOD 310
#define T_QUESTION 311
#define T_COLON 312
#define T_SCOPE 313
#define T_RANGE 314
#define T_COMMA 315
#define T_OPEN 316
#define T_CLOSE 317
#define T_OBJECT_OPEN 318
#define T_OBJECT_CLOSE 319
#define T_ARRAY_OPEN 320
#define T_ARRAY_CLOSE 321
#define T_OUTBOUND 322
#define T_INBOUND 323
#define T_ANY 324
#define T_ALL 325
#define T_NONE 326
#define T_AT_LEAST 327
#define UMINUS 328
#define UPLUS 329
#define UNEGATION 330
#define FUNCCALL 331
#define REFERENCE 332
#define INDEXED 333
#define EXPANSION 334

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 47 "grammar.y"

  arangodb::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 235 "grammar.hpp"

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


#endif /* !YY_AQL_GRAMMAR_HPP_INCLUDED  */
