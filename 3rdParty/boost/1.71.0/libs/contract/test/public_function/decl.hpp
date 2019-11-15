
#ifndef BOOST_CONTRACT_TEST_PUBLIC_FUNCTION_DECL_HPP_
#define BOOST_CONTRACT_TEST_PUBLIC_FUNCTION_DECL_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with and without pre, post, and inv declarations.

#include "../detail/oteststream.hpp"
#include <boost/contract/public_function.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/override.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>

boost::contract::test::detail::oteststream out;

bool c_pre = true, c_post = true;
bool c_entering_static_inv = true, c_entry_static_inv = true,
        c_exit_static_inv = true;
bool c_entering_inv = true, c_entry_inv = true, c_exit_inv = true;
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
            if(c_entering_inv) BOOST_CONTRACT_ASSERT(c_entry_inv);
            else BOOST_CONTRACT_ASSERT(c_exit_inv);
            c_entering_inv = false;
        }
    #endif

    virtual void f(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            #ifndef BOOST_CONTRACT_TEST_NO_C_PRE
                .precondition([] {
                    out << "c::f::pre" << std::endl;
                    BOOST_CONTRACT_ASSERT(c_pre);
                })
            #endif
            .old([] { out << "c::f::old" << std::endl; })
            #ifndef BOOST_CONTRACT_TEST_NO_C_POST
                .postcondition([] {
                    out << "c::f::post" << std::endl;
                    BOOST_CONTRACT_ASSERT(c_post);
                })
            #endif
        ;
        out << "c::f::body" << std::endl;
    }
};

bool b_pre = true, b_post = true;
bool b_entering_static_inv = true, b_entry_static_inv = true,
        b_exit_static_inv = true;
bool b_entering_inv = true, b_entry_inv = true, b_exit_inv = true;
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
            if(b_entering_inv) BOOST_CONTRACT_ASSERT(b_entry_inv);
            else BOOST_CONTRACT_ASSERT(b_exit_inv);
            b_entering_inv = false;
        }
    #endif

    virtual void f(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            #ifndef BOOST_CONTRACT_TEST_NO_B_PRE
                .precondition([] {
                    out << "b::f::pre" << std::endl;
                    BOOST_CONTRACT_ASSERT(b_pre);
                })
            #endif
            .old([] { out << "b::f::old" << std::endl; })
            #ifndef BOOST_CONTRACT_TEST_NO_B_POST
                .postcondition([] {
                    out << "b::f::post" << std::endl;
                    BOOST_CONTRACT_ASSERT(b_post);
                })
            #endif
        ;
        out << "a::f::body" << std::endl;
    }
};

bool a_pre = true, a_post = true;
bool a_entering_static_inv = true, a_entry_static_inv = true,
        a_exit_static_inv = true;
bool a_entering_inv = true, a_entry_inv = true, a_exit_inv = true;
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
            if(a_entering_inv) BOOST_CONTRACT_ASSERT(a_entry_inv);
            else BOOST_CONTRACT_ASSERT(a_exit_inv);
            a_entering_inv = false;
        }
    #endif

    virtual void f(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
                v, &a::f, this)
            #ifndef BOOST_CONTRACT_TEST_NO_A_PRE
                .precondition([] {
                    out << "a::f::pre" << std::endl;
                    BOOST_CONTRACT_ASSERT(a_pre);
                })
            #endif
            .old([] { out << "a::f::old" << std::endl; })
            #ifndef BOOST_CONTRACT_TEST_NO_A_POST
                .postcondition([] {
                    out << "a::f::post" << std::endl;
                    BOOST_CONTRACT_ASSERT(a_post);
                })
            #endif
        ;
        out << "a::f::body" << std::endl;
    }
    BOOST_CONTRACT_OVERRIDE(f)
};

#endif // #include guard

