/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC8_EXPRESSION_DEF_HPP)
#define BOOST_SPIRIT_X3_CALC8_EXPRESSION_DEF_HPP

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include "ast.hpp"
#include "ast_adapted.hpp"
#include "expression.hpp"
#include "common.hpp"
#include "error_handler.hpp"

namespace client { namespace parser
{
    using x3::uint_;
    using x3::char_;
    using x3::raw;
    using x3::lexeme;
    using namespace x3::ascii;

    struct additive_expr_class;
    struct multiplicative_expr_class;
    struct unary_expr_class;
    struct primary_expr_class;

    typedef x3::rule<additive_expr_class, ast::expression> additive_expr_type;
    typedef x3::rule<multiplicative_expr_class, ast::expression> multiplicative_expr_type;
    typedef x3::rule<unary_expr_class, ast::operand> unary_expr_type;
    typedef x3::rule<primary_expr_class, ast::operand> primary_expr_type;

    expression_type const expression = "expression";
    additive_expr_type const additive_expr = "additive_expr";
    multiplicative_expr_type const multiplicative_expr = "multiplicative_expr";
    unary_expr_type unary_expr = "unary_expr";
    primary_expr_type primary_expr = "primary_expr";

    auto const additive_expr_def =
        multiplicative_expr
        >> *(   (char_('+') > multiplicative_expr)
            |   (char_('-') > multiplicative_expr)
            )
        ;

    auto const multiplicative_expr_def =
        unary_expr
        >> *(   (char_('*') > unary_expr)
            |   (char_('/') > unary_expr)
            )
        ;

    auto const unary_expr_def =
            primary_expr
        |   (char_('-') > primary_expr)
        |   (char_('+') > primary_expr)
        ;

    auto const primary_expr_def =
            uint_
        |   identifier
        |   '(' > expression > ')'
        ;

    auto const expression_def = additive_expr;

    BOOST_SPIRIT_DEFINE(
        expression
      , additive_expr
      , multiplicative_expr
      , unary_expr
      , primary_expr
    );

    struct unary_expr_class : x3::annotate_on_success {};
    struct primary_expr_class : x3::annotate_on_success {};

}}

namespace client
{
    parser::expression_type const& expression()
    {
        return parser::expression;
    }
}

#endif
