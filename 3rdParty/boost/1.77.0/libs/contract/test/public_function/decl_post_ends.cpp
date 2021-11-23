
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test only derived and grandparent classes (ends) with postconditions.

#undef BOOST_CONTRACT_TEST_NO_A_POST
#define BOOST_CONTRACT_TEST_NO_B_POST
#undef BOOST_CONTRACT_TEST_NO_C_POST
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>

std::string ok_begin() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
            << "c::inv" << std::endl
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "c::f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::f::old" << std::endl
            << "b::f::old" << std::endl
            << "a::f::old" << std::endl
        #endif
        << "a::f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::static_inv" << std::endl
            << "c::inv" << std::endl
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
    ;
    return ok.str();
}

struct err {}; // Global decl so visible in MSVC10 lambdas.

int main() {
    std::ostringstream ok;
    
    a aa;
    
    a_post = true;
    b_post = true;
    c_post = true;
    out.str("");
    aa.f();
    ok.str(""); ok // Test nothing failed.
        << ok_begin()
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "c::f::old" << std::endl
            << "c::f::post" << std::endl
            << "b::f::old" << std::endl
            << "a::f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    boost::contract::set_postcondition_failure(
            [] (boost::contract::from) { throw err(); });

    a_post = false;
    b_post = true;
    c_post = true;
    out.str("");
    try {
        aa.f();
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_begin()
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "c::f::old" << std::endl
                << "c::f::post" << std::endl
                << "b::f::old" << std::endl
                << "a::f::post" << std::endl // Test this failed.
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_post = true;
    b_post = false;
    c_post = true;
    out.str("");
    try {
        aa.f();
        ok.str(""); ok
            << ok_begin()
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "c::f::old" << std::endl
                << "c::f::post" << std::endl
                << "b::f::old" << std::endl
                // Test no failure here.
                << "a::f::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_post = true;
    b_post = true;
    c_post = false;
    out.str("");
    try {
        aa.f();
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_begin()
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "c::f::old" << std::endl
                << "c::f::post" << std::endl // Test this failed.
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_post = false;
    b_post = false;
    c_post = false;
    out.str("");
    try {
        aa.f();
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_begin()
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "c::f::old" << std::endl
                << "c::f::post" << std::endl // Test this failed (as all did).
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

