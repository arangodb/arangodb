/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC9_STATEMENT_DEF_HPP)
#define BOOST_SPIRIT_X3_CALC9_STATEMENT_DEF_HPP

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include "ast.hpp"
#include "ast_adapted.hpp"
#include "statement.hpp"
#include "expression.hpp"
#include "common.hpp"
#include "error_handler.hpp"

namespace client { namespace parser
{
    using x3::raw;
    using x3::lexeme;
    using namespace x3::ascii;

    struct statement_list_class;
    struct variable_declaration_class;
    struct assignment_class;
    struct variable_class;

    typedef x3::rule<statement_list_class, ast::statement_list> statement_list_type;
    typedef x3::rule<variable_declaration_class, ast::variable_declaration> variable_declaration_type;
    typedef x3::rule<assignment_class, ast::assignment> assignment_type;
    typedef x3::rule<variable_class, ast::variable> variable_type;

    statement_type const statement("statement");
    statement_list_type const statement_list("statement_list");
    variable_declaration_type const variable_declaration("variable_declaration");
    assignment_type const assignment("assignment");
    variable_type const variable("variable");

    // Import the expression rule
    namespace { auto const& expression = client::expression(); }

    auto const statement_list_def =
        +(variable_declaration | assignment)
        ;

    auto const variable_declaration_def =
            lexeme["var" >> !(alnum | '_')] // make sure we have whole words
        >   assignment
        ;

    auto const assignment_def =
            variable
        >   '='
        >   expression
        >   ';'
        ;

    auto const variable_def = identifier;
    auto const statement_def = statement_list;

    BOOST_SPIRIT_DEFINE(
        statement
      , statement_list
      , variable_declaration
      , assignment
      , variable
    );

    struct statement_class : error_handler_base, x3::annotate_on_success {};
    struct assignment_class : x3::annotate_on_success {};
    struct variable_class : x3::annotate_on_success {};
}}

namespace client
{
    parser::statement_type const& statement()
    {
        return parser::statement;
    }
}

#endif
