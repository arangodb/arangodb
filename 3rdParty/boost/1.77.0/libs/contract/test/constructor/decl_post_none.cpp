
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test only middle base class with postconditions.

#define BOOST_CONTRACT_TEST_NO_A_POST
#define BOOST_CONTRACT_TEST_NO_B_POST
#define BOOST_CONTRACT_TEST_NO_C_POST
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>

int main() {
    std::ostringstream ok; ok // Test nothing fails.
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
            << "c::inv" << std::endl
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
    ;
    
    a_post = true;
    b_post = true;
    c_post = true;
    {
        out.str("");
        a aa;
        BOOST_TEST(out.eq(ok.str()));
    }

    a_post = false;
    b_post = true;
    c_post = true;
    {
        out.str("");
        a aa;
        BOOST_TEST(out.eq(ok.str()));
    }
    
    a_post = true;
    b_post = false;
    c_post = true;
    {
        out.str("");
        a aa;
        BOOST_TEST(out.eq(ok.str()));
    }
    
    a_post = true;
    b_post = true;
    c_post = false;
    {
        out.str("");
        a aa;
        BOOST_TEST(out.eq(ok.str()));
    }
    
    a_post = false;
    b_post = false;
    c_post = false;
    {
        out.str("");
        a aa;
        BOOST_TEST(out.eq(ok.str()));
    }
 
    return boost::report_errors();
}

