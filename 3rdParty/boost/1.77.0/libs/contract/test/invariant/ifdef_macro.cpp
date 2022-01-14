
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test invariant compilation on/off (using macro interface).

#include "../detail/oteststream.hpp"
#include "../detail/unprotected_commas.hpp"
#include <boost/contract_macro.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

class a {
public:
    BOOST_CONTRACT_STATIC_INVARIANT({
        boost::contract::test::detail::unprotected_commas<void, void, void>::
                call();
        out << "a::static_inv" << std::endl;
    })
    
    BOOST_CONTRACT_INVARIANT_VOLATILE({
        boost::contract::test::detail::unprotected_commas<void, void, void>::
                call();
        out << "a::cv_inv" << std::endl;
    })

    BOOST_CONTRACT_INVARIANT({
        boost::contract::test::detail::unprotected_commas<void, void, void>::
                call();
        out << "a::const_inv" << std::endl;
    })

    a() { // Test check both cv and const invariant (at exit if no throw).
        BOOST_CONTRACT_CONSTRUCTOR(boost::contract::test::detail::
                unprotected_commas<void, void, void>::same(this));
        out << "a::ctor::body" << std::endl;
    }

    ~a() { // Test check both cv and const invariant (at entry).
        BOOST_CONTRACT_DESTRUCTOR(boost::contract::test::detail::
                unprotected_commas<void, void, void>::same(this));
        out << "a::dtor::body" << std::endl;
    }

    void m() { // Test check const invariant (at entry and exit).
        BOOST_CONTRACT_PUBLIC_FUNCTION(boost::contract::test::detail::
                unprotected_commas<void, void, void>::same(this));
        out << "a::m::body" << std::endl;
    }

    void c() const { // Test check const invariant (at entry and exit).
        BOOST_CONTRACT_PUBLIC_FUNCTION(boost::contract::test::detail::
                unprotected_commas<void, void, void>::same(this));
        out << "a::c::body" << std::endl;
    }
    
    void v() volatile { // Test check cv invariant (at entry and exit).
        BOOST_CONTRACT_PUBLIC_FUNCTION(boost::contract::test::detail::
                unprotected_commas<void, void, void>::same(this));
        out << "a::v::body" << std::endl;
    }
    
    void cv() const volatile { // Test check cv invariant (at entry and exit).
        BOOST_CONTRACT_PUBLIC_FUNCTION(boost::contract::test::detail::
                unprotected_commas<void, void, void>::same(this));
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

