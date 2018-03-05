////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief request C++ API and customize names and arguments
////////////////////////////////////////////////////////////////////////////////
%skeleton "glr.cc" // required since "lalr1.cc" does not suppot %glr-parser
%require "2.4"
%glr-parser
%defines
%define namespace "iresearch::iql"
%expect 7    // expect +7 shift/reduce as per bison generation of grammer below
%expect-rr 9 // expect +9 reduce/reduce as per bison generation of grammer below
%parse-param { iresearch::iql::context& ctx }

////////////////////////////////////////////////////////////////////////////////
/// @brief add to start of HH file
////////////////////////////////////////////////////////////////////////////////
%code requires {
  #define YYSTYPE size_t

  #if YYDEBUG
    #define YYERROR_VERBOSE 1

    // this is required only for %skeleton "glr.cc" since it lacks default init
    // not needed for %skeleton "lalr1.cc" or without %skeleton
    #define YYLTYPE iresearch::iql::location_type1
  #endif

  namespace iresearch {
    namespace iql {
      class context;
    }
  }

  // Suppress warnings due to Bison generated code (push section, pop just below)
  #if defined(_MSC_VER)
    #pragma warning(disable : 4512)
  #endif

  // ALWAYS!!! define YYDEBUG 1 for the length of the header so as to avoid
  // alignment issues when linkning without YYDEBUG agains a library that was
  // built with YYDEBUG or vice versa, reverse at end of header
  #undef YYDEBUG_REQUESTED
  #if YYDEBUG
    #define YYDEBUG_REQUESTED
  #else
    #undef YYDEBUG
    #define YYDEBUG 1
  #endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add to end of HH file
////////////////////////////////////////////////////////////////////////////////
%code provides {
  // ALWAYS!!! define YYDEBUG 1 for the length of the header so as to avoid
  // alignment issues when linkning without YYDEBUG agains a library that was
  // built with YYDEBUG or vice versa, reverse at end of header
  #ifdef YYDEBUG_REQUESTED
    #undef YYDEBUG_REQUESTED
  #else
    #undef YYDEBUG
    #define YYDEBUG 0
  #endif

  // end of suppress of warnings due to Bison generated code (pop section, push just above)
  #if defined(_MSC_VER)
    #pragma warning(default : 4512)
  #endif

  namespace iresearch {
    namespace iql {
      class context {
      public:
        // destructor
        virtual ~context() = default;

        // parser operations
        virtual void yyerror(parser::location_type const& location, std::string const& sError) = 0;
        virtual parser::token_type yylex(parser::semantic_type& value, parser::location_type& location) = 0;

        // value operations
        virtual parser::semantic_type sequence(parser::location_type const& location) = 0;

        // node operations
        virtual parser::semantic_type append(parser::semantic_type const& value, parser::location_type const& location) = 0;
        virtual parser::semantic_type boost(parser::semantic_type const& value, parser::location_type const& location) = 0;
        virtual parser::semantic_type function(parser::semantic_type const& name, parser::semantic_type const& args) = 0;
        virtual parser::semantic_type list(parser::semantic_type const& value1, parser::semantic_type const& value2) = 0;
        virtual parser::semantic_type negation(parser::semantic_type const& value) = 0;
        virtual parser::semantic_type range(parser::semantic_type const& value1, bool bInclusive1, parser::semantic_type const& value2, bool bInclusive2) = 0;

        // comparison operations
        virtual parser::semantic_type op_eq(parser::semantic_type const& value1, parser::semantic_type const& value2) = 0;
        virtual parser::semantic_type op_like(parser::semantic_type const& value1, parser::semantic_type const& value2) = 0;

        // filter operations
        virtual parser::semantic_type op_and(parser::semantic_type const& value1, parser::semantic_type const& value2) = 0;
        virtual parser::semantic_type op_or(parser::semantic_type const& value1, parser::semantic_type const& value2) = 0;

        // query operations
        virtual bool addOrder(parser::semantic_type const& value, bool bAscending = true) = 0;
        virtual bool setLimit(parser::semantic_type const& value) = 0;
        virtual bool setQuery(parser::semantic_type const& value) = 0;
      };

      void debug(parser& parser, bool bEnable);

      // this is required only for %skeleton "glr.cc" since it lacks default init
      // not needed for %skeleton "lalr1.cc" or without %skeleton
      #if YYDEBUG
        class location_type1: public location {};
      #endif
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add to CC file
////////////////////////////////////////////////////////////////////////////////
%code {
  #define yylex(pValue, pLocation) ctx.yylex(*pValue, *pLocation)

  namespace iresearch {
    namespace iql {
      void parser::error(parser::location_type const& location, std::string const& sError) {
          ctx.yyerror(location, sError);
      }
    }
  }

  // this is required only for %skeleton "glr.cc" since it lacks default init
  // but yy_symbol_print uses yylocationp->[begin|end].filename and segfaults
  // not needed for %skeleton "lalr1.cc" or without %skeleton
  #if YYDEBUG
    static void yy_symbol_print(FILE*, int, iresearch::iql::parser::semantic_type const*, iresearch::iql::parser::location_type const*, iresearch::iql::parser&, iresearch::iql::context&);
    static void yy_symbol_print(FILE* filep, int yytype, YYSTYPE const* yyvaluep, YYLTYPE const* yylocationp, iresearch::iql::parser& yyparser, iresearch::iql::context& ctx) {
      iresearch::iql::parser::location_type yylocation = *yylocationp;

      yylocation.begin.filename = nullptr;
      yylocation.end.filename = nullptr;
      yy_symbol_print(filep, yytype, yyvaluep, &yylocation, yyparser, ctx);
    }
  #endif

  // ALWAYS!!! define YYDEBUG 1 for the length of the header so as to avoid
  // alignment issues when linkning without YYDEBUG agains a library that was
  // built with YYDEBUG or vice versa
  // add method stub implementations here for debug methods declared in header
  // since they are masked out in Bison generated code by !YYDEBUG
  #if !YYDEBUG
    namespace iresearch {
      namespace iql {
        inline void parser::yy_symbol_value_print_(int /* yytype */, const semantic_type* /* yyvaluep */, const location_type* /* yylocationp */) {}
        inline void parser::yy_symbol_print_(int /* yytype */, const semantic_type* /* yyvaluep */, const location_type* /* yylocationp */) {}
      }
    }
  #endif

  // Suppress warnings due to Bison generated code (push section, pop at end of file)
  #if defined(_MSC_VER)
    #pragma warning(disable : 4100)
    #pragma warning(disable : 4127)
    #pragma warning(disable : 4244)
    #pragma warning(disable : 4505)
    #pragma warning(disable : 4611)
    #pragma warning(disable : 4706)
  #endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                Bison declarations
// -----------------------------------------------------------------------------

// basic tokens
%token IQL_EOF 0
%token IQL_UNKNOWN   "unknown token marker"
%token IQL_SEP       "token separator"
%token IQL_SEQUENCE  "data sub-sequence"

// logical operators
%token IQL_NOT       "negation operator"
%token IQL_EXCLAIM   "exclamation mark"
%token IQL_AND        "logical AND operator"
%token IQL_AMPAMP     "double ampersand"
%token IQL_OR         "logical OR operator"
%token IQL_PIPEPIPE   "double pipe"

// comparison operators
%token IQL_NE        "not equal operator"
%token IQL_LE        "less than or equal operator"
%token IQL_EQ        "equal operator"
%token IQL_GE        "greater than or equal operator"
%token IQL_LIKE      "similar operator"

// modification operators
%token IQL_ASTERISK  "asterisk operator"
%token IQL_ASC       "ascending sort order"
%token IQL_DESC      "descending sort order"

// grammer delimiters
%token IQL_COMMA     "comma"
%token IQL_SQUOTE    "single quote"
%token IQL_DQUOTE    "double quote"
%token IQL_LCHEVRON  "left chevron '<' delimiter"
%token IQL_RCHEVRON  "right chevron '>' delimiter"
%token IQL_LPAREN    "left parentheses '(' delimiter"
%token IQL_RPAREN    "right parentheses ')' delimiter"
%token IQL_LSBRACKET "left square bracket '[' delimiter"
%token IQL_RSBRACKET "right square bracket ']' delimiter"

// grammer tokens
%token IQL_LIMIT     "result limit section header"
%token IQL_ORDER     "sort order section header"

%start input

// -----------------------------------------------------------------------------
// --SECTION--                                                     Grammar rules
// -----------------------------------------------------------------------------
%%

input:
  optional_sep union order limit optional_sep { if (!ctx.setQuery($2)) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
;

optional_sep:
// empty since it is optional
| optional_sep IQL_SEP
;

union:
  intersection
| union IQL_SEP      IQL_OR       IQL_SEP      intersection { if (!($$ = ctx.op_or($1, $5))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| union optional_sep IQL_PIPEPIPE optional_sep intersection { if (!($$ = ctx.op_or($1, $5))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
;

intersection:
  expression
| intersection IQL_SEP      IQL_AND    IQL_SEP      expression { if (!($$ = ctx.op_and($1, $5))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| intersection optional_sep IQL_AMPAMP optional_sep expression { if (!($$ = ctx.op_and($1, $5))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
;

expression:
  boost
| compare
| negation
| subexpression
;

boost:
  plain_literal optional_sep IQL_ASTERISK optional_sep subexpression { if (!($$ = ctx.boost($5, @1))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| subexpression optional_sep IQL_ASTERISK optional_sep plain_literal { if (!($$ = ctx.boost($1, @5))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
;

negation:
  IQL_NOT     optional_sep subexpression { if (!($$ = ctx.negation($3))) YYERROR; @$.begin = @1.begin; @$.end = @3.end; }
| IQL_EXCLAIM optional_sep subexpression { if (!($$ = ctx.negation($3))) YYERROR; @$.begin = @1.begin; @$.end = @3.end; }
;

subexpression:
  IQL_LPAREN optional_sep union optional_sep IQL_RPAREN { $$ = $3; @$.begin = @1.begin; @$.end = @5.end; }
| function
;

function:
  sequence IQL_LPAREN optional_sep IQL_RPAREN                                { if (!($$ = ctx.function($1, $3))) YYERROR; @$.begin = @1.begin; @$.end = @4.end; }
| sequence IQL_LPAREN optional_sep function_arg_list optional_sep IQL_RPAREN { if (!($$ = ctx.function($1, $4))) YYERROR; @$.begin = @1.begin; @$.end = @6.end; }
;

function_arg:
  sequence
| expression
;

function_arg_list:
  function_arg
| function_arg_list list_sep function_arg { if (!($$ = ctx.list($1, $3))) YYERROR; @$.begin = @1.begin; @$.end = @3.end; }

compare:
  term optional_sep IQL_LIKE     optional_sep term  { if (!($$ = ctx.op_like($1, $5))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| term optional_sep IQL_NE       optional_sep term  { if (!($$ = ctx.negation($$ = ctx.op_eq($1, $$ = ctx.range($5, true, $5, true))))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| term optional_sep IQL_LCHEVRON optional_sep term  { if (!($$ = ctx.op_eq($1, $$ = ctx.range($3, true, $5, false)))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| term optional_sep IQL_LE       optional_sep term  { if (!($$ = ctx.op_eq($1, $$ = ctx.range($3, true, $5, true)))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| term optional_sep IQL_EQ       optional_sep term  { if (!($$ = ctx.op_eq($1, $$ = ctx.range($5, true, $5, true)))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| term optional_sep IQL_GE       optional_sep term  { if (!($$ = ctx.op_eq($1, $$ = ctx.range($5, true, $3, true)))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| term optional_sep IQL_RCHEVRON optional_sep term  { if (!($$ = ctx.op_eq($1, $$ = ctx.range($5, false, $3, true)))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| term optional_sep IQL_NE       optional_sep range { if (!($$ = ctx.negation($$ = ctx.op_eq($1, $5)))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
| term optional_sep IQL_EQ       optional_sep range { if (!($$ = ctx.op_eq($1, $5))) YYERROR; @$.begin = @1.begin; @$.end = @5.end; }
;

range:
  IQL_LSBRACKET optional_sep term list_sep term optional_sep IQL_RSBRACKET { if (!($$ = ctx.range($3, true, $5, true))) YYERROR; @$.begin = @1.begin; @$.end = @7.end; }
| IQL_LSBRACKET optional_sep term list_sep term optional_sep IQL_RPAREN    { if (!($$ = ctx.range($3, true, $5, false))) YYERROR; @$.begin = @1.begin; @$.end = @7.end; }
| IQL_LPAREN    optional_sep term list_sep term optional_sep IQL_RPAREN    { if (!($$ = ctx.range($3, false, $5, false))) YYERROR; @$.begin = @1.begin; @$.end = @7.end; }
| IQL_LPAREN    optional_sep term list_sep term optional_sep IQL_RSBRACKET { if (!($$ = ctx.range($3, false, $5, true))) YYERROR; @$.begin = @1.begin; @$.end = @7.end; }
;

term:
  sequence
| function
;

list_sep:
  optional_sep IQL_COMMA optional_sep
;

sequence:
  plain_literal
| dquoted_literal
| squoted_literal
;

plain_literal:
  IQL_SEQUENCE               { if (!($$ = ctx.sequence(@1))) YYERROR; @$.begin = @1.begin; @$.end = @1.end; }
| plain_literal IQL_SEQUENCE { if (!($$ = ctx.append($1, @2))) YYERROR; @$.begin = @1.begin; @$.end = @2.end; }
;

dquoted_literal:
  IQL_DQUOTE IQL_SEQUENCE IQL_DQUOTE                 { if (!($$ = ctx.sequence(@2))) YYERROR; @$.begin = @1.begin; @$.end = @3.end; }
| dquoted_literal IQL_DQUOTE IQL_SEQUENCE IQL_DQUOTE { if (!($$ = ctx.append($$ = ctx.append($1, @2), @3))) YYERROR; @$.begin = @1.begin; @$.end = @4.end; }
;

squoted_literal:
  IQL_SQUOTE IQL_SEQUENCE IQL_SQUOTE                 { if (!($$ = ctx.sequence(@2))) YYERROR; @$.begin = @1.begin; @$.end = @3.end; }
| squoted_literal IQL_SQUOTE IQL_SEQUENCE IQL_SQUOTE { if (!($$ = ctx.append($$ = ctx.append($1, @2), @3))) YYERROR; @$.begin = @1.begin; @$.end = @4.end; }
;

limit:
// empty since it is optional
| IQL_SEP IQL_LIMIT IQL_SEP term { if (!ctx.setLimit($4)) YYERROR; @$.begin = @1.begin; @$.end = @4.end; };
;

order:
// empty since it is optional
| IQL_SEP IQL_ORDER IQL_SEP order_list
;

order_list:
  order_term
| order_list list_sep order_term
;

order_term:
  term                  { if (!ctx.addOrder($1)) YYERROR; @$.begin = @1.begin; @$.end = @1.end; }
| term IQL_SEP IQL_ASC  { if (!ctx.addOrder($1, true)) YYERROR; @$.begin = @1.begin; @$.end = @3.end; }
| term IQL_SEP IQL_DESC { if (!ctx.addOrder($1, false)) YYERROR; @$.begin = @1.begin; @$.end = @3.end; }
;

%%

// -----------------------------------------------------------------------------
// --SECTION--                                                          Epilogue
// -----------------------------------------------------------------------------

// define after 'yydebug' is declared by bison
namespace iresearch {
  namespace iql {
    void debug(parser& parser, bool bEnable) {
      #if YYDEBUG
        // suppress MSVC C4505 warnings ('#pragma warning(disable:4505)' is ignored by MSVS)
        // defined here only for the sake of being inside some function
        #if defined(_MSC_VER)
          if (false) yypstack(nullptr, 0);
          if (false) yypdumpstack(nullptr);
        #endif

        yydebug = bEnable ? 1 : 0;
        parser.set_debug_level(bEnable ? 1 : 0);
      #endif
    }
  }
}

// end of suppress of warnings due to Bison generated code (pop section, push at start of file)
#if defined(_MSC_VER)
  #pragma warning(default : 4706)
  #pragma warning(default : 4611)
  #pragma warning(default : 4505)
  #pragma warning(default : 4244)
  #pragma warning(default : 4127)
  #pragma warning(default : 4100)
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
