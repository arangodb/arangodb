
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test all derived and base classes with preconditions.

#undef BOOST_CONTRACT_TEST_NO_A_PRE
#undef BOOST_CONTRACT_TEST_NO_B_PRE
#undef BOOST_CONTRACT_TEST_NO_C_PRE
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>

std::string ok_after() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::ctor::old" << std::endl
        #endif
        << "c::ctor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::static_inv" << std::endl
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
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
        #endif
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
    
    a_pre = true;
    b_pre = true;
    c_pre = true;
    {
        out.str("");
        a aa;
        ok.str(""); ok // Test nothing failed.
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::ctor::pre" << std::endl
                << "b::ctor::pre" << std::endl
                << "c::ctor::pre" << std::endl
            #endif
            << ok_after()
        ;
        BOOST_TEST(out.eq(ok.str()));
    }

    boost::contract::set_precondition_failure(
            [] (boost::contract::from) { throw err(); });

    a_pre = false;
    b_pre = true;
    c_pre = true;
    try {
        out.str("");
        a aa;
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::ctor::pre" << std::endl // Test this failed.
            #else
                << ok_after()
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_pre = true;
    b_pre = false;
    c_pre = true;
    try {
        out.str("");
        a aa;
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::ctor::pre" << std::endl
                << "b::ctor::pre" << std::endl // Test this failed.
            #else
                << ok_after()
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_pre = true;
    b_pre = true;
    c_pre = false;
    try {
        out.str("");
        a aa;
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::ctor::pre" << std::endl
                << "b::ctor::pre" << std::endl
                << "c::ctor::pre" << std::endl // Test this failed.
            #else
                << ok_after()
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }
    
    a_pre = false;
    b_pre = false;
    c_pre = false;
    try {
        out.str("");
        a aa;
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::ctor::pre" << std::endl // Test this failed (as all did).
            #else
                << ok_after()
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

