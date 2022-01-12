
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contract compilation on/off (including EXCEPT, using macro interface).

#include "../detail/oteststream.hpp"
#include "../detail/unprotected_commas.hpp"
#include <boost/contract_macro.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct except_error {};

void f(int x) {
    BOOST_CONTRACT_OLD_PTR(
        boost::contract::test::detail::unprotected_commas<int, void, void>::
                type1
    )(
        old_x,
        (boost::contract::test::detail::unprotected_commas<void, void, void>::
                same(x))
    );
    BOOST_CONTRACT_FUNCTION()
        BOOST_CONTRACT_PRECONDITION([] {
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::call();
            out << "f::pre" << std::endl;
        })
        BOOST_CONTRACT_OLD([] {
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::call();
            out << "f::old" << std::endl;
        })
        BOOST_CONTRACT_POSTCONDITION([] {
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::call();
            out << "f::post" << std::endl;
        })
        BOOST_CONTRACT_EXCEPT([] { // Test EXCEPT macro (at least 1 time here).
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::call();
            out << "f::except" << std::endl;
        })
    ;
    out << "f::body" << std::endl;
    if(x == -1) throw except_error();
}

int main() {
    std::ostringstream ok;

    out.str("");
    f(123);
    ok.str(""); ok
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
    BOOST_TEST(out.eq(ok.str()));

    boost::contract::set_except_failure([] (boost::contract::from) {});
    out.str("");
    try {
        f(-1); // Test throw and EXCEPT macro (test only here... that's fine).
        BOOST_TEST(false);
    } catch(except_error const&) {} // OK.
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "f::old" << std::endl
        #endif
        << "f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXCEPTS
            << "f::except" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

