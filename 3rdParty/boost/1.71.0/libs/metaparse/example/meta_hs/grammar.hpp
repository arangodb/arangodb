#ifndef META_HS_GRAMMAR_HPP
#define META_HS_GRAMMAR_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <token.hpp>
#include <semantic.hpp>

#include <boost/metaparse/middle_of.hpp>
#include <boost/metaparse/transform.hpp>
#include <boost/metaparse/sequence.hpp>
#include <boost/metaparse/last_of.hpp>
#include <boost/metaparse/one_of.hpp>
#include <boost/metaparse/foldl_reject_incomplete_start_with_parser.hpp>
#include <boost/metaparse/foldr_start_with_parser.hpp>
#include <boost/metaparse/entire_input.hpp>
#include <boost/metaparse/build_parser.hpp>

namespace grammar
{
  /*
   * The grammar
   *
   * definition ::= token::name+ token::define expression
   * expression ::= cmp_exp
   * cmp_exp ::= plus_exp (token::cmp plus_exp)*
   * plus_exp ::= mult_exp ((token::plus | token::minus) mult_exp)*
   * mult_exp ::= application ((token::mult | token::div) application)*
   * application ::= single_exp+
   * single_exp ::= if_exp | token::name | token::int_ | bracket_exp
   * if_exp ::= token::if_ expression token::then expression token::else_ expression
   * bracket_exp ::= token::open_bracket expression token::close_bracket
   */
  
  struct expression;
  
  typedef
    boost::metaparse::middle_of<
      token::open_bracket,
      expression,
      token::close_bracket
    >
    bracket_exp;
  
  typedef
    boost::metaparse::transform<
      boost::metaparse::sequence<
        boost::metaparse::last_of<token::if_, expression>,
        boost::metaparse::last_of<token::then, expression>,
        boost::metaparse::last_of<token::else_, expression>
      >,
      semantic::if_
    >
    if_exp;
  
  typedef
    boost::metaparse::one_of<
      if_exp,
      boost::metaparse::transform<token::name, semantic::ref>,
      boost::metaparse::transform<token::int_, semantic::value>,
      bracket_exp
    >
    single_exp;
  
  typedef
    boost::metaparse::foldl_reject_incomplete_start_with_parser<
      single_exp,
      single_exp,
      semantic::application
    >
    application;
  
  typedef
    boost::metaparse::foldl_reject_incomplete_start_with_parser<
      boost::metaparse::sequence<
        boost::metaparse::one_of<token::mult, token::div>,
        application
      >,
      application,
      semantic::binary_op
    >
    mult_exp;
  
  typedef
    boost::metaparse::foldl_reject_incomplete_start_with_parser<
      boost::metaparse::sequence<
        boost::metaparse::one_of<token::plus, token::minus>,
        mult_exp
      >,
      mult_exp,
      semantic::binary_op
    >
    plus_exp;
  
  typedef
    boost::metaparse::foldl_reject_incomplete_start_with_parser<
      boost::metaparse::sequence<token::cmp, plus_exp>,
      plus_exp,
      semantic::binary_op
    >
    cmp_exp;
  
  struct expression : cmp_exp {};
  
  typedef
    boost::metaparse::transform<
      boost::metaparse::sequence<
        token::name,
        boost::metaparse::foldr_start_with_parser<
          token::name,
          boost::metaparse::last_of<token::define, expression>,
          semantic::lambda
        >
      >,
      semantic::pair
    >
    definition;
  
  typedef
    boost::metaparse::build_parser<
      boost::metaparse::entire_input<definition>
    >
    def_parser;
}

#endif

