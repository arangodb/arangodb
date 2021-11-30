
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test invariant compilation on/off.

#include "../detail/oteststream.hpp"
#include <boost/contract/constructor.hpp>
#include <boost/contract/destructor.hpp>
#include <boost/contract/public_function.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

class a {
public:
    #ifndef BOOST_CONTRACT_NO_INVARIANTS
        static void static_invariant() {
            out << "a::static_inv" << std::endl;
        }
        
        void invariant() const volatile {
            out << "a::cv_inv" << std::endl;
        }

        void invariant() const {
            out << "a::const_inv" << std::endl;
        }
    #endif

    a() { // Test check both cv and const invariant (at exit if no throw).
        #ifndef BOOST_CONTRACT_NO_CONSTRUCTORS
            boost::contract::check c= boost::contract::constructor(this);
        #endif
        out << "a::ctor::body" << std::endl;
    }

    ~a() { // Test check both cv and const invariant (at entry).
        #ifndef BOOSTT_CONTRACT_NO_DESTRUCTORS
            boost::contract::check c = boost::contract::destructor(this);
        #endif
        out << "a::dtor::body" << std::endl;
    }

    void m() { // Test check const invariant (at entry and exit).
        #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            boost::contract::check c = boost::contract::public_function(this);
        #endif
        out << "a::m::body" << std::endl;
    }

    void c() const { // Test check const invariant (at entry and exit).
        #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            boost::contract::check c = boost::contract::public_function(this);
        #endif
        out << "a::c::body" << std::endl;
    }
    
    void v() volatile { // Test check cv invariant (at entry and exit).
        #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            boost::contract::check c = boost::contract::public_function(this);
        #endif
        out << "a::v::body" << std::endl;
    }
    
    void cv() const volatile { // Test check cv invariant (at entry and exit).
        #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            boost::contract::check c = boost::contract::public_function(this);
        #endif
        out << "a::cv::body" << std::endl;
    }
};

int main() {
    std::ostringstream ok;

    {
        out.str("");
        a aa;
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "a::static_inv" << std::endl
            #endif
            << "a::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::cv_inv" << std::endl
                << "a::const_inv" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));

        out.str("");
        aa.m();
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::const_inv" << std::endl
            #endif
            << "a::m::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::const_inv" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));

        out.str("");
        aa.c();
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::const_inv" << std::endl
            #endif
            << "a::c::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::const_inv" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));

        out.str("");
        aa.v();
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::cv_inv" << std::endl
            #endif
            << "a::v::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::cv_inv" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));

        out.str("");
        aa.cv();
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::cv_inv" << std::endl
            #endif
            << "a::cv::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::cv_inv" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    
        out.str("");
    } // Call dtor.
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::cv_inv" << std::endl
            << "a::const_inv" << std::endl
        #endif
        << "a::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

