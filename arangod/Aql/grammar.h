/* A Bison parser, made by GNU Bison 2.7.12-4996.  */

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
/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int Aqldebug;
#endif

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
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
     T_NULL = 274,
     T_TRUE = 275,
     T_FALSE = 276,
     T_STRING = 277,
     T_QUOTED_STRING = 278,
     T_INTEGER = 279,
     T_DOUBLE = 280,
     T_PARAMETER = 281,
     T_ASSIGN = 282,
     T_NOT = 283,
     T_AND = 284,
     T_OR = 285,
     T_EQ = 286,
     T_NE = 287,
     T_LT = 288,
     T_GT = 289,
     T_LE = 290,
     T_GE = 291,
     T_PLUS = 292,
     T_MINUS = 293,
     T_TIMES = 294,
     T_DIV = 295,
     T_MOD = 296,
     T_EXPAND = 297,
     T_QUESTION = 298,
     T_COLON = 299,
     T_SCOPE = 300,
     T_RANGE = 301,
     T_COMMA = 302,
     T_OPEN = 303,
     T_CLOSE = 304,
     T_DOC_OPEN = 305,
     T_DOC_CLOSE = 306,
     T_LIST_OPEN = 307,
     T_LIST_CLOSE = 308,
     UPLUS = 309,
     UMINUS = 310,
     FUNCCALL = 311,
     REFERENCE = 312,
     INDEXED = 313
   };
#endif


#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{
/* Line 2053 of yacc.c  */
#line 22 "arangod/Aql/grammar.y"

  triagens::aql::AstNode*  node;
  char*                    strval;
  bool                     boolval;
  int64_t                  intval;


/* Line 2053 of yacc.c  */
#line 124 "arangod/Aql/grammar.hpp"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int Aqlparse (void *YYPARSE_PARAM);
#else
int Aqlparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int Aqlparse (triagens::aql::Parser* parser);
#else
int Aqlparse ();
#endif
#endif /* ! YYPARSE_PARAM */

#endif /* !YY_AQL_ARANGOD_AQL_GRAMMAR_HPP_INCLUDED  */
