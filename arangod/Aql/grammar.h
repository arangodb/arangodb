/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

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
    T_REMOVE = 270,
    T_INSERT = 271,
    T_UPDATE = 272,
    T_REPLACE = 273,
    T_UPSERT = 274,
    T_NULL = 275,
    T_TRUE = 276,
    T_FALSE = 277,
    T_STRING = 278,
    T_QUOTED_STRING = 279,
    T_INTEGER = 280,
    T_DOUBLE = 281,
    T_PARAMETER = 282,
    T_ASSIGN = 283,
    T_NOT = 284,
    T_AND = 285,
    T_OR = 286,
    T_EQ = 287,
    T_NE = 288,
    T_LT = 289,
    T_GT = 290,
    T_LE = 291,
    T_GE = 292,
    T_PLUS = 293,
    T_MINUS = 294,
    T_TIMES = 295,
    T_DIV = 296,
    T_MOD = 297,
    T_QUESTION = 298,
    T_COLON = 299,
    T_SCOPE = 300,
    T_RANGE = 301,
    T_COMMA = 302,
    T_OPEN = 303,
    T_CLOSE = 304,
    T_OBJECT_OPEN = 305,
    T_OBJECT_CLOSE = 306,
    T_ARRAY_OPEN = 307,
    T_ARRAY_CLOSE = 308,
    T_NIN = 309,
    UMINUS = 310,
    UPLUS = 311,
    FUNCCALL = 312,
    REFERENCE = 313,
    INDEXED = 314,
    EXPANSION = 315
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 22 "arangod/Aql/grammar.y" /* yacc.c:1909  */

  triagens::aql::AstNode*  node;
  char*                    strval;
  bool                     boolval;
  int64_t                  intval;

#line 123 "arangod/Aql/grammar.hpp" /* yacc.c:1909  */
};
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
