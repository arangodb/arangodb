
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test public static member function throwing.

#include "../detail/oteststream.hpp"
#include <boost/contract/public_function.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct a_err {}; // Global decl so visible in MSVC10 lambdas.

struct a {
    static void static_invariant() { out << "a::static_inv" << std::endl; }
    void invariant() const { out << "a::inv" << std::endl; }

    static void f() {
        boost::contract::check c = boost::contract::public_function<a>()
            .precondition([] { out << "a::f::pre" << std::endl; })
            .old([] { out << "a::f::old" << std::endl; })
            .postcondition([] { out << "a::f::post" << std::endl; })
            .except([] { out << "a::f::except" << std::endl; })
        ;
        out << "a::f::body" << std::endl;
        throw a_err(); // Test this throws.
    }
};

int main() {
    std::ostringstream ok;

    try {
        out.str("");
        a::f();
        BOOST_TEST(false);
    } catch(a_err const&) {
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "a::static_inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::f::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::f::old" << std::endl
            #endif
            << "a::f::body" << std::endl // Test this threw.
            // Test no post (but still static inv) because body threw.
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "a::static_inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_EXCEPTS
                << "a::f::except" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

