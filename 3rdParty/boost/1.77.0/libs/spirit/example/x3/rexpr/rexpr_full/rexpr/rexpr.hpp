/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_REXPR_HPP)
#define BOOST_SPIRIT_X3_REXPR_HPP

#include "ast.hpp"

#include <boost/spirit/home/x3.hpp>

namespace rexpr
{
    namespace x3 = boost::spirit::x3;

    ///////////////////////////////////////////////////////////////////////////
    // rexpr public interface
    ///////////////////////////////////////////////////////////////////////////
    namespace parser
    {
        struct rexpr_class;
        typedef
            x3::rule<rexpr_class, ast::rexpr>
        rexpr_type;
        BOOST_SPIRIT_DECLARE(rexpr_type);
    }

    parser::rexpr_type const& rexpr();
}

#endif
