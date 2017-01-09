/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman
    Copyright (c) 2013-2014 Agustin Berge

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  A Calculator example demonstrating generation of AST. The AST,
//  once created, is traversed, 1) To print its contents and
//  2) To evaluate the result.
//
//  [ JDG April 28, 2008 ]      For BoostCon 2008
//  [ JDG February 18, 2011 ]   Pure attributes. No semantic actions.
//  [ JDG January 9, 2013 ]     Spirit X3
//
///////////////////////////////////////////////////////////////////////////////

#include "grammar.hpp"

namespace client
{
    ///////////////////////////////////////////////////////////////////////////////
    //  The calculator grammar
    ///////////////////////////////////////////////////////////////////////////////
    namespace calculator_grammar
    {
        using x3::uint_;
        using x3::char_;

        x3::rule<class expression, ast::program> const expression("expression");
        x3::rule<class term, ast::program> const term("term");
        x3::rule<class factor, ast::operand> const factor("factor");

        auto const expression_def =
            term
            >> *(   (char_('+') >> term)
                |   (char_('-') >> term)
                )
            ;

        auto const term_def =
            factor
            >> *(   (char_('*') >> factor)
                |   (char_('/') >> factor)
                )
            ;

        auto const factor_def =
                uint_
            |   '(' >> expression >> ')'
            |   (char_('-') >> factor)
            |   (char_('+') >> factor)
            ;

        BOOST_SPIRIT_DEFINE(
            expression
          , term
          , factor
        );

        parser_type calculator()
        {
            return expression;
        }
    }
}