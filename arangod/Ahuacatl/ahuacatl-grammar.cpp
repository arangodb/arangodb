/* A Bison parser, made by GNU Bison 2.7.12-4996.  */

/* Skeleton implementation for Bison LALR(1) parsers in C++
   
      Copyright (C) 2002-2013 Free Software Foundation, Inc.
   
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

// Take the name prefix into account.
#define yylex   Ahuacatllex

/* First part of user declarations.  */


#include <stdio.h>
#include <stdlib.h>

#include <BasicsC/common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/tri-strings.h>

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-error.h"
#include "Ahuacatl/ahuacatl-parser.h"
#include "Ahuacatl/ahuacatl-parser-functions.h"
#include "Ahuacatl/ahuacatl-scope.h"




#include "ahuacatl-grammar.h"

/* User implementation prologue.  */



////////////////////////////////////////////////////////////////////////////////
/// @brief forward for lexer function defined in ahuacatl-tokens.l
////////////////////////////////////////////////////////////////////////////////

int Ahuacatllex (YYSTYPE*, YYLTYPE*, void*);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief register parse error
////////////////////////////////////////////////////////////////////////////////

void Ahuacatlerror (YYLTYPE* locp, TRI_aql_context_t* const context, const char* err) {
  TRI_SetErrorParseAql(context, err, locp->first_line, locp->first_column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro for signalling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM                                                                     \
  TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_OUT_OF_MEMORY, NULL); \
  YYABORT;

#define scanner context->_parser->_scanner





# ifndef YY_NULL
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULL nullptr
#  else
#   define YY_NULL 0
#  endif
# endif

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

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

# ifndef YYLLOC_DEFAULT
#  define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
        }                                                               \
    while (/*CONSTCOND*/ false)
# endif


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
# define YY_SYMBOL_PRINT(Title, Type, Value, Location) YYUSE(Type)
# define YY_REDUCE_PRINT(Rule)        static_cast<void>(0)
# define YY_STACK_PRINT()             static_cast<void>(0)

#endif /* !YYDEBUG */

#define yyerrok		(yyerrstatus_ = 0)
#define yyclearin	(yychar = yyempty_)

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)


namespace Ahuacatl {


  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  parser::yytnamerr_ (const char *yystr)
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
  parser::parser (TRI_aql_context_t* const context_yyarg)
    :
#if YYDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      context (context_yyarg)
  {
  }

  parser::~parser ()
  {
  }

#if YYDEBUG
  /*--------------------------------.
  | Print this symbol on YYOUTPUT.  |
  `--------------------------------*/

  inline void
  parser::yy_symbol_value_print_ (int yytype,
			   const semantic_type* yyvaluep, const location_type* yylocationp)
  {
    YYUSE (yylocationp);
    YYUSE (yyvaluep);
    std::ostream& yyo = debug_stream ();
    std::ostream& yyoutput = yyo;
    YYUSE (yyoutput);
    YYUSE (yytype);
  }


  void
  parser::yy_symbol_print_ (int yytype,
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
  parser::yydestruct_ (const char* yymsg,
			   int yytype, semantic_type* yyvaluep, location_type* yylocationp)
  {
    YYUSE (yylocationp);
    YYUSE (yymsg);
    YYUSE (yyvaluep);

    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

    YYUSE (yytype);
  }

  void
  parser::yypop_ (unsigned int n)
  {
    yystate_stack_.pop (n);
    yysemantic_stack_.pop (n);
    yylocation_stack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  parser::debug_level_type
  parser::debug_level () const
  {
    return yydebug_;
  }

  void
  parser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif

  inline bool
  parser::yy_pact_value_is_default_ (int yyvalue)
  {
    return yyvalue == yypact_ninf_;
  }

  inline bool
  parser::yy_table_value_is_error_ (int yyvalue)
  {
    return yyvalue == yytable_ninf_;
  }

  int
  parser::parse ()
  {
    /// Lookahead and lookahead in internal form.
    int yychar = yyempty_;
    int yytoken = 0;

    // State.
    int yyn;
    int yylen = 0;
    int yystate = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// Semantic value of the lookahead.
    static semantic_type yyval_default;
    semantic_type yylval = yyval_default;
    /// Location of the lookahead.
    location_type yylloc;
    /// The locations where the error started and ended.
    location_type yyerror_range[3];

    /// $$.
    semantic_type yyval;
    /// @$.
    location_type yyloc;

    int yyresult;

    // FIXME: This shoud be completely indented.  It is not yet to
    // avoid gratuitous conflicts when merging into the master branch.
    try
      {
    YYCDEBUG << "Starting parse" << std::endl;


    /* Initialize the stacks.  The initial state will be pushed in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystate_stack_.clear ();
    yysemantic_stack_.clear ();
    yylocation_stack_.clear ();
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
        yychar = yylex (&yylval, &yylloc, scanner);
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

    // Compute the default @$.
    {
      slice<location_type, location_stack_type> slice (yylocation_stack_, yylen);
      YYLLOC_DEFAULT (yyloc, slice, yylen);
    }

    // Perform the reduction.
    YY_REDUCE_PRINT (yyn);
    switch (yyn)
      {
          case 2:

    {
    }
    break;

  case 3:

    {
    }
    break;

  case 4:

    {
    }
    break;

  case 5:

    {
    }
    break;

  case 6:

    {
    }
    break;

  case 7:

    {
    }
    break;

  case 8:

    {
    }
    break;

  case 9:

    {
    }
    break;

  case 10:

    {
    }
    break;

  case 11:

    {
      TRI_aql_node_t* node;
      
      if (! TRI_StartScopeAql(context, TRI_AQL_SCOPE_FOR)) {
        ABORT_OOM
      }
      
      node = TRI_CreateNodeForAql(context, (yysemantic_stack_[(4) - (2)].strval), (yysemantic_stack_[(4) - (4)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 12:

    {
      TRI_aql_node_t* node = TRI_CreateNodeFilterAql(context, (yysemantic_stack_[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 13:

    {
    }
    break;

  case 14:

    {
    }
    break;

  case 15:

    {
    }
    break;

  case 16:

    {
      TRI_aql_node_t* node = TRI_CreateNodeLetAql(context, (yysemantic_stack_[(3) - (1)].strval), (yysemantic_stack_[(3) - (3)].node));

      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 17:

    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 18:

    {
      TRI_aql_node_t* node = TRI_CreateNodeCollectAql(context, TRI_PopStackParseAql(context), (yysemantic_stack_[(4) - (4)].strval));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 19:

    {
    }
    break;

  case 20:

    {
    }
    break;

  case 21:

    {
      TRI_aql_node_t* node = TRI_CreateNodeAssignAql(context, (yysemantic_stack_[(3) - (1)].strval), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      if (! TRI_PushListAql(context, node)) {
        ABORT_OOM
      }
    }
    break;

  case 22:

    {
      (yyval.strval) = NULL;
    }
    break;

  case 23:

    {
      (yyval.strval) = (yysemantic_stack_[(2) - (2)].strval);
    }
    break;

  case 24:

    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 25:

    {
      TRI_aql_node_t* list = TRI_PopStackParseAql(context);
      TRI_aql_node_t* node = TRI_CreateNodeSortAql(context, list);
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 26:

    {
      if (! TRI_PushListAql(context, (yysemantic_stack_[(1) - (1)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 27:

    {
      if (! TRI_PushListAql(context, (yysemantic_stack_[(3) - (3)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 28:

    {
      TRI_aql_node_t* node = TRI_CreateNodeSortElementAql(context, (yysemantic_stack_[(2) - (1)].node), (yysemantic_stack_[(2) - (2)].boolval));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 29:

    {
      (yyval.boolval) = true;
    }
    break;

  case 30:

    {
      (yyval.boolval) = true;
    }
    break;

  case 31:

    {
      (yyval.boolval) = false;
    }
    break;

  case 32:

    {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, TRI_CreateNodeValueIntAql(context, 0), (yysemantic_stack_[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
    }
    break;

  case 33:

    {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, (yysemantic_stack_[(4) - (2)].node), (yysemantic_stack_[(4) - (4)].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
    break;

  case 34:

    {
      TRI_aql_node_t* node = TRI_CreateNodeReturnAql(context, (yysemantic_stack_[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      if (! TRI_EndScopeByReturnAql(context)) {
        ABORT_OOM
      }
      
      // $$ = node;
    }
    break;

  case 35:

    {
      (yyval.node) = (yysemantic_stack_[(3) - (2)].node);
    }
    break;

  case 36:

    {
      if (! TRI_StartScopeAql(context, TRI_AQL_SCOPE_SUBQUERY)) {
        ABORT_OOM
      }
    }
    break;

  case 37:

    {
      TRI_aql_node_t* result;
      TRI_aql_node_t* subQuery;
      TRI_aql_node_t* nameNode;
      
      if (! TRI_EndScopeAql(context)) {
        ABORT_OOM
      }

      subQuery = TRI_CreateNodeSubqueryAql(context);

      if (subQuery == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, subQuery)) {
        ABORT_OOM
      }

      nameNode = TRI_AQL_NODE_MEMBER(subQuery, 0);
      if (nameNode == NULL) {
        ABORT_OOM
      }

      result = TRI_CreateNodeReferenceAql(context, TRI_AQL_NODE_STRING(nameNode));
      if (result == NULL) {
        ABORT_OOM
      }

      // return the result
      (yyval.node) = result;
    }
    break;

  case 38:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 39:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 40:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 41:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 42:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 43:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 44:

    {
      TRI_aql_node_t* node;
      TRI_aql_node_t* list;

      if ((yysemantic_stack_[(3) - (1)].node) == NULL || (yysemantic_stack_[(3) - (3)].node) == NULL) {
        ABORT_OOM
      }
      
      list = TRI_CreateNodeListAql(context);
      if (list == NULL) {
        ABORT_OOM
      }
       
      if (TRI_ERROR_NO_ERROR != TRI_PushBackVectorPointer(&list->_members, (void*) (yysemantic_stack_[(3) - (1)].node))) {
        ABORT_OOM
      }
      if (TRI_ERROR_NO_ERROR != TRI_PushBackVectorPointer(&list->_members, (void*) (yysemantic_stack_[(3) - (3)].node))) {
        ABORT_OOM
      }
      
      node = TRI_CreateNodeFcallAql(context, "RANGE", list);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 45:

    {
      (yyval.strval) = (yysemantic_stack_[(1) - (1)].strval);

      if ((yyval.strval) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 46:

    {
      if ((yysemantic_stack_[(3) - (1)].strval) == NULL || (yysemantic_stack_[(3) - (3)].strval) == NULL) {
        ABORT_OOM
      }

      (yyval.strval) = TRI_RegisterString3Aql(context, (yysemantic_stack_[(3) - (1)].strval), "::", (yysemantic_stack_[(3) - (3)].strval));

      if ((yyval.strval) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 47:

    {
      TRI_aql_node_t* node;

      if (! TRI_PushStackParseAql(context, (yysemantic_stack_[(1) - (1)].strval))) {
        ABORT_OOM
      }

      node = TRI_CreateNodeListAql(context);
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 48:

    {
      TRI_aql_node_t* list = TRI_PopStackParseAql(context);
      TRI_aql_node_t* node = TRI_CreateNodeFcallAql(context, TRI_PopStackParseAql(context), list);
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 49:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryPlusAql(context, (yysemantic_stack_[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 50:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryMinusAql(context, (yysemantic_stack_[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 51:

    { 
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryNotAql(context, (yysemantic_stack_[(2) - (2)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 52:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryOrAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 53:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryAndAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 54:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryPlusAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 55:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryMinusAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 56:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryTimesAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 57:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryDivAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 58:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryModAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 59:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryEqAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 60:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryNeAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 61:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLtAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 62:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGtAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 63:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLeAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 64:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGeAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 65:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryInAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 66:

    {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorTernaryAql(context, (yysemantic_stack_[(5) - (1)].node), (yysemantic_stack_[(5) - (3)].node), (yysemantic_stack_[(5) - (5)].node));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 67:

    {
    }
    break;

  case 68:

    {
    }
    break;

  case 69:

    {
      if (! TRI_PushListAql(context, (yysemantic_stack_[(1) - (1)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 70:

    {
      if (! TRI_PushListAql(context, (yysemantic_stack_[(3) - (3)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 71:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 72:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 73:

    {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 74:

    {
      (yyval.node) = TRI_PopStackParseAql(context);
    }
    break;

  case 75:

    {
    }
    break;

  case 76:

    {
    }
    break;

  case 77:

    {
      if (! TRI_PushListAql(context, (yysemantic_stack_[(1) - (1)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 78:

    {
      if (! TRI_PushListAql(context, (yysemantic_stack_[(3) - (3)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 79:

    {
      TRI_aql_node_t* node = TRI_CreateNodeArrayAql(context);
      if (node == NULL) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    }
    break;

  case 80:

    {
      (yyval.node) = TRI_PopStackParseAql(context);
    }
    break;

  case 81:

    {
    }
    break;

  case 82:

    {
    }
    break;

  case 83:

    {
    }
    break;

  case 84:

    {
    }
    break;

  case 85:

    {
      if (! TRI_PushArrayAql(context, (yysemantic_stack_[(3) - (1)].strval), (yysemantic_stack_[(3) - (3)].node))) {
        ABORT_OOM
      }
    }
    break;

  case 86:

    {
      // start of reference (collection or variable name)
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 87:

    {
      // expanded variable access, e.g. variable[*]
      TRI_aql_node_t* node;
      char* varname = TRI_GetNameParseAql(context);

      if (varname == NULL) {
        ABORT_OOM
      }
      
      // push the varname onto the stack
      TRI_PushStackParseAql(context, varname);
      
      // push on the stack what's going to be expanded (will be popped when we come back) 
      TRI_PushStackParseAql(context, (yysemantic_stack_[(1) - (1)].node));

      // create a temporary variable for the row iterator (will be popped by "expansion" rule")
      node = TRI_CreateNodeReferenceAql(context, varname);

      if (node == NULL) {
        ABORT_OOM
      }

      // push the variable
      TRI_PushStackParseAql(context, node);
    }
    break;

  case 88:

    {
      // return from the "expansion" subrule
      TRI_aql_node_t* expanded = TRI_PopStackParseAql(context);
      TRI_aql_node_t* expand;
      TRI_aql_node_t* nameNode;
      char* varname = TRI_PopStackParseAql(context);

      // push the actual expand node into the statement list
      expand = TRI_CreateNodeExpandAql(context, varname, expanded, (yysemantic_stack_[(4) - (4)].node));

      if (expand == NULL) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, expand)) {
        ABORT_OOM
      }

      nameNode = TRI_AQL_NODE_MEMBER(expand, 1);

      if (nameNode == NULL) {
        ABORT_OOM
      }

      // return a reference only
      (yyval.node) = TRI_CreateNodeReferenceAql(context, TRI_AQL_NODE_STRING(nameNode));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 89:

    {
      // variable or collection
      TRI_aql_node_t* node;
      
      if (TRI_VariableExistsScopeAql(context, (yysemantic_stack_[(1) - (1)].strval))) {
        node = TRI_CreateNodeReferenceAql(context, (yysemantic_stack_[(1) - (1)].strval));
      }
      else {
        node = TRI_CreateNodeCollectionAql(context, (yysemantic_stack_[(1) - (1)].strval));
      }

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 90:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 91:

    {
      // named variable access, e.g. variable.reference
      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].strval));
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 92:

    {
      // named variable access, e.g. variable.@reference
      (yyval.node) = TRI_CreateNodeBoundAttributeAccessAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 93:

    {
      // indexed variable access, e.g. variable[index]
      (yyval.node) = TRI_CreateNodeIndexedAql(context, (yysemantic_stack_[(4) - (1)].node), (yysemantic_stack_[(4) - (3)].node));
      
      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 94:

    {
      // named variable access, continuation from * expansion, e.g. [*].variable.reference
      TRI_aql_node_t* node = TRI_PopStackParseAql(context);

      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, node, (yysemantic_stack_[(2) - (2)].strval));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 95:

    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.@reference
      TRI_aql_node_t* node = TRI_PopStackParseAql(context);

      (yyval.node) = TRI_CreateNodeBoundAttributeAccessAql(context, node, (yysemantic_stack_[(2) - (2)].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 96:

    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable[index]
      TRI_aql_node_t* node = TRI_PopStackParseAql(context);

      (yyval.node) = TRI_CreateNodeIndexedAql(context, node, (yysemantic_stack_[(3) - (2)].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 97:

    {
      // named variable access, continuation from * expansion, e.g. [*].variable.xx.reference
      (yyval.node) = TRI_CreateNodeAttributeAccessAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].strval));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 98:

    {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.xx.@reference
      (yyval.node) = TRI_CreateNodeBoundAttributeAccessAql(context, (yysemantic_stack_[(3) - (1)].node), (yysemantic_stack_[(3) - (3)].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 99:

    {
      // indexed variable access, continuation from * expansion, e.g. [*].variable.xx.[index]
      (yyval.node) = TRI_CreateNodeIndexedAql(context, (yysemantic_stack_[(4) - (1)].node), (yysemantic_stack_[(4) - (3)].node));

      if ((yyval.node) == NULL) {
        ABORT_OOM
      }
    }
    break;

  case 100:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 101:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 102:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 103:

    {
      TRI_aql_node_t* node;
      double value;

      if ((yysemantic_stack_[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }
      
      value = TRI_DoubleString((yysemantic_stack_[(1) - (1)].strval));

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
        YYABORT;
      }
      
      node = TRI_CreateNodeValueDoubleAql(context, value);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 104:

    {
      TRI_aql_node_t* node = TRI_CreateNodeValueStringAql(context, (yysemantic_stack_[(1) - (1)].strval));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 105:

    {
      (yyval.node) = (yysemantic_stack_[(1) - (1)].node);
    }
    break;

  case 106:

    {
      TRI_aql_node_t* node = TRI_CreateNodeValueNullAql(context);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 107:

    {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, true);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 108:

    {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, false);

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 109:

    {
      TRI_aql_node_t* node = TRI_CreateNodeParameterAql(context, (yysemantic_stack_[(1) - (1)].strval));

      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
    }
    break;

  case 110:

    {
      if ((yysemantic_stack_[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }

      (yyval.strval) = (yysemantic_stack_[(1) - (1)].strval);
    }
    break;

  case 111:

    {
      if ((yysemantic_stack_[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }

      (yyval.strval) = (yysemantic_stack_[(1) - (1)].strval);
    }
    break;

  case 112:

    {
      (yyval.strval) = (yysemantic_stack_[(1) - (1)].strval);
    }
    break;

  case 113:

    {
      TRI_aql_node_t* node;
      int64_t value;

      if ((yysemantic_stack_[(1) - (1)].strval) == NULL) {
        ABORT_OOM
      }

      value = TRI_Int64String((yysemantic_stack_[(1) - (1)].strval));
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
        YYABORT;
      }

      node = TRI_CreateNodeValueIntAql(context, value);
      if (node == NULL) {
        ABORT_OOM
      }

      (yyval.node) = node;
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
    while (1 < yystate_stack_.height ())
      {
        yydestruct_ ("Cleanup: popping",
                     yystos_[yystate_stack_[0]],
                     &yysemantic_stack_[0],
                     &yylocation_stack_[0]);
        yypop_ ();
      }

    return yyresult;
    }
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack"
                 << std::endl;
        // Do not try to display the values of the reclaimed symbols,
        // as their printer might throw an exception.
        if (yychar != yyempty_)
          {
            /* Make sure we have latest lookahead translation.  See
               comments at user semantic actions for why this is
               necessary.  */
            yytoken = yytranslate_ (yychar);
            yydestruct_ (YY_NULL, yytoken, &yylval, &yylloc);
          }

        while (1 < yystate_stack_.height ())
          {
            yydestruct_ (YY_NULL,
                         yystos_[yystate_stack_[0]],
                         &yysemantic_stack_[0],
                         &yylocation_stack_[0]);
            yypop_ ();
          }
        throw;
      }
  }

  // Generate an error message.
  std::string
  parser::yysyntax_error_ (int yystate, int yytoken)
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

    char const* yyformat = YY_NULL;
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
  const signed char parser::yypact_ninf_ = -75;
  const short int
  parser::yypact_[] =
  {
       -75,    30,   119,   -75,    32,    32,   174,   174,   -75,   -75,
      71,   -75,   -75,   -75,   -75,   -75,   -75,   -75,   -75,   -75,
      35,     9,   -75,    58,   -75,   -75,   -75,    39,   -75,   -75,
     -75,   -75,   174,   174,   174,   174,   -75,   -75,   283,    43,
     -75,   -75,   -75,   -75,   -75,   -75,   -75,    74,   -34,   -75,
     -75,   -75,   -75,   -75,   283,    32,   174,    67,   174,    32,
     174,   -75,   -75,   -75,   198,   -75,    76,   174,   174,   174,
     174,   174,   174,   174,   174,   174,   174,   174,   174,   174,
     174,   174,   174,   174,    95,    75,    82,   174,     1,     2,
     -75,   110,    91,   -75,   233,    71,   283,   -75,   283,   -75,
      73,   -75,   -75,    88,    93,   -75,    97,   283,    89,   100,
     -24,   344,   333,   319,   319,    24,    24,    24,    24,    80,
      80,   -75,   -75,   -75,   258,   308,   -75,   174,   -31,     7,
     -75,   -75,    32,    32,   -75,   174,   174,   -75,   -75,   -75,
     -75,   -75,   -75,    76,   174,   -75,   174,   174,   283,    99,
     102,   174,     4,   -30,   -75,   -75,   -75,   283,   -75,   -75,
     283,   283,   308,   -75,   174,    72,   -75,   -75,   174,    29,
     283,   -75,   139,   -75,   -75,   -75
  };

  /* YYDEFACT[S] -- default reduction number in state S.  Performed when
     YYTABLE doesn't specify something else to do.  Zero means the
     default is an error.  */
  const unsigned char
  parser::yydefact_[] =
  {
         3,     0,     0,     1,     0,     0,     0,     0,    17,    24,
       0,     4,     5,     7,     6,     8,     9,    10,     2,   112,
       0,    13,    14,     0,   106,   107,   108,    89,   104,   113,
     103,   109,     0,     0,     0,    36,    79,    73,    12,    47,
      90,    38,    39,    40,    41,    71,    72,    43,    86,    42,
     105,   100,   101,   102,    34,     0,     0,    32,     0,     0,
       0,    51,    49,    50,     0,     3,    81,    75,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    22,
      19,     0,    25,    26,    29,     0,    11,    15,    16,    35,
       0,   110,   111,     0,    82,    83,     0,    77,     0,    76,
      65,    53,    52,    59,    60,    61,    62,    63,    64,    54,
      55,    56,    57,    58,     0,    44,    46,    67,     0,     0,
      91,    92,     0,     0,    18,     0,     0,    30,    31,    28,
      33,    37,    80,     0,     0,    74,     0,     0,    69,     0,
      68,     0,     0,    88,    93,    23,    20,    21,    27,    84,
      85,    78,    66,    48,     0,     0,    94,    95,     0,     0,
      70,    96,     0,    97,    98,    99
  };

  /* YYPGOTO[NTERM-NUM].  */
  const signed char
  parser::yypgoto_[] =
  {
       -75,    81,   -75,   -75,   -75,   -75,   -75,   -75,    90,   -75,
     -75,   -75,     6,   -75,   -75,   -75,   -75,    11,   -75,   -75,
     -75,    -6,   -75,   -75,   -75,   -75,   -75,   -75,   -75,   -75,
     -75,   -75,   -75,   -75,   -75,   -75,   -75,   -75,   -75,   -75,
       5,   -75,   -75,   -75,   -75,    -7,   -75,   -75,   -74,   -75,
      -2,   -75
  };

  /* YYDEFGOTO[NTERM-NUM].  */
  const short int
  parser::yydefgoto_[] =
  {
        -1,     1,     2,    11,    12,    13,    14,    21,    22,    15,
      55,    89,    90,   134,    16,    56,    92,    93,   139,    17,
      18,    94,    65,    39,    40,    85,    41,    42,    43,   149,
     150,    44,    45,    67,   108,   109,    46,    66,   103,   104,
     105,    47,    86,    48,   153,    49,    50,    51,    52,   106,
      23,    53
  };

  /* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule which
     number is the opposite.  If YYTABLE_NINF_, syntax error.  */
  const signed char parser::yytable_ninf_ = -88;
  const short int
  parser::yytable_[] =
  {
        38,    54,    20,    57,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    87,   131,   132,   151,   168,   130,    68,
      88,   166,    31,   152,   169,    31,    61,    62,    63,    64,
       3,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,   133,    82,   173,    58,    83,    19,
      31,    59,    96,    91,    98,   154,    77,    78,    79,    80,
      81,   107,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   167,   -45,
      60,   129,   -45,    84,    68,    24,    25,    26,   140,    28,
      29,    30,    31,   101,   102,   174,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    95,
      82,   -87,   126,    83,    79,    80,    81,   141,   127,   128,
     171,   148,     4,     5,     6,     7,     8,     9,    10,   157,
     155,    91,   135,   136,   142,   143,   144,   145,   160,   156,
     161,   162,   146,   163,   164,   165,   100,   158,   159,    97,
       0,    68,     0,     0,     0,     0,     0,     0,   170,     0,
       0,     0,   172,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,     0,    82,     0,     0,
      83,     0,     0,     0,     0,     0,     0,   175,    24,    25,
      26,    27,    28,    29,    30,    31,     0,    32,     0,     0,
       0,     0,     0,     0,     0,     0,    33,    34,     0,     0,
      68,     0,     0,     0,     0,     0,     0,    35,     0,    36,
       0,    37,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,     0,    82,     0,     0,    83,
       0,     0,    99,   137,   138,    68,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      68,    82,     0,     0,    83,     0,     0,     0,     0,     0,
       0,     0,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    68,    82,   147,     0,    83,
       0,     0,     0,     0,     0,     0,     0,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      68,    82,     0,     0,    83,     0,     0,     0,     0,     0,
       0,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    68,    82,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    68,    69,     0,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81
  };

  /* YYCHECK.  */
  const short int
  parser::yycheck_[] =
  {
         6,     7,     4,    10,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    47,    88,    13,    47,    47,    17,    12,
      54,    17,    21,    54,    54,    21,    32,    33,    34,    35,
       0,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    42,    38,    17,    12,    41,    17,
      21,    42,    58,    55,    60,    48,    32,    33,    34,    35,
      36,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,   152,    40,
      22,    87,    43,    40,    12,    14,    15,    16,    95,    18,
      19,    20,    21,    17,    18,   169,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    42,
      38,    37,    17,    41,    34,    35,    36,    44,    43,    37,
      48,   127,     3,     4,     5,     6,     7,     8,     9,   135,
     132,   133,    22,    42,    46,    42,    39,    48,   144,   133,
     146,   147,    42,    44,    42,   151,    65,   136,   143,    59,
      -1,    12,    -1,    -1,    -1,    -1,    -1,    -1,   164,    -1,
      -1,    -1,   168,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    38,    -1,    -1,
      41,    -1,    -1,    -1,    -1,    -1,    -1,    48,    14,    15,
      16,    17,    18,    19,    20,    21,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    32,    33,    -1,    -1,
      12,    -1,    -1,    -1,    -1,    -1,    -1,    43,    -1,    45,
      -1,    47,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    -1,    38,    -1,    -1,    41,
      -1,    -1,    44,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      12,    38,    -1,    -1,    41,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    12,    38,    39,    -1,    41,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      12,    38,    -1,    -1,    41,    -1,    -1,    -1,    -1,    -1,
      -1,    12,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    12,    38,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    12,    24,    -1,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36
  };

  /* STOS_[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
  const unsigned char
  parser::yystos_[] =
  {
         0,    56,    57,     0,     3,     4,     5,     6,     7,     8,
       9,    58,    59,    60,    61,    64,    69,    74,    75,    17,
     105,    62,    63,   105,    14,    15,    16,    17,    18,    19,
      20,    21,    23,    32,    33,    43,    45,    47,    76,    78,
      79,    81,    82,    83,    86,    87,    91,    96,    98,   100,
     101,   102,   103,   106,    76,    65,    70,   100,    12,    42,
      22,    76,    76,    76,    76,    77,    92,    88,    12,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    38,    41,    40,    80,    97,    47,    54,    66,
      67,   105,    71,    72,    76,    42,    76,    63,    76,    44,
      56,    17,    18,    93,    94,    95,   104,    76,    89,    90,
      76,    76,    76,    76,    76,    76,    76,    76,    76,    76,
      76,    76,    76,    76,    76,    76,    17,    43,    37,    76,
      17,   103,    13,    42,    68,    22,    42,    10,    11,    73,
     100,    44,    46,    42,    39,    48,    42,    39,    76,    84,
      85,    47,    54,    99,    48,   105,    67,    76,    72,    95,
      76,    76,    76,    44,    42,    76,    17,   103,    47,    54,
      76,    48,    76,    17,   103,    48
  };

#if YYDEBUG
  /* TOKEN_NUMBER_[YYLEX-NUM] -- Internal symbol number corresponding
     to YYLEX-NUM.  */
  const unsigned short int
  parser::yytoken_number_[] =
  {
         0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,    46
  };
#endif

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
  const unsigned char
  parser::yyr1_[] =
  {
         0,    55,    56,    57,    57,    58,    58,    58,    58,    58,
      58,    59,    60,    61,    62,    62,    63,    65,    64,    66,
      66,    67,    68,    68,    70,    69,    71,    71,    72,    73,
      73,    73,    74,    74,    75,    76,    77,    76,    76,    76,
      76,    76,    76,    76,    76,    78,    78,    80,    79,    81,
      81,    81,    82,    82,    82,    82,    82,    82,    82,    82,
      82,    82,    82,    82,    82,    82,    83,    84,    84,    85,
      85,    86,    86,    88,    87,    89,    89,    90,    90,    92,
      91,    93,    93,    94,    94,    95,    96,    97,    96,    98,
      98,    98,    98,    98,    99,    99,    99,    99,    99,    99,
     100,   100,   101,   101,   102,   102,   102,   102,   102,   103,
     104,   104,   105,   106
  };

  /* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
  const unsigned char
  parser::yyr2_[] =
  {
         0,     2,     2,     0,     2,     1,     1,     1,     1,     1,
       1,     4,     2,     2,     1,     3,     3,     0,     4,     1,
       3,     3,     0,     2,     0,     3,     1,     3,     2,     0,
       1,     1,     2,     4,     2,     3,     0,     4,     1,     1,
       1,     1,     1,     1,     3,     1,     3,     0,     5,     2,
       2,     2,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     5,     0,     1,     1,
       3,     1,     1,     0,     4,     0,     1,     1,     3,     0,
       4,     0,     1,     1,     3,     3,     1,     0,     4,     1,
       1,     3,     3,     4,     2,     2,     3,     3,     3,     4,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1
  };


  /* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
     First, the terminals, then, starting at \a yyntokens_, nonterminals.  */
  const char*
  const parser::yytname_[] =
  {
    "\"end of query string\"", "error", "$undefined", "\"FOR declaration\"",
  "\"LET declaration\"", "\"FILTER declaration\"",
  "\"RETURN declaration\"", "\"COLLECT declaration\"",
  "\"SORT declaration\"", "\"LIMIT declaration\"", "\"ASC keyword\"",
  "\"DESC keyword\"", "\"IN keyword\"", "\"INTO keyword\"", "\"null\"",
  "\"true\"", "\"false\"", "\"identifier\"", "\"quoted string\"",
  "\"integer number\"", "\"number\"", "\"bind parameter\"",
  "\"assignment\"", "\"not operator\"", "\"and operator\"",
  "\"or operator\"", "\"== operator\"", "\"!= operator\"",
  "\"< operator\"", "\"> operator\"", "\"<= operator\"", "\">= operator\"",
  "\"+ operator\"", "\"- operator\"", "\"* operator\"", "\"/ operator\"",
  "\"% operator\"", "\"[*] operator\"", "\"?\"", "\":\"", "\"::\"",
  "\"..\"", "\",\"", "\"(\"", "\")\"", "\"{\"", "\"}\"", "\"[\"", "\"]\"",
  "UPLUS", "UMINUS", "FUNCCALL", "REFERENCE", "INDEXED", "'.'", "$accept",
  "query", "optional_statement_block_statements",
  "statement_block_statement", "for_statement", "filter_statement",
  "let_statement", "let_list", "let_element", "collect_statement", "$@1",
  "collect_list", "collect_element", "optional_into", "sort_statement",
  "$@2", "sort_list", "sort_element", "sort_direction", "limit_statement",
  "return_statement", "expression", "$@3", "function_name",
  "function_call", "$@4", "operator_unary", "operator_binary",
  "operator_ternary", "optional_function_call_arguments",
  "function_arguments_list", "compound_type", "list", "$@5",
  "optional_list_elements", "list_elements_list", "array", "$@6",
  "optional_array_elements", "array_elements_list", "array_element",
  "reference", "$@7", "single_reference", "expansion", "atomic_value",
  "numeric_value", "value_literal", "bind_parameter", "array_element_name",
  "variable_name", "integer_value", YY_NULL
  };

#if YYDEBUG
  /* YYRHS -- A `-1'-separated list of the rules' RHS.  */
  const parser::rhs_number_type
  parser::yyrhs_[] =
  {
        56,     0,    -1,    57,    75,    -1,    -1,    57,    58,    -1,
      59,    -1,    61,    -1,    60,    -1,    64,    -1,    69,    -1,
      74,    -1,     3,   105,    12,    76,    -1,     5,    76,    -1,
       4,    62,    -1,    63,    -1,    62,    42,    63,    -1,   105,
      22,    76,    -1,    -1,     7,    65,    66,    68,    -1,    67,
      -1,    66,    42,    67,    -1,   105,    22,    76,    -1,    -1,
      13,   105,    -1,    -1,     8,    70,    71,    -1,    72,    -1,
      71,    42,    72,    -1,    76,    73,    -1,    -1,    10,    -1,
      11,    -1,     9,   100,    -1,     9,   100,    42,   100,    -1,
       6,    76,    -1,    43,    76,    44,    -1,    -1,    43,    77,
      56,    44,    -1,    81,    -1,    82,    -1,    83,    -1,    86,
      -1,   100,    -1,    96,    -1,    76,    41,    76,    -1,    17,
      -1,    78,    40,    17,    -1,    -1,    78,    80,    43,    84,
      44,    -1,    32,    76,    -1,    33,    76,    -1,    23,    76,
      -1,    76,    25,    76,    -1,    76,    24,    76,    -1,    76,
      32,    76,    -1,    76,    33,    76,    -1,    76,    34,    76,
      -1,    76,    35,    76,    -1,    76,    36,    76,    -1,    76,
      26,    76,    -1,    76,    27,    76,    -1,    76,    28,    76,
      -1,    76,    29,    76,    -1,    76,    30,    76,    -1,    76,
      31,    76,    -1,    76,    12,    76,    -1,    76,    38,    76,
      39,    76,    -1,    -1,    85,    -1,    76,    -1,    85,    42,
      76,    -1,    87,    -1,    91,    -1,    -1,    47,    88,    89,
      48,    -1,    -1,    90,    -1,    76,    -1,    90,    42,    76,
      -1,    -1,    45,    92,    93,    46,    -1,    -1,    94,    -1,
      95,    -1,    94,    42,    95,    -1,   104,    39,    76,    -1,
      98,    -1,    -1,    96,    97,    37,    99,    -1,    17,    -1,
      79,    -1,    98,    54,    17,    -1,    98,    54,   103,    -1,
      98,    47,    76,    48,    -1,    54,    17,    -1,    54,   103,
      -1,    47,    76,    48,    -1,    99,    54,    17,    -1,    99,
      54,   103,    -1,    99,    47,    76,    48,    -1,   102,    -1,
     103,    -1,   106,    -1,    20,    -1,    18,    -1,   101,    -1,
      14,    -1,    15,    -1,    16,    -1,    21,    -1,    17,    -1,
      18,    -1,    17,    -1,    19,    -1
  };

  /* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
     YYRHS.  */
  const unsigned short int
  parser::yyprhs_[] =
  {
         0,     0,     3,     6,     7,    10,    12,    14,    16,    18,
      20,    22,    27,    30,    33,    35,    39,    43,    44,    49,
      51,    55,    59,    60,    63,    64,    68,    70,    74,    77,
      78,    80,    82,    85,    90,    93,    97,    98,   103,   105,
     107,   109,   111,   113,   115,   119,   121,   125,   126,   132,
     135,   138,   141,   145,   149,   153,   157,   161,   165,   169,
     173,   177,   181,   185,   189,   193,   197,   203,   204,   206,
     208,   212,   214,   216,   217,   222,   223,   225,   227,   231,
     232,   237,   238,   240,   242,   246,   250,   252,   253,   258,
     260,   262,   266,   270,   275,   278,   281,   285,   289,   293,
     298,   300,   302,   304,   306,   308,   310,   312,   314,   316,
     318,   320,   322,   324
  };

  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
  const unsigned short int
  parser::yyrline_[] =
  {
         0,   184,   184,   189,   191,   196,   198,   200,   202,   204,
     206,   211,   230,   243,   248,   250,   255,   269,   269,   290,
     292,   297,   310,   313,   319,   319,   341,   346,   354,   365,
     368,   371,   377,   388,   401,   421,   424,   424,   460,   463,
     466,   469,   472,   475,   478,   509,   516,   530,   530,   555,
     563,   571,   582,   590,   598,   606,   614,   622,   630,   638,
     646,   654,   662,   670,   678,   686,   697,   709,   711,   716,
     721,   729,   732,   738,   738,   751,   753,   758,   763,   771,
     771,   784,   786,   791,   793,   798,   806,   810,   810,   868,
     885,   892,   900,   908,   919,   929,   939,   949,   957,   965,
     976,   979,   985,   988,  1013,  1022,  1025,  1034,  1043,  1055,
    1067,  1074,  1083,  1089
  };

  // Print the state stack on the debug stream.
  void
  parser::yystack_print_ ()
  {
    *yycdebug_ << "Stack now";
    for (state_stack_type::const_iterator i = yystate_stack_.begin ();
	 i != yystate_stack_.end (); ++i)
      *yycdebug_ << ' ' << *i;
    *yycdebug_ << std::endl;
  }

  // Report on the debug stream that the rule \a yyrule is going to be reduced.
  void
  parser::yy_reduce_print_ (int yyrule)
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
  parser::token_number_type
  parser::yytranslate_ (int t)
  {
    static
    const token_number_type
    translate_table[] =
    {
           0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,    54,     2,     2,     2,
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
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53
    };
    if ((unsigned int) t <= yyuser_token_number_max_)
      return translate_table[t];
    else
      return yyundef_token_;
  }

  const int parser::yyeof_ = 0;
  const int parser::yylast_ = 380;
  const int parser::yynnts_ = 52;
  const int parser::yyempty_ = -2;
  const int parser::yyfinal_ = 3;
  const int parser::yyterror_ = 1;
  const int parser::yyerrcode_ = 256;
  const int parser::yyntokens_ = 55;

  const unsigned int parser::yyuser_token_number_max_ = 308;
  const parser::token_number_type parser::yyundef_token_ = 2;


} // Ahuacatl

