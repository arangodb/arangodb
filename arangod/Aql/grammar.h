/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

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
    T_SEARCH = 261,
    T_RETURN = 262,
    T_COLLECT = 263,
    T_SORT = 264,
    T_LIMIT = 265,
    T_ASC = 266,
    T_DESC = 267,
    T_IN = 268,
    T_WITH = 269,
    T_INTO = 270,
    T_AGGREGATE = 271,
    T_VIEW = 272,
    T_GRAPH = 273,
    T_SHORTEST_PATH = 274,
    T_DISTINCT = 275,
    T_REMOVE = 276,
    T_INSERT = 277,
    T_UPDATE = 278,
    T_REPLACE = 279,
    T_UPSERT = 280,
    T_NULL = 281,
    T_TRUE = 282,
    T_FALSE = 283,
    T_STRING = 284,
    T_QUOTED_STRING = 285,
    T_INTEGER = 286,
    T_DOUBLE = 287,
    T_PARAMETER = 288,
    T_DATA_SOURCE_PARAMETER = 289,
    T_ASSIGN = 290,
    T_NOT = 291,
    T_AND = 292,
    T_OR = 293,
    T_NIN = 294,
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
    FUNCCALL = 327,
    REFERENCE = 328,
    INDEXED = 329,
    EXPANSION = 330
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 32 "Aql/grammar.y" /* yacc.c:1909  */

  arangodb::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 141 "Aql/grammar.hpp" /* yacc.c:1909  */
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
