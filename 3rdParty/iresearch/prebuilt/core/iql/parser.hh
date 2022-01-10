// A Bison parser, made by GNU Bison 3.5.1.

// Skeleton interface for Bison GLR parsers in C++

// Copyright (C) 2002-2015, 2018-2020 Free Software Foundation, Inc.

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

// Undocumented macros, especially those whose name start with YY_,
// are private implementation details.  Do not rely on them.

#ifndef YY_YY_IQL_PARSER_HH_INCLUDED
# define YY_YY_IQL_PARSER_HH_INCLUDED
// "%code requires" blocks.
#line 39 "/home/gnusi/git/iresearch/core/iql/parser.yy"

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

#line 76 "iql/parser.hh"

#include <iostream>
#include <stdexcept>
#include <string>

#if defined __cplusplus
# define YY_CPLUSPLUS __cplusplus
#else
# define YY_CPLUSPLUS 199711L
#endif

// Support move semantics when possible.
#if 201103L <= YY_CPLUSPLUS
# define YY_MOVE           std::move
# define YY_MOVE_OR_COPY   move
# define YY_MOVE_REF(Type) Type&&
# define YY_RVREF(Type)    Type&&
# define YY_COPY(Type)     Type
#else
# define YY_MOVE
# define YY_MOVE_OR_COPY   copy
# define YY_MOVE_REF(Type) Type&
# define YY_RVREF(Type)    const Type&
# define YY_COPY(Type)     const Type&
#endif

// Support noexcept when possible.
#if 201103L <= YY_CPLUSPLUS
# define YY_NOEXCEPT noexcept
# define YY_NOTHROW
#else
# define YY_NOEXCEPT
# define YY_NOTHROW throw ()
#endif

// Support constexpr when possible.
#if 201703 <= YY_CPLUSPLUS
# define YY_CONSTEXPR constexpr
#else
# define YY_CONSTEXPR
#endif
# include "location.hh"


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif

# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

// This skeleton is based on C, yet compiles it as C++.
// So expect warnings about C style casts.
#if defined __clang__ && 306 <= __clang_major__ * 100 + __clang_minor__
# pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined __GNUC__ && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

// On MacOS, PTRDIFF_MAX is defined as long long, which Clang's
// -pedantic reports as being a C++11 extension.
#if defined __APPLE__ && YY_CPLUSPLUS < 201103L \
    && defined __clang__ && 4 <= __clang_major__
# pragma clang diagnostic ignored "-Wc++11-long-long"
#endif

// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
# if defined __GNUC__ && !defined __EXCEPTIONS
#  define YY_EXCEPTIONS 0
# else
#  define YY_EXCEPTIONS 1
# endif
#endif

/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

#line 31 "/home/gnusi/git/iresearch/core/iql/parser.yy"
namespace iresearch { namespace iql {
#line 218 "iql/parser.hh"




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
      syntax_error (const location_type& l, const std::string& m)
        : std::runtime_error (m)
        , location (l)
      {}

      syntax_error (const syntax_error& s)
        : std::runtime_error (s.what ())
        , location (s.location)
      {}

      ~syntax_error () YY_NOEXCEPT YY_NOTHROW;

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
    typedef signed char token_number_type;


    /// Build a parser object.
    parser (iresearch::iql::context& ctx_yyarg);
    virtual ~parser ();

    /// Parse.  An alias for parse ().
    /// \returns  0 iff parsing succeeded.
    int operator() ();

    /// Parse.
    /// \returns  0 iff parsing succeeded.
    virtual int parse ();

#if YYDEBUG
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
#endif

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

#line 31 "/home/gnusi/git/iresearch/core/iql/parser.yy"
} } // iresearch::iql
#line 372 "iql/parser.hh"

// "%code provides" blocks.
#line 76 "/home/gnusi/git/iresearch/core/iql/parser.yy"

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

#line 438 "iql/parser.hh"


#endif // !YY_YY_IQL_PARSER_HH_INCLUDED
