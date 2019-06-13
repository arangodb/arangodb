
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contract compilation on/off.

#include "../detail/oteststream.hpp"
#include <boost/contract/core/config.hpp>
#ifndef BOOST_CONTRACT_NO_CHECKS
    #include <boost/contract/check.hpp>
    #include <boost/contract/assert.hpp>
#else
    #include <boost/contract/core/check_macro.hpp>
    #include <boost/contract/core/exception.hpp>
#endif
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

void f(bool check1, bool check2) {
    BOOST_CONTRACT_CHECK([&] () -> bool { // Macro already so #ifdef needed.
        out << "f::check1" << std::endl;
        return check1;
    }());
    #ifndef BOOST_CONTRACT_NO_CHECKS
        boost::contract::check c = [&] {
            out << "f::check2" << std::endl;
            BOOST_CONTRACT_ASSERT(check2);
        };
    #endif
    out << "f::body" << std::endl;
}

struct err {}; // Global decl so visible in MSVC10 lambdas.

int main() {
    std::ostringstream ok;

    out.str("");
    f(true, true);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_CHECKS
            << "f::check1" << std::endl
            << "f::check2" << std::endl
        #endif
        << "f::body" << std::endl
    ;
    BOOST_TEST(out.eq(ok.str()));

    boost::contract::set_check_failure([] { throw err(); });

    out.str("");
    try {
        f(false, true);
        #ifndef BOOST_CONTRACT_NO_CHECKS
            BOOST_TEST(false);
            } catch(err const&) {
                ok.str("");
                ok << "f::check1" << std::endl;
        #else
            ok.str("");
            ok << "f::body" << std::endl;
        #endif
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    out.str("");
    try {
        f(true, false);
        #ifndef BOOST_CONTRACT_NO_CHECKS
            BOOST_TEST(false);
            } catch(err const&) {
                ok.str("");
                ok << "f::check1" << std::endl;
                ok << "f::check2" << std::endl;
        #else
            ok.str("");
            ok << "f::body" << std::endl;
        #endif
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    // No need to test `f(false, false)` because same as `f(false, true)`.

    return boost::report_errors();
}

