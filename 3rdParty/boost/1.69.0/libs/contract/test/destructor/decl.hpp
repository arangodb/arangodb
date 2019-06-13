
#ifndef BOOST_CONTRACT_TEST_DESTRUCTOR_DECL_HPP_
#define BOOST_CONTRACT_TEST_DESTRUCTOR_DECL_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with and without pre, post, and inv declarations.

#include "../detail/oteststream.hpp"
#include <boost/contract/destructor.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>
#include <boost/config.hpp>

boost::contract::test::detail::oteststream out;

bool c_pre = true, c_post = true;
bool c_entering_static_inv = true, c_entry_static_inv = true,
        c_exit_static_inv = true;
bool c_entry_inv = true; // Only entry non-static inv for dtors.
struct c {
    #ifndef BOOST_CONTRACT_TEST_NO_C_STATIC_INV
        static void static_invariant() {
            out << "c::static_inv" << std::endl;
            if(c_entering_static_inv) BOOST_CONTRACT_ASSERT(c_entry_static_inv);
            else BOOST_CONTRACT_ASSERT(c_exit_static_inv);
            c_entering_static_inv = false;
        }
    #endif
    #ifndef BOOST_CONTRACT_TEST_NO_C_INV
        void invariant() const {
            out << "c::inv" << std::endl;
            BOOST_CONTRACT_ASSERT(c_entry_inv);
        }
    #endif

    virtual ~c() BOOST_NOEXCEPT_IF(false) {
        boost::contract::check c = boost::contract::destructor(this)
            #ifdef BOOST_CONTRACT_TEST_NO_C_PRE
                #error "destructors cannot have preconditions"
            #endif
            .old([] { out << "c::dtor::old" << std::endl; })
            #ifndef BOOST_CONTRACT_TEST_NO_C_POST
                .postcondition([] {
                    out << "c::dtor::post" << std::endl;
                    BOOST_CONTRACT_ASSERT(c_post);
                })
            #endif
        ;
        out << "c::dtor::body" << std::endl;
    }
};

bool b_pre = true, b_post = true;
bool b_entering_static_inv = true, b_entry_static_inv = true,
        b_exit_static_inv = true;
bool b_entry_inv = true; // Only entry non-static inv for dtors.
struct b
    #define BASES public c
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    #ifndef BOOST_CONTRACT_TEST_NO_B_STATIC_INV
        static void static_invariant() {
            out << "b::static_inv" << std::endl;
            if(b_entering_static_inv) BOOST_CONTRACT_ASSERT(b_entry_static_inv);
            else BOOST_CONTRACT_ASSERT(b_exit_static_inv);
            b_entering_static_inv = false;
        }
    #endif
    #ifndef BOOST_CONTRACT_TEST_NO_B_INV
        void invariant() const {
            out << "b::inv" << std::endl;
            BOOST_CONTRACT_ASSERT(b_entry_inv);
        }
    #endif

    virtual ~b() BOOST_NOEXCEPT_IF(false) {
        boost::contract::check c = boost::contract::destructor(this)
            #ifdef BOOST_CONTRACT_TEST_NO_B_PRE
                #error "destructors cannot have preconditions"
            #endif
            .old([] { out << "b::dtor::old" << std::endl; })
            #ifndef BOOST_CONTRACT_TEST_NO_B_POST
                .postcondition([] {
                    out << "b::dtor::post" << std::endl;
                    BOOST_CONTRACT_ASSERT(b_post);
                })
            #endif
        ;
        out << "b::dtor::body" << std::endl;
    }
};

bool a_pre = true, a_post = true;
bool a_entering_static_inv = true, a_entry_static_inv = true,
        a_exit_static_inv = true;
bool a_entry_inv = true; // Only entry non-static inv for dtors.
struct a
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    #ifndef BOOST_CONTRACT_TEST_NO_A_STATIC_INV
        static void static_invariant() {
            out << "a::static_inv" << std::endl;
            if(a_entering_static_inv) BOOST_CONTRACT_ASSERT(a_entry_static_inv);
            else BOOST_CONTRACT_ASSERT(a_exit_static_inv);
            a_entering_static_inv = false;
        }
    #endif
    #ifndef BOOST_CONTRACT_TEST_NO_A_INV
        void invariant() const {
            out << "a::inv" << std::endl;
            BOOST_CONTRACT_ASSERT(a_entry_inv);
        }
    #endif

    virtual ~a() BOOST_NOEXCEPT_IF(false) {
        boost::contract::check c = boost::contract::destructor(this)
            #ifdef BOOST_CONTRACT_TEST_NO_A_PRE
                #error "destructors cannot have preconditions"
            #endif
            .old([] { out << "a::dtor::old" << std::endl; })
            #ifndef BOOST_CONTRACT_TEST_NO_A_POST
                .postcondition([] {
                    out << "a::dtor::post" << std::endl;
                    BOOST_CONTRACT_ASSERT(a_post);
                })
            #endif
        ;
        out << "a::dtor::body" << std::endl;
    }
};

#endif // #include guard

