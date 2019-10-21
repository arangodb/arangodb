
#ifndef BOOST_CONTRACT_TEST_FUNCTION_DECL_HPP_
#define BOOST_CONTRACT_TEST_FUNCTION_DECL_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with and without pre and post declarations.

#include "../detail/oteststream.hpp"
#include <boost/contract/function.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>

boost::contract::test::detail::oteststream out;

bool f_pre = true, f_post = true;
void f() {
    boost::contract::check c = boost::contract::function()
        #ifndef BOOST_CONTRACT_TEST_NO_F_PRE
            .precondition([] {
                out << "f::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(f_pre);
            })
        #endif
        .old([] { out << "f::old" << std::endl; })
        #ifndef BOOST_CONTRACT_TEST_NO_F_POST
            .postcondition([] {
                out << "f::post" << std::endl;
                BOOST_CONTRACT_ASSERT(f_post);
            })
        #endif
    ;
    out << "f::body" << std::endl;
}

#endif // #include guard

