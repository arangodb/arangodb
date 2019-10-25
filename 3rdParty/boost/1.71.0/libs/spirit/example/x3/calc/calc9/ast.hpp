/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC9_AST_HPP)
#define BOOST_SPIRIT_X3_CALC9_AST_HPP

#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/optional.hpp>
#include <list>

namespace client { namespace ast
{
    ///////////////////////////////////////////////////////////////////////////
    //  The AST
    ///////////////////////////////////////////////////////////////////////////
    namespace x3 = boost::spirit::x3;

    struct nil {};
    struct unary;
    struct expression;

    struct variable : x3::position_tagged
    {
        variable(std::string const& name = "") : name(name) {}
        std::string name;
    };

    struct operand :
        x3::variant<
            nil
          , unsigned int
          , variable
          , x3::forward_ast<unary>
          , x3::forward_ast<expression>
        >
    {
        using base_type::base_type;
        using base_type::operator=;
    };
    
    enum optoken
    {
        op_plus,
        op_minus,
        op_times,
        op_divide,
        op_positive,
        op_negative,
        op_not,
        op_equal,
        op_not_equal,
        op_less,
        op_less_equal,
        op_greater,
        op_greater_equal,
        op_and,
        op_or
    };

    struct unary
    {
        optoken operator_;
        operand operand_;
    };

    struct operation : x3::position_tagged
    {
        optoken operator_;
        operand operand_;
    };

    struct expression : x3::position_tagged
    {
        operand first;
        std::list<operation> rest;
    };

    struct assignment : x3::position_tagged
    {
        variable lhs;
        expression rhs;
    };

    struct variable_declaration
    {
        assignment assign;
    };
    
    struct if_statement;
    struct while_statement;
    struct statement_list;

    struct statement :
        x3::variant<
            variable_declaration
          , assignment
          , boost::recursive_wrapper<if_statement>
          , boost::recursive_wrapper<while_statement>
          , boost::recursive_wrapper<statement_list>
        >
    {
        using base_type::base_type;
        using base_type::operator=;
    };
    
    struct statement_list : std::list<statement> {};

    struct if_statement
    {
        expression condition;
        statement then;
        boost::optional<statement> else_;
    };

    struct while_statement
    {
        expression condition;
        statement body;
    };

    // print functions for debugging
    inline std::ostream& operator<<(std::ostream& out, nil)
    {
        out << "nil";
        return out;
    }

    inline std::ostream& operator<<(std::ostream& out, variable const& var)
    {
        out << var.name; return out;
    }
}}

#endif
