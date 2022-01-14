
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contracts in .cpp compiled to never check post/except (but not in .hpp).

#include "lib_x.hpp"
#include "lib_y.hpp"
#include "../detail/oteststream.hpp"
#include <boost/contract/function.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

void f() {
    using boost::contract::test::detail::out;
    boost::contract::check c = boost::contract::function()
        // Capturing [&] so out() visible in MSVC10 lambdas.
        .precondition([&] { out("f::pre\n"); })
        .old([&] { out("f::old\n"); })
        .postcondition([&] { out("f::post\n"); })
    ;
    out("f::body\n");
}

int main() {
    using boost::contract::test::detail::out;
    std::ostringstream ok;

    out("");
    f();
    ok.str(""); ok // Test normal (no lib) case.
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "f::old" << std::endl
        #endif
        << "f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "f::post" << std::endl
        #endif
    ;
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(), ok.str()));

    out("");
    x();
    ok.str(""); ok // Test contracts in .cpp so no post (NO_POST in build file).
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "x::pre" << std::endl
        #endif
        << "x::body" << std::endl
    ;
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(), ok.str()));
    
    out("");
    y();
    ok.str(""); ok // Test contracts in .hpp so post (NO_POST in build file).
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "y::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "y::old" << std::endl
        #endif
        << "y::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "y::post" << std::endl
        #endif
    ;
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(), ok.str()));
    
    return boost::report_errors();
}

