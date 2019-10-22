
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test only derived and grandparent classes (ends) with entry invariants.

#undef BOOST_CONTRACT_TEST_NO_A_INV
#define BOOST_CONTRACT_TEST_NO_B_INV
#undef BOOST_CONTRACT_TEST_NO_C_INV
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string> 

std::string ok_a(bool failed = false) {
    std::ostringstream ok; ok << "" // Suppress a warning.
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl // This can fail.
        #endif
    ;
    if(!failed) ok
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

std::string ok_b(bool threw = false) {
    std::ostringstream ok; ok << "" // Suppress a warning.
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::dtor::old" << std::endl
        #endif
        << "b::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl
            // No b::inv here (not even when threw).
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << (!threw ? "b::dtor::post\n" : "")
        #endif
    ;
    return ok.str();
}

enum checked { passed, failed, threw };
        
std::string ok_c(checked check = passed) {
    std::ostringstream ok; ok << "" // Suppress a warning.
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
            << "c::inv" << std::endl // This can fail.
        #endif
    ;
    if(check != failed) ok
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::dtor::old" << std::endl
        #endif
        << "c::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::static_inv" << std::endl
            << (check == threw ? "c::inv\n" : "")
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << (check == passed ? "c::dtor::post\n" : "")
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

    #ifdef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
        #define BOOST_CONTRACT_TEST_entry_inv 0
    #else
        #define BOOST_CONTRACT_TEST_entry_inv 1
    #endif

    a_entry_inv = false;
    b_entry_inv = true;
    c_entry_inv = true;
    try {
        {
            a aa;
            ok.str(""); ok
                // Test entry a::inv failed...
                << ok_a(BOOST_CONTRACT_TEST_entry_inv)
            ;
            out.str("");
        }
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok // ...then exec other dtors and check inv on throw (as dtor threw).
            << ok_b(BOOST_CONTRACT_TEST_entry_inv)
            << ok_c(BOOST_CONTRACT_TEST_entry_inv ? threw : passed)
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_entry_inv = true;
    b_entry_inv = false;
    c_entry_inv = true;
    {
        a aa;
        out.str("");
    }
    ok.str(""); ok
        << ok_a()
        << ok_b() // Test no entry b::inv so no failure here.
        << ok_c()
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    a_entry_inv = true;
    b_entry_inv = true;
    c_entry_inv = false;
    try {
        {
            a aa;
            ok.str(""); ok
                << ok_a()
                << ok_b()
                // Test entry c::inv failed...
                << ok_c(BOOST_CONTRACT_TEST_entry_inv ? failed : passed)
            ;
            out.str("");
        }
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        // ...then exec other dtors and check inv on throw (as dtor threw).
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
        // Test entry a::inv failed (as all did).
        << ok_a(BOOST_CONTRACT_TEST_entry_inv)
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::dtor::body" << std::endl
        #endif
        
        << ok_b() // Test no entry b::inv so no failure here.
        
        // Test entry c::inv failed (as all did).
        << ok_c(BOOST_CONTRACT_TEST_entry_inv ? failed : passed)
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::dtor::body" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    #undef BOOST_CONTRACT_TEST_entry_inv
    return boost::report_errors();
}

