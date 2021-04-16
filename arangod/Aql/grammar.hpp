/* A Bison parser, made by GNU Bison 3.5.1.  */

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

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

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
  };
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

#line 145 "Aql/grammar.hpp"

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
