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

#ifndef YY_AHUACATL_ARANGOD_AHUACATL_AHUACATL_GRAMMAR_H_INCLUDED
# define YY_AHUACATL_ARANGOD_AHUACATL_AHUACATL_GRAMMAR_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int Ahuacatldebug;
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
    T_INTO = 268,
    T_NULL = 269,
    T_TRUE = 270,
    T_FALSE = 271,
    T_STRING = 272,
    T_QUOTED_STRING = 273,
    T_INTEGER = 274,
    T_DOUBLE = 275,
    T_PARAMETER = 276,
    T_ASSIGN = 277,
    T_NOT = 278,
    T_AND = 279,
    T_OR = 280,
    T_EQ = 281,
    T_NE = 282,
    T_LT = 283,
    T_GT = 284,
    T_LE = 285,
    T_GE = 286,
    T_PLUS = 287,
    T_MINUS = 288,
    T_TIMES = 289,
    T_DIV = 290,
    T_MOD = 291,
    T_EXPAND = 292,
    T_QUESTION = 293,
    T_COLON = 294,
    T_SCOPE = 295,
    T_RANGE = 296,
    T_COMMA = 297,
    T_OPEN = 298,
    T_CLOSE = 299,
    T_DOC_OPEN = 300,
    T_DOC_CLOSE = 301,
    T_LIST_OPEN = 302,
    T_LIST_CLOSE = 303,
    UMINUS = 304,
    UPLUS = 305,
    FUNCCALL = 306,
    REFERENCE = 307,
    INDEXED = 308
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 26 "arangod/Ahuacatl/ahuacatl-grammar.y" /* yacc.c:1915  */

  TRI_aql_node_t* node;
  char* strval;
  bool boolval;
  int64_t intval;

#line 116 "arangod/Ahuacatl/ahuacatl-grammar.h" /* yacc.c:1915  */
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



int Ahuacatlparse (TRI_aql_context_t* const context);

#endif /* !YY_AHUACATL_ARANGOD_AHUACATL_AHUACATL_GRAMMAR_H_INCLUDED  */
