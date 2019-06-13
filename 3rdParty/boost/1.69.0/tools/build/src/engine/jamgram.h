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

#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    _BANG_t = 258,
    _BANG_EQUALS_t = 259,
    _AMPER_t = 260,
    _AMPERAMPER_t = 261,
    _LPAREN_t = 262,
    _RPAREN_t = 263,
    _PLUS_EQUALS_t = 264,
    _COLON_t = 265,
    _SEMIC_t = 266,
    _LANGLE_t = 267,
    _LANGLE_EQUALS_t = 268,
    _EQUALS_t = 269,
    _RANGLE_t = 270,
    _RANGLE_EQUALS_t = 271,
    _QUESTION_EQUALS_t = 272,
    _LBRACKET_t = 273,
    _RBRACKET_t = 274,
    ACTIONS_t = 275,
    BIND_t = 276,
    BREAK_t = 277,
    CASE_t = 278,
    CLASS_t = 279,
    CONTINUE_t = 280,
    DEFAULT_t = 281,
    ELSE_t = 282,
    EXISTING_t = 283,
    FOR_t = 284,
    IF_t = 285,
    IGNORE_t = 286,
    IN_t = 287,
    INCLUDE_t = 288,
    LOCAL_t = 289,
    MODULE_t = 290,
    ON_t = 291,
    PIECEMEAL_t = 292,
    QUIETLY_t = 293,
    RETURN_t = 294,
    RULE_t = 295,
    SWITCH_t = 296,
    TOGETHER_t = 297,
    UPDATED_t = 298,
    WHILE_t = 299,
    _LBRACE_t = 300,
    _BAR_t = 301,
    _BARBAR_t = 302,
    _RBRACE_t = 303,
    ARG = 304,
    STRING = 305
  };
#endif
/* Tokens.  */
#define _BANG_t 258
#define _BANG_EQUALS_t 259
#define _AMPER_t 260
#define _AMPERAMPER_t 261
#define _LPAREN_t 262
#define _RPAREN_t 263
#define _PLUS_EQUALS_t 264
#define _COLON_t 265
#define _SEMIC_t 266
#define _LANGLE_t 267
#define _LANGLE_EQUALS_t 268
#define _EQUALS_t 269
#define _RANGLE_t 270
#define _RANGLE_EQUALS_t 271
#define _QUESTION_EQUALS_t 272
#define _LBRACKET_t 273
#define _RBRACKET_t 274
#define ACTIONS_t 275
#define BIND_t 276
#define BREAK_t 277
#define CASE_t 278
#define CLASS_t 279
#define CONTINUE_t 280
#define DEFAULT_t 281
#define ELSE_t 282
#define EXISTING_t 283
#define FOR_t 284
#define IF_t 285
#define IGNORE_t 286
#define IN_t 287
#define INCLUDE_t 288
#define LOCAL_t 289
#define MODULE_t 290
#define ON_t 291
#define PIECEMEAL_t 292
#define QUIETLY_t 293
#define RETURN_t 294
#define RULE_t 295
#define SWITCH_t 296
#define TOGETHER_t 297
#define UPDATED_t 298
#define WHILE_t 299
#define _LBRACE_t 300
#define _BAR_t 301
#define _BARBAR_t 302
#define _RBRACE_t 303
#define ARG 304
#define STRING 305

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */
