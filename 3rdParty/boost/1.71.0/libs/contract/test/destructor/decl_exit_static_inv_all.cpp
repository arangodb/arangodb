
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test all derived and base classes with exit static invariants.

#undef BOOST_CONTRACT_TEST_NO_A_STATIC_INV
#undef BOOST_CONTRACT_TEST_NO_B_STATIC_INV
#undef BOOST_CONTRACT_TEST_NO_C_STATIC_INV
#include "decl.hpp"

#include <boost/preprocessor/control/iif.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>

std::string ok_a(bool failed = false) {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::dtor::old" << std::endl
        #endif
        << "a::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl // This can fail.
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << (!failed ? "a::dtor::post\n" : "")
        #endif
    ;
    return ok.str();
}

enum checked { passed, failed, threw };

std::string ok_b(checked check = passed) {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::dtor::old" << std::endl
        #endif
        << "b::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl // This can fail.
            << (check == threw ? "b::inv\n" : "")
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << (check == passed ? "b::dtor::post\n" : "")
        #endif
    ;
    return ok.str();
}
        
std::string ok_c(checked check = passed) {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
            << "c::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::dtor::old" << std::endl
        #endif
        << "c::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::static_inv" << std::endl // This can fail.
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

    #ifdef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
        #define BOOST_CONTRACT_TEST_entry_inv 0
    #else
        #define BOOST_CONTRACT_TEST_entry_inv 1
    #endif
    #ifdef BOOST_CONTRACT_NO_EXIT_INVARIANTS
        #define BOOST_CONTRACT_TEST_exit_inv 0
    #else
        #define BOOST_CONTRACT_TEST_exit_inv 1
    #endif
    
    a_exit_static_inv = true;
    b_exit_static_inv = true;
    c_exit_static_inv = true;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
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
    
    boost::contract::set_exit_invariant_failure([&ok] (boost::contract::from) {
        BOOST_TEST(out.eq(ok.str())); // Must check before dtor throws.
        throw err(); // For testing only (as dtors should not throw otherwise).
    });

    a_exit_static_inv = false;
    b_exit_static_inv = true;
    c_exit_static_inv = true;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
    try {
        {
            a aa;
            ok.str(""); ok
                // Test a::static_inv failed...
                << ok_a(BOOST_CONTRACT_TEST_exit_inv)
            ;
            out.str("");
        }
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok // ...then exec other dtors and check inv on throw (as dtor threw).
            << ok_b(BOOST_CONTRACT_TEST_exit_inv ? threw : passed)
            << ok_c(BOOST_CONTRACT_TEST_exit_inv ? threw : passed)
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_exit_static_inv = true;
    b_exit_static_inv = false;
    c_exit_static_inv = true;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
    try {
        {
            a aa;
            ok.str(""); ok
                << ok_a()
                // Test b::static_inv failed...
                << ok_b(BOOST_CONTRACT_TEST_exit_inv ? failed : passed)
            ;
            out.str("");
        }
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok // ...then exec other dtors and check inv on throw (as dtor threw).
            << ok_c(BOOST_CONTRACT_TEST_exit_inv ? threw : passed)
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_exit_static_inv = true;
    b_exit_static_inv = true;
    c_exit_static_inv = false;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
    try {
        {
            a aa;
            ok.str(""); ok
                << ok_a()
                << ok_b()
                // Test exit c::static_inv failed.
                << ok_c(BOOST_CONTRACT_TEST_exit_inv ? failed : passed)
            ;
            out.str("");
        }
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        // ... then exec other dtors and check inv on throw (as dtor threw).
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    boost::contract::set_exit_invariant_failure([] (boost::contract::from) {
        // Testing multiple failures but dtors must not throw multiple except,
        // just ignore failure and continue test program (for testing only).
    });
    
    a_exit_static_inv = false;
    b_exit_static_inv = false;
    c_exit_static_inv = false;
    a_entering_static_inv = b_entering_static_inv = c_entering_static_inv =
            BOOST_PP_IIF(BOOST_CONTRACT_TEST_entry_inv, true, false);
    {
        a aa;
        out.str("");
    }
    ok.str(""); ok
        // Test exit a::static_inv failed (as all did).
        << ok_a(BOOST_CONTRACT_TEST_exit_inv)
        // Test exit b::static_inv failed (as all did).
        << ok_b(BOOST_CONTRACT_TEST_exit_inv ? failed : passed)
        // Test exit c::static_inv failed (as all did).
        << ok_c(BOOST_CONTRACT_TEST_exit_inv ? failed : passed)
    ;
    BOOST_TEST(out.eq(ok.str()));

    #undef BOOST_CONTRACT_TEST_entry_inv
    #undef BOOST_CONTRACT_TEST_exit_inv
    return boost::report_errors();
}

