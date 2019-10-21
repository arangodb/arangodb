
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with postconditions.

#undef BOOST_CONTRACT_TEST_NO_F_POST
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>
        
std::string ok_f() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "f::old" << std::endl
        #endif
        << "f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "f::post" << std::endl // This can fail.
        #endif
    ;
    return ok.str();
}

struct err {}; // Global decl so visible in MSVC10 lambdas.

int main() {
    std::ostringstream ok;

    f_post = true;
    out.str("");
    f();
    ok.str(""); ok // Test nothing failed.
        << ok_f()
    ;
    BOOST_TEST(out.eq(ok.str()));

    boost::contract::set_postcondition_failure(
            [] (boost::contract::from) { throw err(); });

    f_post = false;
    out.str("");
    try {
        f();
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_f() // Test f::post failed.
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

