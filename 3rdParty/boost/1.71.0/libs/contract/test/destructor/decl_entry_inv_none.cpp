
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test all derived and base classes without entry invariants.

#define BOOST_CONTRACT_TEST_NO_A_INV
#define BOOST_CONTRACT_TEST_NO_B_INV
#define BOOST_CONTRACT_TEST_NO_C_INV
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>

int main() {
    std::ostringstream ok; ok // Test nothing fails.
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

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
        #endif
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
            
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::dtor::old" << std::endl
        #endif
        << "c::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "c::dtor::post" << std::endl
        #endif
    ;
    
    a_entry_inv = true;
    b_entry_inv = true;
    c_entry_inv = true;
    {
        a aa;
        out.str("");
    }
    BOOST_TEST(out.eq(ok.str()));
    
    a_entry_inv = false;
    b_entry_inv = true;
    c_entry_inv = true;
    {
        a aa;
        out.str("");
    }
    BOOST_TEST(out.eq(ok.str()));
    
    a_entry_inv = true;
    b_entry_inv = false;
    c_entry_inv = true;
    {
        a aa;
        out.str("");
    }
    BOOST_TEST(out.eq(ok.str()));
    
    a_entry_inv = true;
    b_entry_inv = true;
    c_entry_inv = false;
    {
        a aa;
        out.str("");
    }
    BOOST_TEST(out.eq(ok.str()));
    
    a_entry_inv = false;
    b_entry_inv = false;
    c_entry_inv = false;
    {
        a aa;
        out.str("");
    }
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

