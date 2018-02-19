/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC7_AST_HPP)
#define BOOST_SPIRIT_X3_CALC7_AST_HPP

#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/fusion/include/io.hpp>
#include <list>

namespace client { namespace ast
{
    namespace x3 = boost::spirit::x3;

    ///////////////////////////////////////////////////////////////////////////
    //  The AST
    ///////////////////////////////////////////////////////////////////////////
    struct nil {};
    struct signed_;
    struct expression;

    struct operand : x3::variant<
            nil
          , unsigned int
          , x3::forward_ast<signed_>
          , x3::forward_ast<expression>
        >
    {
        using base_type::base_type;
        using base_type::operator=;
    };

    struct signed_
    {
        char sign;
        operand operand_;
    };

    struct operation
    {
        char operator_;
        operand operand_;
    };

    struct expression
    {
        operand first;
        std::list<operation> rest;
    };

    // print function for debugging
    inline std::ostream& operator<<(std::ostream& out, nil)
    {
        out << "nil";
        return out;
    }
}}

#endif
