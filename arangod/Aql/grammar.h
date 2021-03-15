/* A Bison parser, made by GNU Bison 3.7.1.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_AQL_AQL_GRAMMAR_HPP_INCLUDED
# define YY_AQL_AQL_GRAMMAR_HPP_INCLUDED
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
<<<<<<< HEAD
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
    T_ASC = 265,                   /* "ASC keyword"  */
    T_DESC = 266,                  /* "DESC keyword"  */
    T_IN = 267,                    /* "IN keyword"  */
    T_WITH = 268,                  /* "WITH keyword"  */
    T_INTO = 269,                  /* "INTO keyword"  */
    T_AGGREGATE = 270,             /* "AGGREGATE keyword"  */
    T_GRAPH = 271,                 /* "GRAPH keyword"  */
    T_SHORTEST_PATH = 272,         /* "SHORTEST_PATH keyword"  */
    T_K_SHORTEST_PATHS = 273,      /* "K_SHORTEST_PATHS keyword"  */
    T_DISTINCT = 274,              /* "DISTINCT modifier"  */
    T_REMOVE = 275,                /* "REMOVE command"  */
    T_INSERT = 276,                /* "INSERT command"  */
    T_UPDATE = 277,                /* "UPDATE command"  */
    T_REPLACE = 278,               /* "REPLACE command"  */
    T_UPSERT = 279,                /* "UPSERT command"  */
    T_NULL = 280,                  /* "null"  */
    T_TRUE = 281,                  /* "true"  */
    T_FALSE = 282,                 /* "false"  */
    T_STRING = 283,                /* "identifier"  */
    T_QUOTED_STRING = 284,         /* "quoted string"  */
    T_INTEGER = 285,               /* "integer number"  */
    T_DOUBLE = 286,                /* "number"  */
    T_PARAMETER = 287,             /* "bind parameter"  */
    T_DATA_SOURCE_PARAMETER = 288, /* "bind data source parameter"  */
    T_ASSIGN = 289,                /* "assignment"  */
    T_NOT = 290,                   /* "not operator"  */
    T_AND = 291,                   /* "and operator"  */
    T_OR = 292,                    /* "or operator"  */
    T_REGEX_MATCH = 293,           /* "~= operator"  */
    T_REGEX_NON_MATCH = 294,       /* "~! operator"  */
    T_EQ = 295,                    /* "== operator"  */
    T_NE = 296,                    /* "!= operator"  */
    T_LT = 297,                    /* "< operator"  */
    T_GT = 298,                    /* "> operator"  */
    T_LE = 299,                    /* "<= operator"  */
    T_GE = 300,                    /* ">= operator"  */
    T_LIKE = 301,                  /* "like operator"  */
    T_PLUS = 302,                  /* "+ operator"  */
    T_MINUS = 303,                 /* "- operator"  */
    T_TIMES = 304,                 /* "* operator"  */
    T_DIV = 305,                   /* "/ operator"  */
    T_MOD = 306,                   /* "% operator"  */
    T_QUESTION = 307,              /* "?"  */
    T_COLON = 308,                 /* ":"  */
    T_SCOPE = 309,                 /* "::"  */
    T_RANGE = 310,                 /* ".."  */
    T_COMMA = 311,                 /* ","  */
    T_OPEN = 312,                  /* "("  */
    T_CLOSE = 313,                 /* ")"  */
    T_OBJECT_OPEN = 314,           /* "{"  */
    T_OBJECT_CLOSE = 315,          /* "}"  */
    T_ARRAY_OPEN = 316,            /* "["  */
    T_ARRAY_CLOSE = 317,           /* "]"  */
    T_OUTBOUND = 318,              /* "outbound modifier"  */
    T_INBOUND = 319,               /* "inbound modifier"  */
    T_ANY = 320,                   /* "any modifier"  */
    T_ALL = 321,                   /* "all modifier"  */
    T_NONE = 322,                  /* "none modifier"  */
    UMINUS = 323,                  /* UMINUS  */
    UPLUS = 324,                   /* UPLUS  */
    UNEGATION = 325,               /* UNEGATION  */
    FUNCCALL = 326,                /* FUNCCALL  */
    REFERENCE = 327,               /* REFERENCE  */
    INDEXED = 328,                 /* INDEXED  */
    EXPANSION = 329                /* EXPANSION  */
=======
    T_END = 0,
    T_FOR = 258,
    T_LET = 259,
    T_FILTER = 260,
    T_RETURN = 261,
    T_COLLECT = 262,
    T_SORT = 263,
    T_LIMIT = 264,
    T_WINDOW = 265,
    T_ASC = 266,
    T_DESC = 267,
    T_IN = 268,
    T_WITH = 269,
    T_INTO = 270,
    T_AGGREGATE = 271,
    T_GRAPH = 272,
    T_SHORTEST_PATH = 273,
    T_K_SHORTEST_PATHS = 274,
    T_K_PATHS = 275,
    T_DISTINCT = 276,
    T_REMOVE = 277,
    T_INSERT = 278,
    T_UPDATE = 279,
    T_REPLACE = 280,
    T_UPSERT = 281,
    T_NULL = 282,
    T_TRUE = 283,
    T_FALSE = 284,
    T_STRING = 285,
    T_QUOTED_STRING = 286,
    T_INTEGER = 287,
    T_DOUBLE = 288,
    T_PARAMETER = 289,
    T_DATA_SOURCE_PARAMETER = 290,
    T_ASSIGN = 291,
    T_NOT = 292,
    T_AND = 293,
    T_OR = 294,
    T_REGEX_MATCH = 295,
    T_REGEX_NON_MATCH = 296,
    T_EQ = 297,
    T_NE = 298,
    T_LT = 299,
    T_GT = 300,
    T_LE = 301,
    T_GE = 302,
    T_LIKE = 303,
    T_PLUS = 304,
    T_MINUS = 305,
    T_TIMES = 306,
    T_DIV = 307,
    T_MOD = 308,
    T_QUESTION = 309,
    T_COLON = 310,
    T_SCOPE = 311,
    T_RANGE = 312,
    T_COMMA = 313,
    T_OPEN = 314,
    T_CLOSE = 315,
    T_OBJECT_OPEN = 316,
    T_OBJECT_CLOSE = 317,
    T_ARRAY_OPEN = 318,
    T_ARRAY_CLOSE = 319,
    T_OUTBOUND = 320,
    T_INBOUND = 321,
    T_ANY = 322,
    T_ALL = 323,
    T_NONE = 324,
    UMINUS = 325,
    UPLUS = 326,
    UNEGATION = 327,
    FUNCCALL = 328,
    REFERENCE = 329,
    INDEXED = 330,
    EXPANSION = 331
>>>>>>> 018cbb408d61b8051528fb5bce47c17a5a70c09e
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 37 "Aql/grammar.y"

  arangodb::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

<<<<<<< HEAD
#line 148 "Aql/grammar.hpp"
=======
#line 145 "Aql/grammar.hpp"
>>>>>>> 018cbb408d61b8051528fb5bce47c17a5a70c09e

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
