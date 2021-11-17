/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC8_AST_ADAPTED_HPP)
#define BOOST_SPIRIT_X3_CALC8_AST_ADAPTED_HPP

#include "ast.hpp"
#include <boost/fusion/include/adapt_struct.hpp>

BOOST_FUSION_ADAPT_STRUCT(client::ast::signed_,
    sign, operand_
)

BOOST_FUSION_ADAPT_STRUCT(client::ast::operation,
    operator_, operand_
)

BOOST_FUSION_ADAPT_STRUCT(client::ast::expression,
    first, rest
)

BOOST_FUSION_ADAPT_STRUCT(client::ast::variable_declaration,
    assign
)

BOOST_FUSION_ADAPT_STRUCT(client::ast::assignment,
    lhs, rhs
)

#endif
