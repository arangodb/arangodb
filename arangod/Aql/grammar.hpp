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
    T_TAKE = 261,                  /* "TAKE declaration"  */
    T_DROP = 262,                  /* "DROP declaration"  */
    T_RETURN = 263,                /* "RETURN declaration"  */
    T_COLLECT = 264,               /* "COLLECT declaration"  */
    T_SORT = 265,                  /* "SORT declaration"  */
    T_LIMIT = 266,                 /* "LIMIT declaration"  */
    T_WINDOW = 267,                /* "WINDOW declaration"  */
    T_ASC = 268,                   /* "ASC keyword"  */
    T_DESC = 269,                  /* "DESC keyword"  */
    T_IN = 270,                    /* "IN keyword"  */
    T_WITH = 271,                  /* "WITH keyword"  */
    T_INTO = 272,                  /* "INTO keyword"  */
    T_AGGREGATE = 273,             /* "AGGREGATE keyword"  */
    T_GRAPH = 274,                 /* "GRAPH keyword"  */
    T_SHORTEST_PATH = 275,         /* "SHORTEST_PATH keyword"  */
    T_K_SHORTEST_PATHS = 276,      /* "K_SHORTEST_PATHS keyword"  */
    T_K_PATHS = 277,               /* "K_PATHS keyword"  */
    T_DISTINCT = 278,              /* "DISTINCT modifier"  */
    T_REMOVE = 279,                /* "REMOVE command"  */
    T_INSERT = 280,                /* "INSERT command"  */
    T_UPDATE = 281,                /* "UPDATE command"  */
    T_REPLACE = 282,               /* "REPLACE command"  */
    T_UPSERT = 283,                /* "UPSERT command"  */
    T_NULL = 284,                  /* "null"  */
    T_TRUE = 285,                  /* "true"  */
    T_FALSE = 286,                 /* "false"  */
    T_STRING = 287,                /* "identifier"  */
    T_QUOTED_STRING = 288,         /* "quoted string"  */
    T_INTEGER = 289,               /* "integer number"  */
    T_DOUBLE = 290,                /* "number"  */
    T_PARAMETER = 291,             /* "bind parameter"  */
    T_DATA_SOURCE_PARAMETER = 292, /* "bind data source parameter"  */
    T_ASSIGN = 293,                /* "assignment"  */
    T_NOT = 294,                   /* "not operator"  */
    T_AND = 295,                   /* "and operator"  */
    T_OR = 296,                    /* "or operator"  */
    T_REGEX_MATCH = 297,           /* "~= operator"  */
    T_REGEX_NON_MATCH = 298,       /* "~! operator"  */
    T_EQ = 299,                    /* "== operator"  */
    T_NE = 300,                    /* "!= operator"  */
    T_LT = 301,                    /* "< operator"  */
    T_GT = 302,                    /* "> operator"  */
    T_LE = 303,                    /* "<= operator"  */
    T_GE = 304,                    /* ">= operator"  */
    T_LIKE = 305,                  /* "like operator"  */
    T_PLUS = 306,                  /* "+ operator"  */
    T_MINUS = 307,                 /* "- operator"  */
    T_TIMES = 308,                 /* "* operator"  */
    T_DIV = 309,                   /* "/ operator"  */
    T_MOD = 310,                   /* "% operator"  */
    T_QUESTION = 311,              /* "?"  */
    T_COLON = 312,                 /* ":"  */
    T_SCOPE = 313,                 /* "::"  */
    T_RANGE = 314,                 /* ".."  */
    T_COMMA = 315,                 /* ","  */
    T_OPEN = 316,                  /* "("  */
    T_CLOSE = 317,                 /* ")"  */
    T_OBJECT_OPEN = 318,           /* "{"  */
    T_OBJECT_CLOSE = 319,          /* "}"  */
    T_ARRAY_OPEN = 320,            /* "["  */
    T_ARRAY_CLOSE = 321,           /* "]"  */
    T_OUTBOUND = 322,              /* "outbound modifier"  */
    T_INBOUND = 323,               /* "inbound modifier"  */
    T_ANY = 324,                   /* "any modifier"  */
    T_ALL = 325,                   /* "all modifier"  */
    T_NONE = 326,                  /* "none modifier"  */
    UMINUS = 327,                  /* UMINUS  */
    UPLUS = 328,                   /* UPLUS  */
    UNEGATION = 329,               /* UNEGATION  */
    FUNCCALL = 330,                /* FUNCCALL  */
    REFERENCE = 331,               /* REFERENCE  */
    INDEXED = 332,                 /* INDEXED  */
    EXPANSION = 333                /* EXPANSION  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 43 "Aql/grammar.y"

  arangodb::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 152 "Aql/grammar.hpp"

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
