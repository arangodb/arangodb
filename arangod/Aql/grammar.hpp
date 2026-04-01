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
    T_ASC = 271,                   /* "ASC keyword"  */
    T_DESC = 272,                  /* "DESC keyword"  */
    T_IN = 273,                    /* "IN keyword"  */
    T_WITH = 274,                  /* "WITH keyword"  */
    T_INTO = 275,                  /* "INTO keyword"  */
    T_AGGREGATE = 276,             /* "AGGREGATE keyword"  */
    T_GRAPH = 277,                 /* "GRAPH keyword"  */
    T_SHORTEST_PATH = 278,         /* "SHORTEST_PATH keyword"  */
    T_K_SHORTEST_PATHS = 279,      /* "K_SHORTEST_PATHS keyword"  */
    T_K_PATHS = 280,               /* "K_PATHS keyword"  */
    T_ALL_SHORTEST_PATHS = 281,    /* "ALL_SHORTEST_PATHS keyword"  */
    T_DISTINCT = 282,              /* "DISTINCT modifier"  */
    T_REMOVE = 283,                /* "REMOVE command"  */
    T_INSERT = 284,                /* "INSERT command"  */
    T_UPDATE = 285,                /* "UPDATE command"  */
    T_REPLACE = 286,               /* "REPLACE command"  */
    T_UPSERT = 287,                /* "UPSERT command"  */
    T_NULL = 288,                  /* "null"  */
    T_TRUE = 289,                  /* "true"  */
    T_FALSE = 290,                 /* "false"  */
    T_STRING = 291,                /* "identifier"  */
    T_QUOTED_STRING = 292,         /* "quoted string"  */
    T_INTEGER = 293,               /* "integer number"  */
    T_DOUBLE = 294,                /* "number"  */
    T_PARAMETER = 295,             /* "bind parameter"  */
    T_DATA_SOURCE_PARAMETER = 296, /* "bind data source parameter"  */
    T_ASSIGN = 297,                /* "assignment"  */
    T_NOT = 298,                   /* "not operator"  */
    T_AND = 299,                   /* "and operator"  */
    T_OR = 300,                    /* "or operator"  */
    T_NOT_IN = 301,                /* "not in operator"  */
    T_REGEX_MATCH = 302,           /* "~= operator"  */
    T_REGEX_NON_MATCH = 303,       /* "~! operator"  */
    T_EQ = 304,                    /* "== operator"  */
    T_NE = 305,                    /* "!= operator"  */
    T_LT = 306,                    /* "< operator"  */
    T_GT = 307,                    /* "> operator"  */
    T_LE = 308,                    /* "<= operator"  */
    T_GE = 309,                    /* ">= operator"  */
    T_LIKE = 310,                  /* "like operator"  */
    T_PLUS = 311,                  /* "+ operator"  */
    T_MINUS = 312,                 /* "- operator"  */
    T_TIMES = 313,                 /* "* operator"  */
    T_DIV = 314,                   /* "/ operator"  */
    T_MOD = 315,                   /* "% operator"  */
    T_QUESTION = 316,              /* "?"  */
    T_COLON = 317,                 /* ":"  */
    T_SCOPE = 318,                 /* "::"  */
    T_RANGE = 319,                 /* ".."  */
    T_COMMA = 320,                 /* ","  */
    T_OPEN = 321,                  /* "("  */
    T_CLOSE = 322,                 /* ")"  */
    T_OBJECT_OPEN = 323,           /* "{"  */
    T_OBJECT_CLOSE = 324,          /* "}"  */
    T_ARRAY_OPEN = 325,            /* "["  */
    T_ARRAY_CLOSE = 326,           /* "]"  */
    T_OUTBOUND = 327,              /* "outbound modifier"  */
    T_INBOUND = 328,               /* "inbound modifier"  */
    T_ANY = 329,                   /* "any modifier"  */
    T_ALL = 330,                   /* "all modifier"  */
    T_NONE = 331,                  /* "none modifier"  */
    T_AT_LEAST = 332,              /* "at least modifier"  */
    UMINUS = 333,                  /* UMINUS  */
    UPLUS = 334,                   /* UPLUS  */
    UNEGATION = 335,               /* UNEGATION  */
    FUNCCALL = 336,                /* FUNCCALL  */
    REFERENCE = 337,               /* REFERENCE  */
    INDEXED = 338,                 /* INDEXED  */
    EXPANSION = 339                /* EXPANSION  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

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

#line 158 "grammar.hpp"

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
