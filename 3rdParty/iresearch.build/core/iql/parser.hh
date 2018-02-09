// A Bison parser, made by GNU Bison 3.0.4.

// Skeleton interface for Bison GLR parsers in C++

// Copyright (C) 2002-2015 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// C++ GLR parser skeleton written by Akim Demaille.

#ifndef YY_YY_PARSER_HH_INCLUDED
# define YY_YY_PARSER_HH_INCLUDED
// //                    "%code requires" blocks.
#line 39 "/home/user/git-root/arangodb-iresearch/3rdParty/iresearch/core/iql/parser.yy" // glr.cc:329

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

#line 73 "parser.hh" // glr.cc:329


#include <stdexcept>
#include <string>
#include <iostream>
#include "location.hh"

/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

#line 31 "/home/user/git-root/arangodb-iresearch/3rdParty/iresearch/core/iql/parser.yy" // glr.cc:329
namespace iresearch { namespace iql {
#line 88 "parser.hh" // glr.cc:329


  /// A Bison parser.
  class parser
  {
  public:
#ifndef YYSTYPE
    /// Symbol semantic values.
    typedef int semantic_type;
#else
    typedef YYSTYPE semantic_type;
#endif
    /// Symbol locations.
    typedef location location_type;

    /// Syntax errors thrown from user actions.
    struct syntax_error : std::runtime_error
    {
      syntax_error (const location_type& l, const std::string& m);
      location_type location;
    };

    /// Tokens.
    struct token
    {
      enum yytokentype
      {
        IQL_EOF = 0,
        IQL_UNKNOWN = 258,
        IQL_SEP = 259,
        IQL_SEQUENCE = 260,
        IQL_NOT = 261,
        IQL_EXCLAIM = 262,
        IQL_AND = 263,
        IQL_AMPAMP = 264,
        IQL_OR = 265,
        IQL_PIPEPIPE = 266,
        IQL_NE = 267,
        IQL_LE = 268,
        IQL_EQ = 269,
        IQL_GE = 270,
        IQL_LIKE = 271,
        IQL_ASTERISK = 272,
        IQL_ASC = 273,
        IQL_DESC = 274,
        IQL_COMMA = 275,
        IQL_SQUOTE = 276,
        IQL_DQUOTE = 277,
        IQL_LCHEVRON = 278,
        IQL_RCHEVRON = 279,
        IQL_LPAREN = 280,
        IQL_RPAREN = 281,
        IQL_LSBRACKET = 282,
        IQL_RSBRACKET = 283,
        IQL_LIMIT = 284,
        IQL_ORDER = 285
      };
    };

    /// (External) token type, as returned by yylex.
    typedef token::yytokentype token_type;

    /// Symbol type: an internal symbol number.
    typedef int symbol_number_type;

    /// The symbol type number to denote an empty symbol.
    enum { empty_symbol = -2 };

    /// Internal symbol number for tokens (subsumed by symbol_number_type).
    typedef unsigned char token_number_type;

    /// A complete symbol.
    ///
    /// Expects its Base type to provide access to the symbol type
    /// via type_get().
    ///
    /// Provide access to semantic value and location.
    template <typename Base>
    struct basic_symbol : Base
    {
      /// Alias to Base.
      typedef Base super_type;

      /// Default constructor.
      basic_symbol ();

      /// Copy constructor.
      basic_symbol (const basic_symbol& other);

      /// Constructor for valueless symbols.
      basic_symbol (typename Base::kind_type t,
                    const location_type& l);

      /// Constructor for symbols with semantic value.
      basic_symbol (typename Base::kind_type t,
                    const semantic_type& v,
                    const location_type& l);

      /// Destroy the symbol.
      ~basic_symbol ();

      /// Destroy contents, and record that is empty.
      void clear ();

      /// Whether empty.
      bool empty () const;

      /// Destructive move, \a s is emptied into this.
      void move (basic_symbol& s);

      /// The semantic value.
      semantic_type value;

      /// The location.
      location_type location;

    private:
      /// Assignment operator.
      basic_symbol& operator= (const basic_symbol& other);
    };

    /// Type access provider for token (enum) based symbols.
    struct by_type
    {
      /// Default constructor.
      by_type ();

      /// Copy constructor.
      by_type (const by_type& other);

      /// The symbol type as needed by the constructor.
      typedef token_type kind_type;

      /// Constructor from (external) token numbers.
      by_type (kind_type t);

      /// Record that this symbol is empty.
      void clear ();

      /// Steal the symbol type from \a that.
      void move (by_type& that);

      /// The (internal) type number (corresponding to \a type).
      /// \a empty when empty.
      symbol_number_type type_get () const;

      /// The token.
      token_type token () const;

      /// The symbol type.
      /// \a empty_symbol when empty.
      /// An int, not token_number_type, to be able to store empty_symbol.
      int type;
    };

    /// "External" symbols: returned by the scanner.
    typedef basic_symbol<by_type> symbol_type;



    /// Build a parser object.
    parser (iresearch::iql::context& ctx_yyarg);
    virtual ~parser ();

    /// Parse.
    /// \returns  0 iff parsing succeeded.
    virtual int parse ();

    /// The current debugging stream.
    std::ostream& debug_stream () const;
    /// Set the current debugging stream.
    void set_debug_stream (std::ostream &);

    /// Type for debugging levels.
    typedef int debug_level_type;
    /// The current debugging level.
    debug_level_type debug_level () const;
    /// Set the current debugging level.
    void set_debug_level (debug_level_type l);

  public:
    /// Report a syntax error.
    /// \param loc    where the syntax error is found.
    /// \param msg    a description of the syntax error.
    virtual void error (const location_type& loc, const std::string& msg);

# if YYDEBUG
  public:
    /// \brief Report a symbol value on the debug stream.
    /// \param yytype       The token type.
    /// \param yyvaluep     Its semantic value.
    /// \param yylocationp  Its location.
    virtual void yy_symbol_value_print_ (int yytype,
                                         const semantic_type* yyvaluep,
                                         const location_type* yylocationp);
    /// \brief Report a symbol on the debug stream.
    /// \param yytype       The token type.
    /// \param yyvaluep     Its semantic value.
    /// \param yylocationp  Its location.
    virtual void yy_symbol_print_ (int yytype,
                                   const semantic_type* yyvaluep,
                                   const location_type* yylocationp);
  private:
    // Debugging.
    std::ostream* yycdebug_;
#endif


    // User arguments.
    iresearch::iql::context& ctx;
  };



#ifndef YYSTYPE
# define YYSTYPE iresearch::iql::parser::semantic_type
#endif
#ifndef YYLTYPE
# define YYLTYPE iresearch::iql::parser::location_type
#endif

#line 31 "/home/user/git-root/arangodb-iresearch/3rdParty/iresearch/core/iql/parser.yy" // glr.cc:329
} } // iresearch::iql
#line 312 "parser.hh" // glr.cc:329
// //                    "%code provides" blocks.
#line 76 "/home/user/git-root/arangodb-iresearch/3rdParty/iresearch/core/iql/parser.yy" // glr.cc:329

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

#line 377 "parser.hh" // glr.cc:329


#endif // !YY_YY_PARSER_HH_INCLUDED
