
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test only middle class with exit invariants.

#define BOOST_CONTRACT_TEST_NO_A_INV
#undef BOOST_CONTRACT_TEST_NO_B_INV
#define BOOST_CONTRACT_TEST_NO_C_INV
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>
            
std::string ok_c() {
    std::ostringstream ok; ok << "" // Suppress a warning.
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "a::ctor::pre" << std::endl
            << "b::ctor::pre" << std::endl
            << "c::ctor::pre" << std::endl
        #endif
        
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::ctor::old" << std::endl
        #endif
        << "c::ctor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::static_inv" << std::endl
            // No failure here.
        #endif
    ;
    return ok.str();
}

std::string ok_b() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "c::ctor::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::ctor::old" << std::endl
        #endif
        << "b::ctor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl // This can fail.
        #endif
    ;
    return ok.str();
}

std::string ok_a() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::ctor::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::ctor::old" << std::endl
        #endif
        << "a::ctor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl
            // No failure here.
        #endif
    ;
    return ok.str();
}

std::string ok_end() {
    std::ostringstream ok; ok << "" // Suppress a warning.
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::ctor::post" << std::endl
        #endif
    ;
    return ok.str();
}

struct err {}; // Global decl so visible in MSVC10 lambdas.

int main() {
    std::ostringstream ok;
    
    a_exit_inv = true;
    b_exit_inv = true;
    c_exit_inv = true;
    {
        out.str("");
        a aa;
        ok.str(""); ok // Test nothing failed.
            << ok_c()
            << ok_b()
            << ok_a()
            << ok_end()
        ;
        BOOST_TEST(out.eq(ok.str()));
    }
    
    boost::contract::set_exit_invariant_failure(
            [] (boost::contract::from) { throw err(); });

    a_exit_inv = false;
    b_exit_inv = true;
    c_exit_inv = true;
    try {
        out.str("");
        a aa;
        ok.str(""); ok
            << ok_c()
            << ok_b()
            << ok_a() // Test no failure here.
            << ok_end()
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    a_exit_inv = true;
    b_exit_inv = false;
    c_exit_inv = true;
    try {
        out.str("");
        a aa;
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_c()
            << ok_b() // Test b::inv failed.
            #ifdef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << ok_a()
                << ok_end()
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_exit_inv = true;
    b_exit_inv = true;
    c_exit_inv = false;
    try {
        out.str("");
        a aa;
        ok.str(""); ok
            << ok_c() // Test no failure here.
            << ok_b()
            << ok_a()
            << ok_end()
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_exit_inv = false;
    b_exit_inv = false;
    c_exit_inv = false;
    try {
        out.str("");
        a aa;
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_c()
            << ok_b() // Test b::inv failed.
            #ifdef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << ok_a()
                << ok_end()
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

