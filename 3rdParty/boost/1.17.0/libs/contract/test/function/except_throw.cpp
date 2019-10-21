
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test throw from free function .old().

#include "../detail/oteststream.hpp"
#include <boost/contract/function.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct err {}; // Global decl so visible in MSVC10 lambdas.

void f() {
    boost::contract::check c = boost::contract::function()
        .precondition([] { out << "f::pre" << std::endl; })
        .old([] {
            out << "f::old" << std::endl;
            throw err(); // Test .old() throws.
        })
        .postcondition([] { out << "f::post" << std::endl; })
    ;
    out << "f::body" << std::endl;
}

int main() {
    std::ostringstream ok;

    boost::contract::set_old_failure([] (boost::contract::from) { throw; });

    try {
        out.str("");
        f();
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            BOOST_TEST(false);
        #endif
    } catch(err const&) {
        ok.str(""); ok << "" // Suppress a warning.
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "f::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "f::old" << std::endl // Test this threw.
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

