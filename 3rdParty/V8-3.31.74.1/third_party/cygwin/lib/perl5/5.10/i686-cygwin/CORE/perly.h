#ifdef PERL_CORE
/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     WORD = 258,
     METHOD = 259,
     FUNCMETH = 260,
     THING = 261,
     PMFUNC = 262,
     PRIVATEREF = 263,
     FUNC0SUB = 264,
     UNIOPSUB = 265,
     LSTOPSUB = 266,
     LABEL = 267,
     FORMAT = 268,
     SUB = 269,
     ANONSUB = 270,
     PACKAGE = 271,
     USE = 272,
     WHILE = 273,
     UNTIL = 274,
     IF = 275,
     UNLESS = 276,
     ELSE = 277,
     ELSIF = 278,
     CONTINUE = 279,
     FOR = 280,
     GIVEN = 281,
     WHEN = 282,
     DEFAULT = 283,
     LOOPEX = 284,
     DOTDOT = 285,
     FUNC0 = 286,
     FUNC1 = 287,
     FUNC = 288,
     UNIOP = 289,
     LSTOP = 290,
     RELOP = 291,
     EQOP = 292,
     MULOP = 293,
     ADDOP = 294,
     DOLSHARP = 295,
     DO = 296,
     HASHBRACK = 297,
     NOAMP = 298,
     LOCAL = 299,
     MY = 300,
     MYSUB = 301,
     REQUIRE = 302,
     COLONATTR = 303,
     PREC_LOW = 304,
     DOROP = 305,
     OROP = 306,
     ANDOP = 307,
     NOTOP = 308,
     ASSIGNOP = 309,
     DORDOR = 310,
     OROR = 311,
     ANDAND = 312,
     BITOROP = 313,
     BITANDOP = 314,
     SHIFTOP = 315,
     MATCHOP = 316,
     REFGEN = 317,
     UMINUS = 318,
     POWOP = 319,
     POSTDEC = 320,
     POSTINC = 321,
     PREDEC = 322,
     PREINC = 323,
     ARROW = 324,
     PEG = 325
   };
#endif
/* Tokens.  */
#define WORD 258
#define METHOD 259
#define FUNCMETH 260
#define THING 261
#define PMFUNC 262
#define PRIVATEREF 263
#define FUNC0SUB 264
#define UNIOPSUB 265
#define LSTOPSUB 266
#define LABEL 267
#define FORMAT 268
#define SUB 269
#define ANONSUB 270
#define PACKAGE 271
#define USE 272
#define WHILE 273
#define UNTIL 274
#define IF 275
#define UNLESS 276
#define ELSE 277
#define ELSIF 278
#define CONTINUE 279
#define FOR 280
#define GIVEN 281
#define WHEN 282
#define DEFAULT 283
#define LOOPEX 284
#define DOTDOT 285
#define FUNC0 286
#define FUNC1 287
#define FUNC 288
#define UNIOP 289
#define LSTOP 290
#define RELOP 291
#define EQOP 292
#define MULOP 293
#define ADDOP 294
#define DOLSHARP 295
#define DO 296
#define HASHBRACK 297
#define NOAMP 298
#define LOCAL 299
#define MY 300
#define MYSUB 301
#define REQUIRE 302
#define COLONATTR 303
#define PREC_LOW 304
#define DOROP 305
#define OROP 306
#define ANDOP 307
#define NOTOP 308
#define ASSIGNOP 309
#define DORDOR 310
#define OROR 311
#define ANDAND 312
#define BITOROP 313
#define BITANDOP 314
#define SHIFTOP 315
#define MATCHOP 316
#define REFGEN 317
#define UMINUS 318
#define POWOP 319
#define POSTDEC 320
#define POSTINC 321
#define PREDEC 322
#define PREINC 323
#define ARROW 324
#define PEG 325




#endif /* PERL_CORE */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{
    I32	ival; /* __DEFAULT__ (marker for regen_perly.pl;
				must always be 1st union member) */
    char *pval;
    OP *opval;
    GV *gvval;
#ifdef PERL_IN_MADLY_C
    TOKEN* p_tkval;
    TOKEN* i_tkval;
#else
    char *p_tkval;
    I32	i_tkval;
#endif
#ifdef PERL_MAD
    TOKEN* tkval;
#endif
}
/* Line 1489 of yacc.c.  */
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



