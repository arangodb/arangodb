
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test all derived and base classes without preconditions.

#define BOOST_CONTRACT_TEST_NO_A_PRE
#define BOOST_CONTRACT_TEST_NO_B_PRE
#define BOOST_CONTRACT_TEST_NO_C_PRE
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>

int main() {
    std::ostringstream ok; ok // Test nothing fails.
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
            << "c::inv" << std::endl
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        // No preconditions here.
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
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "c::f::old" << std::endl // Old only if post (or except) run.
            << "c::f::post" << std::endl
            << "b::f::old" << std::endl
            << "b::f::post" << std::endl
            << "a::f::post" << std::endl
        #endif
    ;

    a aa;
    
    a_pre = true;
    b_pre = true;
    c_pre = true;
    out.str("");
    aa.f();
    BOOST_TEST(out.eq(ok.str()));

    a_pre = true;
    b_pre = false;
    c_pre = false;
    out.str("");
    aa.f();
    BOOST_TEST(out.eq(ok.str()));
    
    a_pre = false;
    b_pre = true;
    c_pre = false;
    out.str("");
    aa.f();
    BOOST_TEST(out.eq(ok.str()));
    
    a_pre = false;
    b_pre = false;
    c_pre = true;
    out.str("");
    aa.f();
    BOOST_TEST(out.eq(ok.str()));
    
    a_pre = false;
    b_pre = false;
    c_pre = false;
    out.str("");
    aa.f();
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

