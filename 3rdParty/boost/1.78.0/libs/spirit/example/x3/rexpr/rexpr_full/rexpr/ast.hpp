/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_REPR_AST_HPP)
#define BOOST_SPIRIT_X3_REPR_AST_HPP

#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>

#include <map>

namespace rexpr { namespace ast
{
    ///////////////////////////////////////////////////////////////////////////
    //  The AST
    ///////////////////////////////////////////////////////////////////////////
    namespace x3 = boost::spirit::x3;

    struct rexpr;

    struct rexpr_value : x3::variant<
            std::string
          , x3::forward_ast<rexpr>
        >
    {
        using base_type::base_type;
        using base_type::operator=;
    };

    typedef std::map<std::string, rexpr_value> rexpr_map;
    typedef std::pair<std::string, rexpr_value> rexpr_key_value;

    struct rexpr : x3::position_tagged
    {
        rexpr_map entries;
    };
}}

#endif
