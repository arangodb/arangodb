
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test only middle base class with exit static invariants.

#define BOOST_CONTRACT_TEST_NO_A_STATIC_INV
#undef BOOST_CONTRACT_TEST_NO_B_STATIC_INV
#define BOOST_CONTRACT_TEST_NO_C_STATIC_INV
#include "decl.hpp"

#include <boost/preprocessor/control/iif.hpp>
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

        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::ctor::old" << std::endl
        #endif
        << "c::ctor::body" << std::endl
        // No failure here.
    ;
    return ok.str();
}

std::string ok_b() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::inv" << std::endl
        #endif
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
            << "b::static_inv" << std::endl // This can fail.
        #endif
    ;
    return ok.str();
}

std::string ok_a() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::ctor::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::ctor::old" << std::endl
        #endif
        << "a::ctor::body" << std::endl
        // No failure here.
    ;
    return ok.str();
}

std::string ok_end() {
    std::ostringstream ok; ok << "" // Suppress a warning.
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::ctor::post" << std::endl
        #endif
    ;
    return ok.str();
}

struct err {}; // Global decl so visible in MSVC10 lambdas.

int main() {
    std::ostringstream ok;

    #ifdef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
        #define BOOST_CONTRACT_TEST_entry_inv 0
    #else
        #define BOOST_CONTRACT_TEST_entry_inv 1
    #endif
    
    a_exit_static_inv = true;
    b_exit_static_inv = true;
    c_exit_static_inv = true;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
    {
        out.str("");
        a aa;
        ok.str(""); ok // Test nothing fails.
            << ok_c()
            << ok_b()
            << ok_a()
            << ok_end()
        ;
        BOOST_TEST(out.eq(ok.str()));
    }
    
    boost::contract::set_exit_invariant_failure(
            [] (boost::contract::from) { throw err(); });

    a_exit_static_inv = false;
    b_exit_static_inv = true;
    c_exit_static_inv = true;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
    try {
        out.str("");
        a aa;
        ok.str(""); ok
            << ok_c()
            << ok_b()
            << ok_a() // Test no a::static_inv so no failure.
            << ok_end()
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_exit_static_inv = true;
    b_exit_static_inv = false;
    c_exit_static_inv = true;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
    try {
        out.str("");
        a aa;
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_c()
            << ok_b() // Test b::static_inv failed.
            #ifdef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << ok_a()
                << ok_end()
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_exit_static_inv = true;
    b_exit_static_inv = true;
    c_exit_static_inv = false;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
    try {
        out.str("");
        a aa;
        ok.str(""); ok
            << ok_c() // Test no c::static_inv so no failure.
            << ok_b()
            << ok_a()
            << ok_end()
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_exit_static_inv = false;
    b_exit_static_inv = false;
    c_exit_static_inv = false;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
    try {
        out.str("");
        a aa;
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_c()
            << ok_b() // Test b::static_inv failed.
            #ifdef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << ok_a()
                << ok_end()
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

