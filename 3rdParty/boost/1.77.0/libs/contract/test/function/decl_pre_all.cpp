
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with preconditions.

#undef BOOST_CONTRACT_TEST_NO_F_PRE
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>

std::string ok_f(bool failed = false) {
    std::ostringstream ok; ok << "" // Suppress a warning.
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "f::pre" << std::endl // Test no failure here.
        #endif
    ;
    if(!failed) ok
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "f::old" << std::endl
        #endif
        << "f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "f::post" << std::endl
        #endif
    ;
    return ok.str();
}

struct err {}; // Global decl so visible in MSVC10 lambdas.

int main() {
    std::ostringstream ok;

    f_pre = true;
    out.str("");
    f();
    ok.str(""); ok
        << ok_f()
    ;
    BOOST_TEST(out.eq(ok.str()));

    #ifdef BOOST_CONTRACT_NO_PRECONDITIONS
        #define BOOST_CONTRACT_TEST_pre 0
    #else
        #define BOOST_CONTRACT_TEST_pre 1
    #endif

    boost::contract::set_precondition_failure(
            [] (boost::contract::from) { throw err(); });

    f_pre = false;
    out.str("");
    try {
        f();
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_f(BOOST_CONTRACT_TEST_pre)
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    #undef BOOST_CONTRACT_TEST_pre
    return boost::report_errors();
}

