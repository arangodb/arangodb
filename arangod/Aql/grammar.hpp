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
    T_K_PATHS = 274,               /* "K_PATHS keyword"  */
    T_DISTINCT = 275,              /* "DISTINCT modifier"  */
    T_REMOVE = 276,                /* "REMOVE command"  */
    T_INSERT = 277,                /* "INSERT command"  */
    T_UPDATE = 278,                /* "UPDATE command"  */
    T_REPLACE = 279,               /* "REPLACE command"  */
    T_UPSERT = 280,                /* "UPSERT command"  */
    T_NULL = 281,                  /* "null"  */
    T_TRUE = 282,                  /* "true"  */
    T_FALSE = 283,                 /* "false"  */
    T_STRING = 284,                /* "identifier"  */
    T_QUOTED_STRING = 285,         /* "quoted string"  */
    T_INTEGER = 286,               /* "integer number"  */
    T_DOUBLE = 287,                /* "number"  */
    T_PARAMETER = 288,             /* "bind parameter"  */
    T_DATA_SOURCE_PARAMETER = 289, /* "bind data source parameter"  */
    T_ASSIGN = 290,                /* "assignment"  */
    T_NOT = 291,                   /* "not operator"  */
    T_AND = 292,                   /* "and operator"  */
    T_OR = 293,                    /* "or operator"  */
    T_REGEX_MATCH = 294,           /* "~= operator"  */
    T_REGEX_NON_MATCH = 295,       /* "~! operator"  */
    T_EQ = 296,                    /* "== operator"  */
    T_NE = 297,                    /* "!= operator"  */
    T_LT = 298,                    /* "< operator"  */
    T_GT = 299,                    /* "> operator"  */
    T_LE = 300,                    /* "<= operator"  */
    T_GE = 301,                    /* ">= operator"  */
    T_LIKE = 302,                  /* "like operator"  */
    T_PLUS = 303,                  /* "+ operator"  */
    T_MINUS = 304,                 /* "- operator"  */
    T_TIMES = 305,                 /* "* operator"  */
    T_DIV = 306,                   /* "/ operator"  */
    T_MOD = 307,                   /* "% operator"  */
    T_QUESTION = 308,              /* "?"  */
    T_COLON = 309,                 /* ":"  */
    T_SCOPE = 310,                 /* "::"  */
    T_RANGE = 311,                 /* ".."  */
    T_COMMA = 312,                 /* ","  */
    T_OPEN = 313,                  /* "("  */
    T_CLOSE = 314,                 /* ")"  */
    T_OBJECT_OPEN = 315,           /* "{"  */
    T_OBJECT_CLOSE = 316,          /* "}"  */
    T_ARRAY_OPEN = 317,            /* "["  */
    T_ARRAY_CLOSE = 318,           /* "]"  */
    T_OUTBOUND = 319,              /* "outbound modifier"  */
    T_INBOUND = 320,               /* "inbound modifier"  */
    T_ANY = 321,                   /* "any modifier"  */
    T_ALL = 322,                   /* "all modifier"  */
    T_NONE = 323,                  /* "none modifier"  */
    UMINUS = 324,                  /* UMINUS  */
    UPLUS = 325,                   /* UPLUS  */
    UNEGATION = 326,               /* UNEGATION  */
    FUNCCALL = 327,                /* FUNCCALL  */
    REFERENCE = 328,               /* REFERENCE  */
    INDEXED = 329,                 /* INDEXED  */
    EXPANSION = 330                /* EXPANSION  */
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

#line 149 "Aql/grammar.hpp"

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
