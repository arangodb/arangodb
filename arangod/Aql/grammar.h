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
    T_DISTINCT = 270,
    T_REMOVE = 271,
    T_INSERT = 272,
    T_UPDATE = 273,
    T_REPLACE = 274,
    T_UPSERT = 275,
    T_NULL = 276,
    T_TRUE = 277,
    T_FALSE = 278,
    T_STRING = 279,
    T_QUOTED_STRING = 280,
    T_INTEGER = 281,
    T_DOUBLE = 282,
    T_PARAMETER = 283,
    T_ASSIGN = 284,
    T_NOT = 285,
    T_AND = 286,
    T_OR = 287,
    T_EQ = 288,
    T_NE = 289,
    T_LT = 290,
    T_GT = 291,
    T_LE = 292,
    T_GE = 293,
    T_PLUS = 294,
    T_MINUS = 295,
    T_TIMES = 296,
    T_DIV = 297,
    T_MOD = 298,
    T_QUESTION = 299,
    T_COLON = 300,
    T_SCOPE = 301,
    T_RANGE = 302,
    T_COMMA = 303,
    T_OPEN = 304,
    T_CLOSE = 305,
    T_OBJECT_OPEN = 306,
    T_OBJECT_CLOSE = 307,
    T_ARRAY_OPEN = 308,
    T_ARRAY_CLOSE = 309,
    T_NIN = 310,
    UMINUS = 311,
    UPLUS = 312,
    FUNCCALL = 313,
    REFERENCE = 314,
    INDEXED = 315,
    EXPANSION = 316
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 17 "arangod/Aql/grammar.y" /* yacc.c:1915  */

  triagens::aql::AstNode*  node;
  struct {
    char*                  value;
    size_t                 length;
  }                        strval;
  bool                     boolval;
  int64_t                  intval;

#line 127 "arangod/Aql/grammar.hpp" /* yacc.c:1915  */
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



int Aqlparse (triagens::aql::Parser* parser);

#endif /* !YY_AQL_ARANGOD_AQL_GRAMMAR_HPP_INCLUDED  */
