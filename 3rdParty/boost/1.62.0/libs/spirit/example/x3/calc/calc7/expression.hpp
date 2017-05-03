/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC7_EXPRESSION_HPP)
#define BOOST_SPIRIT_X3_CALC7_EXPRESSION_HPP

#include <boost/spirit/home/x3.hpp>
#include "ast.hpp"

namespace client
{
    namespace x3 = boost::spirit::x3;
    namespace calculator_grammar
    {
        struct expression_class;
        typedef x3::rule<expression_class, ast::expression> expression_type;
        BOOST_SPIRIT_DECLARE(expression_type);
    }
    
    calculator_grammar::expression_type expression();
}

#endif
