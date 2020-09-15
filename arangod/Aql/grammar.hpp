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
    T_K_SHORTEST_PATHS = 273,
    T_DISTINCT = 274,
    T_REMOVE = 275,
    T_INSERT = 276,
    T_UPDATE = 277,
    T_REPLACE = 278,
    T_UPSERT = 279,
    T_NULL = 280,
    T_TRUE = 281,
    T_FALSE = 282,
    T_STRING = 283,
    T_QUOTED_STRING = 284,
    T_INTEGER = 285,
    T_DOUBLE = 286,
    T_PARAMETER = 287,
    T_DATA_SOURCE_PARAMETER = 288,
    T_ASSIGN = 289,
    T_NOT = 290,
    T_AND = 291,
    T_OR = 292,
    T_REGEX_MATCH = 293,
    T_REGEX_NON_MATCH = 294,
    T_EQ = 295,
    T_NE = 296,
    T_LT = 297,
    T_GT = 298,
    T_LE = 299,
    T_GE = 300,
    T_LIKE = 301,
    T_PLUS = 302,
    T_MINUS = 303,
    T_TIMES = 304,
    T_DIV = 305,
    T_MOD = 306,
    T_QUESTION = 307,
    T_COLON = 308,
    T_SCOPE = 309,
    T_RANGE = 310,
    T_COMMA = 311,
    T_OPEN = 312,
    T_CLOSE = 313,
    T_OBJECT_OPEN = 314,
    T_OBJECT_CLOSE = 315,
    T_ARRAY_OPEN = 316,
    T_ARRAY_CLOSE = 317,
    T_OUTBOUND = 318,
    T_INBOUND = 319,
    T_ANY = 320,
    T_ALL = 321,
    T_NONE = 322,
    UMINUS = 323,
    UPLUS = 324,
    UNEGATION = 325,
    FUNCCALL = 326,
    REFERENCE = 327,
    INDEXED = 328,
    EXPANSION = 329
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 36 "Aql/grammar.y" /* yacc.c:1909  */

  arangodb::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 140 "Aql/grammar.hpp" /* yacc.c:1909  */
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
