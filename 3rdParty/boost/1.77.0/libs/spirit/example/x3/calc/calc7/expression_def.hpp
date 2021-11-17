/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC7_EXPRESSION_DEF_HPP)
#define BOOST_SPIRIT_X3_CALC7_EXPRESSION_DEF_HPP

#include <boost/spirit/home/x3.hpp>
#include "ast.hpp"
#include "ast_adapted.hpp"
#include "expression.hpp"
#include "error_handler.hpp"

namespace client { namespace calculator_grammar
{
    using x3::uint_;
    using x3::char_;

    struct expression_class;
    struct term_class;
    struct factor_class;

    typedef x3::rule<expression_class, ast::expression> expression_type;
    typedef x3::rule<term_class, ast::expression> term_type;
    typedef x3::rule<factor_class, ast::operand> factor_type;

    expression_type const expression = "expression";
    term_type const term = "term";
    factor_type const factor = "factor";

    auto const expression_def =
        term
        >> *(   (char_('+') > term)
            |   (char_('-') > term)
            )
        ;

    auto const term_def =
        factor
        >> *(   (char_('*') > factor)
            |   (char_('/') > factor)
            )
        ;

    auto const factor_def =
            uint_
        |   '(' > expression > ')'
        |   (char_('-') > factor)
        |   (char_('+') > factor)
        ;

    BOOST_SPIRIT_DEFINE(
        expression
      , term
      , factor
    );

    struct expression_class : error_handler {};
}}

namespace client
{
    calculator_grammar::expression_type expression()
    {
        return calculator_grammar::expression;
    }
}

#endif
