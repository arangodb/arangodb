/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC7_COMPILER_HPP)
#define BOOST_SPIRIT_X3_CALC7_COMPILER_HPP

#include "ast.hpp"

namespace client
{
    ///////////////////////////////////////////////////////////////////////////
    //  The Compiler
    ///////////////////////////////////////////////////////////////////////////
    struct compiler
    {
        typedef void result_type;

        std::vector<int>& code;
        compiler(std::vector<int>& code)
          : code(code) {}

        void operator()(ast::nil) const;
        void operator()(unsigned int n) const;
        void operator()(ast::operation const& x) const;
        void operator()(ast::signed_ const& x) const;
        void operator()(ast::expression const& x) const;
    };
}

#endif
