/* A Bison parser, made by GNU Bison 2.5.  */

/* Skeleton implementation for Bison LALR(1) parsers in C++
   
      Copyright (C) 2002-2011 Free Software Foundation, Inc.
   
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


/* First part of user declarations.  */



#include <Basics/Common.h>
#include <Basics/StringUtils.h>
#include <Variant/VariantArray.h>
#include <Variant/VariantBoolean.h>
#include <Variant/VariantDouble.h>
#include <Variant/VariantInt32.h>
#include <Variant/VariantInt64.h>
#include <Variant/VariantNull.h>
#include <Variant/VariantObject.h>
#include <Variant/VariantString.h>
#include <Variant/VariantUInt32.h>
#include <Variant/VariantUInt64.h>
#include <Variant/VariantVector.h>

#include "JsonParserX/JsonParserXDriver.h"

using namespace std;
using namespace triagens::basics;

#define YYSCANNER driver.scanner
#define YYENABLE_NLS 0 // warning should be suppressed in newer releases of bison




#include "JsonParserX.h"

/* User implementation prologue.  */


YY_DECL;



#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* FIXME: INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                               \
 do                                                                    \
   if (N)                                                              \
     {                                                                 \
       (Current).begin = YYRHSLOC (Rhs, 1).begin;                      \
       (Current).end   = YYRHSLOC (Rhs, N).end;                        \
     }                                                                 \
   else                                                                \
     {                                                                 \
       (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;        \
     }                                                                 \
 while (false)
#endif

/* Suppress unused-variable warnings by "using" E.  */
#define YYUSE(e) ((void) (e))

/* Enable debugging if requested.  */
#if YYDEBUG

/* A pseudo ostream that takes yydebug_ into account.  */
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)	\
do {							\
  if (yydebug_)						\
    {							\
      *yycdebug_ << Title << ' ';			\
      yy_symbol_print_ ((Type), (Value), (Location));	\
      *yycdebug_ << std::endl;				\
    }							\
} while (false)

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug_)				\
    yy_reduce_print_ (Rule);		\
} while (false)

# define YY_STACK_PRINT()		\
do {					\
  if (yydebug_)				\
    yystack_print_ ();			\
} while (false)

#else /* !YYDEBUG */

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_REDUCE_PRINT(Rule)
# define YY_STACK_PRINT()

#endif /* !YYDEBUG */

#define yyerrok		(yyerrstatus_ = 0)
#define yyclearin	(yychar = yyempty_)

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)


namespace triagens { namespace json_parser {


  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  JsonParserX::yytnamerr_ (const char *yystr)
  {
    if (*yystr == '"')
      {
        std::string yyr = "";
        char const *yyp = yystr;

        for (;;)
          switch (*++yyp)
            {
            case '\'':
            case ',':
              goto do_not_strip_quotes;

            case '\\':
              if (*++yyp != '\\')
                goto do_not_strip_quotes;
              /* Fall through.  */
            default:
              yyr += *yyp;
              break;

            case '"':
              return yyr;
            }
      do_not_strip_quotes: ;
      }

    return yystr;
  }


  /// Build a parser object.
  JsonParserX::JsonParserX (triagens::rest::JsonParserXDriver& driver_yyarg)
    :
#if YYDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      driver (driver_yyarg)
  {
  }

  JsonParserX::~JsonParserX ()
  {
  }

#if YYDEBUG
  /*--------------------------------.
  | Print this symbol on YYOUTPUT.  |
  `--------------------------------*/

  inline void
  JsonParserX::yy_symbol_value_print_ (int yytype,
			   const semantic_type* yyvaluep, const location_type* yylocationp)
  {
    YYUSE (yylocationp);
    YYUSE (yyvaluep);
    switch (yytype)
      {
        case 10: /* "\"string constant\"" */

	{ debug_stream() << *(yyvaluep->str); };

	break;
       default:
	  break;
      }
  }


  void
  JsonParserX::yy_symbol_print_ (int yytype,
			   const semantic_type* yyvaluep, const location_type* yylocationp)
  {
    *yycdebug_ << (yytype < yyntokens_ ? "token" : "nterm")
	       << ' ' << yytname_[yytype] << " ("
	       << *yylocationp << ": ";
    yy_symbol_value_print_ (yytype, yyvaluep, yylocationp);
    *yycdebug_ << ')';
  }
#endif

  void
  JsonParserX::yydestruct_ (const char* yymsg,
			   int yytype, semantic_type* yyvaluep, location_type* yylocationp)
  {
    YYUSE (yylocationp);
    YYUSE (yymsg);
    YYUSE (yyvaluep);

    YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

    switch (yytype)
      {
  
	default:
	  break;
      }
  }

  void
  JsonParserX::yypop_ (unsigned int n)
  {
    yystate_stack_.pop (n);
    yysemantic_stack_.pop (n);
    yylocation_stack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  JsonParserX::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  JsonParserX::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  JsonParserX::debug_level_type
  JsonParserX::debug_level () const
  {
    return yydebug_;
  }

  void
  JsonParserX::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif

  inline bool
  JsonParserX::yy_pact_value_is_default_ (int yyvalue)
  {
    return yyvalue == yypact_ninf_;
  }

  inline bool
  JsonParserX::yy_table_value_is_error_ (int yyvalue)
  {
    return yyvalue == yytable_ninf_;
  }

  int
  JsonParserX::parse ()
  {
    /// Lookahead and lookahead in internal form.
    int yychar = yyempty_;
    int yytoken = 0;

    /* State.  */
    int yyn;
    int yylen = 0;
    int yystate = 0;

    /* Error handling.  */
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// Semantic value of the lookahead.
    semantic_type yylval;
    /// Location of the lookahead.
    location_type yylloc;
    /// The locations where the error started and ended.
    location_type yyerror_range[3];

    /// $$.
    semantic_type yyval;
    /// @$.
    location_type yyloc;

    int yyresult;

    YYCDEBUG << "Starting parse" << std::endl;


    /* User initialization code.  */
    
{
  // reset the location
  yylloc.step();
}


    /* Initialize the stacks.  The initial state will be pushed in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystate_stack_ = state_stack_type (0);
    yysemantic_stack_ = semantic_stack_type (0);
    yylocation_stack_ = location_stack_type (0);
    yysemantic_stack_.push (yylval);
    yylocation_stack_.push (yylloc);

    /* New state.  */
  yynewstate:
    yystate_stack_.push (yystate);
    YYCDEBUG << "Entering state " << yystate << std::endl;

    /* Accept?  */
    if (yystate == yyfinal_)
      goto yyacceptlab;

    goto yybackup;

    /* Backup.  */
  yybackup:

    /* Try to take a decision without lookahead.  */
    yyn = yypact_[yystate];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    /* Read a lookahead token.  */
    if (yychar == yyempty_)
      {
	YYCDEBUG << "Reading a token: ";
	yychar = yylex (&yylval, &yylloc, YYSCANNER);
      }


    /* Convert token to internal form.  */
    if (yychar <= yyeof_)
      {
	yychar = yytoken = yyeof_;
	YYCDEBUG << "Now at end of input." << std::endl;
      }
    else
      {
	yytoken = yytranslate_ (yychar);
	YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
      }

    /* If the proper action on seeing token YYTOKEN is to reduce or to
       detect an error, take that action.  */
    yyn += yytoken;
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yytoken)
      goto yydefault;

    /* Reduce or error.  */
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
	if (yy_table_value_is_error_ (yyn))
	  goto yyerrlab;
	yyn = -yyn;
	goto yyreduce;
      }

    /* Shift the lookahead token.  */
    YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

    /* Discard the token being shifted.  */
    yychar = yyempty_;

    yysemantic_stack_.push (yylval);
    yylocation_stack_.push (yylloc);

    /* Count tokens shifted since error; after three, turn off error
       status.  */
    if (yyerrstatus_)
      --yyerrstatus_;

    yystate = yyn;
    goto yynewstate;

  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[yystate];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;

  /*-----------------------------.
  | yyreduce -- Do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    /* If YYLEN is nonzero, implement the default value of the action:
       `$$ = $1'.  Otherwise, use the top of the stack.

       Otherwise, the following line sets YYVAL to garbage.
       This behavior is undocumented and Bison
       users should not rely upon it.  */
    if (yylen)
      yyval = yysemantic_stack_[yylen - 1];
    else
      yyval = yysemantic_stack_[0];

    {
      slice<location_type, location_stack_type> slice (yylocation_stack_, yylen);
      YYLLOC_DEFAULT (yyloc, slice, yylen);
    }
    YY_REDUCE_PRINT (yyn);
    switch (yyn)
      {
	  case 2:

    {
      driver.addVariantArray((yysemantic_stack_[(1) - (1)].variantArray));
    }
    break;

  case 3:

    {
      driver.addVariantDouble((yysemantic_stack_[(1) - (1)].double_type));
    }
    break;

  case 4:

    {
      driver.addVariantBoolean(false);
    }
    break;

  case 5:

    {
      driver.addVariantNull();
    }
    break;

  case 6:

    {
      driver.addVariantInt32((yysemantic_stack_[(1) - (1)].int32_type));
    }
    break;

  case 7:

    {
      driver.addVariantInt64((yysemantic_stack_[(1) - (1)].int64_type));
    }
    break;

  case 8:

    {
      driver.addVariantString(*(yysemantic_stack_[(1) - (1)].str));
      delete (yysemantic_stack_[(1) - (1)].str);
    }
    break;

  case 9:

    {
      driver.addVariantBoolean(true);
    }
    break;

  case 10:

    {
      driver.addVariantUInt32((yysemantic_stack_[(1) - (1)].uint32_type));
    }
    break;

  case 11:

    {
      driver.addVariantUInt64((yysemantic_stack_[(1) - (1)].uint64_type));
    }
    break;

  case 12:

    {
      driver.addVariantVector((yysemantic_stack_[(1) - (1)].variantVector));
    }
    break;

  case 13:

    {
      (yyval.variantArray) = (yysemantic_stack_[(3) - (2)].keyValueList);
    }
    break;

  case 14:

    {
      (yyval.variantArray) = new VariantArray();
    }
    break;

  case 15:

    {
      (yyval.variantVector) = (yysemantic_stack_[(3) - (2)].valueList);
    }
    break;

  case 16:

    {
      (yyval.variantVector) = new VariantVector();
    }
    break;

  case 17:

    {
      (yyval.keyValueList) = new VariantArray();
      (yyval.keyValueList)->add(*(yysemantic_stack_[(3) - (1)].str),(yysemantic_stack_[(3) - (3)].variantObject));    
      delete (yysemantic_stack_[(3) - (1)].str);      
    }
    break;

  case 18:

    {
      // nothing to add 
    }
    break;

  case 19:

    {
      (yyval.keyValueList)->add(*(yysemantic_stack_[(5) - (3)].str),(yysemantic_stack_[(5) - (5)].variantObject));    
      delete (yysemantic_stack_[(5) - (3)].str);      
    }
    break;

  case 20:

    {
      (yyval.valueList) = new VariantVector();
      (yyval.valueList)->add((yysemantic_stack_[(1) - (1)].variantObject));
    }
    break;

  case 21:

    {
      // nothing to add
    }
    break;

  case 22:

    {
      (yysemantic_stack_[(3) - (1)].valueList)->add((yysemantic_stack_[(3) - (3)].variantObject));
    }
    break;

  case 23:

    {
      (yyval.variantObject) = new VariantDouble((yysemantic_stack_[(1) - (1)].double_type));
    }
    break;

  case 24:

    {
      (yyval.variantObject) = new VariantDouble(StringUtils::doubleDecimal(*(yysemantic_stack_[(1) - (1)].str)));
    }
    break;

  case 25:

    {
      (yyval.variantObject) = new VariantInt32((yysemantic_stack_[(1) - (1)].int32_type));
    }
    break;

  case 26:

    {
      (yyval.variantObject) = new VariantInt32(StringUtils::int32(*(yysemantic_stack_[(1) - (1)].str)));
    }
    break;

  case 27:

    {
      (yyval.variantObject) = new VariantInt64((yysemantic_stack_[(1) - (1)].int64_type));
    }
    break;

  case 28:

    {
      (yyval.variantObject) = new VariantInt64(StringUtils::int64(*(yysemantic_stack_[(1) - (1)].str)));
    }
    break;

  case 29:

    {
      (yyval.variantObject) = new VariantString(*(yysemantic_stack_[(1) - (1)].str));
      delete (yysemantic_stack_[(1) - (1)].str);
    }
    break;

  case 30:

    {
      (yyval.variantObject) = new VariantBoolean(false);
    }
    break;

  case 31:

    {
      (yyval.variantObject) = new VariantBoolean(true);
    }
    break;

  case 32:

    {
      (yyval.variantObject) = new VariantNull();
    }
    break;

  case 33:

    {
      (yyval.variantObject) = new VariantUInt32((yysemantic_stack_[(1) - (1)].uint32_type));
    }
    break;

  case 34:

    {
      (yyval.variantObject) = new VariantUInt32(StringUtils::uint32(*(yysemantic_stack_[(1) - (1)].str)));
    }
    break;

  case 35:

    {
      (yyval.variantObject) = new VariantUInt64((yysemantic_stack_[(1) - (1)].uint64_type));
    }
    break;

  case 36:

    {
      (yyval.variantObject) = new VariantUInt64(StringUtils::uint64(*(yysemantic_stack_[(1) - (1)].str)));
    }
    break;

  case 37:

    {
      (yyval.variantObject) = (yysemantic_stack_[(1) - (1)].variantArray);
    }
    break;

  case 38:

    {
      (yyval.variantObject) = (yysemantic_stack_[(1) - (1)].variantVector);
    }
    break;



	default:
          break;
      }
    /* User semantic actions sometimes alter yychar, and that requires
       that yytoken be updated with the new translation.  We take the
       approach of translating immediately before every use of yytoken.
       One alternative is translating here after every semantic action,
       but that translation would be missed if the semantic action
       invokes YYABORT, YYACCEPT, or YYERROR immediately after altering
       yychar.  In the case of YYABORT or YYACCEPT, an incorrect
       destructor might then be invoked immediately.  In the case of
       YYERROR, subsequent parser actions might lead to an incorrect
       destructor call or verbose syntax error message before the
       lookahead is translated.  */
    YY_SYMBOL_PRINT ("-> $$ =", yyr1_[yyn], &yyval, &yyloc);

    yypop_ (yylen);
    yylen = 0;
    YY_STACK_PRINT ();

    yysemantic_stack_.push (yyval);
    yylocation_stack_.push (yyloc);

    /* Shift the result of the reduction.  */
    yyn = yyr1_[yyn];
    yystate = yypgoto_[yyn - yyntokens_] + yystate_stack_[0];
    if (0 <= yystate && yystate <= yylast_
	&& yycheck_[yystate] == yystate_stack_[0])
      yystate = yytable_[yystate];
    else
      yystate = yydefgoto_[yyn - yyntokens_];
    goto yynewstate;

  /*------------------------------------.
  | yyerrlab -- here on detecting error |
  `------------------------------------*/
  yyerrlab:
    /* Make sure we have latest lookahead translation.  See comments at
       user semantic actions for why this is necessary.  */
    yytoken = yytranslate_ (yychar);

    /* If not already recovering from an error, report this error.  */
    if (!yyerrstatus_)
      {
	++yynerrs_;
	if (yychar == yyempty_)
	  yytoken = yyempty_;
	error (yylloc, yysyntax_error_ (yystate, yytoken));
      }

    yyerror_range[1] = yylloc;
    if (yyerrstatus_ == 3)
      {
	/* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

	if (yychar <= yyeof_)
	  {
	  /* Return failure if at end of input.  */
	  if (yychar == yyeof_)
	    YYABORT;
	  }
	else
	  {
	    yydestruct_ ("Error: discarding", yytoken, &yylval, &yylloc);
	    yychar = yyempty_;
	  }
      }

    /* Else will try to reuse lookahead token after shifting the error
       token.  */
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:

    /* Pacify compilers like GCC when the user code never invokes
       YYERROR and the label yyerrorlab therefore never appears in user
       code.  */
    if (false)
      goto yyerrorlab;

    yyerror_range[1] = yylocation_stack_[yylen - 1];
    /* Do not reclaim the symbols of the rule which action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    yystate = yystate_stack_[0];
    goto yyerrlab1;

  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;	/* Each real token shifted decrements this.  */

    for (;;)
      {
	yyn = yypact_[yystate];
	if (!yy_pact_value_is_default_ (yyn))
	{
	  yyn += yyterror_;
	  if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == yyterror_)
	    {
	      yyn = yytable_[yyn];
	      if (0 < yyn)
		break;
	    }
	}

	/* Pop the current state because it cannot handle the error token.  */
	if (yystate_stack_.height () == 1)
	YYABORT;

	yyerror_range[1] = yylocation_stack_[0];
	yydestruct_ ("Error: popping",
		     yystos_[yystate],
		     &yysemantic_stack_[0], &yylocation_stack_[0]);
	yypop_ ();
	yystate = yystate_stack_[0];
	YY_STACK_PRINT ();
      }

    yyerror_range[2] = yylloc;
    // Using YYLLOC is tempting, but would change the location of
    // the lookahead.  YYLOC is available though.
    YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
    yysemantic_stack_.push (yylval);
    yylocation_stack_.push (yyloc);

    /* Shift the error token.  */
    YY_SYMBOL_PRINT ("Shifting", yystos_[yyn],
		     &yysemantic_stack_[0], &yylocation_stack_[0]);

    yystate = yyn;
    goto yynewstate;

    /* Accept.  */
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;

    /* Abort.  */
  yyabortlab:
    yyresult = 1;
    goto yyreturn;

  yyreturn:
    if (yychar != yyempty_)
      {
        /* Make sure we have latest lookahead translation.  See comments
           at user semantic actions for why this is necessary.  */
        yytoken = yytranslate_ (yychar);
        yydestruct_ ("Cleanup: discarding lookahead", yytoken, &yylval,
                     &yylloc);
      }

    /* Do not reclaim the symbols of the rule which action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    while (yystate_stack_.height () != 1)
      {
	yydestruct_ ("Cleanup: popping",
		   yystos_[yystate_stack_[0]],
		   &yysemantic_stack_[0],
		   &yylocation_stack_[0]);
	yypop_ ();
      }

    return yyresult;
  }

  // Generate an error message.
  std::string
  JsonParserX::yysyntax_error_ (int yystate, int yytoken)
  {
    std::string yyres;
    // Number of reported tokens (one for the "unexpected", one per
    // "expected").
    size_t yycount = 0;
    // Its maximum.
    enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
    // Arguments of yyformat.
    char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];

    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yytoken) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yychar.
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state
         merging (from LALR or IELR) and default reductions corrupt the
         expected token list.  However, the list is correct for
         canonical LR with one exception: it will still contain any
         token that will not be accepted due to an error action in a
         later state.
    */
    if (yytoken != yyempty_)
      {
        yyarg[yycount++] = yytname_[yytoken];
        int yyn = yypact_[yystate];
        if (!yy_pact_value_is_default_ (yyn))
          {
            /* Start YYX at -YYN if negative to avoid negative indexes in
               YYCHECK.  In other words, skip the first -YYN actions for
               this state because they are default actions.  */
            int yyxbegin = yyn < 0 ? -yyn : 0;
            /* Stay within bounds of both yycheck and yytname.  */
            int yychecklim = yylast_ - yyn + 1;
            int yyxend = yychecklim < yyntokens_ ? yychecklim : yyntokens_;
            for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
              if (yycheck_[yyx + yyn] == yyx && yyx != yyterror_
                  && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
                {
                  if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                    {
                      yycount = 1;
                      break;
                    }
                  else
                    yyarg[yycount++] = yytname_[yyx];
                }
          }
      }

    char const* yyformat = 0;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
        YYCASE_(0, YY_("syntax error"));
        YYCASE_(1, YY_("syntax error, unexpected %s"));
        YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    // Argument number.
    size_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += yytnamerr_ (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
  const signed char JsonParserX::yypact_ninf_ = -27;
  const signed char
  JsonParserX::yypact_[] =
  {
        44,   -27,   -27,   -27,   -27,   -27,   -27,   -27,   -27,     7,
      -3,   -27,     2,   -27,   -27,    -7,   -27,     1,   -27,   -27,
     -27,   -27,   -27,   -27,   -27,   -27,   -27,   -27,   -27,   -27,
     -27,   -27,   -27,   -27,   -27,     5,   -27,   -27,    32,   -27,
       4,   -27,    32,   -27,    -1,   -27,    32,   -27
  };

  /* YYDEFACT[S] -- default reduction number in state S.  Performed when
     YYTABLE doesn't specify something else to do.  Zero means the
     default is an error.  */
  const unsigned char
  JsonParserX::yydefact_[] =
  {
         0,     3,     6,     7,     8,    10,    11,     4,     5,     0,
       0,     9,     0,     2,    12,     0,    14,     0,    23,    24,
      25,    26,    27,    28,    29,    33,    34,    35,    36,    16,
      30,    32,    31,    37,    38,     0,    20,     1,     0,    13,
      18,    15,    21,    17,     0,    22,     0,    19
  };

  /* YYPGOTO[NTERM-NUM].  */
  const signed char
  JsonParserX::yypgoto_[] =
  {
       -27,   -27,    25,    27,   -27,   -27,   -26
  };

  /* YYDEFGOTO[NTERM-NUM].  */
  const signed char
  JsonParserX::yydefgoto_[] =
  {
        -1,    12,    33,    34,    17,    35,    36
  };

  /* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule which
     number is the opposite.  If YYTABLE_NINF_, syntax error.  */
  const signed char JsonParserX::yytable_ninf_ = -1;
  const unsigned char
  JsonParserX::yytable_[] =
  {
        18,    19,    37,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    43,    38,    44,    29,    45,    15,    39,    46,
      47,    30,    40,    41,    16,    13,    42,    14,    31,     9,
      10,     0,     0,     0,     0,    18,    19,    32,    20,    21,
      22,    23,    24,    25,    26,    27,    28,     1,     0,     0,
       2,     0,     3,     0,     4,     5,    30,     6,     0,     0,
       0,     0,     0,    31,     9,    10,     0,     0,     7,     0,
       0,     0,    32,     0,     0,     8,     9,    10,     0,     0,
       0,     0,     0,     0,    11
  };

  /* YYCHECK.  */
  const signed char
  JsonParserX::yycheck_[] =
  {
         3,     4,     0,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    38,    20,    10,    18,    42,    10,    17,    20,
      46,    24,    21,    18,    17,     0,    21,     0,    31,    32,
      33,    -1,    -1,    -1,    -1,     3,     4,    40,     6,     7,
       8,     9,    10,    11,    12,    13,    14,     3,    -1,    -1,
       6,    -1,     8,    -1,    10,    11,    24,    13,    -1,    -1,
      -1,    -1,    -1,    31,    32,    33,    -1,    -1,    24,    -1,
      -1,    -1,    40,    -1,    -1,    31,    32,    33,    -1,    -1,
      -1,    -1,    -1,    -1,    40
  };

  /* STOS_[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
  const unsigned char
  JsonParserX::yystos_[] =
  {
         0,     3,     6,     8,    10,    11,    13,    24,    31,    32,
      33,    40,    45,    46,    47,    10,    17,    48,     3,     4,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    18,
      24,    31,    40,    46,    47,    49,    50,     0,    20,    17,
      21,    18,    21,    50,    10,    50,    20,    50
  };

#if YYDEBUG
  /* TOKEN_NUMBER_[YYLEX-NUM] -- Internal symbol number corresponding
     to YYLEX-NUM.  */
  const unsigned short int
  JsonParserX::yytoken_number_[] =
  {
         0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298
  };
#endif

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
  const unsigned char
  JsonParserX::yyr1_[] =
  {
         0,    44,    45,    45,    45,    45,    45,    45,    45,    45,
      45,    45,    45,    46,    46,    47,    47,    48,    48,    48,
      49,    49,    49,    50,    50,    50,    50,    50,    50,    50,
      50,    50,    50,    50,    50,    50,    50,    50,    50
  };

  /* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
  const unsigned char
  JsonParserX::yyr2_[] =
  {
         0,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     3,     2,     3,     2,     3,     2,     5,
       1,     2,     3,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1
  };

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
  /* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
     First, the terminals, then, starting at \a yyntokens_, nonterminals.  */
  const char*
  const JsonParserX::yytname_[] =
  {
    "\"end of file\"", "error", "$undefined", "\"decimal constant\"",
  "\"decimal constant string\"", "\"identifier\"",
  "\"signed integer constant\"", "\"signed integer constant string\"",
  "\"signed long integer constant\"",
  "\"signed long integer constant string\"", "\"string constant\"",
  "\"unsigned integer constant\"", "\"unsigned integer constant string\"",
  "\"unsigned long integer constant\"",
  "\"unsigned long integer constant string\"", "\"&&\"", "\":=\"", "\"}\"",
  "\"]\"", "\")\"", "\":\"", "\",\"", "\".\"", "\"==\"", "\"false\"",
  "\">=\"", "\">\"", "\"<=\"", "\"<\"", "\"-\"", "\"<>\"", "\"null\"",
  "\"{\"", "\"[\"", "\"(\"", "\"||\"", "\"+\"", "\"/\"", "\";\"", "\"*\"",
  "\"true\"", "\"string_constant_null\"", "\"unquoted string\"",
  "NEGATION", "$accept", "jsonDefinition", "variantArray", "variantVector",
  "keyValueList", "valueList", "variantObject", 0
  };
#endif

#if YYDEBUG
  /* YYRHS -- A `-1'-separated list of the rules' RHS.  */
  const JsonParserX::rhs_number_type
  JsonParserX::yyrhs_[] =
  {
        45,     0,    -1,    46,    -1,     3,    -1,    24,    -1,    31,
      -1,     6,    -1,     8,    -1,    10,    -1,    40,    -1,    11,
      -1,    13,    -1,    47,    -1,    32,    48,    17,    -1,    32,
      17,    -1,    33,    49,    18,    -1,    33,    18,    -1,    10,
      20,    50,    -1,    48,    21,    -1,    48,    21,    10,    20,
      50,    -1,    50,    -1,    49,    21,    -1,    49,    21,    50,
      -1,     3,    -1,     4,    -1,     6,    -1,     7,    -1,     8,
      -1,     9,    -1,    10,    -1,    24,    -1,    40,    -1,    31,
      -1,    11,    -1,    12,    -1,    13,    -1,    14,    -1,    46,
      -1,    47,    -1
  };

  /* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
     YYRHS.  */
  const unsigned char
  JsonParserX::yyprhs_[] =
  {
         0,     0,     3,     5,     7,     9,    11,    13,    15,    17,
      19,    21,    23,    25,    29,    32,    36,    39,    43,    46,
      52,    54,    57,    61,    63,    65,    67,    69,    71,    73,
      75,    77,    79,    81,    83,    85,    87,    89,    91
  };

  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
  const unsigned short int
  JsonParserX::yyrline_[] =
  {
         0,   189,   189,   193,   197,   201,   205,   209,   213,   218,
     222,   226,   230,   237,   241,   248,   252,   259,   265,   269,
     277,   282,   286,   293,   297,   301,   305,   309,   313,   317,
     322,   326,   330,   334,   338,   342,   346,   350,   354
  };

  // Print the state stack on the debug stream.
  void
  JsonParserX::yystack_print_ ()
  {
    *yycdebug_ << "Stack now";
    for (state_stack_type::const_iterator i = yystate_stack_.begin ();
	 i != yystate_stack_.end (); ++i)
      *yycdebug_ << ' ' << *i;
    *yycdebug_ << std::endl;
  }

  // Report on the debug stream that the rule \a yyrule is going to be reduced.
  void
  JsonParserX::yy_reduce_print_ (int yyrule)
  {
    unsigned int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    /* Print the symbols being reduced, and their result.  */
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
	       << " (line " << yylno << "):" << std::endl;
    /* The symbols being reduced.  */
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
		       yyrhs_[yyprhs_[yyrule] + yyi],
		       &(yysemantic_stack_[(yynrhs) - (yyi + 1)]),
		       &(yylocation_stack_[(yynrhs) - (yyi + 1)]));
  }
#endif // YYDEBUG

  /* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
  JsonParserX::token_number_type
  JsonParserX::yytranslate_ (int t)
  {
    static
    const token_number_type
    translate_table[] =
    {
           0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43
    };
    if ((unsigned int) t <= yyuser_token_number_max_)
      return translate_table[t];
    else
      return yyundef_token_;
  }

  const int JsonParserX::yyeof_ = 0;
  const int JsonParserX::yylast_ = 84;
  const int JsonParserX::yynnts_ = 7;
  const int JsonParserX::yyempty_ = -2;
  const int JsonParserX::yyfinal_ = 37;
  const int JsonParserX::yyterror_ = 1;
  const int JsonParserX::yyerrcode_ = 256;
  const int JsonParserX::yyntokens_ = 44;

  const unsigned int JsonParserX::yyuser_token_number_max_ = 298;
  const JsonParserX::token_number_type JsonParserX::yyundef_token_ = 2;


} } // triagens::json_parser





// .............................................................................
// postamble
// .............................................................................

void triagens::json_parser::JsonParserX::error (const triagens::json_parser::JsonParserX::location_type& l, const string& m) {
  triagens::json_parser::position last = l.end - 1;
  driver.setError(last.line, last.column, m);
}

