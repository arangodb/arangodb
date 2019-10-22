
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test only middle base class with entry invariants.

#define BOOST_CONTRACT_TEST_NO_A_INV
#undef BOOST_CONTRACT_TEST_NO_B_INV
#define BOOST_CONTRACT_TEST_NO_C_INV
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>
        
std::string ok_a() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::dtor::old" << std::endl
        #endif
        << "a::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::dtor::post" << std::endl
        #endif
    ;
    return ok.str();
}

std::string ok_b(bool failed = false) {
    std::ostringstream ok; ok << "" // Suppress a warning.
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl // This can fail.
        #endif
    ;
    if(!failed) ok
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::dtor::old" << std::endl
        #endif
        << "b::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::dtor::post" << std::endl
        #endif
    ;
    return ok.str();
}
        
std::string ok_c(bool threw = false) {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::dtor::old" << std::endl
        #endif
        << "c::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::static_inv" << std::endl
            // No b::inv here (not even when threw).
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << (!threw ? "c::dtor::post\n" : "")
        #endif
    ;
    return ok.str();
}

struct err {}; // Global decl so visible in MSVC10 lambdas.

int main() {
    std::ostringstream ok;
    
    a_entry_inv = true;
    b_entry_inv = true;
    c_entry_inv = true;
    {
        a aa;
        out.str("");
    }
    ok.str(""); ok // Test nothing failed.
        << ok_a()
        << ok_b()
        << ok_c()
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    boost::contract::set_entry_invariant_failure([&ok] (boost::contract::from) {
        BOOST_TEST(out.eq(ok.str())); // Must check before dtor throws...
        throw err(); // for testing (as dtors should never throw anyways).
    });

    a_entry_inv = false;
    b_entry_inv = true;
    c_entry_inv = true;
    try {
        {
            a aa;
            ok.str("");
            out.str("");
        }
        ok.str(""); ok
            << ok_a() // Test no entry a::inv so no failure here.
            << ok_b()
            << ok_c()
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    #ifdef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
        #define BOOST_CONTRACT_TEST_entry_inv 0
    #else
        #define BOOST_CONTRACT_TEST_entry_inv 2
    #endif
    
    a_entry_inv = true;
    b_entry_inv = false;
    c_entry_inv = true;
    try {
        {
            a aa;
            ok.str(""); ok
                << ok_a()
                // Test entry b::inv failed...
                << ok_b(bool(BOOST_CONTRACT_TEST_entry_inv))
            ;
            out.str("");
        }
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok // ...then exec other dtors and check inv on throw (as dtor threw).
            << ok_c(bool(BOOST_CONTRACT_TEST_entry_inv))
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_entry_inv = true;
    b_entry_inv = true;
    c_entry_inv = false;
    try {
        {
            a aa;
            ok.str("");
            out.str("");
        }
        ok.str(""); ok
            << ok_a()
            << ok_b()
            << ok_c() // Test no entry c::inv so no failure here.
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    boost::contract::set_entry_invariant_failure([] (boost::contract::from) {
        // Testing multiple failures so dtors must not throw multiple
        // exceptions, just ignore failure and continue test program...
    });
    
    a_entry_inv = false;
    b_entry_inv = false;
    c_entry_inv = false;
    {
        a aa;
        out.str("");
    }
    ok.str(""); ok
        << ok_a() // Test no entry a::inv so no failure here.
        
        // Test entry b::inv failed (as all did).
        << ok_b(bool(BOOST_CONTRACT_TEST_entry_inv))
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::dtor::body" << std::endl
        #endif
        
        << ok_c() // Test no entry c::inv so no failure here.
    ;
    BOOST_TEST(out.eq(ok.str()));

    #undef BOOST_CONTRACT_TEST_entry_inv
    return boost::report_errors();
}

