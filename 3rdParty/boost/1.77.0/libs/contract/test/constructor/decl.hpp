
#ifndef BOOST_CONTRACT_TEST_CONSTRUCTOR_DECL_HPP_
#define BOOST_CONTRACT_TEST_CONSTRUCTOR_DECL_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with and without pre, post, and inv declarations.

#include "../detail/oteststream.hpp"
#include <boost/contract/constructor.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>

boost::contract::test::detail::oteststream out;

bool c_pre = true, c_post = true;
bool c_entering_static_inv = true, c_entry_static_inv = true,
        c_exit_static_inv = true;
bool c_exit_inv = true; // Only exit non-static inv for ctors.
struct c
    #ifndef BOOST_CONTRACT_TEST_NO_C_PRE
        : private boost::contract::constructor_precondition<c>
    #endif
{
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
            BOOST_CONTRACT_ASSERT(c_exit_inv);
        }
    #endif

    c()
        #ifndef BOOST_CONTRACT_TEST_NO_C_PRE
            : boost::contract::constructor_precondition<c>([] {
                out << "c::ctor::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(c_pre);
            })
        #endif
    {
        boost::contract::check c = boost::contract::constructor(this)
            .old([] { out << "c::ctor::old" << std::endl; })
            #ifndef BOOST_CONTRACT_TEST_NO_C_POST
                .postcondition([] {
                    out << "c::ctor::post" << std::endl;
                    BOOST_CONTRACT_ASSERT(c_post);
                })
            #endif
        ;
        out << "c::ctor::body" << std::endl;
    }
};

bool b_pre = true, b_post = true;
bool b_entering_static_inv = true, b_entry_static_inv = true,
        b_exit_static_inv = true;
bool b_exit_inv = true; // Only exit non-static inv for ctors.
struct b
    #ifndef BOOST_CONTRACT_TEST_NO_B_PRE
        #define BASES \
            private boost::contract::constructor_precondition<b>, public c
    #else
        #define BASES public c
    #endif
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
            BOOST_CONTRACT_ASSERT(b_exit_inv);
        }
    #endif

    b()
        #ifndef BOOST_CONTRACT_TEST_NO_B_PRE
            : boost::contract::constructor_precondition<b>([] {
                out << "b::ctor::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(b_pre);
            })
        #endif
    {
        boost::contract::check c = boost::contract::constructor(this)
            .old([] { out << "b::ctor::old" << std::endl; })
            #ifndef BOOST_CONTRACT_TEST_NO_B_POST
                .postcondition([] {
                    out << "b::ctor::post" << std::endl;
                    BOOST_CONTRACT_ASSERT(b_post);
                })
            #endif
        ;
        out << "b::ctor::body" << std::endl;
    }
};

bool a_pre = true, a_post = true;
bool a_entering_static_inv = true, a_entry_static_inv = true,
        a_exit_static_inv = true;
bool a_exit_inv = true; // Only exit non-static inv for ctors.
struct a
    #ifndef BOOST_CONTRACT_TEST_NO_A_PRE
        #define BASES \
            private boost::contract::constructor_precondition<a>, public b
    #else
        #define BASES public b
    #endif
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
            BOOST_CONTRACT_ASSERT(a_exit_inv);
        }
    #endif

    a()
        #ifndef BOOST_CONTRACT_TEST_NO_A_PRE
            : boost::contract::constructor_precondition<a>([] {
                out << "a::ctor::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(a_pre);
            })
        #endif
    {
        boost::contract::check c = boost::contract::constructor(this)
            .old([] { out << "a::ctor::old" << std::endl; })
            #ifndef BOOST_CONTRACT_TEST_NO_A_POST
                .postcondition([] {
                    out << "a::ctor::post" << std::endl;
                    BOOST_CONTRACT_ASSERT(a_post);
                })
            #endif
        ;
        out << "a::ctor::body" << std::endl;
    }
};

#endif // #include guard

