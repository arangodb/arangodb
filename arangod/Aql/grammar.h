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
    T_GRAPH = 270,
    T_DISTINCT = 271,
    T_REMOVE = 272,
    T_INSERT = 273,
    T_UPDATE = 274,
    T_REPLACE = 275,
    T_UPSERT = 276,
    T_NULL = 277,
    T_TRUE = 278,
    T_FALSE = 279,
    T_STRING = 280,
    T_QUOTED_STRING = 281,
    T_INTEGER = 282,
    T_DOUBLE = 283,
    T_PARAMETER = 284,
    T_ASSIGN = 285,
    T_NOT = 286,
    T_AND = 287,
    T_OR = 288,
    T_EQ = 289,
    T_NE = 290,
    T_LT = 291,
    T_GT = 292,
    T_LE = 293,
    T_GE = 294,
    T_PLUS = 295,
    T_MINUS = 296,
    T_TIMES = 297,
    T_DIV = 298,
    T_MOD = 299,
    T_QUESTION = 300,
    T_COLON = 301,
    T_SCOPE = 302,
    T_RANGE = 303,
    T_COMMA = 304,
    T_OPEN = 305,
    T_CLOSE = 306,
    T_OBJECT_OPEN = 307,
    T_OBJECT_CLOSE = 308,
    T_ARRAY_OPEN = 309,
    T_ARRAY_CLOSE = 310,
    T_OUTBOUND = 311,
    T_INBOUND = 312,
    T_ANY = 313,
    T_NIN = 314,
    UMINUS = 315,
    UPLUS = 316,
    FUNCCALL = 317,
    REFERENCE = 318,
    INDEXED = 319,
    EXPANSION = 320
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 17 "arangod/Aql/grammar.y" /* yacc.c:1915  */

  triagens::aql::AstNode*  node;
  char*                    strval;
  bool                     boolval;
  int64_t                  intval;

#line 128 "arangod/Aql/grammar.hpp" /* yacc.c:1915  */
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
