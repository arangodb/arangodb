
// no #include guard

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with and without all invariants (static/cv/const-only) declarations.

#include "../detail/oteststream.hpp"
#include <boost/contract/base_types.hpp>
#include <boost/contract/constructor.hpp>
#include <boost/contract/destructor.hpp>
#include <boost/contract/public_function.hpp>
#include <boost/contract/function.hpp>
#include <boost/contract/override.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct b : private boost::contract::constructor_precondition<b> {
    // Test also with no base_types.

    #ifdef BOOST_CONTRACT_TEST_STATIC_INV
        static void static_invariant() { out << "b::static_inv" << std::endl; }
    #endif
    #ifdef BOOST_CONTRACT_TEST_CV_INV
        void invariant() const volatile { out << "b::cv_inv" << std::endl; }
    #endif
    #ifdef BOOST_CONTRACT_TEST_CONST_INV
        void invariant() const { out << "b::const_inv" << std::endl; }
    #endif
    
    b() : boost::contract::constructor_precondition<b>([] {
        out << "b::ctor::pre" << std::endl;
    }) {
        boost::contract::check c = boost::contract::constructor(this)
            .old([] { out << "b::ctor::old" << std::endl; })
            .postcondition([] { out << "b::ctor::post" << std::endl; })
        ;
        out << "b::ctor::body" << std::endl;
    }

    virtual ~b() {
        boost::contract::check c = boost::contract::destructor(this)
            .old([] { out << "b::dtor::old" << std::endl; })
            .postcondition([] { out << "b::dtor::post" << std::endl; })
        ;
        out << "b::dtor::body" << std::endl;
    }

    virtual void f(char x, boost::contract::virtual_* v = 0) volatile {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([&] {
                out << "b::f::volatile_pre" << std::endl;
                BOOST_CONTRACT_ASSERT(x == 'b');
            })
            .old([] { out << "b::f::volatile_old" << std::endl; })
            .postcondition([] { out << "b::f::volatile_post" << std::endl; })
        ;
        out << "b::f::volatile_body" << std::endl;
    }
    
    virtual void f(char x, boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([&] {
                out << "b::f::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(x == 'b');
            })
            .old([] { out << "b::f::old" << std::endl; })
            .postcondition([] { out << "b::f::post" << std::endl; })
        ;
        out << "b::f::body" << std::endl;
    }
};

struct a
    #define BASES private boost::contract::constructor_precondition<a>, public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    #ifdef BOOST_CONTRACT_TEST_STATIC_INV
        static void static_invariant() { out << "a::static_inv" << std::endl; }
    #endif
    #ifdef BOOST_CONTRACT_TEST_CV_INV
        void invariant() const volatile { out << "a::cv_inv" << std::endl; }
    #endif
    #ifdef BOOST_CONTRACT_TEST_CONST_INV
        void invariant() const { out << "a::const_inv" << std::endl; }
    #endif

    a() : boost::contract::constructor_precondition<a>([] {
        out << "a::ctor::pre" << std::endl;
    }) {
        boost::contract::check c = boost::contract::constructor(this)
            .old([] { out << "a::ctor::old" << std::endl; })
            .postcondition([] { out << "a::ctor::post" << std::endl; })
        ;
        out << "a::ctor::body" << std::endl;
    }
    
    virtual ~a() {
        boost::contract::check c = boost::contract::destructor(this)
            .old([] { out << "a::dtor::old" << std::endl; })
            .postcondition([] { out << "a::dtor::post" << std::endl; })
        ;
        out << "a::dtor::body" << std::endl;
    }

    virtual void f(char x, boost::contract::virtual_* v = 0)
            volatile /* override */ {
        boost::contract::check c = boost::contract::public_function<
                override_f>(
            v,
            static_cast<void (a::*)(char x, boost::contract::virtual_*)
                    volatile>(&a::f),
            this, x
        )
            .precondition([&] {
                out << "a::f::volatile_pre" << std::endl;
                BOOST_CONTRACT_ASSERT(x == 'a');
            })
            .old([] { out << "a::f::volatile_old" << std::endl; })
            .postcondition([] { out << "a::f::volatile_post" << std::endl; })
        ;
        out << "a::f::volatile_body" << std::endl;
    }
    
    virtual void f(char x, boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<
                override_f>(
            v,
            static_cast<void (a::*)(char x, boost::contract::virtual_*)>(&a::f),
            this, x
        )
            .precondition([&] {
                out << "a::f::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(x == 'a');
            })
            .old([] { out << "a::f::old" << std::endl; })
            .postcondition([] { out << "a::f::post" << std::endl; })
        ;
        out << "a::f::body" << std::endl;
    }

    static void s() {
        boost::contract::check c = boost::contract::public_function<a>()
            .precondition([] { out << "a::s::pre" << std::endl; })
            .old([] { out << "a::s::old" << std::endl; })
            .postcondition([] { out << "a::s::post" << std::endl; })
        ;
        out << "a::s::body" << std::endl;
    }

protected:
    void p() volatile {
        boost::contract::check c = boost::contract::function()
            .precondition([] { out << "a::p::volatile_pre" << std::endl; })
            .old([] { out << "a::p::volatile_old" << std::endl; })
            .postcondition([] { out << "a::p::volatile_post" << std::endl; })
        ;
        out << "a::p::volatile_body" << std::endl;
    }
    
    void p() {
        boost::contract::check c = boost::contract::function()
            .precondition([] { out << "a::p::pre" << std::endl; })
            .old([] { out << "a::p::old" << std::endl; })
            .postcondition([] { out << "a::p::post" << std::endl; })
        ;
        out << "a::p::body" << std::endl;
    }
public:
    void call_p() volatile { p(); }
    void call_p() { p(); }

private:
    void q() volatile {
        boost::contract::check c = boost::contract::function()
            .precondition([] { out << "a::q::volatile_pre" << std::endl; })
            .old([] { out << "a::q::volatile_old" << std::endl; })
            .postcondition([] { out << "a::q::volatile_post" << std::endl; })
        ;
        out << "a::q::volatile_body" << std::endl;
    }
    
    void q() {
        boost::contract::check c = boost::contract::function()
            .precondition([] { out << "a::q::pre" << std::endl; })
            .old([] { out << "a::q::old" << std::endl; })
            .postcondition([] { out << "a::q::post" << std::endl; })
        ;
        out << "a::q::body" << std::endl;
    }
public:
    void call_q() volatile { q(); }
    void call_q() { q(); }

    BOOST_CONTRACT_OVERRIDE(f)
};

int main() {
    std::ostringstream ok;

    { // Test volatile call with bases.
        out.str("");
        a volatile av;
        ok.str(""); ok // Ctors always check const_inv (even if volatile).
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::ctor::pre" << std::endl
                << "b::ctor::pre" << std::endl
            #endif

            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "b::ctor::old" << std::endl
            #endif
            << "b::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "b::cv_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "b::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "b::ctor::post" << std::endl
            #endif

            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::ctor::old" << std::endl
            #endif
            << "a::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "a::cv_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "a::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::ctor::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));

        out.str("");
        av.f('a');
        ok.str(""); ok // Volatile checks static and cv (but not const) inv.
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "b::cv_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "a::cv_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "b::f::volatile_pre" << std::endl
                << "a::f::volatile_pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "b::f::volatile_old" << std::endl
                << "a::f::volatile_old" << std::endl
            #endif
            << "a::f::volatile_body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "b::cv_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "a::cv_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "b::f::volatile_old" << std::endl
                << "b::f::volatile_post" << std::endl
                << "a::f::volatile_post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    
        out.str("");
        av.s(); // Test static call.
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::s::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::s::old" << std::endl
            #endif
            << "a::s::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::s::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        out.str("");
        av.call_p(); // Test (indirect) protected call.
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::p::volatile_pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::p::volatile_old" << std::endl
            #endif
            << "a::p::volatile_body" << std::endl
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::p::volatile_post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        out.str("");
        av.call_q(); // Test (indirect) private call.
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::q::volatile_pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::q::volatile_old" << std::endl
            #endif
            << "a::q::volatile_body" << std::endl
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::q::volatile_post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        out.str("");
    } // Call a's destructor.
    ok.str(""); ok // Dtors always check const_inv (even if volatile).
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "a::static_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CV_INV
                << "a::cv_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CONST_INV
                << "a::const_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::dtor::old" << std::endl
        #endif
        << "a::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "a::static_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::dtor::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "b::static_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CV_INV
                << "b::cv_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CONST_INV
                << "b::const_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::dtor::old" << std::endl
        #endif
        << "b::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "b::static_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::dtor::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    { // Test non-volatile call with bases.
        out.str("");
        a aa;
        ok.str(""); ok // Ctors always check cv_inv (even if not volatile).
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::ctor::pre" << std::endl
                << "b::ctor::pre" << std::endl
            #endif

            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "b::ctor::old" << std::endl
            #endif
            << "b::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "b::cv_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "b::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "b::ctor::post" << std::endl
            #endif

            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::ctor::old" << std::endl
            #endif
            << "a::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "a::cv_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "a::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::ctor::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));

        out.str("");
        aa.f('a');
        ok.str(""); ok // Non-cv checks static and const (but not cv) inv.
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "b::const_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "a::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "b::f::pre" << std::endl
                << "a::f::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "b::f::old" << std::endl
                << "a::f::old" << std::endl
            #endif
            << "a::f::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "b::const_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "a::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "b::f::old" << std::endl
                << "b::f::post" << std::endl
                << "a::f::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        out.str("");
        aa.s(); // Test static call.
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::s::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::s::old" << std::endl
            #endif
            << "a::s::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "a::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::s::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        out.str("");
        aa.call_p(); // Test (indirect) protected call.
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::p::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::p::old" << std::endl
            #endif
            << "a::p::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::p::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        out.str("");
        aa.call_q(); // Test (indirect) private call.
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::q::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::q::old" << std::endl
            #endif
            << "a::q::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::q::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));

        out.str("");
    } // Call a's destructor.
    ok.str(""); ok // Dtors always check cv_inv (even if not volatile).
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "a::static_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CV_INV
                << "a::cv_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CONST_INV
                << "a::const_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::dtor::old" << std::endl
        #endif
        << "a::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "a::static_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::dtor::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "b::static_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CV_INV
                << "b::cv_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CONST_INV
                << "b::const_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::dtor::old" << std::endl
        #endif
        << "b::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "b::static_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::dtor::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    
    { // Test volatile call with no bases.
        out.str("");
        b volatile bv;
        ok.str(""); ok // Ctors always check const_inv (even if volatile).
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "b::ctor::pre" << std::endl
            #endif

            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "b::ctor::old" << std::endl
            #endif
            << "b::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "b::cv_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "b::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "b::ctor::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        out.str("");
        bv.f('b');
        ok.str(""); ok // Volatile checks static and cv (but not const) inv.
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "b::cv_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "b::f::volatile_pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "b::f::volatile_old" << std::endl
            #endif
            << "b::f::volatile_body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "b::cv_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "b::f::volatile_post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        out.str("");
    } // Call b's destructor.
    ok.str(""); ok // Dtors always check const_inv (even if volatile).
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "b::static_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CV_INV
                << "b::cv_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CONST_INV
                << "b::const_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::dtor::old" << std::endl
        #endif
        << "b::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "b::static_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::dtor::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    { // Test non-volatile call with no bases.
        out.str("");
        b bb;
        ok.str(""); ok // Ctors always check cv_inv (even if not volatile).
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "b::ctor::pre" << std::endl
            #endif
            
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "b::ctor::old" << std::endl
            #endif
            << "b::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CV_INV
                    << "b::cv_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "b::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "b::ctor::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));

        out.str("");
        bb.f('b');
        ok.str(""); ok // Non-cv checks static and const (but not cv) inv.
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "b::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "b::f::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "b::f::old" << std::endl
            #endif
            << "b::f::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                    << "b::static_inv" << std::endl
                #endif
                #ifdef BOOST_CONTRACT_TEST_CONST_INV
                    << "b::const_inv" << std::endl
                #endif
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "b::f::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        out.str("");
    } // Call b's destructor.
    ok.str(""); ok // Dtors always check cv_inv (even if not volatile).
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "b::static_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CV_INV
                << "b::cv_inv" << std::endl
            #endif
            #ifdef BOOST_CONTRACT_TEST_CONST_INV
                << "b::const_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::dtor::old" << std::endl
        #endif
        << "b::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            #ifdef BOOST_CONTRACT_TEST_STATIC_INV
                << "b::static_inv" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::dtor::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

